// license: BSD-3-Clause
// copyright-holders: Dirk Best
// Sega 315-5649 register behavior adapted from MAME src/mame/sega/315_5649.*
// at dddd73680656e355bb2b5beecab1167c9f07bf81. Device framework and save-state
// registration become plain state; callbacks have the reference's offset 0
// removed. Address decoding and cabinet wiring belong to the board adapter.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace rt {

class Model2IO {
public:
    struct Callbacks {
        std::array<std::function<uint8_t()>, 7> input;
        std::array<std::function<void(uint8_t)>, 7> output;
        std::array<std::function<uint8_t()>, 8> analog;
        std::array<std::function<uint8_t()>, 2> serial_read;
        std::array<std::function<void(uint8_t)>, 2> serial_write;
        std::array<std::function<uint16_t()>, 4> counter;
    };
    struct State {
        std::array<uint8_t, 7> port_value = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        uint8_t port_config = 0, mode = 0, analog_channel = 0;
    };

    Model2IO() = default;
    explicit Model2IO(Callbacks callbacks);
    void set_callbacks(Callbacks callbacks);
    const State &state() const { return state_; }
    // The chip's device reset changes only direction and mode, retaining the
    // output latches and analog mux selection. Call reset() at board reset.
    void reset();
    // Registers are chip offsets 0..15, not CPU byte addresses. Peeking still
    // invokes the input callbacks but suppresses the two auto-increments.
    uint8_t read(unsigned offset, bool side_effects = true);
    void write(unsigned offset, uint8_t data);

    // Pinned MAME limitation: RX buffers always full, TX buffers empty.
    // This constant does not implement serial timing or framing errors.
    static constexpr uint8_t reference_serial_status = 0x0c;

private:
    State state_;
    Callbacks callbacks_;
};

} // namespace rt
