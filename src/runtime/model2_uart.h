// license:BSD-3-Clause
// copyright-holders:smf, Robbbert, Carl, Miodrag Milanovic, Vas Crabb
// Edge-driven asynchronous i8251/uPD71051, from pinned public MAME.
#pragma once

#include <cstdint>
#include <functional>

namespace rt {

class Model2UART {
public:
    struct Callbacks {
        std::function<void(bool)> txd, dtr, rts, rx_ready, tx_ready, tx_empty, syndet;
    };
    enum Status : uint8_t { TxReady = 1, RxReady = 2, TxEmpty = 4,
                           ParityError = 8, OverrunError = 16, FramingError = 32 };
    explicit Model2UART(Callbacks callbacks = {});
    void set_callbacks(Callbacks callbacks);
    void reset();
    uint8_t data_read(bool side_effects = true);
    void data_write(uint8_t data);
    uint8_t status_read();
    void control_write(uint8_t data);
    void write_rxd(bool level) { rxd_ = level; }
    void write_cts(bool level);
    void write_dsr(bool level) { dsr_ = !level; }
    void write_txc(bool level); // transmit on falling edges
    void write_rxc(bool level); // receive on rising edges
    void write_syn(bool level);

    bool tx_ready() const { return tx_enabled() && (status_ & TxReady); }
    bool rx_ready() const { return (command_ & 4) && (status_ & RxReady); }
    bool tx_empty() const { return status_ & TxEmpty; }
    bool txd_level() const { return txd_; }
    bool dtr_level() const { return dtr_; }
    bool rts_level() const { return rts_; }
    uint8_t mode() const { return mode_; }
    uint8_t command() const { return command_; }
    unsigned baud_factor() const { return baud_factor_; }
    bool expects_mode() const { return next_ == Mode; }

    struct Snapshot {
        uint8_t status, command, mode, rx_data, tx_data, parser;
        bool delayed_tx_enable, cts, dsr, rxd, rxc, txc, txd, rts, dtr;
        unsigned rx_divider, tx_divider, baud_factor, start_bits, data_bits, tx_stop_bits, rx_stop_bits, parity;
        uint16_t rx_shift, tx_shift;
        bool rx_wait_start, rx_sync, rx_full, rx_framing_error, rx_parity_error, tx_shift_empty;
        unsigned rx_received, rx_length, tx_sent, tx_length, sync_byte_count;
        uint16_t sync_word;
        uint8_t sync_byte;
        bool external_sync;
        bool operator==(const Snapshot &) const = default;
    };
    Snapshot snapshot() const;

    // Deliberately preserved reference limitations: sync is an explicit
    // activity frontier (idle sync programming/reset is supported); async
    // BRKDET is absent; 1.5 TX stop bits become two, RX checks
    // one stop bit for both 1.5/two-stop configurations. Parity on TX counts
    // the entire written byte, even in narrower data formats.
private:
    bool tx_enabled() const { return (command_ & 1) && !cts_; }
    void mode_write(uint8_t data);
    void command_write(uint8_t data);
    void check_tx_start();
    void start_tx();
    void transmit_clock();
    void receive_clock();
    void receive_bit(bool bit);
    void receive_extract();
    void receive_reset();
    void receive_character(uint8_t data);
    void update_rx_ready();
    void update_tx_ready();
    void update_tx_empty();
    void output_txd(bool level);
    void output_rts(bool level);
    void output_dtr(bool level);
    void clear_syndet();

    Callbacks callbacks_;
    uint8_t status_ = 0, command_ = 0, mode_ = 0, rx_data_ = 0, tx_data_ = 0;
    enum Phase : uint8_t { Command, Mode, Sync1, Sync2 };
    Phase next_ = Mode;
    bool delayed_tx_enable_ = false;
    bool cts_ = true, dsr_ = true, rxd_ = true, rxc_ = false, txc_ = false;
    bool txd_ = true, rts_ = true, dtr_ = true;
    unsigned rx_divider_ = 0, tx_divider_ = 0, baud_factor_ = 1;
    unsigned start_bits_ = 0, data_bits_ = 0, tx_stop_bits_ = 0, rx_stop_bits_ = 0;
    unsigned parity_ = 0; // none0, odd1, even2 (pinned serial interface)
    uint16_t rx_shift_ = 0x8000, tx_shift_ = 0;
    bool rx_wait_start_ = false, rx_sync_ = false, rx_full_ = false;
    bool rx_framing_error_ = false, rx_parity_error_ = false, tx_shift_empty_ = true;
    unsigned rx_received_ = 0, rx_length_ = 0, tx_sent_ = 0, tx_length_ = 0;
    unsigned sync_byte_count_ = 0;
    uint16_t sync_word_ = 0;
    uint8_t sync_byte_ = 0;
    bool external_sync_ = false;
};

} // namespace rt
