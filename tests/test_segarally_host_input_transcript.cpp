#include "tools/srally_boot/host_input_transcript.h"
#include <cstdio>
#include <vector>

namespace {
using namespace srally::host_input;
unsigned checks = 0;
void require(bool condition, const char *message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error &) { rejected = true; }
    require(rejected, "unsupported or malformed transcript accepted");
}
Context context() { return {std::string(64, 'a'), false, true}; }
ApproveContext approval() {
    // Test-only synthetic identity. Production must inspect actual manifest,
    // startup seeds, image/audit/generation/executable hashes and options.
    return [](const Context &c) { if (c != context()) throw std::runtime_error("synthetic context mismatch"); };
}
const Witness initial{0, 0, 0, 0, 0x420};
std::vector<Resume> steps() {
    Resume a; a.budget = 1; a.before = initial; a.after = {1, 475600, 0, 0, 0x424};
    a.inputs = {0x00, 0x01, 0x7f, 0x80, {0xff, 0xfe, 0x10, 0xa0, 0xbc, 0x55, 0x42, 0x99}};
    Resume b = a; b.index = 1; b.before = a.after; b.after = {1, 475609, 19024360, 1, 0x500};
    b.reason = Machine::Reason::Frame; b.inputs.analog[0] = 0x23;
    Resume c = b; c.index = 2; c.before = b.after; c.after = {2, 475610, 19024400, 1, 0x504};
    c.reason = Machine::Reason::Budget; c.inputs.in0 = 0xfe;
    return {a, b, c};
}
bool equal_pins(const rt::Model2AIO::Inputs &a, const rt::Model2AIO::Inputs &b) {
    return a.in0 == b.in0 && a.in1 == b.in1 && a.in2 == b.in2 && a.switches == b.switches && a.analog == b.analog;
}
std::string transcript() {
    std::ostringstream out; Writer writer(out, context(), initial, approval());
    for (const auto &r : steps()) writer.append(r);
    require(!writer.complete_transcript(), "unfinished writer reported completion");
    writer.finish(steps().back().after);
    require(writer.complete_transcript(), "writer missed completion");
    rejects([&] { writer.finish(steps().back().after); });
    return out.str();
}
void consume(const std::string &text, Limits limits = {}) {
    std::istringstream in(text); Reader reader(in, context(), initial, approval(), limits);
    while (reader.next()) {}
    require(reader.complete_transcript(), "reader failed to require terminal marker");
}
std::string replace(std::string text, const std::string &from, const std::string &to) {
    const auto at = text.find(from); require(at != std::string::npos, "fixture replacement not found");
    text.replace(at, from.size(), to); return text;
}
void stream_contract() {
    const auto text = transcript(); std::istringstream in(text); Reader reader(in, context(), initial, approval());
    require(!reader.complete_transcript(), "header alone considered complete");
    for (const auto &expected : steps()) {
        const auto seen = reader.next();
        require(seen && seen->index == expected.index && seen->budget == expected.budget &&
            seen->before == expected.before && seen->after == expected.after && seen->reason == expected.reason &&
            equal_pins(seen->inputs, expected.inputs), "resume boundary, reason, budget, or raw cabinet pins changed");
    }
    require(!reader.complete_transcript(), "last resume accepted without terminal marker");
    require(!reader.next() && reader.complete_transcript() && reader.records() == 3, "completed stream not recognized");
    require(!reader.next(), "completion was not stable");
    rejects([&] { consume(text.substr(0, text.find("end "))); });
    rejects([&] { consume(text.substr(0, text.size() - 1)); });
    rejects([&] { consume(text + "resume\n"); });
    rejects([&] { consume(replace(text, "end 3 ", "end 2 ")); });
    rejects([&] { consume(replace(text, "end 3 2 475610", "end 3 2 475611")); });
    rejects([&] { consume(replace(text, "resume 1 1 ", "resume 0 1 ")); });
    rejects([&] { consume(replace(text, "resume 1 1 1 475600", "resume 1 1 1 475599")); });
    rejects([&] { consume(replace(text, "1 475609 19024360 1", "1 475599 19024360 1")); });
    rejects([&] { consume(replace(text, "2 475610 19024400 1", "2 475610 19024359 1")); });
    rejects([&] { consume(replace(text, "2 475610 19024400 1", "2 475610 19024400 0")); });
    rejects([&] { consume(replace(text, " budget\n", " missing\n")); });
    rejects([&] { consume(replace(text, " frame\n", " budget\n")); });
    rejects([&] { consume(replace(text, " frame\n", "unknown\n")); });
    rejects([&] { consume(replace(text, "resume 0 1 ", "resume 0 0 ")); });
    rejects([&] { consume(replace(text, "resume 0 1 ", "resume 0 100001 ")); });
    rejects([&] { consume(replace(text, "00 01 7f", "0 01 7f")); });
    rejects([&] { consume(replace(text, "00 01 7f", "gg 01 7f")); });
    rejects([&] { consume(replace(text, "resume 0 1 ", "resume 0 +1 ")); });
    rejects([&] { consume(replace(text, "resume 0 1 ", "resume 0 18446744073709551616 ")); });
    rejects([&] { consume(replace(text, " budget\n", " budget extra\n")); });
    rejects([&] { consume(replace(text, "segarally-host-resume-v1", "segarally-input-v1")); });
    rejects([&] { consume(replace(text, " 0 1\ninitial", " 0 2\ninitial")); });
    rejects([&] { consume(replace(text, "initial 0 0 0 0 00000420", "initial 0 0 0 0 00000424")); });
    rejects([&] { consume(replace(text, "context " + context().provenance_sha256, "context " + std::string(64, 'b'))); });
    rejects([&] { consume(text, {2, 65536}); });
    rejects([&] { consume(text, {3, uint64_t(text.size() - 1)}); });
    consume(text, {3, uint64_t(text.size())});
    rejects([&] { consume(replace(text, "resume 0 1 ", std::string(1025, 'x'))); });
    // CRLF remains bounded and preserves every row.
    std::string windows; for (auto c : text) { if (c == '\n') windows += '\r'; windows += c; }
    consume(windows);
    {
        std::istringstream bad(replace(text, "resume 0 1 ", "resume 0 0 "));
        Reader poisoned(bad, context(), initial, approval());
        rejects([&] { poisoned.next(); }); const auto position = bad.tellg();
        rejects([&] { poisoned.next(); });
        require(!poisoned.complete_transcript() && bad.tellg() == position, "failed reader resumed parsing subsequent records");
    }
    {
        std::istringstream bad(text); Reader poisoned(bad, context(), initial, approval());
        bad.setstate(std::ios::badbit); rejects([&] { poisoned.next(); });
        require(!poisoned.complete_transcript(), "read failure reported completion");
    }
}
void wide_counts_and_overflow() {
    // Exercise every witness field's width; completed counts above 2^32 are
    // reachable within the stream cap, without constructing a long fixture.
    const Witness wide{UINT64_C(0x100000001), UINT64_C(0xfffffffffffffff0),
        UINT64_C(0x100000002), UINT64_C(0x100000003), 0xf1234567};
    std::ostringstream fields; detail::witness(fields, wide); std::istringstream encoded(fields.str());
    require(detail::witness(encoded) == wide, "64-bit witness or unsigned 32-bit PC was narrowed");
    // The actual public Writer/Reader path also preserves large cycles/time/
    // frame counters, rather than merely parsing an isolated numeric token.
    std::ostringstream out; Writer writer(out, context(), initial, approval());
    auto r = steps()[0]; r.reason = Machine::Reason::Frame; r.after = wide; r.after.instructions = 1;
    writer.append(r); writer.finish(r.after); std::istringstream stream(out.str());
    Reader reader(stream, context(), initial, approval()); const auto seen = reader.next();
    require(seen && seen->after == r.after && !reader.next() && reader.complete_transcript(), "wide recorded boundary changed");
    // Machine rejects an overflowing requested target even when a frame could
    // finish earlier. Apply that same rule to a local resume near count max.
    Resume edge; edge.before = {UINT64_MAX - 1, 10, 100, 0, 0x420}; edge.after = edge.before;
    edge.after.instructions = UINT64_MAX; edge.budget = 1;
    detail::resume(edge, 0, edge.before, false);
    edge.budget = 2; edge.reason = Machine::Reason::Frame; edge.after = edge.before; ++edge.after.frames;
    rejects([&] { detail::resume(edge, 0, edge.before, false); });
}
void writer_contract() {
    std::ostringstream out;
    rejects([&] { Writer w(out, context(), initial, {}); });
    require(out.str().empty(), "unapproved context emitted a transcript header");
    rejects([&] { Writer w(out, context(), initial, [](const Context &) { throw std::runtime_error("unverified assets"); }); });
    require(out.str().empty(), "rejected provenance emitted a transcript header");
    rejects([&] { Writer w(out, {"unverified", false, false}, initial, approval()); });
    rejects([&] { Writer w(out, context(), {1, 0, 0, 0, 0x420}, approval()); });
    rejects([&] { Writer w(out, context(), initial, approval(), {250001, 65536}); });
    {
        std::ostringstream limited; Writer w(limited, context(), initial, approval(), {1, 65536});
        w.append(steps()[0]); rejects([&] { w.append(steps()[1]); });
        rejects([&] { w.finish(steps()[0].after); });
        rejects([&] { consume(limited.str(), {1, 65536}); });
    }
    {
        std::ostringstream limited; Writer w(limited, context(), initial, approval(), {3, 150});
        rejects([&] { w.append(steps()[0]); });
        require(!w.complete_transcript(), "bounded recording failure reported completion");
        rejects([&] { consume(limited.str()); });
    }
    for (unsigned invalid = 0; invalid < 3; ++invalid) {
        std::ostringstream bad; Writer w(bad, context(), initial, approval()); auto r = steps()[0];
        if (invalid == 0) r.reason = Machine::Reason::Frame; // No rendered frame.
        if (invalid == 1) r.after.instructions = 0; // Budget was not completed.
        if (invalid == 2) r.after.instructions = 2; // Actual resume exceeded its budget.
        rejects([&] { w.append(r); }); rejects([&] { w.finish(initial); });
    }
    {
        std::ostringstream evidence; Writer w(evidence, context(), initial, approval()); auto r = steps()[0];
        r.reason = Machine::Reason::MissingCode; w.append(r); w.finish(r.after); consume(evidence.str());
    } // Complete frontier evidence, not a completed race.
    {
        std::ostringstream stopped; Writer w(stopped, context(), initial, approval()); auto r = steps()[0];
        r.reason = Machine::Reason::MissingCode; w.append(r); rejects([&] { w.append(steps()[1]); });
    }
    {
        std::ostringstream bad; Writer w(bad, context(), initial, approval());
        bad.setstate(std::ios::badbit); rejects([&] { w.append(steps()[0]); });
        require(!w.complete_transcript(), "write failure reported completion");
    }
    {
        std::ostringstream bad; Writer w(bad, context(), initial, approval());
        w.append(steps()[0]); rejects([&] { w.finish(initial); });
    }
    // No implicit capability follows from a matching digest or disabled sound.
    rejects([&] { require_independent_cpu_support(context()); });
    rejects([&] { require_independent_cpu_support(context(), {true, false, true}); });
    rejects([&] { require_independent_cpu_support(context(), {true, true, false}); });
    auto sound = context(); sound.sound = true;
    rejects([&] { require_independent_cpu_support(sound, {true, true, true}); });
    require_independent_cpu_support(context(), {true, true, true});
}
} // namespace
int main() {
    try { stream_contract(); wide_counts_and_overflow(); writer_contract(); std::printf("Host input transcript: %u checks passed\n", checks); return 0; }
    catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
