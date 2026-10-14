// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
//
// Scheduling/phase contracts checked against MAME
// dddd73680656e355bb2b5beecab1167c9f07bf81 (https://github.com/mamedev/mame):
// src/emu/schedule.cpp adjust/timer_list_insert: equal deadlines retain
// insertion order; rescheduling receives a new position among tied timers.
// SHA256 d7a54f21126144df71ba9a4b40fc3451c5bef1096fe96f89653922015b3c21f9
// src/emu/screen.cpp device_start/configure/time_until_pos/vblank_begin/end:
// SHA256 00950a7da23d4468573dc0c1314b441821802ceccdf67a2e4b8320e6c6006ce0
// src/emu/screen.h time_until_vblank_start/frame_number:
// SHA256 a3ee394293462d5f4319349b09d8c70209ab9dce73dc58925508d633c7f4631b
// src/mame/sega/model2.cpp model2_screen/screen_vblank:
// SHA256 359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6
//
// 16MHz pixels, 656x424 total, 384 visible lines => 41us lines, 17.384ms
// frames, 1.640ms vblank. Startup time is already relative to vblank start;
// time_until_pos(384) advances a full frame. Thus first begin is 17.384ms,
// first end is 19.024ms, with no startup end callback at 1.640ms. Model 2's
// frame latch updates at begin; screen's counter increments after end render.
//
// This native queue has no MAME timeslices, CPU clock, partial-screen updates,
// dynamic video configuration or frame-rate override. It preserves event
// ordering only for devices attached to the same queue. Whole-machine timing
// parity still requires CPU/UART/device scheduling against the reference.

#include "runtime/model2a_clock.h"
#include "runtime/cpu.h"
#include <limits>
#include <utility>

namespace rt {

std::optional<uint64_t> Model2AClock::next_deadline() const
{
    if (events_.empty()) return std::nullopt;
    return events_.begin()->first.first;
}

Model2AClock::Snapshot Model2AClock::snapshot() const
{
    Snapshot state{now_ns_, insertion_, {}};
    state.pending.reserve(events_.size());
    for (const auto &[key, callback] : events_) {
        (void)callback;
        state.pending.push_back(Token{key.first, key.second});
    }
    return state;
}

Model2AClock::Token Model2AClock::schedule_at(uint64_t deadline_ns, Callback callback)
{
    if (!callback) throw Fatal("Model2A clock event requires a callback");
    if (deadline_ns < now_ns_) throw Fatal("Model2A clock cannot schedule into the past");
    if (insertion_ == std::numeric_limits<uint64_t>::max())
        throw Fatal("Model2A clock insertion counter overflow");
    const Token token{deadline_ns, insertion_ + 1};
    events_.emplace(Key{token.deadline_ns, token.insertion}, std::move(callback));
    insertion_ = token.insertion;
    return token;
}

Model2AClock::Token Model2AClock::schedule_after(uint64_t delay_ns, Callback callback)
{
    if (delay_ns > std::numeric_limits<uint64_t>::max() - now_ns_)
        throw Fatal("Model2A clock event deadline overflow");
    return schedule_at(now_ns_ + delay_ns, std::move(callback));
}

void Model2AClock::cancel(Token token)
{
    if (token.insertion) events_.erase(Key{token.deadline_ns, token.insertion});
}

Model2AClock::Token Model2AClock::reschedule_at(Token token, uint64_t deadline_ns, Callback callback)
{
    // Validate and allocate first so a rejected replacement retains the event.
    const auto replacement = schedule_at(deadline_ns, std::move(callback));
    cancel(token);
    return replacement;
}

void Model2AClock::advance(uint64_t target_ns)
{
    if (advancing_) {
        if (target_ns == now_ns_) return;
        throw Fatal("Model2A clock callback cannot recursively advance time");
    }
    if (target_ns < now_ns_) throw Fatal("Model2A clock moved backwards");
    advancing_ = true;
    try {
        while (!events_.empty() && events_.begin()->first.first <= target_ns) {
            auto event = events_.extract(events_.begin());
            now_ns_ = event.key().first;
            event.mapped()(now_ns_);
        }
        now_ns_ = target_ns;
        advancing_ = false;
    } catch (...) {
        // The delivered event stays consumed and time stays at its deadline.
        advancing_ = false;
        throw;
    }
}

Model2AScreen::Model2AScreen(Model2AClock &clock) : clock_(clock)
{
    begin_token_ = clock_.schedule_after(FRAME_NS, [this](uint64_t time) { begin(time); });
}

Model2AScreen::~Model2AScreen()
{
    clock_.cancel(begin_token_);
    clock_.cancel(end_token_);
}

void Model2AScreen::begin(uint64_t deadline_ns)
{
    latched_frame_ = screen_frame_;
    if (on_begin) on_begin(deadline_ns, latched_frame_);
    // Preserve MAME's order: callback first, then next begin, then this end.
    begin_token_ = clock_.schedule_after(FRAME_NS, [this](uint64_t time) { begin(time); });
    end_token_ = clock_.schedule_after(VBLANK_NS, [this](uint64_t time) { end(time); });
}

void Model2AScreen::end(uint64_t deadline_ns)
{
    if (on_end) on_end(deadline_ns, screen_frame_);
    if (screen_frame_ == std::numeric_limits<uint64_t>::max())
        throw Fatal("Model2A screen frame counter overflow");
    ++screen_frame_;
}

} // namespace rt
