// Synthetic bus/pin wiring tests, with no emulated sound peer.
#include "runtime/model2a_uart.h"
#include "runtime/model2a_bus.h"
#include <cassert>
#include <cstdio>

int main() {
    rt::Model2UART chip;
    rt::Model2AUART uart(chip);
    uart.reset();
    rt::Model2ABus bus({}, {});
    bus.add_model(&uart);
    assert(uart.claims(0x01c80000) && uart.claims(0x01c80003));
    assert(!uart.claims(0x01c7ffff) && !uart.claims(0x01c80004));
    assert(bus.read_word(0x01c80002) == 0x85);
    assert(bus.read_dword(0x01c80000) == 0x00850000);
    assert(bus.read_byte(0x01c80001) == 0 && bus.read_byte(0x01c80003) == 0);
    assert(bus.flags(0x01c80000) == 0);
    bus.write_byte(0x01c80003, 0xff);
    assert(chip.expects_mode());
    bus.write_word(0x01c80002, 0xff4d); // control lower byte, upper hole ignored
    bus.write_byte(0x01c80002, 0x27);
    assert(chip.mode() == 0x4d && chip.command() == 0x27 && chip.tx_ready());
    unsigned transitions = 0;
    rt::Model2UART::Callbacks callbacks;
    callbacks.txd = [&](bool) { ++transitions; };
    chip.set_callbacks(callbacks);
    bus.write_word(0x01c80000, 0xff5a);
    assert(!chip.tx_empty());
    // TXC then RXC ordering is explicit; reads never advance either clock.
    for (unsigned i = 0; i < 10; ++i) {
        (void)bus.read_dword(0x01c80000);
        assert(transitions == i);
        uart.clock_edge(true);
        assert(transitions == i);
        uart.clock_edge(false);
        assert(transitions == i + 1);
    }
    uart.clock_edge(true); uart.clock_edge(false);
    assert(chip.tx_empty());
    bool fenced = false;
    try { (void)bus.read_byte(0x01c80004); }
    catch (const rt::Model2AAccess &) { fenced = true; }
    assert(fenced);
    std::puts("Model2A UART: data/status byte lanes, holes, reset CTS, exact boundary and external edges passed");
}
