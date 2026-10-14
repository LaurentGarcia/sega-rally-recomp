// Self-authored register/callback fixtures; no game ROM data.
#include "runtime/model2_io.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <limits>

namespace {
void constructor_and_reset() {
    rt::Model2IO io;
    assert(io.state().port_config == 0 && io.state().mode == 0 && io.state().analog_channel == 0);
    for (unsigned port = 0; port < 7; ++port) assert(io.read(port) == 0xff);
    assert(io.read(0x0b) == 0 && io.read(0x0c) == 0);
    io.write(0x0e, 0x80);
    assert(io.read(6) == 0); // unbound counter resolver
    io.write(0x0f, 7);
    for (unsigned port = 0; port < 7; ++port) io.write(port, uint8_t(0x60 + port));
    io.reset();
    assert(io.state().port_config == 0xff && io.state().mode == 0 && io.state().analog_channel == 7);
    for (unsigned port = 0; port < 7; ++port) {
        assert(io.state().port_value[port] == 0x60 + port);
        assert(io.read(port) == 0xff);
    }
    assert(io.read(0x0f, false) == 0xff && io.state().analog_channel == 7);
    assert(io.read(0x0f) == 0xff && io.state().analog_channel == 0);
    io.write(8, 0);
    for (unsigned port = 0; port < 7; ++port) assert(io.read(port) == 0x60 + port);
}

void ports_and_directions() {
    rt::Model2IO::Callbacks callbacks;
    std::array<unsigned, 7> input_calls{}, output_calls{};
    std::array<uint8_t, 7> output_values{};
    for (unsigned port = 0; port < 7; ++port) {
        callbacks.input[port] = [&, port] { ++input_calls[port]; return uint8_t(0x10 + port); };
        callbacks.output[port] = [&, port](uint8_t value) { ++output_calls[port]; output_values[port] = value; };
    }
    rt::Model2IO io(callbacks);
    io.reset();
    for (unsigned mask = 0; mask < 256; ++mask) {
        io.write(8, uint8_t(mask));
        for (unsigned port = 0; port < 7; ++port) {
            const unsigned before_input = input_calls[port], before_output = output_calls[port];
            const uint8_t value = uint8_t(mask ^ (port * 17));
            io.write(port, value);
            assert(output_calls[port] == before_output + 1 && output_values[port] == value);
            assert(io.read(port) == ((mask & (1u << port)) ? uint8_t(0x10 + port) : value));
            assert(input_calls[port] == before_input + bool(mask & (1u << port)));
        }
    }
    io.write(0x0e, 0x80);
    io.write(6, 0x80); // reference does not reset counters; output still fires
    assert(output_values[6] == 0x80);
    const auto state = io.state();
    io.reset();
    assert(io.state().port_value == state.port_value);
    io.write(8, 0);
    assert(io.read(6) == 0x80);
    // Replacing host wiring neither clears chip state nor invents inputs.
    io.set_callbacks({});
    assert(io.read(6) == 0x80);
    io.write(8, 0xff);
    assert(io.read(6) == 0xff);
}

void analog_mux() {
    rt::Model2IO::Callbacks callbacks;
    std::array<unsigned, 8> calls{};
    for (unsigned channel = 0; channel < 8; ++channel)
        callbacks.analog[channel] = [&, channel] { ++calls[channel]; return uint8_t(0x30 + channel); };
    rt::Model2IO io(callbacks);
    io.reset();
    for (unsigned selection = 0; selection < 256; ++selection) {
        io.write(0x0f, uint8_t(selection));
        const unsigned channel = selection & 7;
        assert(io.state().analog_channel == channel);
        const unsigned before = calls[channel];
        assert(io.read(0x0f, false) == 0x30 + channel);
        assert(io.read(0x0f, false) == 0x30 + channel);
        assert(calls[channel] == before + 2 && io.state().analog_channel == channel);
        for (unsigned n = 0; n < 16; ++n)
            assert(io.read(0x0f) == 0x30 + ((channel + n) & 7));
        assert(io.state().analog_channel == channel);
    }
}

void counter_mux() {
    rt::Model2IO::Callbacks callbacks;
    const std::array<uint16_t, 4> values = {0x1234, 0x5678, 0x9abc, 0xdef0};
    std::array<unsigned, 4> calls{};
    unsigned ordinary_input = 0, output_calls = 0;
    callbacks.input[6] = [&] { ++ordinary_input; return uint8_t(0x42); };
    callbacks.output[6] = [&](uint8_t) { ++output_calls; };
    for (unsigned channel = 0; channel < 4; ++channel)
        callbacks.counter[channel] = [&, channel] { ++calls[channel]; return values[channel]; };
    rt::Model2IO io(callbacks);
    for (unsigned direction : {0u, 0xffu}) for (unsigned selector = 0; selector < 256; ++selector) {
        io.write(8, uint8_t(direction));
        io.write(0x0e, 0x80);
        io.write(6, uint8_t(selector));
        const unsigned before_output = output_calls;
        for (unsigned n = 0; n < 16; ++n) {
            const unsigned index = (selector + n) & 7;
            const uint8_t expected = uint8_t(values[index >> 1] >> ((index & 1) ? 0 : 8));
            const unsigned before = calls[index >> 1];
            assert(io.read(6, false) == expected && calls[index >> 1] == before + 1);
            assert(io.state().port_value[6] == ((selector & 0xf8) | index));
            assert(io.read(6) == expected && calls[index >> 1] == before + 2);
        }
        assert(io.state().port_value[6] == selector && output_calls == before_output);
    }
    assert(ordinary_input == 0);
    io.write(0x0e, 0x7f);
    assert(io.read(6) == 0x42 && ordinary_input == 1);
    // Every 16-bit callback result, both byte orders. The counter is sampled
    // again for each byte; there is no hidden 16-bit latch in the reference.
    uint16_t count = 0;
    callbacks.counter[0] = [&] { return count++; };
    io.set_callbacks(callbacks);
    io.write(0x0e, 0x80);
    for (unsigned value = 0; value < 0x10000; ++value) {
        io.write(6, 0);
        count = uint16_t(value);
        assert(io.read(6) == value >> 8);
        assert(io.read(6) == uint8_t(value + 1));
    }
}

void serial_and_unused_registers() {
    rt::Model2IO::Callbacks callbacks;
    std::array<unsigned, 2> reads{}, writes{};
    std::array<uint8_t, 2> transmitted{};
    for (unsigned channel = 0; channel < 2; ++channel) {
        callbacks.serial_read[channel] = [&, channel] { ++reads[channel]; return uint8_t(0x90 + channel); };
        callbacks.serial_write[channel] = [&, channel](uint8_t value) { ++writes[channel]; transmitted[channel] = value; };
    }
    rt::Model2IO io(callbacks);
    io.reset();
    for (unsigned mode = 0; mode < 256; ++mode) {
        io.write(0x0e, uint8_t(mode));
        assert(io.state().mode == mode);
        for (unsigned channel = 0; channel < 2; ++channel) {
            const uint8_t value = uint8_t(mode ^ (channel * 0x55));
            io.write(9 + channel, value);
            assert(transmitted[channel] == value && writes[channel] == mode + 1);
            assert(io.read(0x0b + channel, false) == 0x90 + channel);
            assert(reads[channel] == mode + 1);
        }
        // RX-full/TX-empty is a documented MAME hack, independent of traffic.
        assert(io.read(0x0d) == 0x0c);
    }
    for (unsigned offset : {7u, 8u, 9u, 10u, 14u, 16u, 256u, std::numeric_limits<unsigned>::max()})
        assert(io.read(offset) == 0xff);
    const auto state = io.state();
    for (unsigned offset : {7u, 11u, 12u, 13u, 16u, 256u, std::numeric_limits<unsigned>::max()})
        for (unsigned data = 0; data < 256; ++data) io.write(offset, uint8_t(data));
    assert(io.state().port_value == state.port_value && io.state().port_config == state.port_config);
    assert(io.state().mode == state.mode && io.state().analog_channel == state.analog_channel);
}
} // namespace

int main() {
    constructor_and_reset();
    ports_and_directions();
    analog_mux();
    counter_mux();
    serial_and_unused_registers();
    std::puts("315-5649: directions, retained reset state, callbacks, analog/counter muxes and reference serial limitation passed");
}
