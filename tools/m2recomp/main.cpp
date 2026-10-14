// m2recomp: the i960 static recompiler (design doc, i960 static recompiler).
//
//   m2recomp PROGRAM.bin OUTDIR [--seeds FILE]... [--hooks FILE] [--chunk N] [--fast_inaccuracy]
//
// --hooks: "ADDRESS name" lines; the generated code calls rt::hook_<name>(c)
// just before the instruction at ADDRESS (enhancements: see
// src/runtime/enhance.h). A hook that is off does nothing.
//
// Decodes every instruction reachable from the boot record plus the seed
// addresses (src/i960/reach) and emits native C++: each instruction's
// semantics inline, with registers, literals and effective-address forms
// resolved here, at recompile time. Known branch targets are direct gotos; an
// indirect transfer (bx, callx, ret, an interrupt) re-dispatches on the
// address. The semantics are MAME's (src/refcore/i960_ref.cpp, src/runtime/cpu.cpp), expression
// for expression, so the lockstep harness can hold the output to MAME.
//
// --fast_inaccuracy (off by default) groups the speed options for slow targets
// (PS Vita). Without it the output is the exact one the lockstep and trace
// comparisons against MAME need. With it:
//
// 1. Block bookkeeping: the lockstep bookkeeping (IP store, interrupt and
// event check, instruction count) is emitted once per basic block instead of
// before every instruction. Each instruction's semantics and the instruction
// count are unchanged, but an interrupt or a scheduled event is taken at the
// next block start, a few instructions later than MAME takes it, so the output
// no longer matches MAME's interrupt log: the lockstep and trace comparisons
// need the default (exact) output. Faster, for slow targets (PS Vita).
// A block starts at every direct branch target, after every transfer of
// control, at every seed (boot entries, interrupt handlers, harvested indirect
// targets) and at every hook address. An indirect jump into the middle of a
// block (an address no seed names) still lands on the instruction's label;
// its switch case corrects the count for the instructions skipped. The
// runtime applies an event only at its exact count, which a block can step
// over: the generated block_boundary() replays such an event at its own count
// at the next block start, then restores the true count.
//
// 2. Direct chaining: gen::run moves from chunk to chunk itself (binary search
// on the chunk ranges) instead of returning to GameLoop, which re-checked
// has_code and re-scanned every chunk on each transfer between chunks (~1,360
// per frame in a race, 0.68 us each on the Vita: platform/vita perf.log, CPU-2).
// It returns as before when the frame ends (the lockstep is finished) or when
// control reaches an address no chunk has code for, so GameLoop still reports
// that address. Same instructions in the same order: this part alone keeps
// the output exact.
//
// There is no fallback: an instruction without a native template fails the
// recompile, listed by address and mnemonic. At run time, control reaching an
// address that was not recompiled is a hard error naming it (add it to the
// seeds). Nothing is ever interpreted. The output is portable C++20 (no
// inline assembly, no compiler builtins) for x86-64 and ARM64 alike.
//
// The output is derived from the game: write it under build/ (git-ignored),
// never commit it (rules.md rule 6).
//
// Seeds files: one hex address per line, or the MAME harvest format
// ("kind from target count"; the target column is used).

#include "i960/decode.h"
#include "i960/reach.h"
#include "i960/timing.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using i960::Insn;

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08xu", v);
    return b;
}
std::string lbl(uint32_t a) {
    char b[16];
    std::snprintf(b, sizeof b, "L_%08x", a);
    return b;
}
std::string R(unsigned n) { return "R[" + std::to_string(n) + "]"; }
std::string lit(unsigned n) { return std::to_string(n) + "u"; }

// Operand forms, exactly as MAME's get_1_ri / get_2_ri / get_1_ci / get_2_ci.
std::string s1(const Insn &in) { return in.m1 ? lit(in.src1) : R(in.src1); }
std::string s2(const Insn &in) { return in.m2 ? lit(in.src2) : R(in.src2); }
std::string c1(const Insn &in) { return in.m1 ? lit(in.src1) : R(in.src1); }
std::string c2(const Insn &in) { return R(in.src2); }

// Resolve MAME get_1/2_rif and get_1/2_rifl operand modes at compile time.
// See src/refcore/i960_ref.cpp: the architectural FP registers retain double
// values; a general-register single destination rounds through float.
std::string real_source(unsigned reg, bool fp_mode, bool wide) {
    if (fp_mode) return reg < 4 ? "c.m_fp[" + std::to_string(reg) + "]" : reg == 0x16 ? "1.0" : "0.0";
    if (!wide) return "double(gen::u2f(" + R(reg) + "))";
    reg &= 0x1e;
    return "rt::u2d(uint64_t(" + R(reg) + ") | (uint64_t(" + R(reg + 1) + ") << 32))";
}

// MAME get_ea, with the displacement word and IP-relative base known now.
std::string ea(const Insn &in) {
    const std::string ab = R(in.src2), ix = R(in.index);
    const std::string scaled = "(" + ix + " << " + std::to_string(in.scale) + ")";
    switch (in.ea) {
    case i960::EaMode::Offset: return hex(in.offset);
    case i960::EaMode::AbaseOffset: return "(" + ab + " + " + hex(in.offset) + ")";
    case i960::EaMode::Abase: return ab;
    case i960::EaMode::IpDisp: return hex(in.addr + 8 + in.disp); // disp + address after the displacement word
    case i960::EaMode::AbaseIndex: return "(" + ab + " + " + scaled + ")";
    case i960::EaMode::Disp: return hex(in.disp);
    case i960::EaMode::AbaseDisp: return "(" + hex(in.disp) + " + " + ab + ")";
    case i960::EaMode::IndexDisp: return "(" + hex(in.disp) + " + " + scaled + ")";
    case i960::EaMode::AbaseIndexDisp: return "(" + hex(in.disp) + " + " + ab + " + " + scaled + ")";
    default: return "";
    }
}

