#!/usr/bin/env python3
"""Compare newly needed native templates against the reference instruction core.

All instruction bytes are synthetic fixtures assembled here; generated sources
and executable outputs stay below ignored build/validation.
"""
import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/validation/native-template-parity"


def reg(code, a, b, dest, m1=0, m2=0, m3=0):
    return ((code >> 4) << 24) | ((code & 15) << 7) | a | (b << 14) | (dest << 19) | (m1 << 11) | (m2 << 12) | (m3 << 13)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--recompiler", type=Path,
                    help="Use this explicit generator; by default compile a fresh test-local generator")
    ap.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    args = ap.parse_args()
    compiler = shlex.split(args.compiler)
    OUT.mkdir(parents=True, exist_ok=True)
    generator = args.recompiler.resolve() if args.recompiler else OUT / "m2recomp"
    if not args.recompiler:
        generator = OUT / "m2recomp"
        subprocess.run(compiler + ["-std=c++20", "-O1", "-Isrc", "tools/m2recomp/main.cpp", "src/i960/isa.cpp", "src/i960/decode.cpp", "src/i960/reach.cpp", "src/i960/timing.cpp", "-o", str(generator)], cwd=ROOT, check=True)
    cases = []
    src1 = [(16, 0), (17, 0), (0, 1), (3, 1), (22, 1), (31, 1)]
    src2 = [(18, 0), (19, 0), (1, 1), (2, 1), (22, 1), (31, 1)]
    dests = [(24, 0), (25, 0), (0, 1), (2, 1)]
    for code in [0x78b, 0x78c, 0x78d, 0x78f, 0x79b, 0x79c, 0x79d, 0x79f, 0x680, 0x683, 0x6e2]:
        for a, m1 in src1:
            for b, m2 in src2:
                for d, m3 in dests:
                    cases.append(reg(code, a, b, d, m1, m2, m3))
    for code in [0x6c9, 0x6d9, 0x688, 0x68a, 0x68c]:
        for a, m1 in src1:
            for d, m3 in dests:
                cases.append(reg(code, a, 0, d, m1, 0, m3))
    for code in [0x674, 0x675]:
        for a, m1 in [(16, 0), (17, 0), (2, 1), (31, 1)]:
            for d, m3 in dests:
                cases.append(reg(code, a, 0, d, m1, 0, m3))
    for a, m1 in src1:
        for d in [24, 25]:
            cases.append(reg(0x6c3, a, 0, d, m1))
    cases.append(reg(0x66d, 0, 0, 0))
    for a, m1 in src1:
        for b, m2 in src2:
            cases.append(reg(0x695, a, b, 0, m1, m2))
    for a, m1 in [(20, 0), (21, 0), (31, 1)]:
        for b, m2 in [(22, 0), (23, 0), (31, 1)]:
            for d in [20, 22, 24]:
                cases.append(reg(0x5b0, a, b, d, m1, m2))
    image = bytearray(0x10000)
    for addr, value in [(0, 0x100), (4, 0x200), (12, 0x1000), (0x214, 0x300), (0x198, 0x800)]:
        struct.pack_into("<I", image, addr, value)
    addresses = []
    for i, word in enumerate(cases):
        address = 0x1000 + i * 8
        addresses.append(address)
        struct.pack_into("<II", image, address, word, 0x08000000)  # operation, b .
    (OUT / "synthetic.bin").write_bytes(image)
    (OUT / "seeds.txt").write_text("".join(f"{address:x}\n" for address in addresses))
    gen = OUT / "gen"
    gen.mkdir(exist_ok=True)
    for stale in gen.glob("*.cpp"):
        stale.unlink()
    subprocess.run([str(generator), str(OUT / "synthetic.bin"), str(gen), "--board", "model2a", "--seeds", str(OUT / "seeds.txt")], cwd=ROOT, check=True)
    harness = r'''
#include "refcore/i960_ref.h"
#include "runtime/gen_support.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <vector>

// No IRQ/device events in single-instruction fixtures. The real generated
// boundary call still enforces the one-instruction budget.
namespace rt {
Lockstep::Lockstep(Cpu &c) : core_(c) {}
bool Lockstep::apply() { return finished(); }
}
struct Memory : rt::Bus {
    std::vector<uint8_t> bytes;
    explicit Memory(const std::vector<uint8_t>& image): bytes(image) {}
    uint8_t read_byte(uint32_t a) override { return bytes.at(a); }
    uint16_t read_word(uint32_t a) override { return read_byte(a)|uint16_t(read_byte(a+1))<<8; }
    uint32_t read_dword(uint32_t a) override { return read_word(a)|uint32_t(read_word(a+2))<<16; }
    uint32_t fetch(uint32_t a) override { return read_dword(a); }
    void write_byte(uint32_t a,uint8_t v) override { bytes.at(a)=v; }
    void write_word(uint32_t a,uint16_t v) override { write_byte(a,v);write_byte(a+1,v>>8); }
    void write_dword(uint32_t a,uint32_t v) override { write_word(a,v);write_word(a+2,v>>16); }
};
static void seed(rt::Cpu &c,unsigned scenario,uint32_t ip) {
    c.m_IP=ip;c.m_PC=0x001f2002;c.m_AC=((scenario&3u)<<30)|(scenario&7u);c.m_ICR=0xff000000;
    for(unsigned i=0;i<32;i++) c.m_r[i]=0x12340000u+i;
    const double a[]={8.25,-2.5,0.,-0.,1.e-30,1.e30,0.5,3.141592653589793};
    const double b[]={3.5,2.25,1.,0.,2.e-30,2.e30,-0.5,0.25};
    for(unsigned i=0;i<4;i++) c.m_fp[i]=(i&1)?b[scenario]:a[scenario];
    c.m_r[16]=std::bit_cast<uint32_t>(float(a[scenario]));
    c.m_r[17]=std::bit_cast<uint32_t>(float(b[scenario]));
    c.m_r[18]=std::bit_cast<uint32_t>(float(b[scenario]));
    c.m_r[19]=std::bit_cast<uint32_t>(float(a[scenario]));
    // On alternating scenarios real-register pairs hold doubles, including
    // odd source register selectors which must round down to an even pair.
    if(scenario&1) {
        uint64_t av=std::bit_cast<uint64_t>(a[scenario]),bv=std::bit_cast<uint64_t>(b[scenario]);
        c.m_r[16]=av;c.m_r[17]=av>>32;c.m_r[18]=bv;c.m_r[19]=bv>>32;
    }
    const uint32_t carry_a[]={0xffffffffu,0x7fffffffu,0x80000000u,0u,1u,0xfffffffeu,0x80000001u,0x12345678u};
    const uint32_t carry_b[]={1u,1u,0x80000000u,0xffffffffu,0x7fffffffu,2u,0x80000001u,0x87654321u};
    c.m_r[20]=carry_a[scenario];c.m_r[21]=carry_b[scenario];
    c.m_r[22]=carry_b[scenario];c.m_r[23]=carry_a[scenario];
    c.m_rcache_pos=scenario;
    for(unsigned f=0;f<4;f++) {
        c.m_rcache_frame_addr[f]=0x8000+64*f;
        for(unsigned r=0;r<16;r++) c.m_rcache[f][r]=0xabc00000+f*16+r;
    }
}
int main() {
    const std::array<uint32_t,CASE_COUNT> addresses={ADDRESS_LIST};
    const std::array<uint32_t,CASE_COUNT> words={WORD_LIST};
    std::vector<uint8_t> image(0x10000);
    for(unsigned i=0;i<addresses.size();i++) {
        uint32_t branch=0x08000000;
        std::memcpy(image.data()+addresses[i],&words[i],4);
        std::memcpy(image.data()+addresses[i]+4,&branch,4);
    }
    unsigned checks=0;
    for(unsigned i=0;i<addresses.size();i++) for(unsigned scenario=0;scenario<8;scenario++) {
        Memory refmem(image),native_mem(image);
        rt::I960Ref ref(&refmem);rt::Cpu native(&native_mem);
        seed(ref,scenario,addresses[i]);seed(native,scenario,addresses[i]);
        rt::Lockstep ls(native);ls.end_count=1;
        gen::Env env{native,ls};ref.execute_one();
        while(!ls.finished()) gen::run(env);
        if(std::memcmp(ref.m_r,native.m_r,sizeof(ref.m_r)) ||
           std::memcmp(ref.m_fp,native.m_fp,sizeof(ref.m_fp)) ||
           ref.m_AC!=native.m_AC || ref.m_IP!=native.m_IP || ref.reference_cycles!=native.reference_cycles ||
           ref.m_rcache_pos!=native.m_rcache_pos || refmem.bytes!=native_mem.bytes) {
            std::fprintf(stderr,"Parity mismatch opcode=%08x scenario=%u at=%08x\n",words[i],scenario,addresses[i]);
            for(unsigned r=0;r<32;r++) if(ref.m_r[r]!=native.m_r[r]) std::fprintf(stderr," R%u: ref=%08x native=%08x\n",r,ref.m_r[r],native.m_r[r]);
            return 1;
        }
        checks++;
    }
    std::printf("Native templates: %u opcode/mode fixtures x 8 states = %u parity checks passed\n",unsigned(addresses.size()),checks);
}
'''
    harness = harness.replace("CASE_COUNT", str(len(cases))).replace("ADDRESS_LIST", ",".join(f"0x{address:x}" for address in addresses)).replace("WORD_LIST", ",".join(f"0x{word:08x}" for word in cases))
    (OUT / "parity.cpp").write_text(harness)
    executable = OUT / "parity"
    subprocess.run(compiler + ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc", "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp", str(OUT / "parity.cpp")] + [str(source) for source in sorted(gen.glob("*.cpp"))] + ["-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
