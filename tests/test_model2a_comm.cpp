// license:BSD-3-Clause
// Self-authored Model2A bus fixtures; no ROM or firmware content.
#include "runtime/model2a_comm.h"
#include "runtime/model2a_bus.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (0)

namespace {
constexpr uint32_t kRam = 0x01a00000, kReg = 0x01a04000, kMirror = 0x10000;

void mapping_and_shared_ram()
{
    rt::Model2AComm comm;
    rt::Model2ABus bus({}, {});
    bus.add_model(&comm);
    CHECK(comm.link() == rt::CommBoard::Link::Off);
    for (uint32_t mirror : {0u, kMirror}) {
        CHECK(bus.flags(kRam + mirror) == rt::Cpu::BURST);
        CHECK(bus.flags(kRam + mirror + 0x3fff) == rt::Cpu::BURST);
        CHECK(bus.flags(kReg + mirror) == 0);
        CHECK(comm.claims(kRam + mirror));
        CHECK(comm.claims(kRam + mirror + 0x3fff));
        for (unsigned lane = 0; lane < 4; ++lane) CHECK(comm.claims(kReg + mirror + lane));
        for (uint32_t address : {kRam + mirror - 1, kReg + mirror + 4, kReg + mirror + 0xfff})
            CHECK(!comm.claims(address));
    }
    CHECK(!comm.claims(kRam + 0x20000));
    CHECK(comm.region(kRam) == comm.region(kRam + kMirror));
    CHECK(comm.region(kRam + 4) == nullptr);
    CHECK(comm.region(kReg) == nullptr);
    CHECK(std::all_of(comm.region(kRam), comm.region(kRam) + 0x4000,
                      [](uint8_t byte) { return byte == 0; }));
    // Every byte in the shared region must have a distinct mirrored cell.
    for (unsigned offset = 0; offset < 0x4000; ++offset)
        bus.write_byte(kRam + offset, uint8_t((offset ^ (offset >> 8)) * 37));
    for (unsigned offset = 0; offset < 0x4000; ++offset)
        CHECK(bus.read_byte(kRam + kMirror + offset) == uint8_t((offset ^ (offset >> 8)) * 37));
    bus.write_dword(kRam + kMirror + 0x3ffc, 0xa1b2c3d4);
    CHECK(bus.read_dword(kRam + 0x3ffc) == 0xa1b2c3d4);
    bus.write_word(kRam + 0x3ffe, 0x1122);
    CHECK(bus.read_dword(kRam + kMirror + 0x3ffc) == 0x1122c3d4);
    comm.write(kRam + 0x3ffc, 0xff00aaff, 0x00ffff00);
    CHECK(bus.read_dword(kRam + 0x3ffc) == 0x1100aad4);
    uint32_t value = ~0u;
    CHECK(comm.read(kRam + 0x3ffc, 0x00ffff00, value) == rt::DeviceModel::Strict);
    CHECK(value == 0x0000aa00);
    for (uint32_t address : {kRam - 1, kReg + 4, kReg + kMirror + 4}) {
        bool stopped = false;
        try { (void)bus.read_byte(address); }
        catch (const rt::Model2AAccess &) { stopped = true; }
        CHECK(stopped);
    }
    bool unaligned = false;
    try { comm.read(kRam + 1, ~0u, value); }
    catch (const rt::Fatal &) { unaligned = true; }
    CHECK(unaligned);
}

void flags_reset_and_no_peer()
{
    rt::Model2AComm comm;
    rt::Model2ABus bus({}, {});
    bus.add_model(&comm);
    CHECK(bus.read_dword(kReg) == 0x00fe00fe);
    CHECK(bus.read_dword(kReg + kMirror) == 0x00fe00fe);
    for (uint32_t mirror : {0u, kMirror}) {
        CHECK(bus.read_byte(kReg + mirror + 1) == 0);
        CHECK(bus.read_byte(kReg + mirror + 3) == 0);
        bus.write_byte(kReg + mirror + 1, 0xff);
        bus.write_byte(kReg + mirror + 3, 0xff);
        CHECK(bus.read_dword(kReg) == 0x00fe00fe);
    }
    bus.write_byte(kReg + 2, 0xff);
    CHECK(bus.read_word(kReg + kMirror + 2) == 0xff);
    bus.write_byte(kReg + 2, 0xfe);
    CHECK(bus.read_word(kReg + 2) == 0xfe); // Only FG bit zero is latched.
    bus.write_dword(kRam + 0x3210, 0x12345678);
    bus.write_byte(kReg + 2, 1);
    bus.write_byte(kReg + kMirror, 0xfe); // CN low despite other set bits.
    CHECK(bus.read_dword(kReg) == 0x00fe00fe); // CN resets FG and ZFG.
    CHECK(bus.read_dword(kRam + 0x3210) == 0x12345678);

    bus.write_byte(kReg + 2, 1); // Select master before enabling.
    bus.write_byte(kReg + kMirror, 0xff);
    CHECK(bus.read_dword(kReg) == 0x00ff00ff);
    CHECK(comm.link() == rt::CommBoard::Link::Waiting);
    // Pinned M2COMM_SIMULATION startup, including the no-peer status bytes
    // skipped by inherited CommBoard when given a nullptr transport.
    CHECK(bus.read_dword(kRam) == 0xffff0200);
    CHECK(bus.read_dword(kRam + 0x10) == 0x0e000000);
    CHECK(bus.read_word(kRam + 0x14) == 0x01c0);
    CHECK(bus.read_dword(kRam + 0x3210) == 0);
    bus.write_dword(kRam + 0x3210, 0x87654321);
    for (unsigned frame = 0; frame < 1000; ++frame) {
        comm.vblank();
        CHECK(comm.link() == rt::CommBoard::Link::Waiting);
        CHECK(bus.read_dword(kReg) == 0x00ff00ff); // No invented ZFG toggles.
        CHECK(bus.read_dword(kRam) == 0xffff0200); // No invented link-ready.
        CHECK(bus.read_dword(kRam + 0x3210) == 0x87654321);
    }
    // Reference CN enable is a command on every write, not only an edge.
    bus.write_byte(kReg, 1);
    CHECK(bus.read_dword(kRam + 0x3210) == 0);
    bus.write_dword(kRam + 0x3210, 0x12345678);
    comm.reset();
    CHECK(comm.link() == rt::CommBoard::Link::Off);
    CHECK(bus.read_dword(kReg) == 0x00fe00fe);
    CHECK(bus.read_dword(kRam + 0x3210) == 0x12345678);
    bus.write_dword(kRam, 0x44332211);
    comm.vblank();
    CHECK(bus.read_dword(kRam) == 0x44332211); // Disabled board does not touch RAM.

    // One dword writes CN before FG, matching MAME's ascending byte access.
    bus.write_dword(kReg, 0x00010000);
    CHECK(bus.read_dword(kReg) == 0x00ff00fe);
    uint32_t value = ~0u;
    comm.read(kReg, 0xff00ff00, value);
    CHECK(value == 0);
    comm.write(kReg, 0, 0xff00ff00);
    CHECK(bus.read_dword(kReg) == 0x00ff00fe);
}
} // namespace

int main()
{
    mapping_and_shared_ram();
    flags_reset_and_no_peer();
    std::puts("Model2A comm: exact mirrors/lanes, shared RAM, flag/reset/init and no-peer stability passed");
}
