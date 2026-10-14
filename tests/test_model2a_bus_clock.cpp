// Synthetic bus/event integration: no ROM-derived instructions or assets.
#include "runtime/model2a_bus.h"
#include "runtime/model2a_clock.h"
#include "runtime/model2a_control.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::abort(); } } while (0)

int main()
{
    rt::Model2ABus bus({}, {});
    rt::Cpu cpu(&bus);
    rt::Model2AClock clock;
    bool syncing = false;
    const auto sync = [&] {
        if (syncing) return;
        syncing = true;
        try { clock.advance(cpu.reference_cycles * 40); }
        catch (...) { syncing = false; throw; }
        syncing = false;
    };
    bus.set_before_access(sync);
    constexpr uint32_t marker = 0x00200000;
    std::vector<uint64_t> delivered;
    for (uint64_t deadline : {40, 80}) clock.schedule_at(deadline, [&](uint64_t at) {
        delivered.push_back(at);
        // Recursive memory access retains the event cursor. The second event
        // cannot run in the middle of the first callback's memory operation.
        bus.write_dword(marker, uint32_t(at));
        CHECK(clock.now_ns() == at && bus.read_dword(marker) == at);
    });
    cpu.reference_cycles = 3;
    CHECK(bus.read_dword(marker) == 80);
    CHECK(clock.now_ns() == 120 && delivered == std::vector<uint64_t>({40, 80}));

    // Every entry point must synchronize, including fetch and burst metadata.
    const std::vector<std::function<void()>> accesses = {
        [&] { (void)bus.fetch(marker); }, [&] { (void)bus.read_byte(marker); },
        [&] { (void)bus.read_word(marker); }, [&] { (void)bus.read_dword(marker); },
        [&] { bus.write_byte(marker, 1); }, [&] { bus.write_word(marker, 2); },
        [&] { bus.write_dword(marker, 3); }, [&] { CHECK(bus.flags(marker) == rt::Cpu::BURST); }
    };
    for (const auto &access : accesses) {
        const uint64_t due = clock.now_ns() + 40;
        bool fired = false;
        clock.schedule_at(due, [&](uint64_t at) { CHECK(at == due); fired = true; });
        ++cpu.reference_cycles;
        access();
        CHECK(fired && clock.now_ns() == due);
    }

    // Errors consume the event but release the host's recursion guard.
    const uint64_t due = clock.now_ns() + 40;
    clock.schedule_at(due, [](uint64_t) { throw rt::Fatal("fixture failure"); });
    ++cpu.reference_cycles;
    bool failed = false;
    try { bus.write_dword(marker, 99); } catch (const rt::Fatal &) { failed = true; }
    CHECK(failed && !syncing && bus.read_dword(marker) == 3);
    bus.write_dword(marker, 100);
    CHECK(bus.read_dword(marker) == 100);

    // Device events may happen during an access, but CPU line handling must
    // wait until the host's boundary (it can write the interrupt table).
    rt::Model2AControl control(clock);
    bus.add_model(&control);
    std::deque<std::pair<unsigned, bool>> edges;
    control.on_irq_line = [&](unsigned line, bool state) { edges.emplace_back(line, state); };
    cpu.m_PRCB = marker + 0x40;
    cpu.m_PC = 0;
    cpu.m_ICR = 8;
    bus.write_dword(cpu.m_PRCB + 20, marker + 0x100);
    bus.write_dword(0x00e80004, 1);
    clock.schedule_after(120, [&](uint64_t) { control.vblank(); });
    cpu.reference_cycles += 3;
    (void)bus.read_dword(marker);
    CHECK(control.irq_line(0) && edges.size() == 1);
    CHECK(cpu.m_irq_line_state[0] == 0 && !cpu.m_immediate_irq);
    while (!edges.empty()) {
        const auto [line, state] = edges.front();
        edges.pop_front();
        cpu.execute_set_input(int(line), state);
    }
    CHECK(cpu.m_irq_line_state[0] == 1 && cpu.m_immediate_irq);
    std::puts("Model2A bus clock ordering, recursion and deferred IRQ checks passed");
}
