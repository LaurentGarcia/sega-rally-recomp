// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
// Independent deterministic event/phase fixtures; no game ROM or CPU timing.
#include "runtime/model2a_clock.h"
#include "runtime/model2a_control.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (0)

namespace {
using rt::Model2AClock;
using rt::Model2AScreen;
constexpr uint32_t ACK = 0x00e80000, ENABLE = ACK + 4, TIMER = 0x00f00000;
constexpr uint64_t FRAME = Model2AScreen::FRAME_NS, BLANK = Model2AScreen::VBLANK_NS;

void queue_order_and_replacement()
{
    Model2AClock clock;
    std::vector<unsigned> order;
    CHECK(!clock.next_deadline());
    const auto later = clock.schedule_at(20, [&](uint64_t t) { CHECK(t == 20); order.push_back(9); });
    const auto first = clock.schedule_at(10, [&](uint64_t t) { CHECK(t == 10); order.push_back(1); });
    const auto second = clock.schedule_at(10, [&](uint64_t) { order.push_back(2); });
    const auto replacement = clock.reschedule_at(first, 10, [&](uint64_t) { order.push_back(3); });
    CHECK(replacement.insertion > second.insertion);
    clock.cancel(first); // Canceled token cannot cancel its replacement.
    CHECK(clock.next_deadline() == 10);
    clock.advance(9); CHECK(order.empty() && clock.now_ns() == 9);
    clock.advance(10); CHECK(order == std::vector<unsigned>({2, 3}));
    CHECK(clock.next_deadline() == 20);
    clock.cancel(later); clock.cancel(later); clock.cancel({});
    CHECK(!clock.next_deadline());
    clock.advance(100); CHECK(clock.now_ns() == 100);

    const auto retained = clock.schedule_at(110, [&](uint64_t) { order.push_back(4); });
    bool past = false;
    try { clock.reschedule_at(retained, 99, [](uint64_t) {}); }
    catch (const rt::Fatal &) { past = true; }
    CHECK(past && clock.next_deadline() == 110);
    clock.advance(110); CHECK(order.back() == 4);
}

void callbacks_zero_and_reentrancy()
{
    Model2AClock clock;
    std::vector<unsigned> order;
    Model2AClock::Token canceled;
    clock.schedule_at(10, [&](uint64_t deadline) {
        CHECK(deadline == 10 && clock.now_ns() == 10);
        clock.advance(deadline); // Must not recursively deliver the next event.
        CHECK(order.empty());
        order.push_back(1);
        bool rejected = false;
        try { clock.advance(deadline + 1); }
        catch (const rt::Fatal &) { rejected = true; }
        CHECK(rejected && clock.now_ns() == deadline);
        rejected = false;
        try { clock.advance(deadline - 1); }
        catch (const rt::Fatal &) { rejected = true; }
        CHECK(rejected);
        clock.cancel(canceled);
        clock.schedule_after(0, [&](uint64_t same) {
            CHECK(same == 10 && clock.now_ns() == 10);
            order.push_back(3);
        });
    });
    clock.schedule_at(10, [&](uint64_t) { order.push_back(2); });
    canceled = clock.schedule_at(10, [&](uint64_t) { order.push_back(99); });
    clock.advance(100);
    CHECK(order == std::vector<unsigned>({1, 2, 3}));
    CHECK(clock.now_ns() == 100 && !clock.next_deadline());
    bool backward = false;
    try { clock.advance(99); }
    catch (const rt::Fatal &) { backward = true; }
    CHECK(backward);
    bool empty = false;
    try { clock.schedule_after(0, {}); }
    catch (const rt::Fatal &) { empty = true; }
    CHECK(empty);

    Model2AClock failed;
    failed.schedule_at(5, [](uint64_t) { throw std::runtime_error("fixture"); });
    bool next = false;
    failed.schedule_at(7, [&](uint64_t) { next = true; });
    bool thrown = false;
    try { failed.advance(10); }
    catch (const std::runtime_error &) { thrown = true; }
    CHECK(thrown && failed.now_ns() == 5 && failed.next_deadline() == 7);
    failed.advance(10);
    CHECK(next && failed.now_ns() == 10 && !failed.next_deadline());
}

void randomized_queue_contract()
{
    // An independent unsorted-list oracle: after each advance, sort only the
    // due fixtures by (time, creation order). No queue implementation reused.
    struct Fixture { uint64_t deadline, id; bool active; Model2AClock::Token token; };
    using Observation = std::pair<uint64_t, uint64_t>;
    Model2AClock clock;
    std::vector<Fixture> fixtures;
    std::vector<Observation> actual;
    uint64_t random = 0x9380a46beef;
    const auto next = [&]() {
        random ^= random << 13; random ^= random >> 7; random ^= random << 17;
        return uint32_t(random);
    };
    const auto check_advance = [&](uint64_t target) {
        std::vector<Observation> expected;
        for (auto &fixture : fixtures) if (fixture.active && fixture.deadline <= target) {
            expected.emplace_back(fixture.deadline, fixture.id);
            fixture.active = false;
        }
        std::sort(expected.begin(), expected.end());
        actual.clear();
        clock.advance(target);
        CHECK(actual == expected && clock.now_ns() == target);
        std::optional<uint64_t> remaining;
        for (const auto &fixture : fixtures) if (fixture.active)
            remaining = remaining ? std::min(*remaining, fixture.deadline) : fixture.deadline;
        CHECK(clock.next_deadline() == remaining);
    };
    for (unsigned operation = 0; operation < 12000; ++operation) {
        const unsigned kind = next() % 4;
        if (kind < 2 || fixtures.empty()) {
            const uint64_t id = fixtures.size();
            const uint64_t deadline = clock.now_ns() + next() % 100;
            const auto callback = [&, id](uint64_t time) {
                CHECK(clock.now_ns() == time);
                actual.emplace_back(time, id);
            };
            Model2AClock::Token token;
            if (kind == 1 && !fixtures.empty()) {
                auto &old = fixtures[next() % fixtures.size()];
                token = clock.reschedule_at(old.token, deadline, callback);
                old.active = false;
            } else token = clock.schedule_at(deadline, callback);
            fixtures.push_back({deadline, id, true, token});
        } else if (kind == 2) {
            auto &old = fixtures[next() % fixtures.size()];
            clock.cancel(old.token); old.active = false;
        } else check_advance(clock.now_ns() + next() % 100);
    }
    check_advance(clock.now_ns() + 100);
    CHECK(!clock.next_deadline());
}

void deadline_guard_transitions()
{
    Model2AClock clock;
    std::vector<unsigned> order;
    clock.advance(10); // Empty queue, then install an already-due event.
    clock.schedule_at(10, [&](uint64_t) { order.push_back(1); });
    clock.advance(10);
    CHECK(order == std::vector<unsigned>({1}));

    const auto first = clock.schedule_at(20, [&](uint64_t) { order.push_back(99); });
    const auto second = clock.schedule_at(30, [&](uint64_t) { order.push_back(99); });
    clock.cancel(first);
    clock.advance(29);
    CHECK(order.size() == 1 && clock.next_deadline() == 30);
    clock.reschedule_at(second, 40, [&](uint64_t) { order.push_back(3); });
    clock.schedule_at(35, [&](uint64_t time) {
        CHECK(time == 35 && clock.next_deadline() == 40);
        order.push_back(2);
        clock.schedule_after(0, [&](uint64_t same) {
            CHECK(same == 35);
            order.push_back(4);
        });
        throw std::runtime_error("deadline cache after callback mutation");
    });
    bool threw = false;
    try { clock.advance(50); }
    catch (const std::runtime_error &) { threw = true; }
    CHECK(threw && clock.now_ns() == 35 && clock.next_deadline() == 35);
    clock.advance(35);
    clock.advance(39);
    CHECK(order == std::vector<unsigned>({1, 2, 4}));
    clock.advance(40);
    CHECK(order == std::vector<unsigned>({1, 2, 4, 3}) && !clock.next_deadline());

    const auto maximum = std::numeric_limits<uint64_t>::max();
    clock.advance(maximum - 1);
    clock.schedule_at(maximum, [&](uint64_t time) { CHECK(time == maximum); order.push_back(5); });
    clock.advance(maximum - 1);
    CHECK(order.size() == 4);
    clock.advance(maximum);
    CHECK(order.back() == 5 && order.size() == 5 && !clock.next_deadline());
    clock.advance(maximum); // Empty queue shares the maximum deadline sentinel.
    bool backward = false;
    try { clock.advance(maximum - 1); }
    catch (const rt::Fatal &) { backward = true; }
    CHECK(backward && clock.now_ns() == maximum);
}

void overflow_and_lifetime()
{
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    Model2AClock clock(maximum - 1);
    bool rejected = false;
    try { clock.schedule_after(2, [](uint64_t) {}); }
    catch (const rt::Fatal &) { rejected = true; }
    CHECK(rejected && !clock.next_deadline());
    bool fired = false;
    clock.schedule_after(1, [&](uint64_t t) { CHECK(t == maximum); fired = true; });
    clock.advance(maximum); CHECK(fired);

    Model2AClock queue;
    {
        auto control = std::make_unique<rt::Model2AControl>(queue);
        control->write(ENABLE, 0x405, ~0u);
        control->write(TIMER, 100, ~0u);
        CHECK(queue.next_deadline() == 80);
    }
    CHECK(!queue.next_deadline());
    {
        Model2AScreen screen(queue);
        CHECK(queue.next_deadline() == FRAME);
        queue.advance(FRAME);
        CHECK(queue.next_deadline() == FRAME + BLANK);
    }
    CHECK(!queue.next_deadline());
    queue.advance(FRAME * 10); // No callback to a destroyed device.
    rejected = false;
    try { Model2AScreen screen(clock); }
    catch (const rt::Fatal &) { rejected = true; }
    CHECK(rejected && !clock.next_deadline());
}

void screen_phase_and_latches()
{
    Model2AClock clock;
    Model2AScreen screen(clock);
    using Event = std::tuple<char, uint64_t, uint64_t>;
    std::vector<Event> events;
    screen.on_begin = [&](uint64_t time, uint64_t frame) {
        CHECK(time == clock.now_ns());
        CHECK(frame == screen.screen_frame() && frame == screen.latched_frame());
        events.emplace_back('B', time, frame);
    };
    screen.on_end = [&](uint64_t time, uint64_t frame) {
        CHECK(time == clock.now_ns());
        CHECK(frame == screen.screen_frame()); // Increment is AFTER render callback.
        CHECK(frame == screen.latched_frame());
        events.emplace_back('E', time, frame);
    };
    clock.advance(BLANK); // No synthetic startup vblank-end callback.
    CHECK(events.empty() && screen.screen_frame() == 0);
    clock.advance(15'744'000); // Visible-height duration is not first begin time.
    CHECK(events.empty());
    clock.advance(FRAME - 1);
    screen.reset(); // Soft reset cannot restart phase.
    clock.advance(FRAME);
    CHECK(events == std::vector<Event>({{'B', FRAME, 0}}));
    CHECK(screen.screen_frame() == 0 && screen.latched_frame() == 0);
    clock.advance(FRAME + BLANK - 1); CHECK(events.size() == 1);
    clock.advance(FRAME + BLANK);
    CHECK(events.back() == Event('E', FRAME + BLANK, 0));
    CHECK(screen.screen_frame() == 1 && screen.latched_frame() == 0);
    screen.reset();
    clock.advance(2 * FRAME - 1);
    CHECK(screen.screen_frame() == 1 && screen.latched_frame() == 0);
    clock.advance(2 * FRAME);
    CHECK(events.back() == Event('B', 2 * FRAME, 1));
    CHECK(screen.latched_frame() == 1);
    // Advance across fifty whole frames and independently compute every edge.
    clock.advance(50 * FRAME + BLANK);
    CHECK(events.size() == 100);
    for (uint64_t i = 0; i < 50; ++i) {
        CHECK(events[i * 2] == Event('B', (i + 1) * FRAME, i));
        CHECK(events[i * 2 + 1] == Event('E', (i + 1) * FRAME + BLANK, i));
    }
    CHECK(screen.screen_frame() == 50 && screen.latched_frame() == 49);

    Model2AClock shifted(1234);
    Model2AScreen shifted_screen(shifted);
    CHECK(shifted.next_deadline() == 1234 + FRAME);
}

void screen_callback_insertion_order()
{
    Model2AClock clock;
    Model2AScreen screen(clock);
    std::vector<unsigned> order;
    screen.on_begin = [&](uint64_t time, uint64_t frame) {
        if (frame == 0) {
            clock.schedule_at(time + FRAME, [&](uint64_t) { order.push_back(1); });
            clock.schedule_at(time + BLANK, [&](uint64_t) { order.push_back(2); });
        } else order.push_back(4);
    };
    screen.on_end = [&](uint64_t, uint64_t frame) { if (frame == 0) order.push_back(3); };
    clock.advance(2 * FRAME);
    // vblank_begin schedules its own next begin/end only after host callback.
    CHECK(order == std::vector<unsigned>({2, 3, 1, 4}));
}

std::vector<unsigned> tied_irq_order(bool screen_first, bool replace_timer)
{
    Model2AClock clock;
    rt::Model2AControl control(clock);
    std::vector<unsigned> lines;
    control.on_irq_line = [&](unsigned line, bool asserted) {
        CHECK(asserted && clock.now_ns() == FRAME && control.now_ns() == FRAME);
        lines.push_back(line);
    };
    control.write(ENABLE, 5, ~0u);
    std::unique_ptr<Model2AScreen> screen;
    if (screen_first) screen = std::make_unique<Model2AScreen>(clock);
    control.write(TIMER, uint32_t(FRAME / 40), ~0u);
    if (!screen_first) screen = std::make_unique<Model2AScreen>(clock);
    if (replace_timer) control.write(TIMER, uint32_t(FRAME / 40), ~0u);
    screen->on_begin = [&](uint64_t deadline, uint64_t) {
        CHECK(clock.now_ns() == deadline);
        // This same-time synchronization must not recursively flush tied timers.
        control.vblank();
    };
    const auto before = control.snapshot();
    CHECK(before.events[0].pending && before.events[0].deadline_ns == FRAME);
    clock.advance(FRAME);
    CHECK(control.snapshot().request == 5);
    CHECK(!control.snapshot().events[0].pending);
    return lines;
}

void global_control_ties()
{
    CHECK(tied_irq_order(true, false) == std::vector<unsigned>({0, 2}));
    CHECK(tied_irq_order(false, false) == std::vector<unsigned>({2, 0}));
    CHECK(tied_irq_order(false, true) == std::vector<unsigned>({0, 2}));

    // Screen's startup event was inserted before an IRQ-enable event at the
    // same deadline. Vblank is still masked and cannot become retroactive.
    Model2AClock clock;
    rt::Model2AControl control(clock);
    Model2AScreen screen(clock);
    screen.on_begin = [&](uint64_t, uint64_t) { control.vblank(); };
    clock.advance(FRAME - 80);
    control.write(ENABLE, 1, ~0u);
    clock.advance(FRAME);
    CHECK(control.snapshot().enable == 1 && control.snapshot().request == 0);
    clock.advance(2 * FRAME);
    CHECK(control.snapshot().request == 1);

    // The reverse tie is possible for a separately scheduled external event:
    // mask delivery inserted before vblank must enable that very vblank.
    Model2AClock external;
    rt::Model2AControl second(external);
    second.write(ENABLE, 1, ~0u);
    external.schedule_at(80, [&](uint64_t) { second.vblank(); });
    external.advance(80);
    CHECK(second.snapshot().request == 1);

    // UART and mask events share the same order too; ready level must be set
    // at its callback deadline, with no recursive delivery of the next event.
    Model2AClock uart;
    rt::Model2AControl sound(uart);
    uart.schedule_at(80, [&](uint64_t) { sound.sound_ready(true, false); });
    sound.write(ENABLE, 0x400, ~0u);
    uart.advance(80);
    CHECK(sound.snapshot().request == 0x400);
}

void shared_soft_reset()
{
    Model2AClock clock;
    rt::Model2AControl control(clock);
    Model2AScreen screen(clock);
    control.sound_ready(true, false);
    control.write(ENABLE, 0x400, ~0u);
    control.write(TIMER, 10, ~0u);
    clock.advance(40);
    const auto mask = control.snapshot().events[4];
    control.reset(); screen.reset();
    CHECK(!control.snapshot().events[0].pending);
    CHECK(control.snapshot().events[4] == mask); // Same global insertion retained.
    clock.advance(80);
    CHECK(control.snapshot().enable == 0x400 && control.snapshot().request == 0x400);
    CHECK(clock.next_deadline() == FRAME);
}
} // namespace

int main()
{
    queue_order_and_replacement();
    callbacks_zero_and_reentrancy();
    randomized_queue_contract();
    deadline_guard_transitions();
    overflow_and_lifetime();
    screen_phase_and_latches();
    screen_callback_insertion_order();
    global_control_ties();
    shared_soft_reset();
    std::puts("Model2A shared clock: queue order/cancel/replace/reentrancy, exact screen phase and global IRQ ties passed");
}
