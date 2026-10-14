// license: BSD-3-Clause
// copyright-holders: R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Model2A/Sega Rally wiring adapted from MAME src/mame/sega/model2.cpp,
// dddd73680656e355bb2b5beecab1167c9f07bf81. This adapter runs the I/O register
// chip, not the drive board CPU/firmware or a serial timing model.
#pragma once

#include "runtime/m2_replay_bus.h"
#include "runtime/model2_io.h"

#include <array>
#include <functional>

namespace rt {

class Model2Eeprom;

class Model2AIO final : public DeviceModel {
public:
    using Clock = std::function<uint64_t()>; // explicit board time in nanoseconds
    struct Inputs {
        uint8_t in0 = 0xff, in1 = 0x8f, in2 = 0x00, switches = 0xff;
        std::array<uint8_t, 8> analog = {0x80, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff};
    };

    Model2AIO(Model2Eeprom &eeprom, Clock clock);
    Model2AIO(const Model2AIO &) = delete;
    Model2AIO &operator=(const Model2AIO &) = delete;
    Model2AIO(Model2AIO &&) = delete;
    Model2AIO &operator=(Model2AIO &&) = delete;

    Inputs inputs;
    // A drive command is only latched/forwarded. No drive CPU IRQ or firmware
    // execution is claimed. Lamp bits 2..7 and coin-counter lines 0..1 are
    // forwarded as the raw port F value for optional host bookkeeping.
    std::function<void(uint8_t)> on_drive_command, on_lamp_output;
    uint8_t drive_command() const { return drive_command_; }
    uint8_t lamp_output() const { return lamp_output_; }
    bool eeprom_mux() const { return ctrlmode_; }
    const Model2IO &chip() const { return chip_; }
    // Reset I/O direction/mode and board ctrlmode, retaining chip latches/mux.
    // EEPROM power/reset remains the external owner's responsibility.
    void reset();

    bool claims(uint32_t address) const override;
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override;
    void write(uint32_t address, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t address, uint32_t ours, uint32_t mame) override;

private:
    void port_a(uint8_t data);
    uint8_t port_b();
    Model2Eeprom &eeprom_;
    Clock clock_;
    Model2IO chip_;
    bool ctrlmode_ = false;
    uint8_t drive_command_ = 0, lamp_output_ = 0;
};

} // namespace rt
