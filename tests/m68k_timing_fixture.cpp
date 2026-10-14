// Generates a synthetic instruction fixture using the production emitter.
// Public hardware opcode patterns are supplied by the independent MAME test.
#define main m2sndrecomp_cli_main
#include "../tools/m2sndrecomp/main.cpp"
#undef main
#include "m68000-timing-reference.h"
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    std::ofstream out(argv[1]);
    out<<"#pragma once\n#include <bit>\n#include \"runtime/snd_gen_support.h\"\n";
    const std::set<uint32_t> addresses;
    std::vector<unsigned> emitted;
    for(unsigned op:reference_opcodes) {
        const auto read=[&](uint32_t address)->std::optional<uint16_t>{return address==0x600100?uint16_t(op):0x0010;};
        m68k::Insn in;
        if(!m68k::decode(0x600100,read,in) || !m68k::timing_plan(in))continue;
        Emitter emitter{&addresses,{},0,{}};emitter.timed=true;
        const bool falls=emitter.emit(in);
        if(!emitter.missing.empty())continue;
        out<<"void native_"<<op<<"(snd::Cpu68k&c) { snd::Sched s(c);s.auto_irq=false;uint32_t*D=c.d,*A=c.a;\n"<<emitter.o.str();
        if(falls)out<<"c.pc="<<0x600100+in.length<<";\n";
        out<<"dispatch: return; }\n";
        emitted.push_back(op);
    }
    out<<"bool native_run(unsigned op,snd::Cpu68k&c){switch(op){\n";
    for(auto op:emitted)out<<"case "<<op<<":native_"<<op<<"(c);return true;\n";
    out<<"default:return false;}}\n";
}
