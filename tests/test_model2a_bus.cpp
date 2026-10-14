#include "runtime/model2a_bus.h"
#include "runtime/m2_replay_bus.h"
#include <cassert>
#include <cstdio>

struct RecordingDevice : rt::DeviceModel {
    unsigned reads = 0, writes = 0;
    uint32_t last_address = 0, last_mask = 0, last_value = 0;
    bool claims(uint32_t a) const override { return a >= 0x00884000 && a <= 0x00887fff; }
    Check read(uint32_t a, uint32_t mask, uint32_t &value) override {
        ++reads; last_address = a; last_mask = mask; value = 0x87654321; return Strict;
    }
    void write(uint32_t a, uint32_t value, uint32_t mask) override {
        ++writes; last_address = a; last_mask = mask; last_value = value;
    }
    void measured_mismatch(uint32_t, uint32_t, uint32_t) override {}
};

int main()
{
    std::vector<uint8_t> program(0x100000), data(0x1000004);
    program[0x20000] = 0xa5;
    data[0x1000000] = 0x12; data[0x1000001] = 0x34;
    rt::Model2ABus bus(std::move(program), std::move(data));
    assert(bus.read_dword(0x00900000) == 0x07800f0f);
    assert(bus.read_dword(0x0097fffc) == 0x07800f0f);
    assert(bus.read_dword(0x01d00000) == 0xffffffff);
    assert(bus.read_byte(0x01d03fff) == 0xff);
    bus.write_dword(0x00e80004, 0);
    bus.write_dword(0x00e80000, 0xffffffff);
    assert(bus.read_dword(0x00e80000) == 0);
    assert(bus.read_dword(0x00e80004) == 0);
    assert(bus.diagnostics().irq_reset_writes == 2);
    bool irq_fenced = false;
    try { bus.write_dword(0x00e80004, 1); }
    catch (const rt::Model2AAccess &) { irq_fenced = true; }
    assert(irq_fenced);
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
    bus.write_word(0x01c00202, 0x4d);
    assert(bus.read_word(0x01c00202) == 0);
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
    RecordingDevice device;
    bus.set_model(&device);
    assert(bus.read_word(0x00884002) == 0x8765);
    assert(device.reads == 1 && device.last_address == 0x00884000 && device.last_mask == 0xffff0000);
    bus.write_byte(0x00884001, 0xa5);
    assert(device.writes == 1 && device.last_address == 0x00884000);
    assert(device.last_mask == 0x0000ff00 && device.last_value == 0x0000a500);
    assert(bus.flags(0x00884000) == 0);
    assert(bus.flags(0x00880000) == rt::Cpu::BURST);
    assert(bus.flags(0x00804000) == rt::Cpu::BURST);
    assert(bus.flags(0x00800000) == 0);
    std::puts("Model 2A bus: RAM, ROM aliases, widths, burst flags and explicit device stops passed");
}
