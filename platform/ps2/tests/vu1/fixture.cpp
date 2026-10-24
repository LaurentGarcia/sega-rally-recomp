// Original isolated VIF1/VU1 numerical test. No GS, IOP reset or game inputs.
#include <tamtypes.h>
#include <kernel.h>
#include <dma.h>
#include <vif_registers.h>
#include "vu0_pack.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const unsigned char fixture_vu1_begin[], fixture_vu1_end[], fixture_vu1_dual[];
extern "C" const unsigned char fixture_vu1_repeat2[];
namespace {
constexpr unsigned base = 16, stride = 504, capacity = 96;
constexpr unsigned input = 8, clip = 104, projected = 200, fixed = 296;
constexpr u32 canary = 0x51a7c39du;
struct alignas(16) Quad { u32 word[4]; };
alignas(64) std::array<Quad, 1024> expected;
alignas(64) std::array<u32, 8192> packet;
unsigned used = 0, checked = 0, runs = 0, transformed_vertices = 0;
u16 sequence = 0;
std::uint64_t hash = 1469598103934665603ull;

u32 ticks() { u32 value; asm volatile("sync\n\tmfc0 %0,$9" : "=r"(value) : : "memory"); return value; }
u32 code(unsigned op, unsigned count = 0, unsigned immediate = 0) {
    return (u32(op) << 24) | (u32(count & 255u) << 16) | (immediate & 65535u);
}
void word(u32 value) { if (used >= packet.size()) __builtin_trap(); packet[used++] = value; }
void words(const void *data, unsigned bytes) {
    if (bytes % 4 || used + bytes / 4 > packet.size()) __builtin_trap();
    std::memcpy(packet.data() + used, data, bytes); used += bytes / 4;
}
void begin() { used = 0; }
void unpack(unsigned address, const Quad *data, unsigned count, bool relative = false) {
    while (count) {
        const unsigned chunk = count > 256 ? 256 : count;
        word(code(0x6c, chunk, address | (relative ? 0x8000 : 0)));
        words(data, chunk * 16); address += chunk; data += chunk; count -= chunk;
    }
}
bool launch() {
    ++sequence;
    word(code(0x10)); // FLUSHE: all VU data writes have completed before MARK.
    word(code(0x07, 0, sequence));
    while (used % 4) word(0);
#ifdef RALLY_VIF_TRAILING_NOP
    // Play 0.72 can strand a waiting final-qword command in its VIF buffer
    // after DMA clears STR. One extra qword retains unread DMA payload for
    // resume. This optional diagnostic is not a hardware requirement. The
    // whole packet, including these NOPs, stays owned until DMA completion.
    for (unsigned i=0;i<4;++i) word(0);
#endif
    SyncDCache(packet.data(), packet.data() + used);
    return dma_channel_send_normal(DMA_CHANNEL_VIF1, packet.data(), used / 4, 0, 0) >= 0;
}
bool finish(const char *stage, u32 start) {
    // Fixed 300M CP0 Count tick bound; no wall-time or emulator-speed claim.
    while (VIF1_MARK != sequence) {
        if (ticks() - start > 300000000u) {
            std::printf("VU1 TIMEOUT stage=%s mark=%08lx want=%u stat=%08lx code=%08lx\n",
                stage, (unsigned long)VIF1_MARK, unsigned(sequence),
                (unsigned long)VIF1_STAT, (unsigned long)VIF1_CODE);
            return false;
        }
    }
    if (dma_channel_wait(DMA_CHANNEL_VIF1, 1000000) < 0) return false;
    if (VIF1_STAT & 0x00003000u) { // ER0/ER1 (tag mismatch/invalid VIFcode).
        std::printf("VU1 VIF ERROR stat=%08lx\n", (unsigned long)VIF1_STAT); return false;
    }
    asm volatile("sync" ::: "memory");
    return true;
}
bool submit(const char *stage) {
    const u32 start = ticks();
    return launch() && finish(stage, start);
}
u32 bits(float value) { return std::bit_cast<u32>(value); }
void set_float(Quad &q, unsigned lane, float value) { q.word[lane] = bits(value); }

void prepare(unsigned bank, unsigned count, unsigned variant) {
    const unsigned at = base + bank * stride;
    auto *p = expected.data() + at;
    for (unsigned q = 0; q < stride; ++q) for (unsigned lane = 0; lane < 4; ++lane)
        p[q].word[lane] = canary ^ ((at + q) * 4 + lane);
    // Integer matrix coefficients /4 and vertex components /16 form an
    // independent exact rational oracle; no host/EE matrix math is reused.
    int m[4][4] = {{4,0,0,0},{0,4,0,0},{0,0,4,0},{0,0,0,4}};
    if (variant % 4 == 1) {
        m[0][0]=2; m[1][1]=8; m[2][2]=4; m[3][0]=5; m[3][1]=-3; m[3][2]=1; m[3][3]=8;
    } else if (variant % 4 == 2) {
        m[0][0]=0; m[0][1]=4; m[1][0]=-4; m[1][1]=0;
        m[2][2]=2; m[3][0]=7; m[3][1]=9; m[3][3]=16;
    } else if (variant % 4 == 3) {
        m[0][0]=2; m[0][1]=1; m[1][0]=-1; m[1][1]=4;
        m[2][0]=1; m[2][2]=8; m[3][0]=-3; m[3][1]=2; m[3][2]=-7;
    }
    for (unsigned col=0;col<4;++col) for(unsigned row=0;row<4;++row) set_float(p[col],row,float(m[col][row])/4.f);
    const int scale[4] = {32,16,4,1}, offset[4] = {2048,2048,0,0};
    for (unsigned lane=0;lane<4;++lane) { set_float(p[4],lane,float(scale[lane])); set_float(p[5],lane,float(offset[lane])); }
    p[6].word[0]=count;
    // Upper 16 bits are clear for ISW's 16-bit integer payload.
    p[7].word[0]=at;
    for (unsigned i=0;i<count;++i) {
        const int v[4] = {int((i*7+variant)%113)-56,int((i*11+3*variant)%97)-48,int((i*5+variant)%79)-39,16};
        int c[4] = {};
        for(unsigned lane=0;lane<4;++lane) {
            set_float(p[input+i],lane,float(v[lane])/16.f);
            for(unsigned col=0;col<4;++col) c[lane]+=m[col][lane]*v[col];
        }
        for(unsigned lane=0;lane<4;++lane) {
            set_float(p[clip+i],lane,float(c[lane])/64.f);
            set_float(p[projected+i],lane,float(c[lane])/float(c[3]));
            const int numerator = (c[lane]*scale[lane]+offset[lane]*c[3])*16;
            p[fixed+i].word[lane]=u32(numerator/c[3]);
        }
    }
}

bool check(const char *stage) {
    // Memory-mapped VU data is read only after FLUSHE+MARK and DMA completion.
    const volatile u32 *memory = reinterpret_cast<const volatile u32 *>(0x1100c000u);
    for(unsigned q=0;q<1024;++q) for(unsigned lane=0;lane<4;++lane) {
        const u32 actual=memory[q*4+lane], want=expected[q].word[lane];
        if(actual!=want) {
            std::printf("VU1 MISMATCH stage=%s qword=%u lane=%u got=%08lx want=%08lx stat=%08lx top=%lu tops=%lu\n",
                stage,q,lane,(unsigned long)actual,(unsigned long)want,(unsigned long)VIF1_STAT,
                (unsigned long)VIF1_TOP,(unsigned long)VIF1_TOPS);
            return false;
        }
        hash=(hash^actual)*1099511628211ull; ++checked;
    }
    return true;
}

void batch(unsigned bank, unsigned count, unsigned variant, bool repeated = false, bool short_repeat = false) {
    prepare(bank,count,variant);
    const unsigned at=base+bank*stride;
    // Seed all expected input and guard words, but poison the output and TOP
    // so the verification cannot pass without VU stores taking place.
    alignas(16) std::array<Quad,stride> seed;
    std::memcpy(seed.data(),expected.data()+at,sizeof(seed));
    seed[7].word[0]=0x0000aaaau;
    for(unsigned i=0;i<count;++i) for(unsigned lane=0;lane<4;++lane) {
        seed[clip+i].word[lane]=0xdead1234;
        seed[projected+i].word[lane]=0xdead5678;
        seed[fixed+i].word[lane]=0xdead9abc;
    }
    unpack(0,seed.data(),stride,true);
    const auto *program=short_repeat ? fixture_vu1_repeat2 : repeated ? fixture_vu1_dual : fixture_vu1_begin;
    const auto entry = (reinterpret_cast<std::uintptr_t>(program) -
        reinterpret_cast<std::uintptr_t>(fixture_vu1_begin))/8;
    word(code(0x14,0,unsigned(entry))); // MSCAL also changes TOP/TOPS ownership.
    ++runs;
    transformed_vertices += count * (short_repeat ? 2u : repeated ? 1024u : 1u);
}

bool run() {
    dma_channel_initialize(DMA_CHANNEL_VIF1,nullptr,0);
    VIF1_FBRST=1; // Local VIF reset only; no IOP or GS reset.
    for(unsigned q=0;q<1024;++q) for(unsigned lane=0;lane<4;++lane)
        expected[q].word[lane]=canary^(q*4+lane);
    begin(); word(code(0x01,0,0x0101)); word(code(0x05)); // STCYCL1:1, STMOD normal.
    unpack(0,expected.data(),1024);
    word(code(0x03,0,base)); word(code(0x02,0,stride));
    const unsigned bytes=unsigned(reinterpret_cast<std::uintptr_t>(fixture_vu1_end)-
        reinterpret_cast<std::uintptr_t>(fixture_vu1_begin));
    if(!bytes || bytes%8 || bytes/8>256) return false;
    while(used%2!=1) word(0); // MPG payload starts on a doubleword boundary.
    word(code(0x4a,bytes/8)); words(fixture_vu1_begin,bytes);
    if(!submit("upload") || !check("upload")) return false;
    std::printf("VU1 MICRO uploaded_bytes=%u instructions=%u base=%u stride=%u\n",bytes,bytes/8,base,stride);
    const unsigned counts[]={0,1,2,3,7,31,32,63,95,96};
    unsigned bank=0;
    for(unsigned pass=0;pass<40;++pass) {
        begin(); batch(bank,counts[pass%10],pass);
        if(!submit("serial") || !check("serial")) return false;
        // TOP is checked through the VU's ISW of XTOP above. Play 0.72 does
        // not implement EE MMIO reads of TOP/TOPS; those are not an oracle.
        if(((VIF1_STAT>>7)&1u)!=(1-bank)) {
            std::printf("VU1 BANK FLAG mismatch bank=%u stat=%08lx\n",
                bank,(unsigned long)VIF1_STAT);
            return false;
        }
        bank=1-bank;
    }
    std::printf("VU1 SERIAL PASS runs=%u checked_words=%u\n",runs,checked);
    // Upload into the alternate bank while the preceding transform runs.
    // MSCAL waits for that transform; each of the two banks is used once
    // between explicit FLUSHE barriers. Never overwrite an active bank.
    for(unsigned pass=0;pass<64;++pass) {
        begin(); batch(0,96,pass); batch(1,96,pass+1);
        if(!submit("overlap") || !check("overlap")) return false;
    }
    return true;
}

#ifdef RALLY_DUAL_VU_TEST
struct WorkState { bool dma, vu1, pending; };
WorkState work_state() {
    u32 vpu;
    // VPU-STAT CCR[2,29] bit8 is VBS1, independent of VU0's VF registers.
    asm volatile("cfc2 %0,$29" : "=r"(vpu) : : "memory");
    const u32 dma = *reinterpret_cast<const volatile u32 *>(0x10009000u);
    return {(dma & 0x100u) != 0, (vpu & 0x100u) != 0, VIF1_MARK != sequence};
}

u32 fixed_oracle(float coordinate) {
    // Independent binary32 significand arithmetic, not the VU/FPU conversion.
    const u32 value = bits(coordinate) & 0x7fffffffu;
    const unsigned exponent = value >> 23;
    if (exponent < 123) return 0;
    return ((value & 0x7fffffu) | 0x800000u) >> (146u - exponent);
}

bool dual_unit_check() {
    alignas(64) std::array<ps2::GsCoordinates,257> coordinates;
    alignas(64) std::array<ps2::GsFixedCoordinates,259> output;
    unsigned components=0, active_before=0, dma_before=0, vu1_before=0;
    unsigned active_after=0, dma_after=0, vu1_after=0, vu1_both=0;
    unsigned pending_before=0, pending_after=0, pending_both=0;
    std::uint64_t vu0_hash=1469598103934665603ull;
    const unsigned counts[]={1,2,3,4,7,16,33,64,128,257};
    if(!ps2::vu0_pack_available()) return false;
    // A two-iteration control exercises the backward outer branch without
    // crossing Play's initial synchronous microprogram-execution budget.
    begin(); batch(0,capacity,600,true,true); batch(1,capacity,601,true,true);
    if(!submit("repeat2-control") || !check("repeat2-control")) return false;
    std::printf("DUAL CONTROL repeat2 PASS\n");
    // Isolate the bounded long entry before introducing VU0 work.
    begin(); batch(0,capacity,500,true); batch(1,capacity,501,true);
    const u32 control_start=ticks();
    if(!launch()) return false;
    const auto control_before=work_state();
    if(!finish("repeat-control",control_start)) {
        const auto after=work_state();
        std::printf("DUAL CONTROL FAIL before_dma=%u before_vu1=%u before_pending=%u after_dma=%u after_vu1=%u after_pending=%u\n",
            unsigned(control_before.dma),unsigned(control_before.vu1),unsigned(control_before.pending),
            unsigned(after.dma),unsigned(after.vu1),unsigned(after.pending));
        return false;
    }
    if(!check("repeat-control")) return false;
    std::printf("DUAL CONTROL PASS: long entry completed without VU0 work\n");
    for(unsigned pass=0;pass<64;++pass) {
        const unsigned count=counts[pass%10];
        for(unsigned i=0;i<count;++i) for(unsigned lane=0;lane<4;++lane) {
            const unsigned boundary=(pass*8191+i*61+lane*13)&65535u;
            const float value=float(boundary)/16.f;
            const u32 encoded=bits(value);
            coordinates[i].xy[lane]=lane==1 ? std::bit_cast<float>(encoded+1) :
                lane==2 ? (encoded ? std::bit_cast<float>(encoded-1) : -0.f) :
                lane==3 ? value+0.03125f : value;
        }
        std::memset(output.data(),0xa5,sizeof output);
        // Only eight submissions use the artificial long entry, each with two
        // fixed 1,024-iteration bank transforms. The numerical oracle is unchanged.
        const bool repeated=pass<8;
        begin(); batch(0,capacity,pass+100,repeated); batch(1,capacity,pass+101,repeated);
        const u32 start=ticks();
        if(!launch()) return false;
        // Source preparation occurs before submission. VU0 runs now, before
        // any VIF1/VU1 completion wait; both units own distinct register banks.
        const auto before=work_state();
        ps2::quantize_gs_xy_vu0(coordinates.data(),output.data()+1,count);
        const auto after=work_state();
        active_before+=before.dma || before.vu1; dma_before+=before.dma; vu1_before+=before.vu1;
        active_after+=after.dma || after.vu1; pending_before+=before.pending; pending_after+=after.pending;
        dma_after+=after.dma; pending_both+=before.pending && after.pending;
        vu1_after+=after.vu1; vu1_both+=before.vu1 && after.vu1;
        if(!finish("dual-unit",start)) return false;
        for(unsigned i=0;i<output.size();++i) for(unsigned lane=0;lane<4;++lane) {
            const bool written=i>0 && i<=count;
            const u32 want=written ? fixed_oracle(coordinates[i-1].xy[lane]) : 0xa5a5a5a5u;
            const u32 actual=output[i].xy[lane];
            if(actual!=want) {
                std::printf("DUAL VU0 MISMATCH pass=%u pair=%u lane=%u got=%08lx want=%08lx\n",
                    pass,i,lane,(unsigned long)actual,(unsigned long)want); return false;
            }
            if(written) { ++components; vu0_hash=(vu0_hash^actual)*1099511628211ull; }
        }
        if(!check("dual-unit")) return false;
    }
    std::printf("DUAL VU samples=64 active_before=%u dma_before=%u vu1_before=%u active_after=%u dma_after=%u pending_before=%u pending_after=%u pending_both=%u\n",
        active_before,dma_before,vu1_before,active_after,dma_after,pending_before,pending_after,pending_both);
    std::printf("DUAL VU busy_snapshots before=%u after=%u both=%u cooperation_repeated_calls=16 repeat_count=1024 requested_vertex_evaluations=%u\n",
        vu1_before,vu1_after,vu1_both,transformed_vertices);
    std::printf("DUAL VU0 components=%u hash=%08lx%08lx\n",components,
        (unsigned long)(vu0_hash>>32),(unsigned long)vu0_hash);
    if(!vu1_both) {
        // Play 0.72 CFC2 VPU-STAT returns architectural zero. Keep missing
        // busy evidence separate from verified numerical preservation.
        std::printf("DUAL VU BUSY OBSERVATION INCONCLUSIVE: no VU1 busy snapshots at both VU0 entry and exit\n");
    } else {
        std::printf("DUAL VU BUSY OBSERVATION RECORDED: snapshots do not prove simultaneous instruction issue\n");
    }
    std::printf("DUAL VU PIPELINE pending_both=%u; incomplete completion fence does not prove simultaneous instruction issue\n",pending_both);
    return true;
}
#endif
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0); setvbuf(stderr,nullptr,_IONBF,0);
    std::printf("VU1 TRANSFORM BEGIN original VIF1 microprogram, no XGKICK\n");
    const bool baseline=run();
    std::printf("VU1 TRANSFORM %s runs=%u checked_words=%u hash=%08lx%08lx\n",
        baseline?"PASS":"FAIL",runs,checked,(unsigned long)(hash>>32),(unsigned long)hash);
#ifdef RALLY_DUAL_VU_TEST
    const bool ok=baseline && dual_unit_check();
    std::printf("DUAL VU NUMERICAL %s total_vu1_runs=%u total_checked_words=%u vu1_hash=%08lx%08lx\n",
        ok?"PASS":"FAIL",runs,checked,(unsigned long)(hash>>32),(unsigned long)hash);
#else
    const bool ok=baseline;
#endif
    SleepThread(); return ok?0:1;
}
