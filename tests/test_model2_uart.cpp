// Self-authored pin/format fixtures; no game data or synthetic sound replies.
#include "runtime/model2_uart.h"
#include "runtime/cpu.h"

#include <bit>
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
void tx_pulses(rt::Model2UART &uart, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        uart.write_txc(true); uart.write_txc(true);
        uart.write_txc(false); uart.write_txc(false);
    }
}
void rx_hold(rt::Model2UART &uart, bool bit, unsigned clocks) {
    uart.write_rxd(bit);
    for (unsigned i = 0; i < clocks; ++i) {
        uart.write_rxc(false); uart.write_rxc(false);
        uart.write_rxc(true); uart.write_rxc(true);
    }
}
void send_rx(rt::Model2UART &uart, uint8_t data, bool parity_error = false, bool framing_error = false) {
    const uint8_t mode = uart.mode();
    const unsigned factor = uart.baud_factor(), bits = ((mode >> 2) & 3) + 5;
    const bool parity = mode & 0x10;
    rx_hold(uart, true, factor * 2);
    rx_hold(uart, false, factor);
    for (unsigned bit = 0; bit < bits; ++bit) rx_hold(uart, (data >> bit) & 1, factor);
    if (parity) {
        const uint8_t masked = data & uint8_t((1u << bits) - 1);
        const bool p = (std::popcount(masked) & 1) ^ !(mode & 0x20) ^ parity_error;
        rx_hold(uart, p, factor);
    }
    if (mode & 0xc0) rx_hold(uart, !framing_error, factor);
}

void format_sweep() {
    unsigned frames = 0, receive_frames = 0;
    for (unsigned factor : {1u, 2u, 3u}) for (unsigned width = 0; width < 4; ++width)
    for (unsigned parity = 0; parity < 4; ++parity) for (unsigned stops = 0; stops < 4; ++stops) {
        const uint8_t mode = uint8_t(factor | width << 2 | parity << 4 | stops << 6);
        const unsigned divisor = factor == 1 ? 1 : factor == 2 ? 16 : 64;
        const unsigned stop_count = stops == 0 ? 0 : stops == 1 ? 1 : 2;
        for (unsigned byte = 0; byte < 256; ++byte) {
            std::vector<bool> output;
            rt::Model2UART uart;
            rt::Model2UART::Callbacks callbacks;
            callbacks.txd = [&](bool bit) { output.push_back(bit); };
            uart.set_callbacks(callbacks);
            uart.write_cts(false);
            uart.control_write(mode);
            uart.control_write(0x27);
            uart.data_write(uint8_t(byte));
            std::vector<bool> expected{false};
            for (unsigned bit = 0; bit < width + 5; ++bit) expected.push_back((byte >> bit) & 1);
            if (parity & 1) expected.push_back((std::popcount(byte) & 1) ^ (parity == 1));
            for (unsigned bit = 0; bit < stop_count; ++bit) expected.push_back(true);
            assert(uart.tx_ready() && !uart.tx_empty());
            for (unsigned bit = 0; bit < expected.size(); ++bit) {
                tx_pulses(uart, divisor - 1);
                assert(output.size() == bit);
                tx_pulses(uart, 1);
                assert(output.size() == bit + 1 && output.back() == expected[bit]);
            }
            assert(!uart.tx_empty()); // empty pin updates on the next divided falling edge
            tx_pulses(uart, divisor);
            assert(output.back() && uart.tx_empty());
            assert(uart.status_read() == 0x85);
            send_rx(uart, uint8_t(byte));
            assert(uart.rx_ready());
            assert(uart.data_read(false) == (byte & ((1u << (width + 5)) - 1)));
            assert(uart.rx_ready());
            assert((uart.status_read() & (rt::Model2UART::ParityError | rt::Model2UART::FramingError)) == 0);
            assert(uart.data_read() == (byte & ((1u << (width + 5)) - 1)));
            assert(!uart.rx_ready());
            ++frames; ++receive_frames;
        }
    }
    std::printf("UART format sweep: %u TX and %u RX frames across192 async modes\n", frames, receive_frames);
}

