// license:BSD-3-Clause
// copyright-holders:Ariane Fugmann and contributors
//
// Mapping: MAME src/mame/sega/model2.cpp model2_base_mem, revision
// dddd73680656e355bb2b5beecab1167c9f07bf81 (https://github.com/mamedev/mame).
// Registers/protocol: unchanged inherited runtime/comm_board.{h,cpp}, adapted
// from that revision's src/mame/sega/m2comm.{h,cpp} (M2COMM_SIMULATION).
// Pinned source SHA256:
// model2.cpp 359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6
// m2comm.cpp 4fdc712070b8766e76ba03fdf64be1844f159ea48b6023ee865162973467d204
// m2comm.h   6f626674189b0c14fb4a576f7e70a846213ca409dbf1ab19b435f8e533e15ae6
//
// The pinned header enables M2COMM_SIMULATION. CN=1 initializes the shared
// protocol data, then a failed connection leaves status=0 and id/count=ff.
// A fixed disconnected transport reproduces that path without any sockets.
// CN=0 clears CN/FG/ZFG, preserving shared cells; FG stores only bit zero.
// Readback is CN|fe and FG|7e|(~ZFG<<7), including when no peer is connected.
// The byte-wide flag registers leave their neighboring byte lanes unmapped
// (zero). We claim only the containing dword, not M2Board's wider device page.
// No communication firmware, bank-switching Z80, HDLC or actual network runs
// here. The simulation's initialization is not proof of firmware execution;
// missing firmware remains a separate full-machine reference limitation.

#include "runtime/model2a_comm.h"

namespace rt {
namespace {
constexpr uint32_t kShared = 0x01a00000;
constexpr uint32_t kFlags = 0x01a04000;
constexpr uint32_t kMirror = 0x00010000;
uint32_t canonical(uint32_t address) { return address & ~kMirror; }

void checked_address(const Model2AComm &comm, uint32_t address)
{
    if ((address & 3) || !comm.claims(address))
        throw Fatal("Model2A comm requires an aligned address in its exact mapped ranges");
}
} // namespace

Model2AComm::Model2AComm() : board_(shared_.data())
{
    board_.set_transport(&transport_);
    reset();
}

void Model2AComm::reset() { board_.cn_w(0); }
void Model2AComm::vblank() { board_.vblank(); }

bool Model2AComm::claims(uint32_t address) const
{
    address = canonical(address);
    return (address >= kShared && address < kShared + shared_.size()) ||
           (address >= kFlags && address <= kFlags + 3);
}

DeviceModel::Check Model2AComm::read(uint32_t address, uint32_t mask, uint32_t &value)
{
    checked_address(*this, address);
    address = canonical(address);
    value = 0;
    if (address == kFlags) {
        if (mask & 0x000000ff) value |= board_.cn_r();
        if (mask & 0x00ff0000) value |= uint32_t(board_.fg_r()) << 16;
    } else {
        for (unsigned lane = 0; lane < 4; ++lane)
            value |= uint32_t(shared_[address - kShared + lane]) << (lane * 8);
    }
    value &= mask;
    return Strict;
}

void Model2AComm::write(uint32_t address, uint32_t data, uint32_t mask)
{
    checked_address(*this, address);
    address = canonical(address);
    if (address == kFlags) {
        // Preserve ascending byte order if both registers are written at once.
        if (mask & 0x000000ff) board_.cn_w(uint8_t(data));
        if (mask & 0x00ff0000) board_.fg_w(uint8_t(data >> 16));
    } else {
        for (unsigned lane = 0; lane < 4; ++lane)
            if (mask & (0xffu << (lane * 8)))
                shared_[address - kShared + lane] = uint8_t(data >> (lane * 8));
    }
}

const uint8_t *Model2AComm::region(uint32_t base) const
{
    return canonical(base) == kShared ? shared_.data() : nullptr;
}

void Model2AComm::measured_mismatch(uint32_t, uint32_t, uint32_t)
{
    throw Divergence("Strict Model2A comm unexpectedly received a measured mismatch");
}

} // namespace rt
