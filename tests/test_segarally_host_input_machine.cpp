// Synthetic generated instructions and board data only; no game ROM or GPU.
// A real timer IRQ crosses screen frame end before the next opcode. Capture
// must preserve the resulting equal instruction counts across host resumes.
#include "tools/srally_boot/host_input_transcript.h"
#include "runtime/model2a_clock.h"
#include "runtime/gen_support.h"
#include "runtime/tgp.h"
#include "runtime/video.h"
#include <cstdio>
#include <cstring>
#include <tuple>

namespace {
using namespace srally::host_input;
constexpr uint32_t PRCB = 0x200000, TABLE = 0x200200, STACK = 0x201800;
constexpr uint64_t FRAME_END = (rt::Model2AScreen::FRAME_NS + rt::Model2AScreen::VBLANK_NS) / 40;
unsigned checks = 0;
void require(bool condition, const char *message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
uint32_t code_word(uint32_t pc) { return 0x5c801610u ^ pc; }
Machine::Images images() {
    Machine::Images i; i.program.resize(0x600);
    auto put = [&](uint32_t a, uint32_t w) { for (unsigned b = 0; b < 4; ++b) i.program[a+b] = uint8_t(w >> (8*b)); };
    put(4, PRCB); put(12, 0x420);
    for (uint32_t pc : {0x420u, 0x424u, 0x500u, 0x504u}) put(pc, code_word(pc));
    i.tgp_tables.resize(0x40000); i.copro_data.resize(0x800000); i.polygons.resize(4); i.textures.resize(2);
    return i;
}
Machine::Options options(bool hook) {
    Machine::Options o; o.comm = o.uart = o.sound = o.external_video = false;
    o.instruction_boundary_hook = hook; return o;
}
void initialize(Machine &m) {
    auto eeprom = m.save_eeprom(), backup = m.save_backup();
    eeprom[7] = 0x23; backup[10] = 0x42; m.load_eeprom(eeprom); m.load_backup(backup);
    auto &c = const_cast<rt::Cpu &>(m.cpu());
    c.m_PRCB = PRCB; c.m_PC = 0; c.m_ICR = 0xf8f8f8f8;
    c.m_r[rt::I960_FP] = 0x200800; c.m_r[rt::I960_SP] = 0x200840;
    c.bus->write_dword(PRCB + 20, TABLE); c.bus->write_dword(PRCB + 24, STACK);
    c.bus->write_dword(TABLE + 36 + (0xf8 - 8) * 4, 0x500);
    c.bus->write_dword(0xe80004, 4); // Enable timer IRQ after its authentic delay.
    c.reference_cycles = 2; (void)c.bus->read_dword(0xe80004);
    c.bus->write_dword(0xf00000, uint32_t(FRAME_END - 5));
}
bool pins_equal(const rt::Model2AIO::Inputs &a, const rt::Model2AIO::Inputs &b) {
    return a.in0 == b.in0 && a.in1 == b.in1 && a.in2 == b.in2 && a.switches == b.switches && a.analog == b.analog;
}
void equal(const Machine &a, const Machine &b) {
    const auto x = a.stats(true), y = b.stats(true);
    require(Witness::of(x) == Witness::of(y) &&
        std::tie(x.irq_entries, x.previous_dispatch, x.frames_parsed, x.screen_frame, x.latched_frame, x.screen_hash) ==
        std::tie(y.irq_entries, y.previous_dispatch, y.frames_parsed, y.screen_frame, y.latched_frame, y.screen_hash),
        "capture changed Machine instruction/cycle/frame/IRQ state or pixels");
    const auto &c = a.cpu(), &d = b.cpu();
    require(!std::memcmp(c.m_r, d.m_r, sizeof(c.m_r)) && !std::memcmp(c.m_rcache, d.m_rcache, sizeof(c.m_rcache)) &&
        !std::memcmp(c.m_rcache_frame_addr, d.m_rcache_frame_addr, sizeof(c.m_rcache_frame_addr)) &&
        !std::memcmp(c.m_fp, d.m_fp, sizeof(c.m_fp)) && !std::memcmp(c.m_irq_line_state, d.m_irq_line_state, sizeof(c.m_irq_line_state)) &&
        std::tie(c.m_IP, c.m_PIP, c.m_PC, c.m_AC, c.m_ICR, c.m_rcache_pos, c.m_immediate_irq, c.m_immediate_vector, c.m_immediate_pri) ==
        std::tie(d.m_IP, d.m_PIP, d.m_PC, d.m_AC, d.m_ICR, d.m_rcache_pos, d.m_immediate_irq, d.m_immediate_vector, d.m_immediate_pri),
        "capture changed CPU registers/cache/IRQ state");
    require(a.save_eeprom() == b.save_eeprom() && a.save_backup() == b.save_backup(), "capture changed persistence");
}
void exact_resume(bool hook) {
    Machine recorded(images(), options(hook)), untouched(images(), options(hook)), replayed(images(), options(hook));
    initialize(recorded); initialize(untouched); initialize(replayed);
    const auto start = Witness::of(recorded.stats());
    const Context context{std::string(64, 'b'), false, hook};
    const auto approval = [=](const Context &c) { require(c == context, "synthetic startup context mismatch"); };
    std::ostringstream text; Writer writer(text, context, start, approval);
    std::vector<Resume> expected;
    for (uint64_t index = 0; index < 3; ++index) {
        Resume r; r.index = index; r.budget = index == 1 ? 2 : 1;
        r.before = Witness::of(recorded.stats()); r.inputs.analog[0] = uint8_t(0x70 + index);
        r.inputs.in0 = index == 2 ? 0xfe : 0xff;
        recorded.inputs() = untouched.inputs() = r.inputs;
        const auto observed = recorded.run_until_frame(r.budget), original = untouched.run_until_frame(r.budget);
        r.after = Witness::of(recorded.stats()); r.reason = observed.reason;
        require(observed.reason == original.reason && observed.instructions == original.instructions && observed.pc == original.pc,
            "recorded and unrecorded resume returns differ");
        equal(recorded, untouched); writer.append(r); equal(recorded, untouched); expected.push_back(r);
    }
    require(expected[0].reason == Machine::Reason::Budget && expected[0].after.instructions == 1 && expected[0].after.frames == 0,
        "fixture first budget unexpectedly synchronized the completed opcode");
    require(expected[1].reason == Machine::Reason::Frame && expected[1].before.instructions == expected[1].after.instructions &&
        expected[1].after.cycles > expected[1].before.cycles && expected[1].after.frames == 1 && recorded.stats().irq_entries == 1,
        "fixture did not establish same-count frame return during real IRQ entry");
    require(recorded.cpu().m_r[16] == 0x72 && recorded.cpu().m_r[17] == 0xfe,
        "opcode after same-count IRQ/frame boundary saw incorrect cabinet input");
    writer.finish(Witness::of(recorded.stats()));
    // Preflight checks footer/provenance/bounds without touching emulation.
    std::istringstream preflight(text.str()); Reader scan(preflight, context, start, approval);
    while (scan.next()) {}
    require(scan.complete_transcript(), "preflight transcript not complete");
    std::istringstream input(text.str()); Reader reader(input, context, Witness::of(replayed.stats()), approval);
    size_t count = 0;
    while (const auto r = reader.next()) {
        require(r->before == Witness::of(replayed.stats()), "replay pre-witness differs");
        replayed.inputs() = r->inputs; const auto outcome = replayed.run_until_frame(r->budget);
        require(r->after == Witness::of(replayed.stats()) && r->reason == outcome.reason, "exact replay boundary differs");
        require(pins_equal(replayed.inputs(), expected.at(count).inputs), "replay pins differ"); ++count;
        // Production sound capture/playback must drain samples here, at each
        // original resume, with matching sound config; this fixture is quiet.
    }
    require(reader.complete_transcript() && count == 3, "replay did not consume terminal marker"); equal(recorded, replayed);
    // A count-only runner stopping at N misses the recorded IRQ/frame state.
    require(expected[0].after.instructions == expected[1].after.instructions && expected[0].after != expected[1].after,
        "count-only replay ambiguity was not demonstrated");
}
} // namespace
namespace gen {
bool instruction_boundary_hook_compatible() { return true; }
bool has_code(uint32_t pc) { return pc == 0x420 || pc == 0x424 || pc == 0x500 || pc == 0x504; }
uint64_t native_instructions() { return 4; }
void run(Env &e) {
    while (!e.ls.boundary_yielded() && has_code(e.c.m_IP) && !e.ls.boundary()) {
        const auto pc = e.c.m_IP;
        if (e.c.bus->read_dword(pc) != code_word(pc)) throw rt::Fatal("synthetic live instruction guard mismatch");
        e.c.consume_reference_cycles(pc == 0x420 ? uint32_t(FRAME_END - 5) : 1);
        if (pc != 0x420) {
            e.c.m_r[16] = (e.c.bus->read_dword(0x01c0001c) >> 16) & 255;
            e.c.m_r[17] = (e.c.bus->read_dword(0x01c00000) >> 16) & 255;
        }
        e.c.m_IP = pc + 4; ++e.ls.count;
    }
}
} // namespace gen
namespace rt::tgpgen {
extern const uint32_t program_crc32 = 0, program_words = 0;
void run(Tgp &, uint64_t) { throw TgpFatal("synthetic input fixture must not execute TGP"); }
}
int main() {
    try { exact_resume(false); exact_resume(true); std::printf("Host resume transcript Machine/IRQ/frame/input: %u checks passed\n", checks); return 0; }
    catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