void commands_and_buffering() {
    std::vector<bool> tx;
    std::vector<bool> ready;
    rt::Model2UART::Callbacks callbacks;
    callbacks.txd = [&](bool bit) { tx.push_back(bit); };
    callbacks.tx_ready = [&](bool bit) { ready.push_back(bit); };
    rt::Model2UART uart(callbacks);
    assert(uart.status_read() == 0x85 && !uart.tx_ready() && !uart.rx_ready() && uart.tx_empty());
    uart.write_dsr(true);
    assert(uart.status_read() == 5);
    uart.write_dsr(false);
    assert(uart.status_read() == 0x85);
    uart.control_write(0x4d); // 8N1 x1
    uart.control_write(0x27);
    assert(!uart.tx_ready() && !uart.dtr_level() && !uart.rts_level());
    uart.data_write(0x5a); // CTS high: hold data, no fabricated transmission
    assert((uart.status_read() & 1) == 0 && uart.tx_empty());
    uart.write_cts(false);
    assert(uart.tx_ready() && !uart.tx_empty());
    uart.data_write(0xa5); // one additional holding byte
    assert(!uart.tx_ready());
    uart.control_write(0x26); // disabled, already queued byte retains delayed enable
    tx.clear();
    tx_pulses(uart, 20); // both complete frames, LSB-first
    assert(tx.size() == 20);
    for (unsigned frame = 0; frame < 2; ++frame) {
        const uint8_t expected = frame ? 0xa5 : 0x5a;
        assert(!tx[frame * 10] && tx[frame * 10 + 9]);
        for (unsigned bit = 0; bit < 8; ++bit) assert(tx[frame * 10 + bit + 1] == bool((expected >> bit) & 1));
    }
    assert(!uart.tx_ready()); // status-bufferready and gated pin are different
    assert(uart.status_read() & 1);
    tx_pulses(uart, 1);
    assert(uart.tx_empty());
    uart.control_write(0x2e); // break asserts immediately while idle
    assert(!uart.txd_level());
    tx_pulses(uart, 5);
    assert(!uart.txd_level());
    uart.control_write(0x26);
    assert(!uart.txd_level()); // mark returns at next physical divided TX edge
    tx_pulses(uart, 1);
    assert(uart.txd_level());
    send_rx(uart, 0x36);
    assert(uart.rx_ready());
    uart.control_write(0x22); // disable receiver masks pin, retains unread data/status
    assert(!uart.rx_ready() && (uart.status_read() & 2));
    uart.control_write(0x26);
    assert(uart.rx_ready() && uart.data_read() == 0x36);
    send_rx(uart, 0x11);
    send_rx(uart, 0x22);
    assert(uart.status_read() & rt::Model2UART::OverrunError);
    assert(uart.data_read() == 0x22); // new receive byte overwrites unread latch
    uart.control_write(0x66); // internal reset returns to mode format only
    assert(uart.expects_mode() && (uart.status_read() & rt::Model2UART::OverrunError));
    uart.control_write(0x7d); // 8E1 x1
    uart.control_write(0x26);
    send_rx(uart, 0x69, true, true);
    assert((uart.status_read() & 0x38) == 0x38);
    uart.control_write(0x36); // error reset does not clear RX ready
    assert((uart.status_read() & 0x38) == 0 && uart.rx_ready());
    uart.reset();
    assert(uart.status_read() == 0x85 && uart.expects_mode());
    assert(uart.data_read() == 0 && uart.txd_level() && uart.rts_level() && uart.dtr_level());
    // Real8251 recovery is safe even when three zero writes program sync mode.
    for (uint8_t byte : {0x00, 0x00, 0x00, 0x40, 0x4e, 0x37}) {
        uart.control_write(byte);
        tx_pulses(uart, 4); // idle sync programming cannot become a fake activity fault
    }
    assert(uart.mode() == 0x4e && uart.command() == 0x37 && uart.baud_factor() == 16);
    const auto state = uart.snapshot();
    assert(uart.snapshot() == state); // no callbacks/side effects in snapshot
    uart.reset();
    uart.control_write(0x3c);
    uart.control_write(0x55);
    uart.control_write(0xaa);
    uart.control_write(0x01); // enabling sync transmission is a frontier on actualclock
    bool unsupported = false;
    try { tx_pulses(uart, 1); }
    catch (const rt::Fatal &) { unsupported = true; }
    assert(unsupported);
    assert(!ready.empty());
}

