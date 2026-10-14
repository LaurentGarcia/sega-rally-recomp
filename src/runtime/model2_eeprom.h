// license:BSD-3-Clause
// copyright-holders:Aaron Giles and contributors
// Native adapter of MAME's 93C46 serial EEPROM contract; see .cpp for provenance.
#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace rt {

// Model 2A's 93C46: 64 x 16 bits, MSB-first serial transfers. The default
// matches MAME's Model 2A configuration (sequential reads disabled). Time is
// monotonic board time in nanoseconds, supplied explicitly by the caller;
// neither polling nor serial clocks advance it inside this device.
class Model2Eeprom {
public:
    using Image = std::array<uint8_t, 128>;
    static constexpr uint64_t WRITE_NS = 1'750'000;
    static constexpr uint64_t ERASE_NS = 1'000'000;
    static constexpr uint64_t WRITE_ALL_NS = 8'000'000;
    static constexpr uint64_t ERASE_ALL_NS = 8'000'000;

    explicit Model2Eeprom(bool sequential_read = false);

    // Power reset, not a physical reset input: preserve NVRAM, deselect pins,
    // disable writes and discard partial commands/busy timing. Completed data
    // changes persist, as in the reference. CS falling alone never write-locks
    // the chip and never cancels a started write/erase cycle.
    void reset();
    void di_write(bool level) { di_ = level; }
    void clk_write(bool level, uint64_t now_ns);
    void cs_write(bool level, uint64_t now_ns);
    bool do_read(uint64_t now_ns) const;
    bool ready(uint64_t now_ns) const;
    bool write_enabled() const { return !locked_; }

    // Raw MAME-compatible NVRAM: 64 little-endian words. Load before clocking
    // the device; it changes only the cells, not serial state. Wrong-sized
    // images are rejected without changing cells. No host file I/O here.
    bool load_image(std::span<const uint8_t> bytes);
    Image save_image() const { return data_; }

private:
    enum class State { Reset, Start, Command, Read, Data, Completion };
    void execute_command(uint64_t now_ns);
    void execute_write(uint64_t now_ns);
    void start_busy(uint64_t now_ns, uint64_t duration_ns);
    uint16_t read_word(unsigned address) const;
    void write_word(unsigned address, uint16_t value);

    Image data_;
    bool sequential_read_;
    bool cs_ = false, clk_ = false, di_ = false, locked_ = true;
    State state_ = State::Reset;
    uint64_t cs_rising_ns_ = 0;
    uint64_t busy_start_ns_ = 0, busy_duration_ns_ = 0;
    unsigned bits_ = 0;
    uint8_t command_address_ = 0, address_ = 0;
    bool write_all_ = false;
    uint32_t shift_ = 0;
};

} // namespace rt
