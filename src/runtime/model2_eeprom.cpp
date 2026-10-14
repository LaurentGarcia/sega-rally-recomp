// license:BSD-3-Clause
// copyright-holders:Aaron Giles and contributors
//
// Adapted from MAME src/devices/machine/{eeprom,eepromser}.{cpp,h}, pinned at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (https://github.com/mamedev/mame).
// Source SHA-256:
// eepromser.cpp 51431fe4a70163b0bf56a30bbb293c0ba8872e7cd6fe051a6bdf2e17f8824caa
// eepromser.h   0877cdad4d2e8c300386d2dce1737c51210c40d94115837c00482b628a7adf74
// eeprom.cpp    e28f650aa8eda8c54897ae08047092bab3053f4dd84c66c72d6711a950423ef6
// eeprom.h      415e84a2e995904615f4c7532c6b73664a4e513832c0bdbde1844af8ef8887fb
//
// Reference limitations deliberately retained: a write/erase changes cells and
// starts its busy interval at the final command/data clock, not at CS falling;
// WRAL programs existing_word & data and thus requires ERAL to restore ones.
// DO is ready/busy only after CS is reasserted. Non-streaming reads reproduce
// the reference's 32-bit output shift register even when overclocked.
// ST DS1077 Rev 20 (https://www.st.com/resource/en/datasheet/m93c46-w.pdf),
// sections 5-8, independently confirms command width, dummy zero, MSB-first
// output, optional sequential wrap, write locking and erased FF. That modern
// part starts programming on CS falling and checks the exact pulse count;
// these differences and analog setup/hold/propagation delays are not modeled.
// The fixed 1750/1000/8000us timings are MAME defaults, not measured Sega PCB
// timings. No power-failure corruption model or save-state format is provided.
//
// Copyright (c) Aaron Giles and contributors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "runtime/model2_eeprom.h"
#include <algorithm>

namespace rt {

Model2Eeprom::Model2Eeprom(bool sequential_read) : sequential_read_(sequential_read)
{
    data_.fill(0xff);
    reset();
}

void Model2Eeprom::reset()
{
    cs_ = clk_ = di_ = false;
    locked_ = true;
    state_ = State::Reset;
    cs_rising_ns_ = busy_start_ns_ = busy_duration_ns_ = 0;
    bits_ = command_address_ = address_ = shift_ = 0;
    write_all_ = false;
}

bool Model2Eeprom::ready(uint64_t now_ns) const
{
    // Elapsed time avoids wrapping an absolute deadline near UINT64_MAX.
    return busy_duration_ns_ == 0 ||
        (now_ns >= busy_start_ns_ && now_ns - busy_start_ns_ >= busy_duration_ns_);
}

bool Model2Eeprom::do_read(uint64_t now_ns) const
{
    if (state_ == State::Start) return ready(now_ns);
    if (state_ == State::Read) return (shift_ & 0x80000000u) != 0;
    return true; // High impedance with the board's pull-up, including CS low.
}

void Model2Eeprom::cs_write(bool level, uint64_t now_ns)
{
    if (cs_ == level) return;
    cs_ = level;
    if (!level) {
        state_ = State::Reset;
    } else {
        cs_rising_ns_ = now_ns;
        state_ = State::Start;
    }
}

void Model2Eeprom::clk_write(bool level, uint64_t now_ns)
{
    if (clk_ == level) return;
    clk_ = level;
    if (!level) return; // This 93C46 clocks both input and output on rising edges.
    switch (state_) {
    case State::Start:
        // Ignore simultaneous CS/CLK edges, as the pinned reference does.
        if (di_ && ready(now_ns) && now_ns > cs_rising_ns_) {
            bits_ = command_address_ = 0;
            state_ = State::Command;
        }
        break;
    case State::Command:
        command_address_ = uint8_t((command_address_ << 1) | unsigned(di_));
        if (++bits_ == 8) execute_command(now_ns); // Two opcode + six address.
        break;
    case State::Read:
        if (bits_ == 16 && sequential_read_) {
            address_ = uint8_t((address_ + 1) & 63);
            bits_ = 0;
        }
        if (bits_ == 0) shift_ = uint32_t(read_word(address_)) << 16;
        else shift_ = (shift_ << 1) | 1u;
        // Beyond 32 overclocks the non-streaming shift register is all ones.
        if (bits_ < 33) ++bits_;
        break;
    case State::Data:
        shift_ = (shift_ << 1) | unsigned(di_);
        if (++bits_ == 16) execute_write(now_ns);
        break;
    case State::Reset:
    case State::Completion:
        break;
    }
}

void Model2Eeprom::execute_command(uint64_t now_ns)
{
    bits_ = 0;
    address_ = command_address_ & 63;
    switch (command_address_ >> 6) {
    case 0:
        switch (address_ >> 4) {
        case 0: locked_ = true; state_ = State::Reset; break; // EWDS 00xxxx
        case 1: // WRAL 01xxxx
            shift_ = 0; write_all_ = true; state_ = State::Data;
            break;
        case 2: // ERAL 10xxxx
            state_ = State::Reset;
            if (!locked_) {
                data_.fill(0xff);
                start_busy(now_ns, ERASE_ALL_NS);
            }
            break;
        case 3: locked_ = false; state_ = State::Reset; break; // EWEN 11xxxx
        }
        address_ = 0;
        break;
    case 1: // WRITE
        shift_ = 0; write_all_ = false; state_ = State::Data;
        break;
    case 2: // READ: the dummy zero is visible before the next rising edge.
        shift_ = 0; state_ = State::Read;
        break;
    case 3: // ERASE
        state_ = State::Reset;
        if (!locked_) {
            write_word(address_, 0xffff);
            start_busy(now_ns, ERASE_NS);
        }
        break;
    }
}

void Model2Eeprom::execute_write(uint64_t now_ns)
{
    state_ = State::Reset;
    if (locked_) return;
    if (write_all_) {
        for (unsigned address = 0; address < 64; ++address)
            write_word(address, uint16_t(read_word(address) & shift_));
        start_busy(now_ns, WRITE_ALL_NS);
    } else {
        write_word(address_, uint16_t(shift_));
        start_busy(now_ns, WRITE_NS);
    }
}

void Model2Eeprom::start_busy(uint64_t now_ns, uint64_t duration_ns)
{
    busy_start_ns_ = now_ns;
    busy_duration_ns_ = duration_ns;
    state_ = State::Completion;
}

uint16_t Model2Eeprom::read_word(unsigned address) const
{
    return uint16_t(data_[address * 2] | (uint16_t(data_[address * 2 + 1]) << 8));
}

void Model2Eeprom::write_word(unsigned address, uint16_t value)
{
    data_[address * 2] = uint8_t(value);
    data_[address * 2 + 1] = uint8_t(value >> 8);
}

bool Model2Eeprom::load_image(std::span<const uint8_t> bytes)
{
    if (bytes.size() != data_.size()) return false;
    std::copy(bytes.begin(), bytes.end(), data_.begin());
    return true;
}

} // namespace rt
