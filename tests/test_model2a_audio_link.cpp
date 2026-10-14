// Real pin-to-pin Model 2A sound link; no sound CPU or game response is mocked.
#include "runtime/model2_scsp.h"
#include "runtime/model2a_uart.h"
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    rt::Model2AClock clock;
    std::vector<uint8_t> ram(0x80000);
    snd::Model2Scsp::ReferenceRandom random;
    snd::Model2Scsp scsp(clock, ram, [&] { return random(); }, 0);
    rt::Model2UART uart;
    rt::Model2AUART board(uart);
    rt::Model2UART::Callbacks callbacks;
    callbacks.txd = [&](bool level) { scsp.midi_in(level); };
    unsigned received_edges = 0, chip_irqs = 0;
    callbacks.rx_ready = [&](bool level) { if (level) ++received_edges; };
    uart.set_callbacks(callbacks);
    scsp.on_midi_tx = [&](bool level) { uart.write_rxd(level); };
    scsp.on_irq = [&](unsigned level, snd::Model2Scsp::LineState state) {
        if (level == 1 && state == snd::Model2Scsp::LineState::Assert) ++chip_irqs;
    };
    board.reset();
    scsp.midi_in(uart.txd_level());
    for (uint8_t value : {0, 0, 0, 0x40, 0x4e, 0x37}) uart.control_write(value);
    scsp.write16(0x424, 8, 0xffff); // MIDI interrupt level 1
    scsp.write16(0x41e, 8, 0xffff); // enable MIDI receive interrupt
    bool clock_level = false;
    std::function<void(uint64_t)> edge;
    edge = [&](uint64_t) {
        clock_level = !clock_level;
        board.clock_edge(clock_level);
        clock.schedule_after(1000, edge);
    };
    clock.schedule_after(1000, edge);
    for (unsigned byte = 0; byte < 256; ++byte) {
        const auto start = clock.now_ns();
        require(uart.tx_ready(), "main UART not ready for next byte");
        require(!uart.rx_ready(), "unread prior UART byte");
        require(!(scsp.read16(0x420, 0xffff) & 8), "unread prior SCSP byte");
        uart.data_write(uint8_t(byte));
        scsp.write16(0x406, uint16_t(byte ^ 0xa5), 0xffff);
        clock.advance(start + 250000);
        require(!uart.rx_ready(), "UART received before the physical byte completed");
        require(!(scsp.read16(0x420, 0xffff) & 8), "SCSP received before the physical byte completed");
        clock.advance(start + 400000);
        require(uart.rx_ready(), "SCSP transmit did not reach UART RX");
        require(uart.data_read() == (byte ^ 0xa5), "SCSP-to-UART byte mismatch");
        require(!(uart.status_read() & (rt::Model2UART::ParityError |
            rt::Model2UART::OverrunError | rt::Model2UART::FramingError)), "UART line error");
        require(scsp.read16(0x420, 0xffff) & 8, "UART transmit did not reach SCSP RX");
        require((scsp.read16(0x404, 0xffff) & 0xff) == byte, "UART-to-SCSP byte mismatch");
    }
    require(received_edges == 256 && chip_irqs >= 256, "receive interrupt edges missing");
    std::puts("Model2A physical sound link: 256 simultaneous bytes each direction passed");
}