// --fast_inaccuracy: the check at a block start. The lockstep runtime applies an event
// (a frame-clock probe, a UART shift) when the count equals the event's; a
// block adds its whole length at its end and can step over it. Such an event
// is applied here at its own count, then the true count is restored: the
// instruction total stays exact, the event runs a few instructions late.
const char *const kBlockBoundary = R"(namespace {
bool block_events(rt::Lockstep &ls) {
    const uint64_t now = ls.count;
    while (ls.next_count < now && ls.next_count < ls.end_count) { // stepped over
        const uint64_t at = ls.next_count;
        ls.count = at;
        const bool taken = ls.boundary(); // applies the events at their count
        ls.count = now;
        if (taken) return true;           // an interrupt was taken: re-dispatch
        if (ls.next_count == at) break;   // nothing consumed (never expected)
    }
    return ls.boundary();
}
inline bool block_boundary(rt::Lockstep &ls) {
    if (ls.count < ls.next_count && ls.count < ls.end_count) [[likely]] return false;
    return block_events(ls);
}
} // namespace

)";

// Enhancement hooks: address -> name (rt::hook_<name>), from --hooks.
std::map<uint32_t, std::string> g_hooks;

// Explicit, ROM-backed analysis ranges. These never change the runtime bus:
// every emitted RAM instruction checks the live bytes before executing.
struct Relocation { uint32_t guest, source, size; };
std::vector<Relocation> g_relocations;
bool relocated(uint32_t a) {
    for (const auto &r : g_relocations)
        if (uint64_t(a) >= r.guest && uint64_t(a) < uint64_t(r.guest) + r.size) return true;
    return false;
}

struct Emitter {
    const std::set<uint32_t> *chunk_addrs; // instructions emitted in this chunk
    std::ostringstream o;
    uint64_t native = 0;
    std::vector<std::string> *missing; // instructions without a native template

    // --fast_inaccuracy block state, set by the caller before each emit().
    bool blocks = false;                   // block bookkeeping
    bool leader = true;                    // first instruction of a block (always, without --fast_inaccuracy)
    unsigned block_end = 0;                // a block's last instruction: the block's length
    std::set<uint32_t> *targets = nullptr; // analysis pass: records every direct transfer target
    bool exits = false;                    // after emit(): the instruction has an explicit exit

    // Transfer to a known address: a goto when it is in this chunk.
    std::string jump(uint32_t t) const {
        if (targets) targets->insert(t);
        if (chunk_addrs->count(t)) return "goto " + lbl(t) + ";";
        return "{ c.m_IP = " + hex(t) + "; goto dispatch; }";
    }

    void line(const std::string &s) { o << "    " << s << "\n"; }

