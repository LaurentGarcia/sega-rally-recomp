// ROM-free event-queue workload. The EE run measures CP0 Count, not wall time
// or physical hardware performance when executed in an emulator.
#include "runtime/model2a_clock.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
constexpr unsigned steps = 8192;
constexpr std::uint64_t seed = 14695981039346656037ull;
std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
    return (hash ^ value) * 1099511628211ull;
}
std::uint64_t ticks() {
#ifdef _EE
    std::uint32_t value;
    asm volatile("sync\n\tmfc0 %0, $9" : "=r"(value) : : "memory");
    return value;
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

// Keep the loop behind a call boundary. Every case executes the same public
// API used by the board host, including repeated reads at one guest timestamp.
__attribute__((noinline)) void advance_steps(rt::Model2AClock &clock,
                                           unsigned step_ns, unsigned repeats) {
    for (unsigned i = 1; i <= steps; ++i)
        for (unsigned j = 0; j < repeats; ++j)
            clock.advance(std::uint64_t(i) * step_ns);
}

bool run_case(const char *name, unsigned step_ns, unsigned repeats,
              bool future, unsigned period_ns) {
    std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t result_hash = 0;
    unsigned result_events = 0;
    for (unsigned trial = 0; trial < 3; ++trial) {
        rt::Model2AClock clock;
        struct State {
            rt::Model2AClock &clock;
            unsigned period, events = 0;
            std::uint64_t hash = seed;
            rt::Model2AClock::Callback recurring;
            void tick(std::uint64_t time) {
                ++events;
                hash = mix(hash, time);
                clock.schedule_after(period, recurring);
            }
        } state{clock, period_ns, 0, seed, {}};
        if (future) clock.schedule_at(std::numeric_limits<std::uint64_t>::max(),
                                      [&state](std::uint64_t) { ++state.events; });
        if (period_ns) {
            // The machine's UART callback captures one pointer; this must
            // fit the same std::function small buffer instead of measuring
            // allocation of an unrelated larger capture on every event.
            state.recurring = [&state](std::uint64_t time) { state.tick(time); };
            clock.schedule_after(period_ns, state.recurring);
        }
        const auto start = ticks();
        advance_steps(clock, step_ns, repeats);
        auto elapsed = ticks() - start;
#ifdef _EE
        elapsed &= 0xffffffffu;
#endif
        best = std::min(best, elapsed);
        const std::uint64_t target = std::uint64_t(steps) * step_ns;
        const unsigned expected_events = period_ns ? unsigned(target / period_ns) : 0;
        std::uint64_t expected_hash = seed;
        for (unsigned i = 1; i <= expected_events; ++i)
            expected_hash = mix(expected_hash, std::uint64_t(i) * period_ns);
        if (clock.now_ns() != target || state.events != expected_events || state.hash != expected_hash)
            return false;
        if (period_ns && clock.next_deadline() != (std::uint64_t(expected_events) + 1) * period_ns)
            return false;
        result_hash = state.hash;
        result_events = state.events;
    }
    std::printf("CLOCK BENCH case=%s calls=%u events=%u best_ticks=%llu hash=%08lx%08lx\n",
                name, steps * repeats, result_events, (unsigned long long)best,
                (unsigned long)(result_hash >> 32), (unsigned long)(result_hash & 0xffffffffu));
    std::fflush(stdout);
    return true;
}
} // namespace

int main() {
    std::printf("CLOCK BENCH BEGIN units=%s\n",
#ifdef _EE
                "EE-CP0-Count"
#else
                "host-nanoseconds"
#endif
    );
    std::fflush(stdout);
    const bool passed = run_case("empty", 40, 3, false, 0) &&
        run_case("future", 40, 3, true, 0) &&
        run_case("uart", 40, 3, false, 1000) &&
        run_case("dense", 1000, 1, false, 1000);
    std::printf("CLOCK BENCH %s\n", passed ? "PASS" : "FAIL");
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return passed ? 0 : 1;
}
