// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
// Self-authored board-clock/register fixtures; no ROM instructions.
#include "runtime/model2a_control.h"
#include "runtime/model2a_bus.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (0)

namespace {
constexpr uint32_t ACK = 0x00e80000, ENABLE = ACK + 4, TIMER = 0x00f00000;
constexpr uint32_t STOPPED = rt::Model2AControl::STOPPED_COUNT;
struct Board {
    uint64_t now = 0;
    rt::Model2AControl control{[this] { return now; }};
    rt::Model2ABus bus{{}, {}};
    using Edge = std::tuple<unsigned, bool, uint64_t>;
    std::vector<Edge> edges;
    Board() {
        bus.add_model(&control);
        control.on_irq_line = [this](unsigned line, bool state) {
            edges.emplace_back(line, state, control.now_ns());
        };
    }
    void at(uint64_t time) { now = time; control.advance(now); }
    void step(uint64_t delta) { at(now + delta); }
    uint32_t request() { return bus.read_dword(ACK); }
    uint32_t enable() { return bus.read_dword(ENABLE); }
    void set_enable(uint32_t value) { bus.write_dword(ENABLE, value); step(80); }
    void timer(unsigned index, uint32_t value) { bus.write_dword(TIMER + index * 4, value); }
    uint32_t timer(unsigned index) { return bus.read_dword(TIMER + index * 4); }
};

void mapping_and_clock()
{
    bool no_clock = false;
    try { rt::Model2AControl invalid({}); }
    catch (const rt::Fatal &) { no_clock = true; }
    CHECK(no_clock);
    Board b;
    for (unsigned lane = 0; lane < 8; ++lane) CHECK(b.control.claims(ACK + lane));
    for (unsigned lane = 0; lane < 16; ++lane) CHECK(b.control.claims(TIMER + lane));
    for (uint32_t address : {ACK - 1, ACK + 8, TIMER - 1, TIMER + 16})
        CHECK(!b.control.claims(address));
    CHECK(b.request() == 0 && b.enable() == 0);
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(!b.control.irq_line(i));
        CHECK(b.timer(i) == STOPPED);
    }
    CHECK(b.edges.empty());
    uint32_t value = 0;
    bool invalid = false;
    try { b.control.read(ACK + 1, ~0u, value); }
    catch (const rt::Fatal &) { invalid = true; }
    CHECK(invalid);
    invalid = false;
    try { (void)b.control.irq_line(4); }
    catch (const rt::Fatal &) { invalid = true; }
    CHECK(invalid);
    b.at(1000);
    invalid = false;
    try { b.control.advance(999); }
    catch (const rt::Fatal &) { invalid = true; }
    CHECK(invalid && b.control.now_ns() == 1000);

    b.timer(0, 10);
    const auto before = b.control.snapshot();
    b.now += 400; // The external clock passed expiry, but no delivery yet.
    CHECK(b.control.snapshot() == before); // Inspection cannot service clock.
    CHECK(before.timers[0].latched == 10 && before.timers[0].running);
    CHECK(before.events[0].pending);
    b.control.advance(b.now);
    CHECK(b.control.snapshot().timers[0].latched == STOPPED);
}

void countdown_and_latched_writes()
{
    Board b;
    for (unsigned i = 0; i < 4; ++i) b.timer(i, i + 1);
    b.at(39);
    for (unsigned i = 0; i < 4; ++i) CHECK(b.timer(i) == i + 1);
    b.at(40);
    CHECK(b.timer(0) == STOPPED);
    CHECK(b.timer(1) == 1 && b.timer(2) == 2 && b.timer(3) == 3);
    b.at(159);
    CHECK(b.timer(2) == STOPPED && b.timer(3) == 1);
    b.at(160);
    for (unsigned i = 0; i < 4; ++i) CHECK(b.timer(i) == STOPPED);
    CHECK(b.request() == 0 && b.edges.empty()); // Disabled expiries don't latch IRQ.

    // A write combines against the original latch if no intervening read.
    b.timer(0, 0x1000);
    b.step(0x101 * 40);
    b.bus.write_byte(TIMER + 1, 1);
    CHECK(b.timer(0) == 0x0100);
    // After a read, the same write must combine with that read's latch.
    b.timer(0, 0x1000);
    b.step(0x101 * 40);
    CHECK(b.bus.read_byte(TIMER) == 0xff); // Updates the full latch to 0xeff.
    b.step(5 * 40); // Write must not freshly update that latch.
    b.bus.write_byte(TIMER + 1, 1);
    CHECK(b.timer(0) == 0x01ff);
    b.step(40);
    CHECK(b.timer(0) == 0x01fe);
    b.timer(0, 2); // Reload replaces the previous scheduled expiration.
    b.step(79); CHECK(b.timer(0) == 1);
    b.step(1); CHECK(b.timer(0) == STOPPED);
    b.step(0x200 * 40); CHECK(b.request() == 0 && b.timer(0) == STOPPED);

    // The 0xfffff reset value does not restrict the timer to twenty bits.
    const uint64_t start = b.now;
    b.timer(1, 0xffffffffu);
    b.step(40); CHECK(b.timer(1) == 0xfffffffeu);
    b.at(start + uint64_t(0xffffffffu) * 40 - 1);
    CHECK(b.timer(1) == 1);
    b.step(1); CHECK(b.timer(1) == STOPPED);

    // A zero count schedules a due event, delivered by the next advance/access.
    b.set_enable(4);
    b.timer(0, 0);
    CHECK(!b.control.irq_line(2));
    b.control.advance(b.now);
    CHECK(b.control.irq_line(2) && b.request() == 4 && b.timer(0) == STOPPED);
}

