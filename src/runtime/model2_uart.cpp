// license:BSD-3-Clause
// copyright-holders:smf, Robbbert, Carl, Miodrag Milanovic, Vas Crabb
// Adapted from MAME dddd73680656e355bb2b5beecab1167c9f07bf81:
// src/devices/machine/i8251.cpp SHA256274e29a659c75302acdc8ab746034b88c7e337e8122135eac9372db6b0832931;
// i8251.h SHA256a99ee4e12ad03574fd99901ec5a88c84314c3972f5111be0897a07fd2f5f94f2;
// src/emu/diserial.cpp SHA25634ab348975fcafebaf7f5154fcce722b942ea1aefe10a0f5f78296b467310803;
// diserial.h SHA256a787f56d47cc003b6f13918e81ba7115908de28549d3d69a8fc6da951637c53f.
// Changes: plain state/callbacks replace device framework/timers/logging;
// physical clock edges are supplied externally. Idle synchronous programming
// and reset recovery are supported; synchronous serial activity is fenced.
// Reference limitations and callback order are retained; no SCSP sound core.
//
// Copyright (c) smf, Robbbert, Carl, Miodrag Milanovic, Vas Crabb.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
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
#include "runtime/model2_uart.h"
#include "runtime/cpu.h"

#include <bit>
#include <utility>

