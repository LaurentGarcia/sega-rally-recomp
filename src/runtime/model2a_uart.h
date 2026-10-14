// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Model2A uPD71051/i8251 bus/clock-pin wiring from pinned MAME model2.cpp.
#pragma once

#include "runtime/m2_replay_bus.h"
#include "runtime/model2_uart.h"

namespace rt {

class Model2AUART final : public DeviceModel {
public:
    explicit Model2AUART(Model2UART &chip) : chip_(chip) {}
    void reset() { chip_.reset(); chip_.write_cts(false); }
    // Called by the external board event queue, not by register polling.
    // Pinned Model2A clock:500kHz, TXC first then RXC on each physical edge.
    void clock_edge(bool level) { chip_.write_txc(level); chip_.write_rxc(level); }
    Model2UART &chip() { return chip_; }
    bool claims(uint32_t address) const override;
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override;
    void write(uint32_t address, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t address, uint32_t ours, uint32_t mame) override;
private:
    Model2UART &chip_;
};

} // namespace rt
