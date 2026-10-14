// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels and contributors
//
// Adapted from MAME revision dddd73680656e355bb2b5beecab1167c9f07bf81:
// https://github.com/mamedev/mame
// src/mame/sega/model2.cpp: timers_r/w, model2_timer_cb, irq_request_r,
// irq_enable_r/w, irq_ack_w, irq_mask_delayed_update, irq_update,
// machine_reset, screen_vblank and sound_ready_w.
// SHA256 359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6
// src/emu/schedule.cpp: adjust and timer_list_insert (equal deadlines FIFO).
// SHA256 d7a54f21126144df71ba9a4b40fc3451c5bef1096fe96f89653922015b3c21f9
// src/emu/emumem.h: memory_write_generic (zero-extend/shift subword data).
// SHA256 4285ca50a0c14b6038c7443bab45340834f0e4b741f0549a127c4ba7cc1c3e2e
// src/emu/emumem_hedp.cpp: write32smo_delegate passes data, discarding mask.
// SHA256 27b25423ab39477d1b719b8922dc7dab16f712cac43a24d4974890ad7af1aeee
//
// Reference quirks retained: partial ACK clears request bits outside its data
// lanes; timer writes combine with the last read latch, not a fresh countdown;
// mask writes replace their pending event and combine with the active mask;
// mask delivery calls irq_update only for ready+enabled sound; mask disable
// never clears latched requests; timer expiry requests only if then enabled.
// Soft reset does not cancel the delayed-enable timer in machine_reset.
//
// Host scheduling replaces MAME's attotime/timeslice machinery. These devices'
// 40/80ns periods are integral nanoseconds. advance dispatches due callbacks
// before the next host access and preserves insertion order on equal times;
// the host must schedule real CPU/UART/vblank events at their board times.
// This does not establish whole-machine timeslice or instruction-cycle parity.
// IRQ callbacks report changed line levels, never run a CPU interrupt handler.

#include "runtime/model2a_control.h"
#include <limits>
#include <utility>

namespace rt {
namespace {
constexpr uint32_t kRequest = 0x00e80000, kEnable = 0x00e80004;
constexpr uint32_t kTimers = 0x00f00000;
constexpr unsigned kEnableEvent = 4;
constexpr uint32_t kSound = 1u << 10;
}

Model2AControl::Model2AControl(Clock clock) : clock_(std::move(clock)),
    owned_clock_(std::make_unique<Model2AClock>()), event_clock_(owned_clock_.get())
{
    if (!clock_) throw Fatal("Model2A control requires an explicit nanosecond clock");
    reset();
}

Model2AControl::Model2AControl(Model2AClock &clock) :
    clock_([&clock] { return clock.now_ns(); }), event_clock_(&clock)
{
    reset();
}

Model2AControl::~Model2AControl()
{
    for (const auto token : tokens_) event_clock_->cancel(token);
}

void Model2AControl::reset()
{
    advance(clock_());
    request_ = enable_ = 0;
    for (unsigned i = 0; i < timers_.size(); ++i) {
        timers_[i].latched = STOPPED_COUNT;
        timers_[i].running = false;
        event_clock_->cancel(tokens_[i]);
        events_[i].pending = false;
    }
    update_lines();
}

bool Model2AControl::irq_line(unsigned line) const
{
    if (line >= irq_lines_.size()) throw Fatal("Model2A IRQ line is outside 0..3");
    return irq_lines_[line];
}

Model2AControl::Snapshot Model2AControl::snapshot() const
{
    return {now_ns(), event_clock_->insertion(), request_, enable_, tx_ready_, rx_ready_,
            irq_lines_, timers_, events_};
}

bool Model2AControl::claims(uint32_t address) const
{
    return (address >= kRequest && address <= kEnable + 3) ||
           (address >= kTimers && address <= kTimers + 15);
}

void Model2AControl::checked_address(uint32_t address) const
{
    if ((address & 3) || !claims(address))
        throw Fatal("Model2A control requires an aligned address in its exact mapped ranges");
}

void Model2AControl::schedule(unsigned event, uint64_t delay_ns, uint32_t parameter)
{
    if (delay_ns > std::numeric_limits<uint64_t>::max() - now_ns())
        throw Fatal("Model2A control event exceeds the nanosecond scheduler range");
    const auto token = event_clock_->reschedule_at(tokens_[event], now_ns() + delay_ns,
        [this, event, parameter](uint64_t) {
            events_[event].pending = false;
            expire(event, parameter);
        });
    tokens_[event] = token;
    events_[event] = {token.deadline_ns, token.insertion, parameter, true};
}

void Model2AControl::advance(uint64_t target_ns)
{
    event_clock_->advance(target_ns);
}

void Model2AControl::expire(unsigned event, uint32_t parameter)
{
    if (event == kEnableEvent) {
        enable_ = parameter;
        sound_request(); // Reference does not otherwise call irq_update here.
        return;
    }
    auto &timer = timers_[event];
    if (!timer.running) return;
    const uint32_t bit = 1u << (event + 2);
    if (enable_ & bit) {
        request_ |= bit;
        update_lines();
    }
    timer.latched = STOPPED_COUNT;
    timer.running = false;
}

void Model2AControl::update_lines()
{
    constexpr uint32_t groups[4] = {0x001, 0x002, 0x3fc, 0xc00};
    for (unsigned line = 0; line < irq_lines_.size(); ++line) {
        const bool level = (request_ & groups[line]) != 0;
        if (irq_lines_[line] != level) {
            irq_lines_[line] = level;
            if (on_irq_line) on_irq_line(line, level);
        }
    }
}

void Model2AControl::sound_request()
{
    if ((tx_ready_ || rx_ready_) && (enable_ & kSound)) {
        request_ |= kSound;
        update_lines();
    }
}

void Model2AControl::vblank()
{
    advance(clock_());
    if (enable_ & 1) {
        request_ |= 1;
        update_lines();
    }
}

void Model2AControl::sound_ready(bool tx, bool rx)
{
    advance(clock_());
    tx_ready_ = tx;
    rx_ready_ = rx;
    sound_request();
}

DeviceModel::Check Model2AControl::read(uint32_t address, uint32_t mask, uint32_t &value)
{
    checked_address(address);
    advance(clock_());
    if (address == kRequest) value = request_;
    else if (address == kEnable) value = enable_;
    else {
        auto &timer = timers_[(address - kTimers) / 4];
        if (timer.running) {
            const uint64_t ticks = (now_ns() - timer.started_ns) / TIMER_TICK_NS;
            timer.latched = timer.original - uint32_t(ticks);
        }
        value = timer.latched;
    }
    value &= mask;
    return Strict;
}

void Model2AControl::write(uint32_t address, uint32_t data, uint32_t mask)
{
    checked_address(address);
    advance(clock_());
    if (address == kRequest) {
        request_ &= data; // irq_ack_w(u32 data) deliberately has no mem_mask.
        update_lines();
    } else if (address == kEnable) {
        schedule(kEnableEvent, ENABLE_DELAY_NS, (enable_ & ~mask) | (data & mask));
    } else {
        const unsigned index = (address - kTimers) / 4;
        auto &timer = timers_[index];
        const uint32_t value = (timer.latched & ~mask) | (data & mask);
        schedule(index, uint64_t(value) * TIMER_TICK_NS);
        timer.latched = timer.original = value;
        timer.started_ns = now_ns();
        timer.running = true;
    }
}

void Model2AControl::measured_mismatch(uint32_t, uint32_t, uint32_t)
{
    throw Divergence("Strict Model2A control unexpectedly received a measured mismatch");
}

} // namespace rt