namespace rt {

Model2UART::Model2UART(Callbacks callbacks) : callbacks_(std::move(callbacks)) { reset(); }
void Model2UART::set_callbacks(Callbacks callbacks) { callbacks_ = std::move(callbacks); }
void Model2UART::output_txd(bool level) { txd_ = level; if (callbacks_.txd) callbacks_.txd(level); }
void Model2UART::output_rts(bool level) { rts_ = level; if (callbacks_.rts) callbacks_.rts(level); }
void Model2UART::output_dtr(bool level) { dtr_ = level; if (callbacks_.dtr) callbacks_.dtr(level); }
void Model2UART::update_rx_ready() { if (callbacks_.rx_ready) callbacks_.rx_ready(rx_ready()); }
void Model2UART::update_tx_ready() { if (callbacks_.tx_ready) callbacks_.tx_ready(tx_ready()); }
void Model2UART::update_tx_empty() { if (callbacks_.tx_empty) callbacks_.tx_empty(tx_empty()); }
void Model2UART::clear_syndet() { status_ &= ~0x40; if (!external_sync_ && callbacks_.syndet) callbacks_.syndet(false); }

void Model2UART::receive_reset() {
    rx_received_ = 0;
    rx_full_ = false;
    rx_sync_ = start_bits_ == 0;
    rx_wait_start_ = start_bits_ != 0;
}

void Model2UART::reset() {
    output_txd(true);
    output_rts(true);
    output_dtr(true);
    tx_shift_empty_ = true;
    receive_reset();
    next_ = Mode;
    status_ = TxEmpty | TxReady;
    mode_ = command_ = rx_data_ = tx_data_ = 0;
    rx_divider_ = tx_divider_ = 0;
    baud_factor_ = 1;
    update_tx_empty();
    update_rx_ready();
    update_tx_ready();
}

void Model2UART::mode_write(uint8_t data) {
    mode_ = data;
    data_bits_ = ((data >> 2) & 3) + 5;
    parity_ = (data & 0x30) == 0x10 ? 1 : (data & 0x30) == 0x30 ? 2 : 0;
    tx_stop_bits_ = (data >> 6) == 0 ? 0 : (data >> 6) == 1 ? 1 : 2;
    rx_stop_bits_ = tx_stop_bits_ ? 1 : 0;
    sync_byte_count_ = (data & 3) ? 0 : (data & 0x80) ? 1 : 2;
    external_sync_ = sync_byte_count_ && (data & 0x40);
    if (sync_byte_count_) {
        tx_stop_bits_ = rx_stop_bits_ = 0;
        sync_word_ = sync_byte_ = 0;
    }
    start_bits_ = sync_byte_count_ ? 0 : 1;
    rx_length_ = data_bits_ + rx_stop_bits_ + bool(parity_);
    baud_factor_ = (data & 3) == 2 ? 16 : (data & 3) == 3 ? 64 : 1;
    next_ = sync_byte_count_ ? (sync_byte_count_ == 1 ? Sync2 : Sync1) : Command;
    receive_reset();
    tx_divider_ = 0;
}

void Model2UART::command_write(uint8_t data) {
    command_ = data;
    output_rts(!(data & 0x20));
    output_dtr(!(data & 2));
    if (data & 0x10) status_ &= ~(ParityError | OverrunError | FramingError);
    if (data & 0x40) next_ = Mode; // returns to mode format, not device_reset
    clear_syndet();
    if (data & 8) output_txd(false);
    check_tx_start();
    update_rx_ready();
    update_tx_ready();
    update_tx_empty();
}

void Model2UART::control_write(uint8_t data) {
    if (next_ == Mode) mode_write(data);
    else if (next_ == Sync1) { sync_word_ = uint16_t(data) << 8; next_ = Sync2; }
    else if (next_ == Sync2) {
        if (sync_byte_count_ == 2) sync_word_ |= data;
        else sync_byte_ = data;
        next_ = Command;
    } else command_write(data);
}

uint8_t Model2UART::status_read() {
    const uint8_t value = (uint8_t(dsr_) << 7) | status_;
    clear_syndet();
    return value;
}

uint8_t Model2UART::data_read(bool side_effects) {
    if (side_effects) { status_ &= ~RxReady; update_rx_ready(); }
    return rx_data_;
}

void Model2UART::data_write(uint8_t data) {
    tx_data_ = data;
    status_ &= ~TxReady;
    update_tx_ready();
    delayed_tx_enable_ = tx_enabled();
    check_tx_start();
    update_tx_ready();
    update_tx_empty();
}

void Model2UART::check_tx_start() {
    if (tx_enabled() && tx_shift_empty_ && !(status_ & TxReady)) start_tx();
}

void Model2UART::start_tx() {
    if (sync_byte_count_) throw Fatal("Model2 UART synchronous transmit activity is not implemented");
    tx_sent_ = tx_length_ = 0;
    tx_shift_empty_ = false;
    auto add = [&](bool bit) { tx_shift_ = uint16_t((tx_shift_ << 1) | bit); ++tx_length_; };
    for (unsigned i = 0; i < start_bits_; ++i) add(false);
    for (unsigned i = 0; i < data_bits_; ++i) add((tx_data_ >> i) & 1);
    if (parity_) add((std::popcount(tx_data_) & 1) ^ (parity_ == 1));
    for (unsigned i = 0; i < tx_stop_bits_; ++i) add(true);
    if (!tx_length_) throw Fatal("Model2 UART has no configured transmit frame");
    status_ &= ~TxEmpty;
    status_ |= TxReady;
}

void Model2UART::transmit_clock() {
    if (++tx_divider_ != baud_factor_) return;
    tx_divider_ = 0;
    if (sync_byte_count_ && tx_enabled()) throw Fatal("Model2 UART synchronous transmit activity is not implemented");
    if (tx_shift_empty_) {
        if (!(status_ & TxReady) && (tx_enabled() || delayed_tx_enable_)) start_tx();
        else { status_ |= TxEmpty; output_txd(!(command_ & 8)); }
        update_tx_ready();
        update_tx_empty();
    }
    if (!tx_shift_empty_) {
        const bool bit = (tx_shift_ >> (tx_length_ - 1 - tx_sent_)) & 1;
        if (++tx_sent_ == tx_length_) tx_shift_empty_ = true;
        output_txd(bit);
    }
}

void Model2UART::receive_bit(bool bit) {
    const bool previous = rx_shift_ & 0x8000;
    rx_shift_ = uint16_t((rx_shift_ >> 1) | (uint16_t(bit) << 15));
    if (rx_wait_start_) {
        if (previous != bit && !bit) {
            rx_wait_start_ = false;
            rx_sync_ = true;
            rx_received_ = 0;
            rx_framing_error_ = rx_parity_error_ = false;
        }
    } else if (rx_sync_) {
        ++rx_received_;
        if (!bit && rx_received_ > rx_length_ - rx_stop_bits_) rx_framing_error_ = true;
        if (rx_received_ == rx_length_) {
            rx_received_ = 0;
            rx_sync_ = false;
            rx_wait_start_ = true;
            rx_full_ = true;
        }
    }
}

void Model2UART::receive_extract() {
    receive_reset();
    const uint8_t data = uint8_t(rx_shift_ >> (16 - rx_length_)) & uint8_t((1u << data_bits_) - 1);
    const bool received_parity = (rx_shift_ >> (16 - rx_length_ + data_bits_)) & 1;
    if (parity_ == 1 && received_parity == bool(std::popcount(data) & 1)) rx_parity_error_ = true;
    if (parity_ == 2 && received_parity != bool(std::popcount(data) & 1)) rx_parity_error_ = true;
    if (rx_parity_error_) status_ |= ParityError;
    if (rx_framing_error_) status_ |= FramingError;
    receive_character(data);
}

void Model2UART::receive_character(uint8_t data) {
    rx_data_ = data;
    if (status_ & RxReady) status_ |= OverrunError;
    status_ |= RxReady;
    update_rx_ready();
}

void Model2UART::receive_clock() {
    if (!(command_ & 4)) return;
    if (sync_byte_count_) throw Fatal("Model2 UART synchronous receive activity is not implemented");
    const bool was_sync = rx_sync_;
    if (was_sync && --rx_divider_) return;
    receive_bit(rxd_);
    if (rx_sync_) rx_divider_ = was_sync ? baud_factor_ : 3 * baud_factor_ / 2;
    if (rx_full_) receive_extract();
}

void Model2UART::write_cts(bool level) {
    cts_ = level;
    check_tx_start();
    update_tx_ready();
    update_tx_empty();
}
void Model2UART::write_txc(bool level) {
    if (txc_ != level) { txc_ = level; if (!txc_) transmit_clock(); }
}
void Model2UART::write_rxc(bool level) {
    if (!rxc_ && level) receive_clock();
    rxc_ = level;
}

void Model2UART::write_syn(bool level) {
    if (external_sync_ && level) throw Fatal("Model2 UART external synchronous detection is not implemented");
}

Model2UART::Snapshot Model2UART::snapshot() const {
    return {status_, command_, mode_, rx_data_, tx_data_, uint8_t(next_), delayed_tx_enable_,
        cts_, dsr_, rxd_, rxc_, txc_, txd_, rts_, dtr_, rx_divider_, tx_divider_, baud_factor_,
        start_bits_, data_bits_, tx_stop_bits_, rx_stop_bits_, parity_, rx_shift_, tx_shift_,
        rx_wait_start_, rx_sync_, rx_full_, rx_framing_error_, rx_parity_error_, tx_shift_empty_,
        rx_received_, rx_length_, tx_sent_, tx_length_, sync_byte_count_, sync_word_, sync_byte_, external_sync_};
}

} // namespace rt
