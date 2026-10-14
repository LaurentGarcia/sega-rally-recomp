#include "runtime/model2a_bus.h"
#include <cassert>
#include <cstdio>

int main()
{
    std::vector<uint8_t> program(0x100000), data(0x1000004);
    program[0x20000] = 0xa5;
    data[0x1000000] = 0x12; data[0x1000001] = 0x34;
    rt::Model2ABus bus(std::move(program), std::move(data));
    // The upper half of CRX RAM must not alias the original Model 2 ROM.
    assert(bus.read_byte(0x00020000) == 0xa5);
    // A documented reference-map gap reads zero and never stores writes.
    assert(bus.read_byte(0x00f80000) == 0);
    bus.write_byte(0x00f80000, 2);
    assert(bus.read_dword(0x00f80000) == 0);
    assert(bus.diagnostics().reference_gap_reads == 2);
    assert(bus.diagnostics().reference_gap_writes == 1);
    assert(bus.diagnostics().reference_gap_last_write == 2);
    assert(bus.flags(0x00f80000) == 0);
    for (uint32_t address : {0x00f7ffffu, 0x00f80004u}) {
        bool gap_end = false;
        try { (void)bus.read_byte(address); }
        catch (const rt::Model2AAccess &) { gap_end = true; }
        assert(gap_end);
    }
    assert(bus.read_dword(0x00220000) == 0);
    bus.write_dword(0x00220000, 0x98765432);
    assert(bus.read_dword(0x00220000) == 0x98765432);
    assert(bus.read_byte(0x00020000) == 0xa5);
    assert(bus.read_word(0x03000000) == 0x3412);
    assert(bus.read_word(0x06000000) == 0x3412);
    bus.write_dword(0x00901234, 0x12345678);
    for (uint32_t mirror = 0; mirror < 4; ++mirror)
        assert(bus.read_dword(0x00901234 + mirror * 0x20000) == 0x12345678);
    bus.write_dword(0x00500001, 0x04030201);
    assert(bus.read_word(0x00500002) == 0x0302);
    assert(bus.flags(0x00220000) == rt::Cpu::BURST);
    assert(bus.flags(0x00e00000) == 0);
    assert(bus.read_dword(0x001ffffc) == 0);
    bus.write_byte(0x00020000, 0);
    assert(bus.read_byte(0x00020000) == 0xa5);
    bool caught = false;
    try { bus.write_word(0x01c00000, 0x55aa); }
    catch (const rt::Model2AAccess &e) {
        caught = e.address == 0x01c00000 && e.width == 2 && e.write && e.value == 0x55aa;
    }
    assert(caught);
    caught = false;
    try { (void)bus.read_dword(0x07000000); }
    catch (const rt::Model2AAccess &) { caught = true; }
    assert(caught);
    // Exercise real generated-code services across a buffer mirror boundary.
    rt::Cpu cpu(&bus);
    bus.write_word(0x0091fffe, 0xbbaa);
    bus.write_word(0x00900000, 0xddcc);
    auto [unaligned, burst] = cpu.i960_read_dword_unaligned_flags(0x0091fffe);
    assert(unaligned == 0xddccbbaa && burst == rt::Cpu::BURST);
    cpu.i960_write_dword_unaligned(0x0091fffe, 0x88776655);
    assert(bus.read_word(0x0091fffe) == 0x6655);
    assert(bus.read_word(0x00900000) == 0x8877);
    std::puts("Model 2A bus: RAM, ROM aliases, widths, burst flags and explicit device stops passed");
}
