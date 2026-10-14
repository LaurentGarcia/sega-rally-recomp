// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels and contributors
// Model 2 IRQ/timer handlers adapted from pinned MAME; provenance in .cpp.
#pragma once

#include "runtime/m2_replay_bus.h"
#include "runtime/model2a_clock.h"
#include <array>
#include <functional>
#include <memory>

namespace rt {

class Model2AControl final : public DeviceModel {
public:
    using Clock = std::function<uint64_t()>; // Monotonic board nanoseconds.
    static constexpr uint64_t TIMER_TICK_NS = 40; // 25 MHz
    static constexpr uint64_t ENABLE_DELAY_NS = 80;
    static constexpr uint32_t STOPPED_COUNT = 0x000fffff;

    explicit Model2AControl(Clock clock);
    // Share insertion order with screen/other devices. The queue must outlive
    // this controller. The host advances it; accesses use its current cursor.
    explicit Model2AControl(Model2AClock &clock);
    ~Model2AControl() override;
    Model2AControl(const Model2AControl &) = delete;
    Model2AControl &operator=(const Model2AControl &) = delete;
    Model2AControl(Model2AControl &&) = delete;
    Model2AControl &operator=(Model2AControl &&) = delete;
    // Soft reset: cancel countdown timers, clear requests/enables/IRQ lines.
    // The pending delayed-enable event survives, matching machine_reset.
    // UART readiness is external and is not reset or fabricated here.
    void reset();
    // Dispatch events at/before now, ordered by deadline then insertion.
    // Accesses and external events also synchronize against Clock. Callbacks
    // must queue CPU line changes; they must not re-enter this controller or
    // synchronously enter an interrupt. No instructions-to-time conversion.
    void advance(uint64_t now_ns);
    void vblank(); // Rising vblank only; the host schedules screen timing.
    void sound_ready(bool tx, bool rx); // Actual external UART pin levels.
    std::function<void(unsigned line, bool asserted)> on_irq_line;
    bool irq_line(unsigned line) const;
    const std::array<bool, 4> &irq_lines() const { return irq_lines_; }
    uint64_t now_ns() const { return event_clock_->now_ns(); } // Callback deadline.

    struct TimerState {
        uint32_t latched = STOPPED_COUNT, original = 0;
        uint64_t started_ns = 0;
        bool running = false;
        bool operator==(const TimerState &) const = default;
    };
    struct EventState {
        uint64_t deadline_ns = 0, insertion = 0;
        uint32_t parameter = 0;
        bool pending = false;
        bool operator==(const EventState &) const = default;
    };
    struct Snapshot {
        uint64_t now_ns, insertion;
        uint32_t request, enable;
        bool tx_ready, rx_ready;
        std::array<bool, 4> irq_lines;
        std::array<TimerState, 4> timers;
        std::array<EventState, 5> events;
        bool operator==(const Snapshot &) const = default;
    };
    // Diagnostic copy with no clock access, event delivery or timer-latch read.
    // This is deliberately not a persistence/save-state restore interface.
    Snapshot snapshot() const;

    bool claims(uint32_t address) const override;
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override;
    void write(uint32_t address, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t address, uint32_t ours, uint32_t mame) override;

private:
    void checked_address(uint32_t address) const;
    void schedule(unsigned event, uint64_t delay_ns, uint32_t parameter = 0);
    void expire(unsigned event, uint32_t parameter);
    void update_lines();
    void sound_request();

    Clock clock_;
    std::unique_ptr<Model2AClock> owned_clock_;
    Model2AClock *event_clock_;
    uint32_t request_ = 0, enable_ = 0;
    bool tx_ready_ = false, rx_ready_ = false;
    std::array<bool, 4> irq_lines_{};
    std::array<TimerState, 4> timers_{};
    std::array<EventState, 5> events_{}; // Four countdowns, one delayed enable.
    std::array<Model2AClock::Token, 5> tokens_{};
};

} // namespace rt
