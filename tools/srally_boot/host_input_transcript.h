// Exact host resume calls, not rendered-frame input events.
// This stream is diagnostic evidence. Completion does not establish race,
// CPU/device parity, or provenance validity. See the caller contract below.
#pragma once

#include "runtime/model2a_machine.h"
#include <charconv>
#include <functional>
#include <iomanip>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace srally::host_input {
using Machine = rt::Model2AMachine;

struct Context {
    // SHA-256 of a canonical manifest binding the ACTUAL startup EEPROM and
    // backup bytes, verified image/audit/generation hashes, executable identity,
    // all Machine options, and SCSP RNG/timing version when sound is enabled.
    // The parser checks syntax/equality, not the truth of that manifest.
    std::string provenance_sha256;
    bool sound = false, instruction_boundary_hook = false;
    bool operator==(const Context &) const = default;
};

// Required integration dependency: validate the manifest against actual asset
// and startup seed bytes/configuration, then approve this binding or throw.
// Merely supplying a callback does NOT verify provenance. Never approve a
// digest copied from an unvalidated transcript. Tests use synthetic contexts.
using ApproveContext = std::function<void(const Context &)>;

struct Witness {
    uint64_t instructions = 0, cycles = 0, board_time_ns = 0, frames = 0;
    uint32_t pc = 0;
    bool operator==(const Witness &) const = default;
    static Witness of(const Machine::Stats &s) {
        return {s.instructions, s.reference_cycles, s.board_time_ns, s.frames_rendered, s.pc};
    }
};
struct Resume {
    uint64_t index = 0, budget = 0;
    Witness before, after;
    rt::Model2AIO::Inputs inputs;
    Machine::Reason reason = Machine::Reason::Budget;
};
struct Limits {
    uint64_t records = 250000, bytes = 64 * 1024 * 1024;
};

namespace detail {
[[noreturn]] inline void fail(const char *message) {
    throw std::runtime_error(std::string("host input transcript: ") + message);
}
inline void approve(const Context &c, const ApproveContext &validator) {
    if (c.provenance_sha256.size() != 64 ||
        c.provenance_sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
        fail("invalid provenance SHA-256 binding");
    if (!validator) fail("manifest/assets/startup-seeds/configuration approval is required");
    validator(c);
}
inline void limits(const Limits &l) {
    if (!l.records || !l.bytes || l.records > 250000 || l.bytes > 64 * 1024 * 1024)
        fail("invalid stream limits");
}
inline uint64_t number(std::istream &in, int base = 10, size_t width = 0) {
    std::string token;
    if (!(in >> token) || (width && token.size() != width)) fail("missing or invalid field");
    uint64_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) fail("invalid number");
    return value;
}
inline void exhausted(std::istream &in) {
    std::string extra;
    if (in >> extra) fail("extra field");
}
inline void witness(std::ostream &out, const Witness &w) {
    out << w.instructions << ' ' << w.cycles << ' ' << w.board_time_ns << ' ' << w.frames
        << ' ' << std::hex << std::setw(8) << std::setfill('0') << w.pc << std::dec;
}
inline Witness witness(std::istream &in) {
    Witness w;
    w.instructions = number(in); w.cycles = number(in); w.board_time_ns = number(in); w.frames = number(in);
    const auto pc = number(in, 16, 8);
    if (pc > UINT32_MAX) fail("PC exceeds 32 bits");
    w.pc = uint32_t(pc);
    return w;
}
inline const char *reason(Machine::Reason r) {
    switch (r) {
    case Machine::Reason::Frame: return "frame";
    case Machine::Reason::Budget: return "budget";
    case Machine::Reason::MissingCode: return "missing";
    case Machine::Reason::DeviceAccess: return "device";
    case Machine::Reason::RuntimeError: return "runtime";
    }
    fail("unknown return reason");
}
inline Machine::Reason reason(std::istream &in) {
    std::string token;
    if (!(in >> token)) fail("missing return reason");
    for (auto r : {Machine::Reason::Frame, Machine::Reason::Budget, Machine::Reason::MissingCode,
                   Machine::Reason::DeviceAccess, Machine::Reason::RuntimeError})
        if (token == reason(r)) return r;
    fail("unknown return reason");
}
inline bool frontier(Machine::Reason r) {
    return r != Machine::Reason::Frame && r != Machine::Reason::Budget;
}
inline void resume(const Resume &r, uint64_t index, const Witness &previous, bool stopped) {
    if (stopped) fail("resume follows a persistent frontier");
    if (r.index != index || r.before != previous) fail("resume index or pre-state is discontinuous");
    if (!r.budget || r.budget > 100000 || r.budget > UINT64_MAX - r.before.instructions)
        fail("budget must be 1..100000 without count overflow");
    if (r.after.instructions < r.before.instructions || r.after.instructions > r.before.instructions + r.budget ||
        r.after.cycles < r.before.cycles || r.after.board_time_ns < r.before.board_time_ns ||
        r.after.frames < r.before.frames) fail("post-state counters moved backwards or exceed budget");
    // Frame wins a budget tie. Frame synchronization/IRQ entry may advance
    // cycles and frames without completing another opcode; equal counts are
    // intentionally valid and must not be collapsed into a count-only event.
    if (r.reason == Machine::Reason::Frame && r.after.frames == r.before.frames)
        fail("frame return did not complete a frame");
    if (r.reason == Machine::Reason::Budget &&
        (r.after.instructions != r.before.instructions + r.budget || r.after.frames != r.before.frames))
        fail("budget return disagrees with instruction/frame boundary");
    (void)reason(r.reason);
}
inline std::string context_line(const Context &c) {
    return "context " + c.provenance_sha256 + " " + std::to_string(c.sound) + " " +
        std::to_string(c.instruction_boundary_hook);
}
inline std::string resume_line(const Resume &r) {
    std::ostringstream out;
    out << "resume " << r.index << ' ' << r.budget << ' '; witness(out, r.before);
    out << std::hex << std::setfill('0');
    for (auto b : {r.inputs.in0, r.inputs.in1, r.inputs.in2, r.inputs.switches})
        out << ' ' << std::setw(2) << unsigned(b);
    for (auto b : r.inputs.analog) out << ' ' << std::setw(2) << unsigned(b);
    out << std::dec << ' '; witness(out, r.after); out << ' ' << reason(r.reason);
    return out.str();
}
} // namespace detail

