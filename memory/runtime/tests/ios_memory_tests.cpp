#include "ios_memory_tests.h"
#include "ios_memory_test_hooks.h"
#include "guest_flat_memory.h"
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
using GuestFlat::Backing;
using GuestFlat::RegionRequest;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F action) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, "expected invalid request or injected failure to throw");
}
void Clean() {
    Check(!GuestFlat::IsActive(), "failed initialization published active state");
    Check(GuestFlat::g_flatGuestBase == 0, "failed initialization published base");
    Check(GuestFlat::HostPointer(0) == nullptr, "failed initialization published mappings");
    Check(GuestFlat::Test::LiveAllocationCount() == 0, "failed initialization leaked VM allocations");
}
std::vector<RegionRequest> WiiMap() {
    return {{0,0x01800000,Backing::Mem1},{0x80000000,0x01800000,Backing::Mem1},
      {0xc0000000,0x01800000,Backing::Mem1},{0x10000000,0x08000000,Backing::Mem2},
      {0x90000000,0x08000000,Backing::Mem2},{0xd0000000,0x08000000,Backing::Mem2},
      {0x81800000,0x00200000,Backing::Owned},{0xe0000000,0x00101000,Backing::Owned}};
}
void Mirrors(uint32_t physical, uint32_t cached, uint32_t uncached, uint32_t size) {
    // Kernel aliases share backing at distinct virtual addresses. Force real
    // observations rather than letting C++ treat disjoint guest offsets as
    // independent objects and reuse a pre-write value. Test instrumentation only.
    volatile uint8_t* p = GuestFlat::HostPointer(physical);
    volatile uint8_t* c = GuestFlat::HostPointer(cached);
    volatile uint8_t* u = GuestFlat::HostPointer(uncached);
    auto* g = reinterpret_cast<volatile uint8_t*>(GuestFlat::g_flatGuestBase);
    Check(p && c && u && g, "missing real Wii mapping");
    auto* gp=g+physical;
    auto* gc=g+cached;
    auto* gu=g+uncached;
    const uint32_t page = static_cast<uint32_t>(getpagesize());
    // Different values per page catch wrong backing offsets, not merely map existence.
    for (uint32_t offset=0; offset<size; offset+=page) p[offset] = uint8_t((offset/page)%251+1);
    for (uint32_t offset=0; offset<size; offset+=page) {
        const uint8_t expected = uint8_t((offset/page)%251+1);
        Check(c[offset]==expected && u[offset]==expected && gp[offset]==expected &&
              gc[offset]==expected && gu[offset]==expected,
              "full-capacity alias coherence failed");
    }
    for (uint32_t offset : {0u,page-1,page,page+1,size-1}) {
        gp[offset]=0x31;
        Check(gc[offset]==0x31 && gu[offset]==0x31 && p[offset]==0x31,
              "physical guest write invisible through aliases");
        gc[offset]=0x5a;
        Check(gp[offset]==0x5a && gu[offset]==0x5a && u[offset]==0x5a,
              "cached guest write invisible through aliases");
        gu[offset]=0xa5;
        Check(gp[offset]==0xa5 && gc[offset]==0xa5 && p[offset]==0xa5 && c[offset]==0xa5,
              "uncached guest write invisible through aliases");
    }
    Check(GuestFlat::HostPointer(uncached+size)==nullptr, "alias accepted address beyond logical end");
}
}

