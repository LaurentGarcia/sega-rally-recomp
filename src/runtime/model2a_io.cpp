// license: BSD-3-Clause
// copyright-holders: R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Wiring/defaults transplanted from MAME model2.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (SHA256
// 359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6).
// Changes: explicit external EEPROM time and editable input bytes; drive CPU
// IRQ/firmware omitted, command and lamp lines latched with optional callbacks.
#include "runtime/model2a_io.h"
#include "runtime/model2_eeprom.h"

#include <utility>

namespace rt {
namespace {
constexpr uint32_t kPorts = 0x01c00000, kNoWrite = 0x01c00040;
void checked_address(const Model2AIO &io, uint32_t address) {
    if ((address & 3) || !io.claims(address))
        throw Fatal("Model2A I/O requires an aligned address in its exact mapped ranges");
}
} // namespace

Model2AIO::Model2AIO(Model2Eeprom &eeprom, Clock clock) : eeprom_(eeprom), clock_(std::move(clock)) {
    if (!clock_) throw Fatal("Model2A I/O requires an explicit nanosecond clock");
    Model2IO::Callbacks callbacks;
    callbacks.output[0] = [this](uint8_t data) { port_a(data); };
    callbacks.input[1] = [this] { return port_b(); };
    callbacks.input[2] = [this] { return inputs.in1; };
    callbacks.input[3] = [this] { return inputs.in2; };
    callbacks.input[6] = [this] { return inputs.switches; };
    callbacks.output[4] = [this](uint8_t data) {
        drive_command_ = data;
        if (on_drive_command) on_drive_command(data);
    };
    callbacks.output[5] = [this](uint8_t data) {
        lamp_output_ = data;
        if (on_lamp_output) on_lamp_output(data);
    };
    for (unsigned channel = 0; channel < inputs.analog.size(); ++channel)
        callbacks.analog[channel] = [this, channel] { return inputs.analog[channel]; };
    chip_.set_callbacks(std::move(callbacks));
    reset();
}

void Model2AIO::reset() {
    chip_.reset();
    ctrlmode_ = false; // model2_state::machine_reset
}

void Model2AIO::port_a(uint8_t data) {
    ctrlmode_ = data & 1;
    const uint64_t now = clock_();
    // Preserve reference order: a rising CLK sees the previous CS state.
    eeprom_.di_write(data & 0x20);
    eeprom_.clk_write(data & 0x80, now);
    eeprom_.cs_write(data & 0x40, now);
}

uint8_t Model2AIO::port_b() {
    if (ctrlmode_)
        return 0xc0 | (uint8_t(eeprom_.do_read(clock_())) << 5) | 0x10 | (inputs.in0 & 0x0f);
    return inputs.in0;
}

bool Model2AIO::claims(uint32_t address) const {
    return (address >= kPorts && address <= kPorts + 0x1f) ||
           (address >= kNoWrite && address <= kNoWrite + 3);
}

DeviceModel::Check Model2AIO::read(uint32_t address, uint32_t mask, uint32_t &value) {
    checked_address(*this, address);
    value = 0; // unselected byte lanes and the write-only nop read are unmapped zero
    if (address != kNoWrite) {
        const unsigned reg = (address - kPorts) / 2;
        if (mask & 0x000000ff) value |= uint32_t(chip_.read(reg));
        if (mask & 0x00ff0000) value |= uint32_t(chip_.read(reg + 1)) << 16;
        value &= mask;
    }
    return Strict;
}

void Model2AIO::write(uint32_t address, uint32_t data, uint32_t mask) {
    checked_address(*this, address);
    if (address == kNoWrite) return;
    const unsigned reg = (address - kPorts) / 2;
    if (mask & 0x000000ff) chip_.write(reg, uint8_t(data));
    if (mask & 0x00ff0000) chip_.write(reg + 1, uint8_t(data >> 16));
}

void Model2AIO::measured_mismatch(uint32_t, uint32_t, uint32_t) {
    throw Divergence("Strict Model2A I/O unexpectedly received a measured mismatch");
}

} // namespace rt