class Writer {
public:
    Writer(std::ostream &stream, Context context, const Witness &initial,
           const ApproveContext &approve_context, Limits limits = {})
        : stream_(stream), previous_(initial), limits_(limits) {
        detail::limits(limits); detail::approve(context, approve_context);
        if (initial.instructions) detail::fail("capture must start before the first opcode");
        write("segarally-host-resume-v1"); write(detail::context_line(context));
        std::ostringstream line; line << "initial "; detail::witness(line, initial); write(line.str());
    }
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    // Call only AFTER the actual host resume, with the pre-call pin snapshot.
    // This method performs no emulation, synchronization, I/O reads, or drain.
    void append(const Resume &r) {
        try {
            if (failed_ || finished_ || count_ == limits_.records) detail::fail("record limit reached or transcript failed/finished");
            detail::resume(r, count_, previous_, stopped_);
            write(detail::resume_line(r)); previous_ = r.after; stopped_ = detail::frontier(r.reason); ++count_;
        } catch (...) { failed_ = true; throw; }
    }
    void finish(const Witness &terminal) {
        try {
            if (failed_ || finished_ || terminal != previous_) detail::fail("invalid/duplicate terminal state or failed transcript");
            std::ostringstream line; line << "end " << count_ << ' '; detail::witness(line, terminal); write(line.str());
            stream_.flush(); if (!stream_) detail::fail("cannot finish transcript"); finished_ = true;
        } catch (...) { failed_ = true; throw; }
    }
    uint64_t records() const { return count_; }
    bool complete_transcript() const { return finished_ && !failed_; }
private:
    void write(const std::string &line) {
        if (line.size() > 1024 || line.size() + 1 > limits_.bytes - bytes_) detail::fail("byte/line limit reached");
        stream_ << line << '\n'; if (!stream_) detail::fail("cannot write transcript"); bytes_ += line.size() + 1;
    }
    std::ostream &stream_;
    Witness previous_;
    Limits limits_;
    uint64_t count_ = 0, bytes_ = 0;
    bool stopped_ = false, finished_ = false, failed_ = false;
};