bool RunIosMemoryTests(std::function<void(const std::string&)> report) {
    try {
        Check(!GuestFlat::IsActive(), "tests must run once in a fresh process");
        Reject([] { GuestFlat::Initialize({}); }); Clean();
        Reject([] { GuestFlat::Initialize({{0,0,Backing::Mem1}}); }); Clean();
        Reject([] { GuestFlat::Initialize({{1,0x4000,Backing::Owned}}); }); Clean();
        Reject([] { GuestFlat::Initialize({{0x80000000,0x4000,Backing::Mem2}}); }); Clean();
        Reject([] { GuestFlat::Initialize({{0,0x4001,Backing::Mem1},{0x4000,0x4000,Backing::Mem1}}); }); Clean();
        Reject([] { GuestFlat::Initialize({{0xffffc000,0x4001,Backing::Owned}}); }); Clean();
        report("Invalid layouts and rounded boundaries: PASS");
        using GuestFlat::Test::FailurePoint;
        for (auto point : {FailurePoint::Reserve,FailurePoint::Protect,FailurePoint::Backing,FailurePoint::Remap}) {
            GuestFlat::Test::SetFailure(point,1);
            Reject([] { GuestFlat::Initialize(WiiMap()); }); Clean();
            GuestFlat::Test::ClearFailure();
        }
        GuestFlat::Test::SetFailure(FailurePoint::Backing,2);
        Reject([] { GuestFlat::Initialize(WiiMap()); }); Clean(); GuestFlat::Test::ClearFailure();
        GuestFlat::Test::SetFailure(FailurePoint::Remap,2);
        Reject([] { GuestFlat::Initialize(WiiMap()); }); Clean(); GuestFlat::Test::ClearFailure();
        report("Partial failure cleanup and retry: PASS");
        GuestFlat::Initialize(WiiMap());
        Check(GuestFlat::IsActive(), "memory initialization never became active");
        Check(GuestFlat::RequiresCheckedAccess()==(getpagesize()>4096), "wrong checked-access mode");
        Check(GuestFlat::g_flatGuestBase && GuestFlat::g_flatGuestBase%getpagesize()==0, "invalid dynamic base");
        Mirrors(0,0x80000000,0xc0000000,0x01800000);
        Mirrors(0x10000000,0x90000000,0xd0000000,0x08000000);
        report("MEM1 24 MiB and MEM2 128 MiB, three mirrors each: PASS");
        auto* overlay=GuestFlat::HostPointer(0x81800000);
        auto* locked=GuestFlat::HostPointer(0xe0000000);
        Check(overlay && locked, "missing owned regions");
        overlay[0]=0x17; locked[0]=0x29;
        Check(GuestFlat::HostPointer(0)[0]==0xa5 && GuestFlat::HostPointer(0x10000000)[0]==0xa5,
              "owned region overwrote MEM1 or MEM2");
        GuestFlat::HostPointer(0)[0]=0x3c;
        Check(GuestFlat::HostPointer(0x10000000)[0]==0xa5 && overlay[0]==0x17 && locked[0]==0x29,
              "independent backing stores alias each other");
        overlay[0x1fffff]=0x71; locked[0x101000-1]=0x92;
        Check(GuestFlat::HostPointer(0x81a00000)==nullptr && GuestFlat::HostPointer(0xe0101000)==nullptr,
              "owned region logical bounds were rounded outward");
        for (uint32_t hole : {0x01800000u,0x18000000u,0x40000000u,0xcc000000u,0xffffffffu})
            Check(GuestFlat::HostPointer(hole)==nullptr,"hole resolved to host RAM");
        const auto base=GuestFlat::g_flatGuestBase;
        const auto allocations=GuestFlat::Test::LiveAllocationCount();
        GuestFlat::Initialize(WiiMap());
        Check(base==GuestFlat::g_flatGuestBase && allocations==GuestFlat::Test::LiveAllocationCount(),
              "idempotent initialization changed allocation");
        Reject([] { GuestFlat::Initialize({{0,0x4000,Backing::Mem1}}); });
        Check(GuestFlat::IsActive() && base==GuestFlat::g_flatGuestBase,"changed layout damaged active map");
        report("Owned regions, holes, logical limits and idempotence: PASS");
        report("Real backend memory tests: PASS (game not tested)");
        return true;
    } catch (const std::exception& e) {
        report(std::string("Real backend memory tests: FAIL: ")+e.what());
        return false;
    }
}
