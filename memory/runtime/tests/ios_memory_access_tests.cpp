#include "memory_access.h"
#include "isa/ppc_isa_quantized.h"
#include <functional>
#include <stdexcept>
#include <string>
#include <cstring>

// Real inline helpers, isolated from the full graphics/HLE runtime. Only the
// checked Memory boundary is replaced; an accidental direct access has no RAM
// at the requested MMIO address and cannot satisfy these expected values.
namespace {
constexpr uint32_t address = 0xcc004000;
constexpr uint32_t writeAddress = 0x80000400;
uint32_t expectedAddress = address;
unsigned calls = 0;
uint64_t lastValue = 0;
void CheckedAddress(uint32_t a) { if(a!=expectedAddress) throw std::runtime_error("checked helper changed address"); ++calls; }
void Expect(bool ok) { if(!ok) throw std::runtime_error("checked-access routing/value assertion failed"); }
}
namespace MemoryInline {
PageEntry g_pageTable[kPageCount]{};
uintptr_t g_fullPageBias[kPageCount]{};
uintptr_t g_fullReadablePageBias[kPageCount]{};
uintptr_t g_fullWritablePageBias[kPageCount]{};
const SparseWritablePageTable* g_sparseWritablePageTables[kPageCount]{};
uint8_t g_deferredReadCoveredPages[kPageCount]{};
uint8_t Read8Slow(uint32_t a) { CheckedAddress(a); return 0x71; }
uint16_t Read16Slow(uint32_t a) { CheckedAddress(a); return 0x8293; }
uint32_t Read32Slow(uint32_t a) { CheckedAddress(a); return 0x3fc00000; }
uint64_t Read64Slow(uint32_t a) { CheckedAddress(a); return 0x4004000000000000ULL; }
void Write8Slow(uint32_t a,uint8_t v) { CheckedAddress(a); lastValue=v; }
void Write16Slow(uint32_t a,uint16_t v) { CheckedAddress(a); lastValue=v; }
void Write32Slow(uint32_t a,uint32_t v) { CheckedAddress(a); lastValue=v; }
void WriteFloat32Slow(uint32_t a,double v) { CheckedAddress(a); if(v!=1.5) throw std::runtime_error("float32 checked value changed"); }
void WriteFloat64Slow(uint32_t a,double v) { CheckedAddress(a); if(v!=2.5) throw std::runtime_error("float64 checked value changed"); }
}
namespace RecompMod {
std::atomic<bool> g_executableWriteGuardEnabled{false};
std::atomic<uint8_t> g_executableWriteGuardPages[kExecutableWriteGuardPageCount]{};
std::atomic<uint8_t> g_executableWriteGuardCoarsePages[kExecutableWriteGuardCoarsePageCount]{};
std::atomic<uint8_t> g_executableWriteGuardMidPages[kExecutableWriteGuardMidPageCount]{};
}
bool RunIosMemoryAccessTests(std::function<void(const std::string&)> report) {
    const bool previous = GuestFlat::g_requiresCheckedAccess;
    GuestFlat::g_requiresCheckedAccess = true;
    try {
        calls=0;
        expectedAddress=address;
        Expect(MemoryInline::FlatRead8(address)==0x71);
        Expect(MemoryInline::FlatRead16(address)==0x8293);
        Expect(MemoryInline::FlatRead32(address)==0x3fc00000);
        Expect(MemoryInline::FlatReadFloat32(address)==1.5f);
        Expect(MemoryInline::FlatReadFloat64(address)==2.5);
        expectedAddress=writeAddress;
        MemoryInline::FlatWrite8(writeAddress,0x43); Expect(lastValue==0x43);
        MemoryInline::FlatWrite16(writeAddress,0x5678); Expect(lastValue==0x5678);
        MemoryInline::FlatWrite32(writeAddress,0x87654321); Expect(lastValue==0x87654321);
        MemoryInline::FlatWriteFloat32(writeAddress,1.5);
        MemoryInline::FlatWriteFloat64(writeAddress,2.5);
        expectedAddress=address;
        MemoryInline::FlatWrite32(address,0x65); Expect(lastValue==0x65);
        Expect(calls==11);
        Expect(PpcTryGetPsqReadableHostInline(address)==nullptr);
        Expect(PpcTryGetPsqWritableHostInline(address)==nullptr);
        Expect(PpcTryGetPsqReadableHostInline(writeAddress)==nullptr);
        Expect(PpcTryGetPsqWritableHostInline(writeAddress)==nullptr);
        // Proven-RAM stores must still use the dynamic base even in checked mode.
        auto* p=GuestFlat::HostPointer(0x80000040);
        Expect(p!=nullptr);
        MemoryInline::FlatWriteRam32(0x80000040,0x12345678);
        Expect(p[0]==0x12 && p[1]==0x34 && p[2]==0x56 && p[3]==0x78);
        report("Checked scalar/float/PSQ routing and dynamic RAM endian: PASS");
        GuestFlat::g_requiresCheckedAccess=previous;
        return true;
    } catch(const std::exception& e) {
        GuestFlat::g_requiresCheckedAccess=previous;
        report(std::string("Checked access: FAIL: ")+e.what());
        return false;
    }
}