class Reader {
public:
    Reader(std::istream &stream, const Context &expected, const Witness &initial,
           const ApproveContext &approve_context, Limits limits = {})
        : stream_(stream), previous_(initial), limits_(limits) {
        detail::limits(limits); detail::approve(expected, approve_context);
        if (line() != "segarally-host-resume-v1") detail::fail("unknown transcript version");
        std::istringstream fields(line()); std::string tag;
        if (!(fields >> tag >> context_.provenance_sha256) || tag != "context") detail::fail("missing context");
        const auto sound = detail::number(fields), hook = detail::number(fields);
        if (sound > 1 || hook > 1) detail::fail("unknown configuration flags");
        context_.sound = bool(sound); context_.instruction_boundary_hook = bool(hook); detail::exhausted(fields);
        if (context_ != expected) detail::fail("context binding/configuration mismatch");
        std::istringstream start(line());
        if (!(start >> tag) || tag != "initial") detail::fail("missing initial state");
        const auto recorded_initial = detail::witness(start); detail::exhausted(start);
        if (initial.instructions || recorded_initial != initial) detail::fail("startup state mismatch");
    }
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;
    // The caller executes each row's EXACT run_until_frame(budget), asserts
    // before/after witnesses and return reason, and drains PCM after each row.
    // Merely reading all rows is not a validated replay.
    std::optional<Resume> next() {
        if (failed_) detail::fail("transcript previously failed");
        if (finished_) return std::nullopt;
        try {
            std::istringstream fields(line()); std::string tag;
            if (!(fields >> tag)) detail::fail("empty record");
            if (tag == "end") {
                const auto count = detail::number(fields); const auto terminal = detail::witness(fields);
                detail::exhausted(fields);
                if (count != count_ || terminal != previous_) detail::fail("terminal state/count mismatch");
                if (stream_.peek() != std::char_traits<char>::eof() || stream_.bad()) detail::fail("data follows completion marker");
                finished_ = true; return std::nullopt;
            }
            if (tag != "resume" || count_ == limits_.records) detail::fail("unknown record or record limit reached");
            Resume r; r.index = detail::number(fields); r.budget = detail::number(fields); r.before = detail::witness(fields);
            auto byte = [&] { const auto n = detail::number(fields, 16, 2); if (n > 255) detail::fail("invalid input byte"); return uint8_t(n); };
            r.inputs.in0 = byte(); r.inputs.in1 = byte(); r.inputs.in2 = byte(); r.inputs.switches = byte();
            for (auto &b : r.inputs.analog) b = byte();
            r.after = detail::witness(fields); r.reason = detail::reason(fields); detail::exhausted(fields);
            detail::resume(r, count_, previous_, stopped_);
            previous_ = r.after; stopped_ = detail::frontier(r.reason); ++count_; return r;
        } catch (...) { failed_ = true; throw; }
    }
    uint64_t records() const { return count_; }
    bool complete_transcript() const { return finished_; }
    const Context &context() const { return context_; }
private:
    std::string line() {
        std::string result;
        for (;;) {
            const auto c = stream_.get();
            if (c == std::char_traits<char>::eof()) detail::fail("truncated transcript (completion marker required)");
            if (++bytes_ > limits_.bytes) detail::fail("byte limit reached");
            if (c == '\n') { if (!result.empty() && result.back() == '\r') result.pop_back(); return result; }
            if (result.size() == 1024) detail::fail("line limit reached");
            result.push_back(char(c));
        }
    }
    std::istream &stream_;
    Witness previous_;
    Context context_;
    Limits limits_;
    uint64_t count_ = 0, bytes_ = 0;
    bool stopped_ = false, finished_ = false, failed_ = false;
};

struct IndependentCpuCapabilities {
    bool startup_seed_images = false;
    bool exact_host_resume_boundaries = false;
    bool known_configuration = false;
};
inline void require_independent_cpu_support(const Context &context, IndependentCpuCapabilities capabilities = {}) {
    if (context.sound) detail::fail("independent CPU harness has no coupled SCSP/UART support");
    if (!capabilities.startup_seed_images || !capabilities.exact_host_resume_boundaries || !capabilities.known_configuration)
        detail::fail("independent CPU harness startup seeds/resume boundaries/configuration are unsupported");
}
} // namespace srally::host_input