    // Emits one instruction. Returns false when control never falls through
    // to the next address.
    bool emit(const Insn &in) {
        const uint32_t pc = in.addr, next = pc + in.length;
        const std::string mn = in.op->mnem;
        const uint16_t code = in.fmt == i960::Format::Reg ? in.op->code : uint16_t(in.word >> 24);
        char comment[96];
        std::snprintf(comment, sizeof comment, "// %08x: %s", pc, i960::format_mame(in).c_str());
        o << lbl(pc) << ": " << comment << "\n";
        // Lockstep: MAME's interrupt events happen before this instruction
        // (--fast_inaccuracy: before the block's first instruction only).
        if (leader) {
            line("c.m_IP = " + hex(pc) + ";");
            line(blocks ? "if (block_boundary(ls)) goto dispatch;" : "if (ls.boundary()) goto dispatch;");
            if (const auto h = g_hooks.find(pc); h != g_hooks.end()) line("rt::hook_" + h->second + "(c);");
        }
        if (relocated(pc)) {
            // Guard both words of long instructions, including fallthrough and
            // direct-goto entries. Self-modifying code must fail closed.
            std::string differs = "c.bus->read_dword(" + hex(pc) + ") != " + hex(in.word);
            if (in.length == 8) differs += " || c.bus->read_dword(" + hex(pc + 4) + ") != " + hex(in.disp);
            line("if (" + differs + ") { c.m_IP = " + hex(pc) +
                 "; throw rt::Fatal(\"relocated code differs at " + hex(pc) + "\"); }");
        }
        const auto cycles = i960::reference_opcode_cycles(in);
        if (!cycles) {
            missing->push_back(hex(pc) + ": missing reference cycle cost for " + i960::format_mame(in));
            return false; // never emit runnable code with invented timing
        }
        if (*cycles) line("c.consume_reference_cycles(" + std::to_string(*cycles) + "u);");

        std::string body, exit;   // body runs before ++count; exit transfers control
        bool falls = true;
        const std::string d = R(in.dst);
        auto set = [&](const std::string &v) { body += d + " = " + v + ";"; };
        auto two = [&](const std::string &v) { // read both sources first, as MAME does
            body += "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; " + d + " = " + v + "; }";
        };
        const uint32_t tgt = in.target, tgt_masked = in.target & ~3u; // bxx/bxx_s mask the target, b/bal/call/bbc/bbs do not

        switch (in.fmt) {
        case i960::Format::Ctrl:
            switch (code) {
            case 0x08: exit = jump(tgt); falls = false; break;                                   // b
            case 0x09: body = "c.m_IP = " + hex(next) + "; c.do_call(" + hex(tgt) + ", 0, R[1]);"; // call
                       exit = jump(tgt); falls = false; break;
            case 0x0a: body = "c.do_ret();"; exit = "goto dispatch;"; falls = false; break;       // ret
            case 0x0b: body = "R[30] = " + hex(next) + ";"; exit = jump(tgt); falls = false; break; // bal
            case 0x10: exit = "if (!(AC & 7)) " + jump(tgt); break;                                // bno (unmasked, as MAME)
            case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
                exit = "if (AC & " + std::to_string(code & 7) + ") " + jump(tgt_masked); break;
            default: return unsupported(in);                                                     // faults
            }
            break;

        case i960::Format::Cobr:
            if (code >= 0x20 && code <= 0x27) { // test*
                const std::string cond = code == 0x20 ? "!(AC & 7)" : "(AC & " + std::to_string(code & 7) + ")";
                body = R(in.src1) + " = " + cond + " ? 1u : 0u;";
            } else if (code == 0x30 || code == 0x37) { // bbc / bbs
                body = "{ const uint32_t t1 = " + c1(in) + " & 0x1f, t2 = " + c2(in) + "; cond = " +
                       (code == 0x37 ? "(t2 & (1u << t1)) != 0" : "!(t2 & (1u << t1))") +
                       "; AC = cond ? ((AC & ~7u) | 2u) : (AC & ~7u); }";
                exit = "if (cond) " + jump(tgt);
            } else if (code >= 0x31 && code <= 0x36) { // cmpob*
                body = "AC = (AC & ~7u) | gen::cc_u(" + c1(in) + ", " + c2(in) + ");";
                exit = "if (AC & " + std::to_string(code & 7) + ") " + jump(tgt_masked);
            } else if (code >= 0x39 && code <= 0x3e) { // cmpib*
                body = "AC = (AC & ~7u) | gen::cc_s(int32_t(" + c1(in) + "), int32_t(" + c2(in) + "));";
                exit = "if (AC & " + std::to_string(code & 7) + ") " + jump(tgt_masked);
            } else return unsupported(in);
            break;

        case i960::Format::Mem: {
            const std::string e = ea(in), r = R(in.src1);
            switch (code) {
            case 0x80: body = r + " = c.bus->read_byte(" + e + ");"; break;                          // ldob
            case 0x82: body = "c.bus->write_byte(" + e + ", uint8_t(" + r + "));"; break;            // stob
            case 0x84: body = "c.m_IP = " + e + ";"; exit = "goto dispatch;"; falls = false; break; // bx
            case 0x85: body = "{ const uint32_t t = " + e + "; " + r + " = " + hex(next) + "; c.m_IP = t; }"; // balx
                       exit = "goto dispatch;"; falls = false; break;
            case 0x86: body = "{ const uint32_t t = " + e + "; c.m_IP = " + hex(next) + "; c.do_call(t, 0, R[1]); }"; // callx
                       exit = "goto dispatch;"; falls = false; break;
            case 0x88: body = r + " = c.i960_read_word_unaligned(" + e + ");"; break;                 // ldos
            case 0x8a: body = "c.i960_write_word_unaligned(" + e + ", uint16_t(" + r + "));"; break;  // stos
            case 0x8c: body = r + " = " + e + ";"; break;                                             // lda
            case 0x90: body = r + " = c.i960_read_dword_unaligned(" + e + ");"; break;                // ld
            case 0x92: body = "c.i960_write_dword_unaligned(" + e + ", " + r + ");"; break;           // st
            case 0x98: case 0xa0: case 0xb0: case 0x9a: case 0xa2: case 0xb2: {                      // ldl/ldt/ldq, stl/stt/stq
                const int n = (code & 0xf0) == 0x90 ? 2 : (code & 0xf0) == 0xa0 ? 3 : 4;
                const unsigned base = in.src1 & (n == 2 ? 0x1eu : 0x1cu);
                const bool store = code & 2;
                // MAME advances the address only on BURST-flagged regions.
                body = "{ uint32_t a = " + e + "; ";
                for (int i = 0; i < n; ++i) {
                    body += store ? "c.i960_write_dword_unaligned(a, " + R(base + i) + "); "
                                  : R(base + i) + " = c.i960_read_dword_unaligned(a); ";
                    if (i + 1 < n) body += "if (c.bus->flags(a) & rt::Cpu::BURST) a += 4; ";
                }
                body += "}";
                break;
            }
            case 0xc0: body = r + " = uint32_t(int32_t(int8_t(c.bus->read_byte(" + e + "))));"; break;     // ldib
            case 0xc2: body = "c.bus->write_byte(" + e + ", uint8_t(" + r + "));"; break;                   // stib
            case 0xc8: body = r + " = uint32_t(int32_t(int16_t(c.i960_read_word_unaligned(" + e + "))));"; break; // ldis
            case 0xca: body = "c.i960_write_word_unaligned(" + e + ", uint16_t(" + r + "));"; break;         // stis
            default: return unsupported(in);
            }
            break;
        }

        case i960::Format::Reg: {
            // An FP mode bit selects fp0-fp3 or a literal. Follow the reference
            // operand helpers; unused fields may also carry mode bits.
            auto fr1 = [&](bool wide = false) { return real_source(in.src1, in.m1, wide); };
            auto fr2 = [&](bool wide = false) { return real_source(in.src2, in.m2, wide); };
            auto real_dest = [&](const std::string &value, bool wide = false) {
                if (in.m3) {
                    if (in.dst >= 4) return false; // set_rif/literal is a MAME fatal error
                    body = "c.m_fp[" + std::to_string(in.dst) + "] = " + value + ";";
                } else if (wide) {
                    const unsigned dest = in.dst & 0x1e;
                    body = "{ const uint64_t v = rt::d2u(" + value + "); " + R(dest) + " = uint32_t(v); "
                           + R(dest + 1) + " = uint32_t(v >> 32); }";
                } else body = d + " = gen::f2u(float(" + value + "));";
                return true;
            };
            switch (code) {
            case 0x580: two("t2 ^ (1u << (t1 & 31))"); break;             // notbit
            case 0x581: two("t2 & t1"); break;                             // and
            case 0x582: two("t2 & ~t1"); break;                            // andnot
            case 0x583: two("t2 | (1u << (t1 & 31))"); break;             // setbit
            case 0x584: two("(~t2) & t1"); break;                          // notand
            case 0x586: two("t2 ^ t1"); break;                             // xor
            case 0x587: two("t2 | t1"); break;                             // or
            case 0x588: two("(~t2) & (~t1)"); break;                       // nor
            case 0x589: two("~(t2 ^ t1)"); break;                          // xnor
            case 0x58a: set("~" + s1(in)); break;                          // not
            case 0x58b: two("t2 | ~t1"); break;                            // ornot
            case 0x58c: two("t2 & ~(1u << (t1 & 31))"); break;            // clrbit
            case 0x58d: two("(~t2) | t1"); break;                          // notor
            case 0x58e: two("~t2 | ~t1"); break;                           // nand
            case 0x58f: two("(AC & 2) ? (t2 | (1u << (t1 & 31))) : (t2 & ~(1u << (t1 & 31)))"); break; // alterbit
            case 0x590: case 0x591: two("t2 + t1"); break;                 // addo, addi (MAME: no overflow)
            case 0x592: case 0x593: two("t2 - t1"); break;                 // subo, subi
            case 0x598: two("t1 >= 32 ? 0u : t2 >> t1"); break;            // shro
            case 0x59a: two("t1 >= 32 ? 0u : (int32_t(t2) < 0 ? ((t2 & ((1u << t1) - 1)) ? uint32_t((int32_t(t2) >> t1) + 1) "
                            ": uint32_t(int32_t(t2) >> t1)) : t2 >> t1)"); break; // shrdi
            case 0x59b: two("t1 >= 32 ? (int32_t(t2) < 0 ? 0xffffffffu : 0u) : uint32_t(int32_t(t2) >> t1)"); break; // shri
            case 0x59c: case 0x59e: two("t1 >= 32 ? 0u : t2 << t1"); break; // shlo, shli
            case 0x59d: two("std::rotl(t2, int(t1 & 0x1f))"); break;       // rotate
            case 0x5a0: body = "AC = (AC & ~7u) | gen::cc_u(" + s1(in) + ", " + s2(in) + ");"; break; // cmpo
            case 0x5a1: body = "AC = (AC & ~7u) | gen::cc_s(int32_t(" + s1(in) + "), int32_t(" + s2(in) + "));"; break; // cmpi
            case 0x5a2: body = "if (!(AC & 4)) AC = (AC & ~7u) | (" + s1(in) + " <= " + s2(in) + " ? 2u : 1u);"; break; // concmpo
            case 0x5a3: body = "if (!(AC & 4)) AC = (AC & ~7u) | (int32_t(" + s1(in) + ") <= int32_t(" + s2(in) + ") ? 2u : 1u);"; break; // concmpi
            case 0x5a4: case 0x5a5: case 0x5a6: case 0x5a7: {              // cmpinco/i, cmpdeco/i
                const bool sgn = code & 1, inc = code < 0x5a6;
                body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; AC = (AC & ~7u) | " +
                       (sgn ? "gen::cc_s(int32_t(t1), int32_t(t2))" : "gen::cc_u(t1, t2)") + "; " + d + " = t2 " +
                       (inc ? "+" : "-") + " 1; }";
                break;
            }
            case 0x5ac: body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; AC &= ~7u; "
                               "if ((t1 & 0xff000000) == (t2 & 0xff000000) || (t1 & 0x00ff0000) == (t2 & 0x00ff0000) || "
                               "(t1 & 0x0000ff00) == (t2 & 0x0000ff00) || (t1 & 0x000000ff) == (t2 & 0x000000ff)) AC |= 2; }"; break; // scanbyte
            case 0x5ae: body = "{ const uint32_t t1 = " + s1(in) + " & 0x1f, t2 = " + s2(in) + "; AC = (t2 & (1u << t1)) ? ((AC & ~7u) | 2u) : (AC & ~7u); }"; break; // chkbit
            case 0x5b0: // addc: preserve the reference's 32-bit sum before uint64_t assignment
                body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; "
                       "const uint64_t res = t2 + (t1 + ((AC >> 1) & 1u)); " + d + " = uint32_t(res); "
                       "AC &= ~3u; AC |= (res & (uint64_t(1) << 32)) ? 2u : 0u; "
                       "AC |= ((res ^ t1) & (res ^ t2) & 0x80000000u) ? 1u : 0u; }";
                break;
            case 0x5cc: set(s1(in)); break;                                // mov
            case 0x5dc: case 0x5ec: case 0x5fc: {                          // movl/movt/movq
                const int n = code == 0x5dc ? 2 : code == 0x5ec ? 3 : 4;
                const unsigned dst = in.dst & (n == 2 ? 0x1eu : 0x1cu);
                if (in.m1) {
                    for (int i = 0; i < n; ++i) body += R(dst + i) + " = " + lit(in.src1) + "; ";
                } else { // copy through temporaries (MAME memcpy; glibc loads before storing for these sizes)
                    body += "{ uint32_t t[4]; ";
                    for (int i = 0; i < n; ++i) body += "t[" + std::to_string(i) + "] = " + R(in.src1 + i) + "; ";
                    for (int i = 0; i < n; ++i) body += R(dst + i) + " = t[" + std::to_string(i) + "]; ";
                    body += "}";
                }
                break;
            }
            case 0x600: body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; if (t1 == 0xff000004u) c.m_ICR = c.bus->read_dword(t2); "
                               "else c.bus->write_dword(t1, c.bus->read_dword(t2)); AC = (AC & ~7u) | 2u; }"; break; // synmov
            case 0x602: body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; if (t1 == 0xff000010u) { c.m_IP = " + hex(next) +
                               "; c.send_iac(t2); } else { for (uint32_t k = 0; k < 16; k += 4) c.bus->write_dword(t1 + k, c.bus->read_dword(t2 + k)); } "
                               "AC = (AC & ~7u) | 2u; }";
                        exit = "if (c.m_IP != " + hex(next) + " && c.m_IP != " + hex(pc) + ") goto dispatch;"; break; // synmovq (an IAC can reinit)
            case 0x645: body = "{ const uint32_t t1 = " + s1(in) + ", t2 = " + s2(in) + "; " + d + " = AC; AC = (AC & ~t1) | (t2 & t1); }"; break; // modac
            case 0x655: body = "{ const uint32_t t1 = c.m_PC, t2 = " + s2(in) + "; c.m_PC = (c.m_PC & ~t2) | (" + d + " & t2); " + d +
                               " = t1; c.m_IP = " + hex(next) + "; if ((t1 >> 16 & 0x1f) > (c.m_PC >> 16 & 0x1f)) c.check_pending_irqs(); }";
                        exit = "if (c.m_IP != " + hex(next) + ") goto dispatch;"; break; // modpc
            case 0x66d: // flushreg, exact cache writes from I960Ref::execute_op
                body = "if (c.m_rcache_pos > 4) c.m_rcache_pos = 4; "
                       "for (uint32_t f = 0; f < uint32_t(c.m_rcache_pos); ++f) "
                       "for (unsigned i = 0; i < 16; ++i) "
                       "c.bus->write_dword(c.m_rcache_frame_addr[f] + i * sizeof(uint32_t), c.m_rcache[f][i]); "
                       "c.m_rcache_pos = 0;";
                break;
            case 0x670: body = "{ const uint64_t p = uint64_t(" + s1(in) + ") * uint64_t(" + s2(in) + "); " + d + " = uint32_t(p); " +
                               R(in.dst + 1) + " = uint32_t(p >> 32); }"; break; // emul
            case 0x671: {                                                  // ediv
                const std::string src2 = in.m2 ? "uint64_t(" + lit(in.src2) + ")"
                                               : "(uint64_t(" + R(in.src2) + ") | (uint64_t(" + R(in.src2 + 1) + ") << 32))";
                body = "{ const uint64_t a = " + s1(in) + ", b = " + src2 + "; const uint32_t rem = uint32_t(b % a), quo = uint32_t(b / a); " +
                       d + " = rem; " + R(in.dst + 1) + " = quo; }";
                break;
            }
            case 0x674: case 0x675: // cvtir/cvtilr: integer src1, real destination
                if (!real_dest("double(int32_t(" + s1(in) + "))", code == 0x675)) return unsupported(in);
                break;
            case 0x676: case 0x677: { // scalerl/scaler
                const bool wide = code == 0x676;
                if (!real_dest(fr2(wide) + " * std::pow(2.0, double(int32_t(" + s1(in) + ")))", wide)) return unsupported(in);
                break;
            }
            case 0x685: body = "AC = (AC & ~7u) | gen::cc_d(" + fr1() + ", " + fr2() + ");"; break;
            case 0x695: body = "AC = (AC & ~7u) | gen::cc_d(" + fr1(true) + ", " + fr2(true) + ");"; break;
            case 0x680: if (!real_dest("std::atan2(" + fr2() + ", " + fr1() + ")")) return unsupported(in); break;
            case 0x683: if (!real_dest("std::fmod(" + fr2() + ", " + fr1() + ")")) return unsupported(in); break;
            case 0x688: if (!real_dest("std::sqrt(" + fr1() + ")")) return unsupported(in); break;
            case 0x68a: if (!real_dest("std::logb(" + fr1() + ")")) return unsupported(in); break;
            case 0x68c: if (!real_dest("std::sin(" + fr1() + ")")) return unsupported(in); break;
            case 0x6c0: set("uint32_t(int32_t(gen::round_to_int(" + fr1() + ", AC)))"); break;
            case 0x6c2: set("uint32_t(int32_t(" + fr1() + "))"); break;
            case 0x6c3: // cvtzril: MAME reads a single real and writes two adjacent integer registers
                if (in.m3 || in.dst == 31) return unsupported(in);
                body = "{ const uint64_t v = uint64_t(int64_t(" + fr1() + ")); " + d + " = uint32_t(v); "
                       + R(in.dst + 1) + " = uint32_t(v >> 32); }";
                break;
            case 0x6c9: case 0x6d9: {
                const bool wide = code == 0x6d9;
                if (!real_dest(fr1(wide), wide)) return unsupported(in);
                break;
            }
            case 0x6e2: // cpysre uses long-real helpers in the inherited reference
                if (!real_dest("(" + fr2(true) + " >= 0.0 ? std::fabs(" + fr1(true) + ") : -std::fabs(" + fr1(true) + "))", true)) return unsupported(in);
                break;
            case 0x701: two("t2 * t1"); break;                             // mulo
            case 0x708: two("t2 % t1"); break;                             // remo (MAME: undefined on 0)
            case 0x70b: two("t1 == 0 ? 0u : t2 / t1"); break;              // divo (MAME: 0 on divide by zero)
            case 0x741: two("uint32_t(int32_t(t2) * int32_t(t1))"); break; // muli
            case 0x748: two("uint32_t(int32_t(t2) % int32_t(t1))"); break; // remi
            case 0x749: two("uint32_t([](int32_t a, int32_t b) { int32_t r = a - (a / b) * b; "
                            "if ((b ^ a) < 0 && r != 0) r += b; return r; }(int32_t(t2), int32_t(t1)))"); break; // modi (sign of src1)
            case 0x74b: two("uint32_t(int32_t(t2) / int32_t(t1))"); break; // divi
            case 0x78b: case 0x78c: case 0x78d: case 0x78f:
            case 0x79b: case 0x79c: case 0x79d: case 0x79f: {
                const bool wide = code >= 0x790;
                const char *op = (code & 15) == 0xb ? " / " : (code & 15) == 0xc ? " * " : (code & 15) == 0xd ? " - " : " + ";
                if (!real_dest("(" + fr2(wide) + op + fr1(wide) + ")", wide)) return unsupported(in);
                break;
            }
            default: return unsupported(in);
            }
            break;
        }
        default: return unsupported(in);
        }

        ++native;
        exits = !exit.empty();
        // --fast_inaccuracy: inside a block the IP is not stored; an instruction that
        // compares the IP with its own address (synmovq's IAC check) gets it.
        if (!leader && exit.find("m_IP != " + hex(pc)) != std::string::npos) line("c.m_IP = " + hex(pc) + ";");
        if (body.find("cond") != std::string::npos) line("{ bool cond;");
        if (!body.empty()) line(body);
        if (!blocks) line("++ls.count;");
        else if (block_end) line(block_end == 1 ? "++ls.count;" : "ls.count += " + std::to_string(block_end) + "u;");
        if (!exit.empty()) line(exit);
        if (body.find("cond") != std::string::npos) line("}");
        return falls;
    }

    // No native template: recorded, and the recompile fails.
    bool unsupported(const Insn &in) {
        char b[128];
        std::snprintf(b, sizeof b, "%08x: %s", in.addr, i960::format_mame(in).c_str());
        missing->push_back(b);
        return false;
    }
};

std::vector<uint8_t> load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "m2recomp: cannot open %s\n", path.c_str());
        std::exit(2);
    }
    return {std::istreambuf_iterator<char>(f), {}};
}