void delayed_enable_and_gating()
{
    Board b;
    b.bus.write_byte(ENABLE, 4);
    b.at(40);
    b.bus.write_byte(ENABLE + 1, 4); // Active enable is still zero, not pending4.
    b.at(80); CHECK(b.enable() == 0);
    b.at(119); CHECK(b.enable() == 0);
    b.at(120); CHECK(b.enable() == 0x400);
    CHECK(b.request() == 0 && b.edges.empty()); // No fabricated UART readiness.
    b.control.sound_ready(true, false);
    CHECK(b.request() == 0x400 && b.control.irq_line(3));
    b.control.sound_ready(false, false);
    CHECK(b.request() == 0x400); // Ready falling does not acknowledge the request.
    b.set_enable(0);
    CHECK(b.request() == 0x400 && b.control.irq_line(3)); // Mask isn't line gating.
    b.bus.write_dword(ACK, ~0x400u);
    CHECK(b.request() == 0 && !b.control.irq_line(3));

    Board gated;
    gated.bus.write_dword(ENABLE, 4);
    gated.timer(0, 1);
    gated.at(40); CHECK(gated.request() == 0);
    gated.at(80); CHECK(gated.enable() == 4 && gated.request() == 0);
    gated.timer(0, 1);
    gated.step(40); CHECK(gated.request() == 4 && gated.control.irq_line(2));
    gated.bus.write_dword(ACK, 0);
    gated.set_enable(0);
    gated.timer(0, 1);
    gated.step(40); CHECK(gated.request() == 0);
    gated.set_enable(4); CHECK(gated.request() == 0); // No retroactive timer request.

    // Replacing even a zero-mask write restarts the reference's delay.
    Board replacement;
    replacement.set_enable(0x400);
    replacement.bus.write_dword(ENABLE, 4);
    replacement.step(40);
    replacement.control.write(ENABLE, 0, 0);
    replacement.step(40); CHECK(replacement.enable() == 0x400);
    replacement.step(40); CHECK(replacement.enable() == 0x400);
}

void insertion_order()
{
    // Enable before timer at identical deadline: timer sees the new mask.
    Board enable_first;
    enable_first.bus.write_dword(ENABLE, 4);
    enable_first.timer(0, 2);
    enable_first.at(80); CHECK(enable_first.request() == 4);
    // Same two events inserted in reverse order: timer sees old disabled mask.
    Board timer_first;
    timer_first.timer(0, 2);
    timer_first.bus.write_dword(ENABLE, 4);
    timer_first.at(80); CHECK(timer_first.request() == 0 && timer_first.enable() == 4);
    // Replacing a pending timer gives it a fresh place in the equal-time order.
    Board replaced;
    replaced.timer(0, 2);
    replaced.bus.write_dword(ENABLE, 4);
    replaced.timer(0, 2);
    replaced.at(80); CHECK(replaced.request() == 4);

    // Multiple crossed deadlines dispatch in timestamp order, not event index.
    Board crossed;
    crossed.control.sound_ready(false, true);
    crossed.set_enable(4); // Sound disabled.
    crossed.timer(0, 3); // At200.
    crossed.bus.write_dword(ENABLE, 0x404); // At160 enables pending UART.
    crossed.at(1000);
    CHECK(crossed.edges.size() == 2);
    CHECK(crossed.edges[0] == Board::Edge(3, true, 160));
    CHECK(crossed.edges[1] == Board::Edge(2, true, 200));
}

