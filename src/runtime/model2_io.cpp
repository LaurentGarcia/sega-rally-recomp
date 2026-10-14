// license: BSD-3-Clause
// copyright-holders: Dirk Best
// Sega 315-5649 register semantics transplanted from MAME
// src/mame/sega/315_5649.cpp, dddd73680656e355bb2b5beecab1167c9f07bf81.
// Source SHA256: 06cc79c2b00b478bbcbba7c5ec7735f0cb48fe25795bc5b3d94a77ebca03943d.
// Changes: no device framework/logging; plain callbacks/state; explicit peek
// flag replaces machine().side_effects_disabled(). Unbound callback values
// are MAME's resolver defaults, not invented cabinet inputs.
#include "runtime/model2_io.h"

#include <utility>

namespace rt {

Model2IO::Model2IO(Callbacks callbacks) : callbacks_(std::move(callbacks)) {}

void Model2IO::set_callbacks(Callbacks callbacks) { callbacks_ = std::move(callbacks); }

void Model2IO::reset() {
    state_.port_config = 0xff;
    state_.mode = 0;
}

uint8_t Model2IO::read(unsigned offset, bool side_effects) {
    if (offset <= 6) {
        if (offset == 6 && (state_.mode & 0x80)) {
            const auto &callback = callbacks_.counter[(state_.port_value[6] >> 1) & 3];
            const uint16_t count = callback ? callback() : 0;
            const uint8_t data = uint8_t(count >> (((state_.port_value[6] & 1) ^ 1) * 8));
            if (side_effects)
                state_.port_value[6] = (state_.port_value[6] & 0xf8) | ((state_.port_value[6] + 1) & 7);
            return data;
        }
        if (state_.port_config & (1u << offset)) {
            const auto &callback = callbacks_.input[offset];
            return callback ? callback() : 0xff;
        }
        return state_.port_value[offset];
    }
    if (offset == 0x0b || offset == 0x0c) {
        const auto &callback = callbacks_.serial_read[offset - 0x0b];
        return callback ? callback() : 0;
    }
    if (offset == 0x0d) return reference_serial_status;
    if (offset == 0x0f) {
        const auto &callback = callbacks_.analog[state_.analog_channel];
        const uint8_t data = callback ? callback() : 0xff;
        if (side_effects) state_.analog_channel = (state_.analog_channel + 1) & 7;
        return data;
    }
    return 0xff;
}

void Model2IO::write(unsigned offset, uint8_t data) {
    if (offset <= 6) {
        // Reference calls outputs even with input direction or counter mode.
        // Port G's counter-reset bit is not implemented by the reference.
        state_.port_value[offset] = data;
        if (const auto &callback = callbacks_.output[offset]) callback(data);
    } else if (offset == 0x08) {
        state_.port_config = data;
    } else if (offset == 0x09 || offset == 0x0a) {
        if (const auto &callback = callbacks_.serial_write[offset - 0x09]) callback(data);
    } else if (offset == 0x0e) {
        // The remaining RS-422 mode bits are stored, without extra behavior.
        state_.mode = data;
    } else if (offset == 0x0f) {
        state_.analog_channel = data & 7;
    }
}

} // namespace rt