void load_hooks(const std::string &path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "m2recomp: cannot open %s\n", path.c_str());
        std::exit(2);
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string addr, name;
        if (ls >> addr >> name) g_hooks[uint32_t(std::strtoul(addr.c_str(), nullptr, 16))] = name;
    }
}

std::vector<uint32_t> load_seeds(const std::string &path) {
    std::ifstream f(path);
    std::vector<uint32_t> v;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream s(line);
        std::string a, b, c;
        s >> a;
        if (s >> b >> c) { // harvest format: kind from target count
            if (a == "icr" || a == "irqat") continue;
            v.push_back(uint32_t(std::strtoul(c.c_str(), nullptr, 16)));
        } else {
            v.push_back(uint32_t(std::strtoul(a.c_str(), nullptr, 16)));
        }
    }
    return v;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m2recomp PROGRAM.bin OUTDIR [--seeds FILE]... [--hooks FILE] [--chunk N] [--fast_inaccuracy]\n"
                             "  [--board model2o|model2a] [--main-data FILE] [--report FILE]\n"
                             "  [--indexed-dispatch] (exact instruction lookup; no timing changes)\n"
                             "  [--relocated-code GUEST:PROGRAM_OFFSET:SIZE]... (Model2A work RAM, guarded)\n");
        return 2;
    }
    const std::vector<uint8_t> img = load(argv[1]);
    const std::string out = argv[2];
    std::string board = "model2o", main_data_path, report_path;
    std::vector<uint32_t> seeds;
    size_t chunk = 1500;
    bool blocks = false, chain = false, indexed_dispatch = false;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--board") && i + 1 < argc) {
            board = argv[++i];
        } else if (!std::strcmp(argv[i], "--main-data") && i + 1 < argc) {
            main_data_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--report") && i + 1 < argc) {
            report_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--relocated-code") && i + 1 < argc) {
            unsigned guest, source, size; char tail;
            if (std::sscanf(argv[++i], "%x:%x:%x%c", &guest, &source, &size, &tail) != 3) {
                std::fprintf(stderr, "m2recomp: invalid relocated range (three hex fields required)\n"); return 2;
            }
            g_relocations.push_back({guest, source, size});
        } else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) {
            auto s = load_seeds(argv[++i]);
            seeds.insert(seeds.end(), s.begin(), s.end());
        } else if (!std::strcmp(argv[i], "--hooks") && i + 1 < argc) {
            load_hooks(argv[++i]);
        } else if (!std::strcmp(argv[i], "--chunk") && i + 1 < argc) {
            chunk = std::strtoul(argv[++i], nullptr, 0);
        } else if (!std::strcmp(argv[i], "--indexed-dispatch")) {
            indexed_dispatch = true;
        } else if (!std::strcmp(argv[i], "--fast_inaccuracy")) {
            blocks = true; // 1. block bookkeeping
            chain = true;  // 2. direct chaining
        } else {
            std::fprintf(stderr, "m2recomp: unknown option %s (--blocks is now --fast_inaccuracy)\n", argv[i]);
            return 2;
        }
    }
    if (indexed_dispatch && blocks) {
        std::fprintf(stderr, "m2recomp: --indexed-dispatch cannot be combined with --fast_inaccuracy\n");
        return 2;
    }
    if (board != "model2o" && board != "model2a") {
        std::fprintf(stderr, "m2recomp: --board must be model2o or model2a\n");
        return 2;
    }
    const std::vector<uint8_t> main_data = main_data_path.empty() ? std::vector<uint8_t>{} : load(main_data_path);
    for (size_t i = 0; i < g_relocations.size(); ++i) {
        const auto &r = g_relocations[i];
        if (board != "model2a" || !r.size || ((r.guest | r.source | r.size) & 3) ||
            r.guest < 0x00500000 || uint64_t(r.guest) + r.size > 0x00600000 ||
            uint64_t(r.source) + r.size > std::min<uint64_t>(img.size(), 0x00200000)) {
            std::fprintf(stderr, "m2recomp: relocated code must be aligned Model2A work RAM backed by program bytes\n"); return 2;
        }
        for (size_t j = 0; j < i; ++j) {
            const auto &p = g_relocations[j];
            if (r.guest < uint64_t(p.guest) + p.size && p.guest < uint64_t(r.guest) + r.size) {
                std::fprintf(stderr, "m2recomp: overlapping relocated code ranges\n"); return 2;
            }
        }
    }

    // Model2A has RAM, not Daytona's program-ROM alias, at 0x00220000.
    // Main-data ROM follows model2_base_mem; only mapped bytes can seed code.
    auto read = [&](uint32_t a) -> std::optional<uint32_t> {
        uint64_t off;
        const std::vector<uint8_t> *region = &img;
        if (a < 0x00200000 && a < img.size()) off = a;
        else if (board == "model2o" && a >= 0x220000 && a < 0x240000) off = a - 0x200000;
        else if (a >= 0x02000000 && a < 0x04000000) { off = a - 0x02000000; region = &main_data; }
        else if (a >= 0x06000000 && a < 0x07000000) { off = a - 0x06000000 + 0x01000000; region = &main_data; }
        else {
            bool found = false;
            for (const auto &r : g_relocations) {
                if (a >= r.guest && uint64_t(a) + 4 <= uint64_t(r.guest) + r.size) {
                    off = uint64_t(a) - r.guest + r.source; found = true; break;
                }
            }
            if (!found) return std::nullopt;
        }
        if (off + 4 > region->size()) return std::nullopt;
        uint32_t v;
        std::memcpy(&v, &(*region)[off], 4);
        return v;
    };
    std::vector<uint32_t> all = i960::boot_seeds(read).all();
    all.insert(all.end(), seeds.begin(), seeds.end());
    const i960::ReachResult r = i960::reach(all, read, !g_relocations.empty());

    std::vector<const Insn *> insns;
    for (const auto &[a, in] : r.insns) insns.push_back(&in);

    // --fast_inaccuracy: block starts. An analysis pass over every instruction records
    // the direct transfer targets and the instructions that end a block.
    std::set<uint32_t> leaders;
    if (blocks) {
        std::set<uint32_t> targets, none;
        std::vector<std::string> ignored; // reported by the real pass
        for (const Insn *in : insns) {
            Emitter dry{&none, {}, 0, &ignored};
            dry.targets = &targets;
            if (!dry.emit(*in) || dry.exits) leaders.insert(in->addr + in->length);
        }
        leaders.insert(targets.begin(), targets.end());
        leaders.insert(all.begin(), all.end());
        for (const auto &h : g_hooks) leaders.insert(h.first);
    }
    size_t block_count = 0;

    uint64_t native = 0;
    std::vector<std::string> missing;
    std::vector<std::pair<uint32_t, uint32_t>> ranges; // per chunk: first, last address
    for (size_t ci = 0, start = 0; start < insns.size(); ++ci, start += chunk) {
        const size_t end = std::min(insns.size(), start + chunk);
        std::set<uint32_t> addrs;
        for (size_t k = start; k < end; ++k) addrs.insert(insns[k]->addr);
        Emitter em{&addrs, {}, 0, &missing};
        em.blocks = blocks;
        // Block structure (every instruction is its own block without --fast_inaccuracy):
        // lead[i] starts a block, pos[i] = instructions before it in its block,
        // len[i] = its block's length.
        const size_t n = end - start;
        std::vector<char> lead(n, 1);
        std::vector<unsigned> pos(n, 0), len(n, 1);
        if (blocks) {
            for (size_t i = 1; i < n; ++i) {
                const Insn &prev = *insns[start + i - 1], &in = *insns[start + i];
                lead[i] = leaders.count(in.addr) || prev.addr + prev.length != in.addr;
            }
            for (size_t i = 0, b = 0; i < n; ++i) {
                if (lead[i]) b = i, ++block_count;
                pos[i] = unsigned(i - b);
            }
            for (size_t i = n; i-- > 0;) len[i] = i + 1 < n && !lead[i + 1] ? len[i + 1] : pos[i] + 1;
        }
        char name[32];
        std::snprintf(name, sizeof name, "chunk_%03zu", ci);
        em.o << "// Generated by m2recomp. Derived from the game: never commit.\n";
        if (blocks) em.o << "// --fast_inaccuracy: interrupt/event checks and the instruction count once per basic block.\n";
        em.o << "#include \"runtime/gen_support.h\"\n\n" << "namespace gen {\n\n";
        if (blocks) em.o << kBlockBoundary;
        em.o << "void " << name << "(Env &e) {\n"
             << "    rt::Cpu &c = e.c;\n    rt::Lockstep &ls = e.ls;\n"
             << "    uint32_t *const R = c.m_r;\n    uint32_t &AC = c.m_AC;\n"
             << "dispatch:\n    if (ls.finished()) return;\n    switch (c.m_IP) {\n";
        for (size_t i = 0; i < n; ++i) {
            const uint32_t a = insns[start + i]->addr;
            em.o << "    case " << hex(a) << ": ";
            // Entered mid-block: the block's count is added at its end.
            if (!lead[i]) em.o << "ls.count -= " << pos[i] << "u; ";
            em.o << "goto " << lbl(a) << ";\n";
        }
        em.o << "    default: return;\n    }\n";
        for (size_t k = start; k < end; ++k) {
            const Insn &in = *insns[k];
            const size_t i = k - start;
            em.leader = lead[i] != 0;
            em.block_end = blocks && (i + 1 == n || lead[i + 1]) ? len[i] : 0;
            const bool falls = em.emit(in);
            const uint32_t next = in.addr + in.length;
            const bool next_is_following = k + 1 < end && insns[k + 1]->addr == next;
            if (falls && !next_is_following) em.line("{ c.m_IP = " + hex(next) + "; goto dispatch; }");
        }
        em.o << "}\n\n} // namespace gen\n";
        std::ofstream(out + "/" + name + ".cpp") << em.o.str();
        native += em.native;
        ranges.emplace_back(insns[start]->addr, insns[end - 1]->addr);
    }

    // Dispatch table and instruction index.
    std::ostringstream t;
    t << "// Generated by m2recomp. Derived from the game: never commit.\n";
    if (blocks) t << "// --fast_inaccuracy: interrupt/event checks and the instruction count once per basic block.\n";
    if (chain) t << "// --fast_inaccuracy: gen::run chains chunks itself.\n";
    t << "#include \"runtime/gen_support.h\"\n\n"
      << "#include <algorithm>\n\nnamespace gen {\n\n";
    for (size_t i = 0; i < ranges.size(); ++i) t << "void chunk_" << (i < 10 ? "00" : i < 100 ? "0" : "") << i << "(Env &);\n";
    if (indexed_dispatch) {
        // A zero entry means no compiled instruction. Page and word lookup
        // replace only the address search and chunk selection; the exact same
        // instruction bodies, live guards and boundary checks remain intact.
        std::map<uint32_t, std::vector<uint32_t>> pages;
        for (size_t i = 0; i < insns.size(); ++i) {
            const uint32_t address = insns[i]->addr;
            if (address & 3) {
                std::fprintf(stderr, "m2recomp: indexed dispatch requires aligned instruction addresses\n");
                return 2;
            }
            auto &page = pages[address >> 12];
            if (page.empty()) page.resize(1024);
            page[(address & 4095) >> 2] = uint32_t(i / chunk + 1);
        }
        if (pages.empty()) {
            t << "bool has_code(uint32_t) { return false; }\nvoid run(Env &) {}\n";
        } else {
            const uint32_t first_page = pages.begin()->first, last_page = pages.rbegin()->first;
            t << "\nnamespace {\nconst uint32_t kInstructionPages[][1024] = {\n";
            for (const auto &[number, entries] : pages) {
                (void)number;
                t << "    {";
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (i % 32 == 0) t << "\n        ";
                    t << entries[i] << ',';
                }
                t << "\n    },\n";
            }
            t << "};\nconst uint32_t kPages[] = {\n";
            uint32_t index = 0;
            for (uint32_t number = first_page; number <= last_page; ++number) {
                if ((number - first_page) % 32 == 0) t << "    ";
                t << (pages.contains(number) ? ++index : 0) << ',';
                if ((number - first_page) % 32 == 31) t << '\n';
            }
            t << "\n};\nvoid (*const kFunctions[])(Env &) = {\n";
            for (size_t i = 0; i < ranges.size(); ++i) {
                char name[32];std::snprintf(name,sizeof name,"chunk_%03zu",i);
                t << "    " << name << ",\n";
            }
            t << "};\nuint32_t chunk_id(uint32_t address) {\n"
              << "    if (address & 3u) return 0;\n"
              << "    const uint32_t page = address >> 12;\n"
              << "    if (page < " << first_page << "u || page > " << last_page << "u) return 0;\n"
              << "    const uint32_t index = kPages[page - " << first_page << "u];\n"
              << "    return index ? kInstructionPages[index - 1][(address & 4095u) >> 2] : 0;\n"
              << "}\n} // namespace\n\n"
              << "bool has_code(uint32_t address) { return chunk_id(address) != 0; }\n"
              << "void run(Env &e) {\n"
              << "    const uint32_t id = chunk_id(e.c.m_IP);\n"
              << "    if (id) kFunctions[id - 1](e);\n"
              << "}\n\n";
        }
    } else {
    t << "\nnamespace {\nconst uint32_t kAddrs[] = {\n";
    for (const Insn *in : insns) t << "    " << hex(in->addr) << ",\n";
    t << "};\nstruct Range { uint32_t first, last; void (*fn)(Env &); };\nconst Range kChunks[] = {\n";
    for (size_t i = 0; i < ranges.size(); ++i) {
        char name[32];
        std::snprintf(name, sizeof name, "chunk_%03zu", i);
        t << "    {" << hex(ranges[i].first) << ", " << hex(ranges[i].second) << ", " << name << "},\n";
    }
    t << "};\n} // namespace\n\n"
      << "bool has_code(uint32_t a) { return std::binary_search(std::begin(kAddrs), std::end(kAddrs), a); }\n\n";
    if (chain) {
        // A chunk returns only when the lockstep is finished or m_IP is not
        // in its switch. Back in the same chunk means no code there: return,
        // and GameLoop's has_code reports the address as before.
        t << "namespace {\n"
          << "const Range *chunk_of(uint32_t a) {\n"
          << "    const Range *r = std::upper_bound(std::begin(kChunks), std::end(kChunks), a,\n"
          << "                                      [](uint32_t x, const Range &c) { return x < c.first; });\n"
          << "    if (r == std::begin(kChunks)) return nullptr;\n"
          << "    --r;\n"
          << "    return a <= r->last ? r : nullptr;\n"
          << "}\n"
          << "} // namespace\n\n"
          << "void run(Env &e) {\n"
          << "    const Range *prev = nullptr;\n"
          << "    do {\n"
          << "        const Range *r = chunk_of(e.c.m_IP);\n"
          << "        if (!r || r == prev) return;\n"
          << "        r->fn(e);\n"
          << "        prev = r;\n"
          << "    } while (!e.ls.finished());\n"
          << "}\n\n";
    } else {
        t << "void run(Env &e) {\n    const uint32_t a = e.c.m_IP;\n"
          << "    for (const Range &r : kChunks)\n        if (a >= r.first && a <= r.last) { r.fn(e); return; }\n}\n\n";
    }
    }
    t
      << "uint64_t native_instructions() { return " << native << "ull; }\n\n} // namespace gen\n";
    if (!report_path.empty()) {
        std::ofstream report(report_path);
        auto addresses = [&](const char *name, const auto &values) {
            report << ",\n  " << std::quoted(name) << ": [";
            bool first = true;
            for (uint32_t a : values) {
                char label[16]; std::snprintf(label, sizeof label, "0x%08x", a);
                if (!first) report << ", ";
                report << std::quoted(label); first = false;
            }
            report << "]";
        };
        report << "{\n  \"schema\": 1,\n  \"board\": " << std::quoted(board)
               << ",\n  \"reference_cycle_accounting\": true"
               << ",\n  \"reference_cycle_accounting_version\": 1"
               << ",\n  \"instruction_count\": " << insns.size()
               << ",\n  \"instruction_bytes\": ";
        uint64_t instruction_bytes = 0;
        std::vector<uint32_t> instruction_addresses;
        for (const Insn *in : insns) { instruction_bytes += in->length; instruction_addresses.push_back(in->addr); }
        report << instruction_bytes << ",\n  \"native_template_count\": " << native
               << ",\n  \"missing_template_count\": " << missing.size()
               << ",\n  \"chunk_count\": " << ranges.size()
               << ",\n  \"complete_game_coverage\": false";
        if (indexed_dispatch) report << ",\n  \"indexed_dispatch\": true";
        report << ",\n  \"relocated_code\": [";
        for (size_t i = 0; i < g_relocations.size(); ++i) {
            const auto &r = g_relocations[i];
            if (i) report << ", ";
            report << "{\"guest\": " << r.guest << ", \"program_offset\": " << r.source
                   << ", \"size\": " << r.size << ", \"runtime_word_guard\": true}";
        }
        report << "]";
        addresses("seeds", all);
        addresses("instruction_addresses", instruction_addresses);
        addresses("indirect_sites", r.indirect_sites);
        addresses("invalid_stops", r.stops);
        addresses("unmapped_targets", r.unmapped_targets);
        report << "\n}\n";
        if (!report) { std::fprintf(stderr, "m2recomp: could not write report %s\n", report_path.c_str()); return 1; }
    }
    if (!missing.empty()) {
        std::fprintf(stderr, "m2recomp: FAILED: %zu reachable instructions have no native template:\n", missing.size());
        for (const std::string &m : missing) std::fprintf(stderr, "  %s\n", m.c_str());
        return 1;
    }
    std::ofstream(out + "/gen_table.cpp") << t.str();

    std::printf("m2recomp: %zu instructions from %zu seeds -> %zu chunks in %s, all native; %zu indirect sites\n",
                insns.size(), all.size(), ranges.size(), out.c_str(), r.indirect_sites.size());
    if (blocks)
        std::printf("m2recomp: --fast_inaccuracy: %zu basic blocks, %.2f instructions per block (not interrupt-exact), "
                    "direct chunk chaining\n",
                    block_count, block_count ? double(insns.size()) / double(block_count) : 0.0);
    return 0;
}
