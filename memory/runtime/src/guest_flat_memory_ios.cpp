#include "guest_flat_memory.h"
#include <mach/mach.h>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef MKW_IOS_MEMORY_TESTING
#include "ios_memory_test_hooks.h"
#endif

namespace GuestFlat {
uintptr_t g_flatGuestBase = 0;
bool g_requiresCheckedAccess = false;
namespace {
struct Store { Backing kind; uint32_t owner; uint64_t size; vm_address_t host = 0; };
struct Mapping { uint32_t base; uint64_t size; uint8_t* host; };
std::mutex mutex;
std::vector<Store> stores;
std::vector<Mapping> mappings;
std::vector<RegionRequest> layout;
bool active = false;
#ifdef MKW_IOS_MEMORY_TESTING
Test::FailurePoint failurePoint;
unsigned failureCountdown = 0;
size_t liveAllocations = 0;
bool Fail(Test::FailurePoint point) {
    return failureCountdown && point == failurePoint && --failureCountdown == 0;
}
#define VM_FAIL(point) Fail(Test::FailurePoint::point)
#define VM_ACQUIRED() (++liveAllocations)
#define VM_RELEASED() (--liveAllocations)
#else
#define VM_FAIL(point) false
#define VM_ACQUIRED() ((void)0)
#define VM_RELEASED() ((void)0)
#endif
void VmError(const char* operation, kern_return_t result) {
    if(result != KERN_SUCCESS)
        throw std::runtime_error(std::string(operation)+": "+mach_error_string(result));
}
uint64_t Round(uint64_t value, uint64_t page) { return (value+page-1)&~(page-1); }
uint64_t Offset(const RegionRequest& r) {
    const uint64_t low=r.base&0x1fffffffu;
    if(r.backing==Backing::Mem1) {
        if(low>=0x10000000) throw std::runtime_error("invalid MEM1 backing offset");
        return low;
    }
    if(r.backing==Backing::Mem2) {
        if(low<0x10000000) throw std::runtime_error("invalid MEM2 backing offset");
        return low-0x10000000;
    }
    if(r.backing!=Backing::Owned) throw std::runtime_error("invalid backing kind");
    return 0;
}
bool Same(const std::vector<RegionRequest>& a,const std::vector<RegionRequest>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y){
        return x.base==y.base && x.size==y.size && x.backing==y.backing;
    });
}
void Release(vm_address_t address, vm_size_t size) noexcept {
    if(vm_deallocate(mach_task_self(),address,size)==KERN_SUCCESS) VM_RELEASED();
}
}
bool IsActive() { return active; }
void Initialize(const std::vector<RegionRequest>& regions) {
    std::lock_guard lock(mutex);
    if(active) {
        if(!Same(layout,regions)) throw std::runtime_error("flat guest layout cannot be remapped");
        return;
    }
    const uint64_t page=static_cast<uint64_t>(getpagesize());
    std::vector<Store> pendingStores;
    std::vector<Mapping> pendingMappings;
    auto pendingLayout=regions;
    for(size_t i=0;i<regions.size();++i) {
        const auto& r=regions[i];
        if(!r.size) continue;
        if(r.size>kGuestSpaceSize-r.base || r.base%page || Offset(r)%page ||
           Round(r.size,page)>kGuestSpaceSize-r.base)
            throw std::runtime_error("invalid or overflowing iOS guest region");
        const uint64_t end=uint64_t(r.base)+Round(r.size,page);
        for(size_t j=0;j<i;++j) {
            const auto& prior=regions[j];
            if(prior.size && r.base<uint64_t(prior.base)+Round(prior.size,page) && prior.base<end)
                throw std::runtime_error("overlapping iOS guest memory regions");
        }
        const uint32_t owner=r.backing==Backing::Owned ? r.base : 0;
        const uint64_t need=Round(Offset(r)+r.size,page);
        auto it=std::find_if(pendingStores.begin(),pendingStores.end(),[&](const Store& s){
            return s.kind==r.backing && s.owner==owner;
        });
        if(it==pendingStores.end()) pendingStores.push_back({r.backing,owner,need});
        else it->size=std::max(it->size,need);
    }
    if(pendingStores.empty()) throw std::runtime_error("empty iOS guest memory layout");
    pendingMappings.reserve(regions.size());
    vm_address_t reserve=0;
    bool reserved=false;
    try {
        VmError("reserve dynamic 4 GiB",VM_FAIL(Reserve)?KERN_RESOURCE_SHORTAGE:
            vm_allocate(mach_task_self(),&reserve,static_cast<vm_size_t>(kGuestSpaceSize),VM_FLAGS_ANYWHERE));
        reserved=true; VM_ACQUIRED();
        VmError("protect guest holes",VM_FAIL(Protect)?KERN_PROTECTION_FAILURE:
            vm_protect(mach_task_self(),reserve,kGuestSpaceSize,FALSE,VM_PROT_NONE));
        for(auto& s:pendingStores) {
            vm_address_t backing=0;
            VmError("allocate guest backing",VM_FAIL(Backing)?KERN_RESOURCE_SHORTAGE:
                vm_allocate(mach_task_self(),&backing,static_cast<vm_size_t>(s.size),VM_FLAGS_ANYWHERE));
            s.host=backing; VM_ACQUIRED();
        }
        for(const auto& r:regions) {
            if(!r.size) continue;
            const uint32_t owner=r.backing==Backing::Owned ? r.base : 0;
            const auto& s=*std::find_if(pendingStores.begin(),pendingStores.end(),[&](const Store& x){
                return x.kind==r.backing && x.owner==owner;
            });
            const vm_address_t expected=reserve+r.base;
            vm_address_t target=expected;
            vm_prot_t current=VM_PROT_NONE,maximum=VM_PROT_NONE;
            VmError("remap shared guest alias",VM_FAIL(Remap)?KERN_RESOURCE_SHORTAGE:
                vm_remap(mach_task_self(),&target,static_cast<vm_size_t>(Round(r.size,page)),0,
                    VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE,mach_task_self(),s.host+Offset(r),FALSE,
                    &current,&maximum,VM_INHERIT_NONE));
            if(target!=expected) throw std::runtime_error("guest alias mapped at unexpected address");
            pendingMappings.push_back({r.base,r.size,reinterpret_cast<uint8_t*>(s.host+Offset(r))});
        }
    } catch(...) {
        if(reserved) Release(reserve,static_cast<vm_size_t>(kGuestSpaceSize));
        for(auto it=pendingStores.rbegin();it!=pendingStores.rend();++it)
            if(it->host) Release(it->host,static_cast<vm_size_t>(it->size));
        throw;
    }
    stores=std::move(pendingStores);
    mappings=std::move(pendingMappings);
    layout=std::move(pendingLayout);
    g_requiresCheckedAccess=page>kGuestPageSize;
    g_flatGuestBase=reserve;
    active=true;
}
uint8_t* HostPointer(uint32_t address) {
    for(const auto& m:mappings)
        if(address>=m.base && uint64_t(address-m.base)<m.size) return m.host+(address-m.base);
    return nullptr;
}
// Checked Memory handles per-Wii-page policies on iPad's wider host pages.
void ProtectDeferredRange(uint32_t,size_t) {}
void UnprotectDeferredRange(uint32_t,size_t) {}
void RegisterExecutableRange(uint32_t,uint32_t) {}
FaultCounters Counters() { return {}; }
void LogFaultSummary() noexcept {}
bool HandleAccessViolation(void*,bool) noexcept { return false; }
#ifdef MKW_IOS_MEMORY_TESTING
namespace Test {
void SetFailure(FailurePoint point,unsigned occurrence) { failurePoint=point; failureCountdown=occurrence; }
void ClearFailure() { failureCountdown=0; }
size_t LiveAllocationCount() { return liveAllocations; }
}
#endif
}
