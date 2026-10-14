// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
#include "runtime/model2_eeprom.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (0)

namespace {
using rt::Model2Eeprom;

// Drive only the public pins; expected words/times come from the command
// table and timing contracts, not from the device's internal state machine.
struct Serial {
    Model2Eeprom eeprom;
    uint64_t now = 1000;
    explicit Serial(bool sequential = false) : eeprom(sequential) {}
    void begin() {
        eeprom.cs_write(false, now);
        eeprom.clk_write(false, now);
        eeprom.cs_write(true, ++now);
    }
    void end() { eeprom.cs_write(false, ++now); }
    bool bit(bool data) {
        eeprom.clk_write(false, ++now);
        eeprom.di_write(data);
        now += 1000;
        eeprom.clk_write(true, now);
        return eeprom.do_read(now);
    }
    void bits(uint32_t value, unsigned count) {
        for (unsigned i = count; i; --i) bit((value >> (i - 1)) & 1);
    }
    void command(unsigned opcode, unsigned address) {
        bit(true);
        bits((opcode << 6) | address, 8);
    }
    void control(unsigned address) { begin(); command(0, address); end(); }
    void enable() { control(0x30); }
    void disable() { control(0x00); }
    void write(unsigned address, uint16_t value) {
        begin(); command(1, address); bits(value, 16);
    }
    uint16_t next_word() {
        uint16_t result = 0;
        for (unsigned i = 0; i < 16; ++i) result = uint16_t((result << 1) | bit(false));
        return result;
    }
    uint16_t read(unsigned address) {
        begin(); command(2, address);
        CHECK(!eeprom.do_read(now)); // One dummy zero, before data clock 1.
        const auto result = next_word();
        end();
        return result;
    }
    void await(uint64_t duration) { end(); now += duration; }
};

Model2Eeprom::Image patterned_image()
{
    Model2Eeprom::Image image{};
    for (unsigned address = 0; address < 64; ++address) {
        const uint16_t word = uint16_t(0x8123u ^ (address * 0x437u));
        image[address * 2] = uint8_t(word);
        image[address * 2 + 1] = uint8_t(word >> 8);
    }
    return image;
}

uint16_t image_word(const Model2Eeprom::Image &image, unsigned address)
{
    return uint16_t(image[address * 2] | (image[address * 2 + 1] << 8));
}

void erased_and_persistence()
{
    Serial s;
    CHECK(s.eeprom.ready(0));
    CHECK(s.eeprom.do_read(0));
    CHECK(!s.eeprom.write_enabled());
    const auto erased = s.eeprom.save_image();
    CHECK(std::all_of(erased.begin(), erased.end(), [](uint8_t v) { return v == 0xff; }));
    for (unsigned address = 0; address < 64; ++address) CHECK(s.read(address) == 0xffff);
    const auto image = patterned_image();
    CHECK(s.eeprom.load_image(image));
    for (unsigned address = 0; address < 64; ++address)
        CHECK(s.read(address) == image_word(image, address));
    CHECK(s.eeprom.save_image() == image);
    CHECK(!s.eeprom.load_image(std::span<const uint8_t>(image.data(), 127)));
    CHECK(!s.eeprom.load_image({}));
    std::array<uint8_t, 129> oversized{};
    CHECK(!s.eeprom.load_image(oversized));
    CHECK(s.eeprom.save_image() == image);
    Serial restored;
    CHECK(restored.eeprom.load_image(s.eeprom.save_image()));
    CHECK(restored.read(63) == image_word(image, 63));
}

void protection_and_all_commands()
{
    Serial s;
    const auto image = patterned_image();
    CHECK(s.eeprom.load_image(image));
    // Every mutating instruction must be inert and must not enter busy while locked.
    s.write(7, 0);
    CHECK(s.eeprom.ready(s.now));
    s.end();
    s.begin(); s.command(3, 7); CHECK(s.eeprom.ready(s.now)); s.end();
    s.control(0x20);
    s.begin(); s.command(0, 0x10); s.bits(0, 16); CHECK(s.eeprom.ready(s.now)); s.end();
    CHECK(s.eeprom.save_image() == image);
    CHECK(!s.eeprom.write_enabled());

    // Low four bits of extended commands are don't-care, not extra opcodes.
    s.control(0x3f);
    CHECK(s.eeprom.write_enabled());
    for (unsigned address = 0; address < 64; ++address) {
        const uint16_t value = uint16_t(0xa55a ^ (address * 7919));
        s.write(address, value);
        CHECK(!s.eeprom.ready(s.now));
        s.await(Model2Eeprom::WRITE_NS);
        CHECK(s.read(address) == value);
        CHECK(s.eeprom.write_enabled()); // CS falling does not lock writes.
    }
    s.begin(); s.command(3, 7);
    CHECK(!s.eeprom.ready(s.now));
    s.await(Model2Eeprom::ERASE_NS);
    CHECK(s.read(7) == 0xffff);
    CHECK(s.read(6) == uint16_t(0xa55a ^ (6 * 7919)));

    s.begin(); s.command(0, 0x2f); // ERAL
    CHECK(!s.eeprom.ready(s.now));
    s.await(Model2Eeprom::ERASE_ALL_NS);
    for (unsigned address = 0; address < 64; ++address) CHECK(s.read(address) == 0xffff);
    s.begin(); s.command(0, 0x1f); s.bits(0x55aa, 16); // WRAL
    CHECK(!s.eeprom.ready(s.now));
    s.await(Model2Eeprom::WRITE_ALL_NS);
    for (unsigned address = 0; address < 64; ++address) CHECK(s.read(address) == 0x55aa);
    s.begin(); s.command(0, 0x10); s.bits(0xaa55, 16);
    s.await(Model2Eeprom::WRITE_ALL_NS);
    for (unsigned address = 0; address < 64; ++address) CHECK(s.read(address) == 0);
    // MAME WRAL ANDs with old cells; ordinary WRITE performs implicit erase.
    s.write(5, 0xffff); s.await(Model2Eeprom::WRITE_NS);
    CHECK(s.read(5) == 0xffff);
    CHECK(s.read(4) == 0);
    s.control(0x0f);
    CHECK(!s.eeprom.write_enabled());
    s.write(5, 0); s.end();
    CHECK(s.read(5) == 0xffff);
}

void busy_boundaries()
{
    for (unsigned operation = 0; operation < 4; ++operation) {
        Serial s;
        s.enable();
        uint64_t duration;
        if (operation == 0) {
            s.write(3, 0x8421); duration = Model2Eeprom::WRITE_NS;
        } else if (operation == 1) {
            s.begin(); s.command(3, 3); duration = Model2Eeprom::ERASE_NS;
        } else if (operation == 2) {
            s.begin(); s.command(0, 0x10); s.bits(0xa55a, 16); duration = Model2Eeprom::WRITE_ALL_NS;
        } else {
            s.begin(); s.command(0, 0x20); duration = Model2Eeprom::ERASE_ALL_NS;
        }
        const uint64_t started = s.now;
        CHECK(!s.eeprom.ready(started));
        CHECK(s.eeprom.do_read(started)); // Reference requires a new CS for status.
        s.end();
        CHECK(s.eeprom.do_read(s.now)); // Deselected DO is pulled high during busy.
        s.begin();
        CHECK(!s.eeprom.do_read(s.now));
        // Busy clocks cannot start a command, disable writes or alter cells.
        const auto before = s.eeprom.save_image();
        s.command(0, 0); // Would be EWDS if accepted.
        CHECK(s.eeprom.write_enabled());
        CHECK(s.eeprom.save_image() == before);
        CHECK(!s.eeprom.ready(started + duration - 1));
        CHECK(!s.eeprom.do_read(started + duration - 1));
        CHECK(s.eeprom.ready(started + duration));
        CHECK(s.eeprom.do_read(started + duration));
        CHECK(s.eeprom.ready(started + duration + 1));
        // No polling side effect: the same instant must stay busy/ready.
        CHECK(!s.eeprom.do_read(started + duration - 1));
        s.now = started + duration;
        // At the exact ready boundary a new start bit is accepted without a
        // further CS edge; construct the following command manually.
        s.eeprom.clk_write(false, s.now);
        s.eeprom.di_write(true);
        s.eeprom.clk_write(true, s.now);
        s.bits(0x80 | 3, 8);
        CHECK(!s.eeprom.do_read(s.now));
        CHECK(s.next_word() == image_word(before, 3));
        s.end();
    }
}

void edges_and_abort()
{
    Serial s;
    const auto image = patterned_image();
    CHECK(s.eeprom.load_image(image));
    // Clocks with CS low and repeated high writes are not rising clock edges.
    s.command(0, 0x30);
    CHECK(!s.eeprom.write_enabled());
    s.begin();
    s.eeprom.di_write(true);
    s.eeprom.clk_write(true, s.now); // Simultaneous CS+CLK must not start.
    s.eeprom.clk_write(true, ++s.now); // Repeated level still does not start.
    for (unsigned i = 0; i < 5; ++i) s.bit(false); // Leading zeroes ignored.
    s.command(2, 11);
    CHECK(!s.eeprom.do_read(s.now));
    uint16_t actual = 0;
    for (unsigned i = 0; i < 16; ++i) {
        const bool bit = s.bit(false);
        s.eeprom.clk_write(true, ++s.now);
        CHECK(s.eeprom.do_read(s.now) == bit);
        s.eeprom.di_write(true); // DI changes do not clock read data.
        CHECK(s.eeprom.do_read(s.now) == bit);
        s.eeprom.clk_write(false, ++s.now);
        CHECK(s.eeprom.do_read(s.now) == bit);
        actual = uint16_t((actual << 1) | bit);
    }
    CHECK(actual == image_word(image, 11));
    s.end();

    // CS abort at every truncated command length must not execute EWEN.
    for (unsigned prefix = 1; prefix < 9; ++prefix) {
        s.begin(); s.bits(0x130 >> (9 - prefix), prefix); s.end();
        CHECK(!s.eeprom.write_enabled());
    }
    s.enable();
    for (unsigned data_bits = 0; data_bits < 16; ++data_bits) {
        s.begin(); s.command(1, 4); s.bits(0, data_bits); s.end();
        CHECK(s.eeprom.ready(s.now));
        CHECK(s.eeprom.save_image() == image);
    }
    s.begin(); s.command(2, 2); s.bits(0, 5); s.end();
    CHECK(s.read(2) == image_word(image, 2));
    // Power reset differs from ordinary CS: write-lock and partial state reset,
    // while the NVRAM image survives, including the reference's early commit.
    s.write(9, 0x1234);
    CHECK(!s.eeprom.ready(s.now));
    s.eeprom.reset();
    CHECK(s.eeprom.ready(s.now));
    CHECK(!s.eeprom.write_enabled());
    CHECK(s.eeprom.do_read(s.now));
    CHECK(s.read(9) == 0x1234);
    s.enable(); s.begin(); s.command(1, 9); s.bits(0, 15);
    s.eeprom.reset();
    CHECK(s.read(9) == 0x1234);
}

void sequential_and_reference_overclock()
{
    const auto image = patterned_image();
    Serial sequential(true);
    CHECK(sequential.eeprom.load_image(image));
    sequential.begin(); sequential.command(2, 62);
    CHECK(!sequential.eeprom.do_read(sequential.now));
    // Two complete address wraps; no intervening dummy bit or CS transition.
    for (unsigned i = 0; i < 131; ++i)
        CHECK(sequential.next_word() == image_word(image, (62 + i) & 63));
    sequential.end();

    Serial single;
    CHECK(single.eeprom.load_image(image));
    single.begin(); single.command(2, 63);
    CHECK(single.next_word() == image_word(image, 63));
    // Model2A's reference defaults to non-streaming; its 32-bit shift register
    // emits sixteen zeroes after the word, then ones, rather than address zero.
    CHECK(single.next_word() == 0);
    CHECK(single.next_word() == 0xffff);
    CHECK(single.next_word() == 0xffff);
    single.end();
    CHECK(single.read(0) == image_word(image, 0));
}
} // namespace

int main()
{
    erased_and_persistence();
    protection_and_all_commands();
    busy_boundaries();
    edges_and_abort();
    sequential_and_reference_overclock();
    std::puts("model2 EEPROM: serial commands, protection, aborts, timing, persistence and wrap passed");
}
