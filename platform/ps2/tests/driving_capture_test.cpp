// Original synthetic observer fixture; no game program or asset bytes.
#include "driving_capture.h"
#include <cstdio>
#include <stdexcept>
#include <string>

struct Bus final : rt::Bus {
    unsigned reads=0;
    std::uint32_t fetch(std::uint32_t) override { throw std::runtime_error("unexpected fetch"); }
    std::uint8_t read_byte(std::uint32_t) override { throw std::runtime_error("unexpected byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw std::runtime_error("unexpected word read"); }
    std::uint32_t read_dword(std::uint32_t address) override {
        if ((address & 3) || !((address>=0x200000 && address<0x240000) ||
                               (address>=0x500000 && address<0x600000)))
            throw std::runtime_error("observer touched a device or invalid RAM");
        ++reads;
        if (address==0x20a290) return 0x902000; // Deliberately inaccessible staging.
        if (address==0x2100e4) return 0xffffffff; // Descriptor arithmetic must not wrap.
        return address ^ 0x12345678;
    }
    void write_byte(std::uint32_t,std::uint8_t) override { throw std::runtime_error("unexpected write"); }
    void write_word(std::uint32_t,std::uint16_t) override { throw std::runtime_error("unexpected write"); }
    void write_dword(std::uint32_t,std::uint32_t) override { throw std::runtime_error("unexpected write"); }
};

int main(int argc,char **argv) {
    if (argc!=3) return 2;
    try {
        const std::string mode=argv[1];
        Bus bus;rt::Cpu cpu(&bus);
        cpu.m_IP=0x5cbce0;
        ps2::offline::observe_driving_instruction(cpu,0,2300,0);
        if (bus.reads) throw std::runtime_error("disabled capture read memory");
        ps2::offline::begin_driving_capture(argv[2],2200,2800);
        cpu.m_r[16]=mode=="forbidden" ? 0x900000 : 0x210000;
        cpu.m_r[17]=0x220000;cpu.m_r[2]=0x5cafec;cpu.m_r[0]=0x500000;
        cpu.m_r[31]=0x500040;cpu.m_r[15]=cpu.m_r[16];cpu.m_AC=0x3f001000;
        cpu.m_fp[0]=-0.0;
        const std::uint32_t relocation=mode=="rom" ? 0 : 0x59f000;
        const unsigned frame=mode=="unselected" ? 2301 : 2300;
        std::uint64_t instruction=1;
        const auto observe=[&](std::uint32_t pc) {
            cpu.m_IP=pc+relocation;cpu.reference_cycles=instruction*3;
            ps2::offline::observe_driving_instruction(cpu,instruction++,frame,123456);
        };
        if (mode!="forbidden") { cpu.m_r[5]=cpu.m_r[16];observe(0x2bfa4); }
        observe(mode=="alternate" ? 0x2e650 : 0x2cce0);
        const auto reads=bus.reads;
        ps2::offline::observe_driving_instruction(cpu,instruction-1,frame,123456);
        if (reads!=bus.reads) throw std::runtime_error("duplicate instruction re-read memory");
        if (mode=="nested") observe(0x2cce0);
        if (mode=="timeout") {
            instruction+=2000001;observe(0x2ccf0);
        }
        observe(0x2e560);observe(0x2e638);
        observe(0x3110c);observe(0x31154);observe(0x31158);
        cpu.m_r[18]=0x230000;cpu.m_r[2]=relocation+0x2fc30;cpu.m_r[0]=0x500040;
        cpu.m_r[31]=0x500100;observe(0x2abc0);observe(0x2ac08);
        if (mode=="nested-query") observe(0x2abc0);
        if (mode!="unfinished-query") { cpu.m_r[31]=0x500040;observe(0x2fc30); }
        cpu.m_r[2]=0x5cafec;
        if (mode!="incomplete") {
            cpu.m_IP=cpu.m_r[2];cpu.m_r[31]=0x500000;
            ps2::offline::observe_driving_instruction(cpu,instruction++,frame,123456);
        }
        ps2::offline::end_driving_capture();
        std::printf("OBSERVER PASS reads=%u\n",bus.reads);
        return 0;
    } catch(const std::exception &error) {
        std::fprintf(stderr,"%s\n",error.what());return 1;
    }
}
