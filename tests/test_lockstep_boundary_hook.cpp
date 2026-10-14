// Synthetic scheduling contract, independent of generated game code.
#include "runtime/lockstep.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>

#ifndef M2_INSTRUCTION_BOUNDARY_HOOK
#error This fixture requires the optional instruction-boundary contract.
#endif

namespace {
using Action = rt::Lockstep::BoundaryAction;
struct Hook {
    rt::Cpu &cpu;
    std::vector<uint64_t> calls;
    bool redirect = false, fail = false;
    uint64_t yield_at = UINT64_MAX;
    static Action invoke(void *context, uint64_t count) {
        auto &self = *static_cast<Hook *>(context);
        self.calls.push_back(count);
        if (self.fail) throw rt::Fatal("synthetic event frontier");
        if (count >= self.yield_at) return Action::Yield;
        if (self.redirect) {
            self.redirect = false;
            self.cpu.m_IP = 0x80;
            return Action::Redispatch;
        }
        return Action::Continue;
    }
};
template<class F> void rejected(F &&fn) {
    bool caught = false;
    try { fn(); } catch (const std::exception &) { caught = true; }
    assert(caught);
}
}

int main(int argc, char **argv) {
    assert(argc == 2);
    static_assert(rt::Lockstep::boundary_hook_abi() != 0);
    {
        rt::Cpu cpu(nullptr);
        rt::Lockstep plain(cpu);
        std::vector<uint64_t> calls;
        plain.end_count = 3;
        plain.add_callback(1, [&] { calls.push_back(plain.count); });
        assert(!plain.boundary());
        plain.count = 1;
        assert(!plain.boundary() && calls == std::vector<uint64_t>{1});
        plain.count = 2;
        assert(!plain.boundary());
        plain.count = 3;
        assert(plain.boundary() && plain.finished());
        assert(!plain.boundary_yielded());
    }
    {
        rt::Cpu cpu(nullptr);
        rt::Lockstep ls(cpu);
        Hook hook{cpu, {}};
        ls.end_count = 8;
        ls.set_boundary_hook(Hook::invoke, &hook);
        // The hook runs at every instruction, even without a count event.
        assert(!ls.boundary());
        ls.count = 1;
        hook.redirect = true;
        assert(ls.boundary() && cpu.m_IP == 0x80 && !ls.finished());
        assert(!ls.boundary());
        assert((hook.calls == std::vector<uint64_t>{0, 1, 1}));
        hook.yield_at = 2;
        ls.count = 2;
        assert(ls.boundary() && ls.finished() && ls.boundary_yielded());
        const auto calls = hook.calls.size();
        assert(ls.boundary() && hook.calls.size() == calls);
        assert(ls.count == 2 && ls.end_count == 8 && ls.next_count == UINT64_MAX);
        ls.reset_boundary_yield();
        hook.yield_at = UINT64_MAX;
        assert(!ls.finished() && !ls.boundary());
        rejected([&] { ls.add_callback(3, [] {}); });
        hook.fail = true;
        rejected([&] { ls.boundary(); });
        assert(ls.count == 2 && !ls.boundary_yielded());
        ls.set_boundary_hook(nullptr);
        ls.add_callback(3, [] {});
        ls.count = 3;
        assert(!ls.boundary());
    }
    {
        rt::Cpu cpu(nullptr);
        rt::Lockstep ls(cpu);
        Hook hook{cpu, {}};
        ls.add_callback(1, [] {});
        rejected([&] { ls.set_boundary_hook(Hook::invoke, &hook); });
        ls.count = 1;
        assert(!ls.boundary());
        ls.set_boundary_hook(Hook::invoke, &hook); // no pending count events
        assert(!ls.boundary());
    }
    {
        std::ofstream log(argv[1]);
        log << "end 4\n";
        log.close();
        rt::Cpu cpu(nullptr);
        rt::Lockstep replay(cpu, argv[1]);
        Hook hook{cpu, {}};
        rejected([&] { replay.set_boundary_hook(Hook::invoke, &hook); });
        assert(!replay.boundary());
        replay.count = 4;
        assert(replay.boundary() && replay.finished());
    }
    std::puts("Instruction boundary hook, null scheduling, yield and mode rejection passed");
}