void line_groups_and_partial_ack()
{
    Board b;
    b.control.vblank();
    b.control.sound_ready(false, true);
    CHECK(b.request() == 0 && b.edges.empty());
    b.set_enable(0x43d); // Vblank, all four timers and sound.
    CHECK(b.request() == 0x400);
    b.control.vblank();
    for (unsigned i = 0; i < 4; ++i) b.timer(i, 1);
    b.step(40);
    CHECK(b.request() == 0x43d);
    CHECK(b.control.irq_line(0) && !b.control.irq_line(1));
    CHECK(b.control.irq_line(2) && b.control.irq_line(3));
    CHECK(b.edges.size() == 3); // Four timer sources share a single line edge.
    b.control.vblank(); b.control.sound_ready(true, false);
    CHECK(b.edges.size() == 3);

    // Maskless MAME ACK consumes the shifted subword payload. Unselected
    // lanes are zero, not implicit ones: a low byte store clears sound too.
    b.bus.write_byte(ACK, 0xff);
    CHECK(b.request() == 0x3d && !b.control.irq_line(3));
    b.control.sound_ready(true, false);
    CHECK(b.request() == 0x43d);
    b.bus.write_byte(ACK + 1, 0xff);
    CHECK(b.request() == 0x400); // Low-byte vblank+timer requests all cleared.
    CHECK(!b.control.irq_line(0) && !b.control.irq_line(2));
    b.bus.write_word(ACK + 2, 0xffff);
    CHECK(b.request() == 0 && !b.control.irq_line(3));
    b.control.vblank();
    b.control.sound_ready(false, true);
    b.bus.write_word(ACK, 0xffff);
    CHECK(b.request() == 0x401);
    b.bus.write_dword(ACK, ~1u);
    CHECK(b.request() == 0x400 && !b.control.irq_line(0));
    b.control.sound_ready(false, false);
    b.bus.write_dword(ACK, 0);
    CHECK(b.request() == 0);
    // Clearing ACK does not immediately re-sample a high UART pin.
    b.control.sound_ready(true, false);
    b.bus.write_dword(ACK, 0);
    CHECK(b.request() == 0 && !b.control.irq_line(3));
    b.bus.write_dword(ENABLE, 0x400);
    b.step(79); CHECK(b.request() == 0);
    b.step(1); CHECK(b.request() == 0x400);
}

void reset_and_range_overflow()
{
    Board b;
    b.control.sound_ready(true, false);
    b.set_enable(0x405);
    b.control.vblank();
    b.timer(0, 10);
    b.bus.write_dword(ENABLE, 0x400); // Pending update at160.
    b.step(40);
    b.control.reset();
    CHECK(b.request() == 0 && b.enable() == 0);
    for (unsigned i = 0; i < 4; ++i) CHECK(b.timer(i) == STOPPED && !b.control.irq_line(i));
    b.at(159); CHECK(b.enable() == 0);
    b.at(160); CHECK(b.enable() == 0x400 && b.request() == 0x400);
    b.at(480); CHECK(b.request() == 0x400 && !b.control.irq_line(2));
    // A fresh controller represents cold start: no inherited pending mask.
    Board fresh;
    fresh.at(1000); CHECK(fresh.request() == 0 && fresh.enable() == 0);

    Board overflow;
    overflow.at(std::numeric_limits<uint64_t>::max() - 39);
    bool rejected = false;
    try { overflow.timer(0, 1); }
    catch (const rt::Fatal &) { rejected = true; }
    CHECK(rejected && overflow.timer(0) == STOPPED);
    rejected = false;
    try { overflow.bus.write_dword(ENABLE, 1); }
    catch (const rt::Fatal &) { rejected = true; }
    CHECK(rejected && overflow.enable() == 0);
    overflow.timer(0, 0); // Zero delay still representable at the end of time.
    overflow.control.advance(overflow.now);
    CHECK(overflow.timer(0) == STOPPED);
}
} // namespace

int main()
{
    mapping_and_clock();
    countdown_and_latched_writes();
    delayed_enable_and_gating();
    insertion_order();
    line_groups_and_partial_ack();
    reset_and_range_overflow();
    std::puts("Model2A control: exact registers, timer latches, delayed mask, IRQ/ACK, ordering and reset passed");
}
