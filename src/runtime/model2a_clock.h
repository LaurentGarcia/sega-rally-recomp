// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace rt {

// A board-time event queue, with one insertion order shared by all devices.
// The host owns advancing it; this class assumes no CPU instruction duration.
class Model2AClock {
public:
    using Callback = std::function<void(uint64_t deadline_ns)>;
    struct Token {
        uint64_t deadline_ns = 0, insertion = 0;
        bool operator==(const Token &) const = default;
    };
    struct Snapshot {
        uint64_t now_ns = 0, insertion = 0;
        std::vector<Token> pending; // deadline/insertion order; excludes callbacks
        bool operator==(const Snapshot &) const = default;
    };
    explicit Model2AClock(uint64_t start_ns = 0) : now_ns_(start_ns) {}
    Model2AClock(const Model2AClock &) = delete;
    Model2AClock &operator=(const Model2AClock &) = delete;
    Model2AClock(Model2AClock &&) = delete;
    Model2AClock &operator=(Model2AClock &&) = delete;

    uint64_t now_ns() const { return now_ns_; }
    uint64_t insertion() const { return insertion_; }
    std::optional<uint64_t> next_deadline() const;
    // Observation only: does not advance time or serialize callback identity.
    Snapshot snapshot() const;
    Token schedule_at(uint64_t deadline_ns, Callback callback);
    Token schedule_after(uint64_t delay_ns, Callback callback);
    // Tokens belong to their originating clock. Canceling an already delivered
    // or canceled token is harmless. Replacement receives a fresh global order.
    void cancel(Token token);
    Token reschedule_at(Token token, uint64_t deadline_ns, Callback callback);
    void advance(uint64_t target_ns);

private:
    using Key = std::pair<uint64_t, uint64_t>;
    uint64_t now_ns_ = 0, insertion_ = 0;
    bool advancing_ = false;
    std::map<Key, Callback> events_;
};

// Pinned screen_device startup/phase for model2_screen's fixed raw timing.
// This implements only frame timing and counters, not the video renderer.
// The shared clock must outlive this object and all its callbacks.
class Model2AScreen {
public:
    static constexpr uint64_t FRAME_NS = 17'384'000;
    static constexpr uint64_t VBLANK_NS = 1'640'000;
    explicit Model2AScreen(Model2AClock &clock);
    ~Model2AScreen();
    Model2AScreen(const Model2AScreen &) = delete;
    Model2AScreen &operator=(const Model2AScreen &) = delete;
    Model2AScreen(Model2AScreen &&) = delete;
    Model2AScreen &operator=(Model2AScreen &&) = delete;

    // At begin the host performs geometry -> vblank IRQ -> comm. At end the
    // host renders; screen_frame increments only after on_end returns.
    std::function<void(uint64_t deadline_ns, uint64_t latched_frame)> on_begin;
    std::function<void(uint64_t deadline_ns, uint64_t screen_frame)> on_end;
    uint64_t screen_frame() const { return screen_frame_; }
    uint64_t latched_frame() const { return latched_frame_; }
    // MAME screen soft reset changes brightness only, preserving timing/frame.
    void reset() {}

private:
    void begin(uint64_t deadline_ns);
    void end(uint64_t deadline_ns);
    Model2AClock &clock_;
    Model2AClock::Token begin_token_, end_token_;
    uint64_t screen_frame_ = 0, latched_frame_ = 0;
};

} // namespace rt
