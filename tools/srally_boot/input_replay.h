// Bounded diagnostic input snapshots at completed rendered-frame boundaries.
// Only cabinet input pins change; game memory and execution remain untouched.
#pragma once

#include "runtime/model2a_io.h"
#include <charconv>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace srally {

class InputReplay {
public:
    struct Event {
        uint64_t frame;
        rt::Model2AIO::Inputs inputs;
    };

    explicit InputReplay(std::istream &stream) {
        std::string line;
        bool header = false;
        size_t line_number = 0;
        while (std::getline(stream, line)) {
            ++line_number;
            if (line.size() > 512) fail(line_number, "line is too long");
            line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
            std::istringstream fields(line);
            std::string token;
            if (!(fields >> token)) continue;
            if (!header) {
                if (token != "segarally-input-v1" || fields >> token)
                    fail(line_number, "expected segarally-input-v1 header");
                header = true;
                continue;
            }
            Event event{};
            event.frame = number(token, 10, 1000000, line_number);
            if (!events_.empty() && event.frame <= events_.back().frame)
                fail(line_number, "frames must strictly increase");
            uint8_t *ports[] = {&event.inputs.in0, &event.inputs.in1,
                &event.inputs.in2, &event.inputs.switches};
            for (auto *port : ports) *port = byte(fields, line_number);
            for (auto &axis : event.inputs.analog) axis = byte(fields, line_number);
            if (fields >> token) fail(line_number, "extra field");
            if (events_.size() == 4096) fail(line_number, "too many input events");
            events_.push_back(event);
        }
        if (stream.bad()) throw std::runtime_error("cannot read input replay");
        if (!header || events_.empty()) throw std::runtime_error("input replay requires a header and events");
    }

    // Called before stepping the next frame. Repeated calls at the same frame
    // are harmless; a skipped event is an error rather than a late input.
    bool apply(uint64_t completed_frames, rt::Model2AIO::Inputs &inputs) {
        if (have_frame_ && completed_frames < previous_frame_)
            throw std::runtime_error("input replay frame moved backwards");
        if (next_ < events_.size() && events_[next_].frame < completed_frames)
            throw std::runtime_error("input replay missed its frame boundary");
        have_frame_ = true;
        previous_frame_ = completed_frames;
        if (next_ == events_.size() || events_[next_].frame != completed_frames) return false;
        inputs = events_[next_++].inputs;
        return true;
    }

    size_t size() const { return events_.size(); }
    size_t applied() const { return next_; }

private:
    [[noreturn]] static void fail(size_t line, const char *message) {
        throw std::runtime_error("input replay line " + std::to_string(line) + ": " + message);
    }
    static uint64_t number(const std::string &text, int base, uint64_t maximum, size_t line) {
        uint64_t value = 0;
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > maximum)
            fail(line, "invalid or out-of-range number");
        return value;
    }
    static uint8_t byte(std::istringstream &fields, size_t line) {
        std::string token;
        if (!(fields >> token) || token.size() != 2) fail(line, "expected two-digit hex input byte");
        return uint8_t(number(token, 16, 255, line));
    }
    std::vector<Event> events_;
    size_t next_ = 0;
    uint64_t previous_frame_ = 0;
    bool have_frame_ = false;
};

} // namespace srally
