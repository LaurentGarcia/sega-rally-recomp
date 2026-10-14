#!/usr/bin/env python3
"""Execute synthetic relocated native branches/loads and reject modified RAM.

No game instructions are included. Generated files stay in ignored build/.
"""
import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recompiler", type=Path, default=ROOT / "build/segarally-relocated/tools/m2recomp")
    args = parser.parse_args()
    out = ROOT / "build/validation/relocation-execution"
    out.mkdir(parents=True, exist_ok=True)
    image = bytearray(0x100)
    # b +8; lda IP+8+16,g0; ld IP+8+8,g1; b .; data
    for address, word in [(12, 0x40), (0x40, 0x08000008), (0x48, 0x8c801400),
                          (0x4c, 16), (0x50, 0x90881400), (0x54, 8),
                          (0x58, 0x08000000), (0x60, 0xabcdef12)]:
        struct.pack_into("<I", image, address, word)
    (out / "fixture.bin").write_bytes(image)
    (out / "seeds.txt").write_text("5a0000\n")
    gen = out / "gen"
    gen.mkdir(exist_ok=True)
    for stale in gen.glob("*.cpp"):
        stale.unlink()
    subprocess.run([str(args.recompiler.resolve()), str(out / "fixture.bin"), str(gen),
                    "--board", "model2a", "--relocated-code", "5a0000:40:20",
                    "--seeds", str(out / "seeds.txt")], cwd=ROOT, check=True)
    source = r'''
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
#include <cstdio>
#include <cstring>
#include <vector>
namespace rt {
Lockstep::Lockstep(Cpu& c):core_(c) {}
bool Lockstep::apply() { return finished(); }
}
int main() {
    for(unsigned mode=0;mode<4;mode++) {
        rt::Model2ABus bus({},{});
        for(auto [address,word]:std::vector<std::pair<uint32_t,uint32_t>>{
            {0x5a0000,0x08000008},{0x5a0008,0x8c801400},{0x5a000c,16},
            {0x5a0010,0x90881400},{0x5a0014,8},{0x5a0018,0x08000000},
            {0x5a0020,0xabcdef12}}) bus.write_dword(address,word);
        rt::Cpu c(&bus);c.m_IP=0x5a0000;
        rt::Lockstep ls(c);ls.end_count=3;gen::Env e{c,ls};
        if(mode) {
            c.m_IP=mode==3?0x5a0010:0x5a0008;
            const uint32_t changed=mode==1?0x5a0008:mode==2?0x5a000c:0x5a0014;
            bus.write_dword(changed,bus.read_dword(changed)^1);
            try { gen::run(e);std::fprintf(stderr,"Modified instruction accepted\n");return 1; }
            catch(const rt::Fatal& f) {
                if(std::strstr(f.what(),"relocated code differs")==nullptr || ls.count!=0 ||
                   c.m_r[16]!=0 || c.m_r[17]!=0) return 1;
            }
        } else {
            while(!ls.finished()) gen::run(e);
            if(c.m_IP!=0x5a0018 || c.m_r[16]!=0x5a0020 || c.m_r[17]!=0xabcdef12) return 1;
            if(gen::has_code(0x5a0020)) return 1;
        }
    }
    std::puts("PASS relocated native relative branch, IP-relative address/load, first/second-word guards and range boundary");
}
'''
    (out / "execution.cpp").write_text(source)
    executable = out / "execution"
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-O1", "-Isrc",
        "src/runtime/model2a_bus.cpp", "src/runtime/cpu.cpp", str(out / "execution.cpp")]
        + list(map(str, sorted(gen.glob("*.cpp")))) + ["-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
