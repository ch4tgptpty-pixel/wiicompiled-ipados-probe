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
unsigned calls = 0;
uint64_t lastValue = 0;
void CheckedAddress(uint32_t a) { if(a!=address) throw std::runtime_error("checked helper changed address"); ++calls; }
void Expect(bool ok) { if(!ok) throw std::runtime_error("checked-access routing/value assertion failed"); }
}
uint8_t Memory::Read8(uint32_t a) { CheckedAddress(a); return 0x71; }
uint16_t Memory::Read16(uint32_t a) { CheckedAddress(a); return 0x8293; }
uint32_t Memory::Read32(uint32_t a) { CheckedAddress(a); return 0x3fc00000; }
uint64_t Memory::Read64(uint32_t a) { CheckedAddress(a); return 0x4004000000000000ULL; }
void Memory::Write8(uint32_t a,uint8_t v) { CheckedAddress(a); lastValue=v; }
void Memory::Write16(uint32_t a,uint16_t v) { CheckedAddress(a); lastValue=v; }
void Memory::Write32(uint32_t a,uint32_t v) { CheckedAddress(a); lastValue=v; }
void Memory::WriteFloat32(uint32_t a,double v) { CheckedAddress(a); if(v!=1.5) throw std::runtime_error("float32 checked value changed"); }
void Memory::WriteFloat64(uint32_t a,double v) { CheckedAddress(a); if(v!=2.5) throw std::runtime_error("float64 checked value changed"); }
// Cold branches are linked but must never run during this checked-access test.
namespace MemoryInline {
void Write8Slow(uint32_t,uint8_t) { throw std::runtime_error("unexpected unchecked slow write8"); }
void Write16Slow(uint32_t,uint16_t) { throw std::runtime_error("unexpected unchecked slow write16"); }
void Write32Slow(uint32_t,uint32_t) { throw std::runtime_error("unexpected unchecked slow write32"); }
void WriteFloat32Slow(uint32_t,double) { throw std::runtime_error("unexpected unchecked slow float32"); }
void WriteFloat64Slow(uint32_t,double) { throw std::runtime_error("unexpected unchecked slow float64"); }
}
bool RunIosMemoryAccessTests(std::function<void(const std::string&)> report) {
    const bool previous = GuestFlat::g_requiresCheckedAccess;
    GuestFlat::g_requiresCheckedAccess = true;
    try {
        calls=0;
        Expect(MemoryInline::FlatRead8(address)==0x71);
        Expect(MemoryInline::FlatRead16(address)==0x8293);
        Expect(MemoryInline::FlatRead32(address)==0x3fc00000);
        Expect(MemoryInline::FlatReadFloat32(address)==1.5f);
        Expect(MemoryInline::FlatReadFloat64(address)==2.5);
        MemoryInline::FlatWrite8(address,0x43); Expect(lastValue==0x43);
        MemoryInline::FlatWrite16(address,0x5678); Expect(lastValue==0x5678);
        MemoryInline::FlatWrite32(address,0x87654321); Expect(lastValue==0x87654321);
        MemoryInline::FlatWriteFloat32(address,1.5);
        MemoryInline::FlatWriteFloat64(address,2.5);
        Expect(calls==10);
        Expect(PpcTryGetPsqReadableHostInline(address)==nullptr);
        Expect(PpcTryGetPsqWritableHostInline(address)==nullptr);
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
