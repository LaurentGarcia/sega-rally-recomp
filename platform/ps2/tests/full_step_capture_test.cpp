#include "full_step_capture.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Memory final:rt::Bus {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x600000);
    unsigned fetches=0,flags_count=0,devices=0,reads=0,writes=0;
    std::uint32_t fetch(std::uint32_t) override { ++fetches;return 0xabcdef01; }
    std::uint32_t read(std::uint32_t a,unsigned n) {
        ++reads;if (a==0x840000) { ++devices;return 0x76543210; }
        if (a==0xdead0000) throw std::runtime_error("deliberate bus exception");
        if (std::uint64_t(a)+n>bytes.size()) throw std::runtime_error("mock read range");
        std::uint32_t v=0;for (unsigned i=0;i<n;++i) v|=std::uint32_t(bytes[a+i])<<(8*i);return v;
    }
    std::uint8_t read_byte(std::uint32_t a) override { return read(a,1); }
    std::uint16_t read_word(std::uint32_t a) override { return read(a,2); }
    std::uint32_t read_dword(std::uint32_t a) override { return read(a,4); }
    void write(std::uint32_t a,std::uint32_t v,unsigned n) {
        ++writes;if (std::uint64_t(a)+n>bytes.size()) throw std::runtime_error("mock write range");
        for (unsigned i=0;i<n;++i) bytes[a+i]=std::uint8_t(v>>(8*i));
    }
    void write_byte(std::uint32_t a,std::uint8_t v) override { write(a,v,1); }
    void write_word(std::uint32_t a,std::uint16_t v) override { write(a,v,2); }
    void write_dword(std::uint32_t a,std::uint32_t v) override { write(a,v,4); }
    std::uint16_t flags(std::uint32_t) override { ++flags_count;return rt::Cpu::BURST; }
};
void require(bool ok) { if (!ok) throw std::runtime_error("fixture assertion failed"); }
}
int main(int argc,char **argv) {
    try {
        if (argc!=2) throw std::runtime_error("output required");
        Memory memory;ps2::offline::StepCaptureBus bus(memory);rt::Cpu cpu(&bus);bus.bind(cpu);
        require(bus.fetch(0)==0xabcdef01 && memory.fetches==1);
        require(bus.flags(0)==rt::Cpu::BURST && memory.flags_count==1);
        bool threw=false;try { (void)bus.read_dword(0xdead0000); } catch (const std::runtime_error &) { threw=true; }
        require(threw);
        memory.write_dword(0x2140cc,0x500000);memory.write_dword(0x500004,0x5d3f40);
        memory.write_dword(0x500010,0x11223344);
        ps2::offline::begin_full_step_capture(argv[1]);
        for (unsigned step=0;step<2;++step) {
            std::array<std::uint8_t,12> controls{};controls[4]=step?0x60:0x80;
            ps2::offline::full_step_controls(10+step,controls);
            cpu.m_IP=0x5d3f40;cpu.m_r[0]=0x510000;cpu.m_r[31]=0x510040;cpu.m_r[16]=0x500000;
            cpu.m_rcache_pos=1;cpu.m_rcache_frame_addr[0]=0x510000;cpu.m_rcache[0][2]=0x5c9bac;
            const auto start=100+100*step;
            auto boundary=[&](const char *name,unsigned instruction) {
                ps2::offline::full_step_boundary(cpu,name,step+1,0x500000,instruction,10+step,1000+instruction);
            };
            boundary("vehicle_entry",start);
            ps2::offline::full_step_instruction(start+1);cpu.m_IP=0x5d4000;
            require(bus.read_dword(0x500010)==(step?0x55ccee44:0x11223344));
            bus.write_word(0x500011,0xaabb);require(bus.read_dword(0x500010)==(step?0x55aabb44:0x11aabb44));
            require(bus.read_dword(0x840000)==0x76543210);
            { ps2::offline::SuppressStepAccess guard;(void)bus.read_dword(0x500010); }
            cpu.m_IP=0x5d6870;cpu.m_r[0]=0x510040;cpu.m_r[31]=0x510300;
            cpu.m_r[17]=0x510080;cpu.m_r[18]=0x510090;cpu.m_rcache_pos=2;
            cpu.m_rcache_frame_addr[1]=0x510040;cpu.m_rcache[1][2]=0x5d5760;
            boundary("balance_helper_pre",start+2);
            ps2::offline::full_step_instruction(start+3);bus.write_dword(0x214200,step+7);
            cpu.m_IP=0x5d5760;cpu.m_r[31]=0x510040;boundary("balance_helper_post",start+4);
            ps2::offline::full_step_instruction(start+5);cpu.m_IP=0x5d61e0;
            (void)bus.read_dword(cpu.m_IP);
            cpu.m_IP=0x5c9bac;cpu.m_r[31]=0x510000;boundary("vehicle_return",start+6);
            if (!step) {
                ps2::offline::full_step_instruction(start+7);cpu.m_IP=0x5a1234;
                bus.write_word(0x500011,0xccdd);bus.write_byte(0x500011,0xee);bus.write_byte(0x500013,0x55);
            }
        }
        ps2::offline::end_full_step_capture();require(memory.devices==2);
        std::cout<<"FULL_STEP_CAPTURE PASS steps=2 device_reads=2\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
