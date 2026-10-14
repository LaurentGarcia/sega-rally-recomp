// license:BSD-3-Clause
// copyright-holders:Ariane Fugmann and contributors
// Model 2A mapping for the inherited MAME-derived CommBoard; see .cpp.
#pragma once

#include "runtime/comm_board.h"
#include "runtime/m2_replay_bus.h"
#include <array>

namespace rt {

// Native no-peer communication-board adapter. This reuses CommBoard's pinned
// M2COMM_SIMULATION protocol; it does not execute the Z80/HDLC firmware or
// open sockets. Both endpoints stay disconnected, so link-up and ZFG activity
// are never invented. Shared RAM and CN/FG use the actual Model 2A map.
class Model2AComm final : public DeviceModel {
public:
    Model2AComm();
    Model2AComm(const Model2AComm &) = delete;
    Model2AComm &operator=(const Model2AComm &) = delete;
    Model2AComm(Model2AComm &&) = delete;
    Model2AComm &operator=(Model2AComm &&) = delete;

    // CN reset clears CN/FG/ZFG and disables the board; shared RAM survives.
    void reset();
    void vblank(); // Wire to the board's real vblank event, not a read count.
    CommBoard::Link link() const { return board_.link(); }

    bool claims(uint32_t address) const override;
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override;
    void write(uint32_t address, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t address, uint32_t ours, uint32_t mame) override;
    const uint8_t *region(uint32_t base) const override;

private:
    // Unlike nullptr (which makes inherited CommBoard return before setting
    // status bytes), closed endpoints execute the reference's no-peer path.
    class DisconnectedTransport final : public LinkTransport {
        void poll() override {}
        bool rx_open() const override { return false; }
        bool tx_open() const override { return false; }
        int read(uint8_t *, int) override { return -1; }
        bool write(const uint8_t *, int) override { return false; }
    } transport_;
    std::array<uint8_t, 0x4000> shared_{};
    CommBoard board_;
};

} // namespace rt