void parser_and_reset_retention() {
    rt::Model2UART uart;
    uart.control_write(0x00); // two sync bytes, despite mode-bit description ambiguity
    uart.control_write(0x12);
    assert(uart.snapshot().parser == 3 && uart.snapshot().sync_word == 0x1200);
    uart.control_write(0x34);
    assert(uart.snapshot().parser == 0 && uart.snapshot().sync_word == 0x1234);
    uart.control_write(0x40); // reset recovery
    uart.control_write(0x80); // one sync byte
    uart.control_write(0x56);
    assert(uart.snapshot().parser == 0 && uart.snapshot().sync_byte == 0x56);
    assert(uart.snapshot().sync_byte_count == 1);
    uart.control_write(0x40);
    uart.control_write(0x7e);
    uart.control_write(0x27);
    uart.write_rxd(false);
    uart.write_cts(false);
    uart.write_dsr(true);
    uart.write_txc(true);
    uart.write_rxc(true);
    const auto before = uart.snapshot();
    uart.reset();
    const auto after = uart.snapshot();
    assert(after.txc == before.txc && after.rxc == before.rxc && after.rxd == before.rxd);
    assert(after.cts == before.cts && after.dsr == before.dsr);
    assert(after.start_bits == before.start_bits && after.data_bits == before.data_bits);
    assert(after.parity == before.parity && after.rx_length == before.rx_length);
    assert(after.tx_shift == before.tx_shift && after.rx_shift == before.rx_shift);
    assert(after.rx_divider == 0 && after.tx_divider == 0 && after.baud_factor == 1);
    assert(after.status == 5 && after.command == 0 && after.mode == 0 && after.parser == 1);
}

void ready_edges_and_rx_phase() {
    for (unsigned factor : {1u, 16u, 64u}) {
        rt::Model2UART uart;
        unsigned tick = 0, ready_tick = 0;
        std::vector<bool> tx_ready;
        rt::Model2UART::Callbacks callbacks;
        callbacks.tx_ready = [&](bool level) { tx_ready.push_back(level); };
        callbacks.rx_ready = [&](bool level) { if (level) ready_tick = tick; };
        uart.set_callbacks(callbacks);
        uart.write_cts(false);
        uart.control_write(uint8_t(0x4c | (factor == 1 ? 1 : factor == 16 ? 2 : 3)));
        uart.control_write(0x27);
        tx_ready.clear();
        uart.data_write(0x5a);
        assert((tx_ready == std::vector<bool>{false, true}));
        tx_ready.clear();
        uart.data_write(0xa5);
        assert((tx_ready == std::vector<bool>{false, false}));
        rx_hold(uart, true, factor * 2);
        constexpr uint8_t received = 0x69;
        const unsigned stop_sample = 1 + 3 * factor / 2 + 8 * factor;
        for (tick = 1; tick <= 11 * factor; ++tick) {
            const unsigned bit = (tick - 1) / factor;
            const bool level = bit == 0 ? false : bit <= 8 ? bool((received >> (bit - 1)) & 1) : true;
            rx_hold(uart, level, 1);
            assert(uart.rx_ready() == (tick >= stop_sample));
        }
        assert(ready_tick == stop_sample && uart.data_read() == received);
    }
    // Pin-to-pin connection uses the same source clock phase as Model2A:
    // TX on the falling edge, RX on the following rising edge.
    rt::Model2UART sender, receiver;
    rt::Model2UART::Callbacks wire;
    wire.txd = [&](bool level) { receiver.write_rxd(level); };
    sender.set_callbacks(wire);
    sender.write_cts(false);
    sender.control_write(0x4e); sender.control_write(0x27);
    receiver.control_write(0x4e); receiver.control_write(0x04);
    sender.data_write(0xa6);
    for (unsigned edge = 0; edge < 16 * 12; ++edge) {
        sender.write_txc(true); receiver.write_rxc(true);
        sender.write_txc(false); receiver.write_rxc(false);
    }
    assert(receiver.rx_ready() && receiver.data_read() == 0xa6);
    assert(!(receiver.status_read() & 0x38));
}
} // namespace

int main() {
    format_sweep();
    commands_and_buffering();
    ready_edges_and_rx_phase();
    parser_and_reset_retention();
    std::puts("UART: async physical edges, formats/errors, pin gating, queued data, resets and sync frontier passed");
}
