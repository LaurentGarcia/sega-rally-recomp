// Self-authored bus/register/serial fixtures; no Sega ROM bytes.
#include "runtime/model2a_bus.h"
#include "runtime/model2a_io.h"
#include "runtime/model2_eeprom.h"

#include <cassert>
#include <cstdio>

namespace {
constexpr uint32_t kIO = 0x01c00000;

void masked_bus_and_inputs() {
    rt::Model2Eeprom eeprom;
    uint64_t now = 123456;
    unsigned clocks = 0;
    rt::Model2AIO io(eeprom, [&] { ++clocks; return now; });
    rt::Model2ABus bus({}, {});
    bus.add_model(&io);
    assert(io.claims(kIO) && io.claims(kIO + 0x1f));
    assert(io.claims(kIO + 0x40) && io.claims(kIO + 0x43));
    for (uint32_t a : {kIO - 1, kIO + 0x20, kIO + 0x3f, kIO + 0x44, kIO + 0x202})
        assert(!io.claims(a));
    assert(bus.read_dword(kIO) == 0x00ff00ff);
    assert(bus.read_dword(kIO + 4) == 0x0000008f);
    assert(bus.read_dword(kIO + 8) == 0x00ff00ff);
    assert(bus.read_dword(kIO + 12) == 0x00ff00ff);
    assert(bus.read_dword(kIO + 0x14) == 0x000000ff);
    assert(bus.read_dword(kIO + 0x18) == 0x000c0000);
    assert(clocks == 0); // non-mux input reads do not inspect EEPROM time
    io.inputs.in0 = 0x96;
    io.inputs.in1 = 0x72;
    io.inputs.in2 = 0x39;
    io.inputs.switches = 0x5a;
    assert(bus.read_word(kIO + 2) == 0x96);
    assert(bus.read_dword(kIO + 4) == 0x00390072);
    assert(bus.read_byte(kIO + 12) == 0x5a);
    for (unsigned offset = 1; offset < 0x20; offset += 2)
        assert(bus.read_byte(kIO + offset) == 0);
    // Upper-byte masks neither call outputs nor touch port latches.
    bus.write_byte(kIO + 1, 0xff);
    bus.write_byte(kIO + 3, 0xff);
    assert(clocks == 0 && io.chip().state().port_value[0] == 0xff);
    bus.write_byte(kIO + 0x10, 0xfc); // PA/PB output; remaining ports input
    bus.write_dword(kIO, 0xaa12bb00);
    assert(clocks == 1);
    assert(bus.read_dword(kIO) == 0x00120000);
    // Re-enable input directions and mux-independent host values return.
    bus.write_byte(kIO + 0x10, 0xff);
    assert(bus.read_byte(kIO + 2) == 0x96);
    bus.write_dword(kIO + 0x40, 0x12345678);
    assert(bus.read_dword(kIO + 0x40) == 0);
    for (uint32_t address : {kIO + 0x20, kIO + 0x44}) {
        bool stopped = false;
        try { (void)bus.read_byte(address); }
        catch (const rt::Model2AAccess &) { stopped = true; }
        assert(stopped);
    }
}

void analog_and_outputs() {
    rt::Model2Eeprom eeprom;
    rt::Model2AIO io(eeprom, [] { return uint64_t(0); });
    rt::Model2ABus bus({}, {});
    bus.add_model(&io);
    assert(bus.read_dword(kIO + 0x1c) == 0x008000ff);
    assert(bus.read_byte(kIO + 0x1e) == 0);
    assert(bus.read_word(kIO + 0x1e) == 0);
    for (unsigned channel = 3; channel < 8; ++channel)
        assert(bus.read_byte(kIO + 0x1e) == 0xff);
    assert(io.chip().state().analog_channel == 0);
    io.inputs.analog = {1, 3, 5, 7, 9, 11, 13, 15};
    bus.write_byte(kIO + 0x1e, 0xfe); // mux6
    const unsigned channel = io.chip().state().analog_channel;
    assert(bus.read_byte(kIO + 0x1f) == 0 && io.chip().state().analog_channel == channel);
    assert(bus.read_word(kIO + 0x1e) == 13);
    assert(bus.read_byte(kIO + 0x1e) == 15);
    assert(bus.read_byte(kIO + 0x1e) == 1);
    unsigned drive_calls = 0, lamp_calls = 0;
    io.on_drive_command = [&](uint8_t value) { ++drive_calls; assert(value == 0xa5); };
    io.on_lamp_output = [&](uint8_t value) { ++lamp_calls; assert(value == 0x5a); };
    bus.write_word(kIO + 8, 0xffa5);
    bus.write_word(kIO + 10, 0xff5a);
    assert(drive_calls == 1 && lamp_calls == 1);
    assert(io.drive_command() == 0xa5 && io.lamp_output() == 0x5a);
    bus.write_byte(kIO + 9, 0);
    bus.write_byte(kIO + 11, 0);
    assert(drive_calls == 1 && lamp_calls == 1);
    bus.write_byte(kIO + 0x1e, 6);
    bus.write_byte(kIO, 1); // switch EEPROM mux on
    const auto ports = io.chip().state().port_value;
    io.reset();
    assert(!io.eeprom_mux() && io.chip().state().port_config == 0xff);
    assert(io.chip().state().analog_channel == 6 && io.chip().state().port_value == ports);
    assert(io.drive_command() == 0xa5 && io.lamp_output() == 0x5a);
    assert(drive_calls == 1 && lamp_calls == 1);
}

void eeprom_mux_serial_read() {
    rt::Model2Eeprom eeprom;
    rt::Model2Eeprom::Image image;
    image.fill(0xff);
    constexpr unsigned word_address = 17;
    constexpr uint16_t expected = 0xa35c;
    image[word_address * 2] = uint8_t(expected);
    image[word_address * 2 + 1] = uint8_t(expected >> 8);
    assert(eeprom.load_image(image));
    uint64_t now = 0;
    rt::Model2AIO io(eeprom, [&] { return now; });
    rt::Model2ABus bus({}, {});
    bus.add_model(&io);
    io.inputs.in0 = 0x4a;
    assert(bus.read_byte(kIO + 2) == 0x4a);
    auto port_a = [&](uint8_t data) { now += 1000; bus.write_byte(kIO, data); };
    auto send_bit = [&](bool bit) {
        port_a(uint8_t(0x41 | (bit ? 0x20 : 0)));
        port_a(uint8_t(0xc1 | (bit ? 0x20 : 0)));
    };
    port_a(0x01); // deselect, mux on
    assert(bus.read_byte(kIO + 2) == 0xfa); // upper bits forced, low inputs retained, DO high
    // CLK must be delivered before CS: raising both here cannot clock the
    // start bit into the previously deselected EEPROM.
    port_a(0xe1);
    send_bit(true);
    constexpr unsigned command = (2u << 6) | word_address; // READ opcode10,address6
    for (int bit = 7; bit >= 0; --bit) send_bit((command >> bit) & 1);
    assert(bus.read_byte(kIO + 2) == 0xda); // READ's dummy0 on DO
    uint16_t observed = 0;
    for (unsigned bit = 0; bit < 16; ++bit) {
        send_bit(false); // only rising CLK advances EEPROM output
        const uint8_t data = bus.read_byte(kIO + 2);
        assert((data & 0xdf) == 0xda);
        observed = uint16_t((observed << 1) | ((data >> 5) & 1));
    }
    assert(observed == expected);
    assert(eeprom.save_image() == image);
    port_a(0); // mux off returns the full external IN0 again
    assert(bus.read_byte(kIO + 2) == 0x4a);
}
} // namespace

int main() {
    bool rejected_clock = false;
    rt::Model2Eeprom eeprom;
    try { rt::Model2AIO missing(eeprom, {}); }
    catch (const rt::Fatal &) { rejected_clock = true; }
    assert(rejected_clock);
    masked_bus_and_inputs();
    analog_and_outputs();
    eeprom_mux_serial_read();
    std::puts("Model2A Rally I/O: exact ranges, byte masks, neutral/editable inputs, outputs and EEPROM serial mux passed");
}
