// Original synthetic observer fixture; no game program or asset bytes.
#include "driving_capture.h"
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

struct Bus final : rt::Bus {
    unsigned reads=0;
    std::map<std::uint32_t,std::uint32_t> values;
    std::uint32_t fetch(std::uint32_t) override { throw std::runtime_error("unexpected fetch"); }
    std::uint8_t read_byte(std::uint32_t) override { throw std::runtime_error("unexpected byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw std::runtime_error("unexpected word read"); }
    std::uint32_t read_dword(std::uint32_t address) override {
        if ((address & 3) || !((address>=0x200000 && address<0x240000) ||
                               (address>=0x500000 && address<0x600000)))
            throw std::runtime_error("observer touched a device or invalid RAM");
        ++reads;
        if (const auto found=values.find(address);found!=values.end()) return found->second;
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
        const bool orientation=mode=="alternate-orientation" || mode.starts_with("orientation-");
        const bool contact=mode=="contact" || mode.starts_with("contact-");
        Bus bus;rt::Cpu cpu(&bus);
        cpu.m_IP=0x5cbce0;
        ps2::offline::observe_driving_instruction(cpu,0,2300,0);
        if (bus.reads) throw std::runtime_error("disabled capture read memory");
        ps2::offline::begin_driving_capture(argv[2],2200,2800);
        cpu.m_r[16]=mode=="forbidden" ? 0x900000 : 0x210000;
        cpu.m_r[17]=0x220000;cpu.m_r[2]=0x5cafec;cpu.m_r[0]=0x500000;
        cpu.m_r[31]=0x500040;cpu.m_r[15]=cpu.m_r[16];cpu.m_AC=0x3f001000;
        cpu.m_fp[0]=-0.0;
        const auto saved_return=[&](std::uint32_t frame,std::uint32_t pc,bool spilled=false) {
            cpu.m_rcache_pos=spilled ? 0 : 1;
            cpu.m_rcache_frame_addr[0]=frame;
            cpu.m_rcache[0][2]=pc;
            bus.values[frame+8]=pc;
        };
        saved_return(cpu.m_r[0]&~63u,cpu.m_r[2],mode=="all-spilled");
        if (mode=="outer-interrupted-entry") cpu.m_r[2]=0x5cbce0;
        const std::uint32_t relocation=mode=="rom" ? 0 : 0x59f000;
        const unsigned frame=mode=="unselected" ? 2301 : 2300;
        std::uint64_t instruction=1;
        const auto observe=[&](std::uint32_t pc) {
            cpu.m_IP=pc+relocation;cpu.reference_cycles=instruction*3;
            ps2::offline::observe_driving_instruction(cpu,instruction++,frame,123456);
        };
        if (mode.starts_with("filter")) {
            bus.values[0x2142c0]=0x3e000000;bus.values[0x202050]=0x12345680;
            observe(0x39a90);
            if (mode=="filter-nested") observe(0x39a90);
        }
        if (mode.starts_with("steering") || mode.starts_with("filter")) {
            cpu.m_r[30]=relocation+0x35100;bus.values[0x202050]=0xabcdef53;
            observe(0x395d8);
            if (mode=="steering-nested") observe(0x395d8);
            cpu.m_r[17]=cpu.m_r[30];cpu.m_r[30]=0;cpu.m_r[16]=0xbe123456;
            if (mode=="steering-wrong-return") cpu.m_r[17]+=4;
            if (mode=="steering-wrong-frame") cpu.m_r[31]+=0x40;
            if (mode!="steering-incomplete") observe(0x39664);
            cpu.m_r[16]=0x210000;cpu.m_r[17]=0x220000;
        }
        if (mode.starts_with("filter")) {
            bus.values[0x2142c0]=0xbe000000;cpu.m_r[16]=0x3e000000;
            if (mode=="filter-wrong-frame") cpu.m_r[31]+=0x40;
            if (mode=="filter-wrong-return") cpu.m_rcache[0][2]+=4;
            if (mode!="filter-incomplete") observe(0x39ae8);
            cpu.m_r[16]=0x210000;
        }
        if (mode!="forbidden") { cpu.m_r[5]=cpu.m_r[16];observe(0x2bfa4); }
        cpu.m_r[20]=relocation+0x34f40;observe(0x2aba8);
        observe(mode=="alternate" || orientation || contact ? 0x2e650 : 0x2cce0);
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
        saved_return(cpu.m_r[0],cpu.m_r[2],mode=="all-spilled");
        if (mode=="query-interrupted-entry") cpu.m_r[2]=relocation+0x2abc0;
        cpu.m_r[31]=0x500100;observe(0x2abc0);observe(0x2ac08);
        if (mode=="nested-query") observe(0x2abc0);
        if (mode!="unfinished-query") { cpu.m_r[31]=0x500040;observe(0x2fc30); }
        cpu.m_r[2]=0x5cafec;
        if (orientation) {
            cpu.m_r[12]=cpu.m_r[16];
            if (mode=="orientation-overflow") cpu.m_r[31]=0xfffffff0;
            if (mode=="orientation-nonram") cpu.m_r[31]=0x900000;
            cpu.m_r[5]=cpu.m_r[31]+0x90;
            if (mode=="orientation-bad-pointer") cpu.m_r[5]+=4;
            if (mode=="orientation-bad-object") cpu.m_r[12]+=0x100;
            const auto candidate=cpu.m_r[5];
            for (unsigned i=0;i<4;++i) bus.values[candidate+4*i]=0x3f000001+i;
            for (unsigned i=0;i<3;++i) bus.values[cpu.m_r[16]+0x38+4*i]=0xbf000001+i;
            if (mode!="orientation-missing-pre") observe(0x2ecb4);
            if (mode=="orientation-duplicate-pre") observe(0x2ecb4);
            if (mode=="orientation-wrong-frame") cpu.m_r[31]+=0x40;
            for (unsigned i=0;i<3;++i) bus.values[candidate+4*i]=0x3e000001+i;
            if (mode!="orientation-missing-fold") observe(0x2ece0);
            for (unsigned i=0;i<3;++i) bus.values[cpu.m_r[16]+0x38+4*i]=0xbe000001+i;
            if (mode!="orientation-incomplete") observe(0x2ee00);
        }
        if (contact) {
            cpu.m_r[16]=0x230000;cpu.m_r[17]=0x500080;
            cpu.m_r[2]=relocation+0x2e8a8;cpu.m_r[0]=0x500040;cpu.m_r[31]=0x500200;
            saved_return(cpu.m_r[0],cpu.m_r[2],mode=="contact-spilled");
            if (mode=="contact-interrupted-entry") cpu.m_r[2]=relocation+0x2acc0;
            if (mode=="contact-cache-mismatch") cpu.m_rcache_frame_addr[0]+=0x40;
            if (mode=="contact-caller-nonram") cpu.m_r[0]=0x900000;
            if (mode=="contact-return-unaligned") cpu.m_rcache[0][2]|=1;
            if (mode=="contact-bad-pointer") cpu.m_r[17]=0x900000;
            if (mode=="contact-bad-frame") cpu.m_r[31]=0x5fffc0;
            for (unsigned i=0;i<12;++i) bus.values[0x500260+4*i]=100+i;
            for (unsigned i=0;i<4;++i) bus.values[0x500250+4*i]=112+i;
            cpu.m_r[21]=116;
            if (mode!="contact-missing-pre") observe(0x2acc0);
            if (mode=="contact-duplicate-pre") observe(0x2acc0);
            if (mode=="contact-wrong-frame") cpu.m_r[31]+=0x40;
            if (mode!="contact-missing-reply") observe(0x2adac);
            if (mode=="contact-duplicate-reply") observe(0x2adac);
            observe(0x2ae68);
            for (unsigned i=0;i<8;++i) bus.values[0x500080+4*i]=200+i;
            cpu.m_r[16]=0xdeadbeef;cpu.m_r[17]=0xcafebabe;
            if (mode!="contact-incomplete") {
                cpu.m_r[31]=mode=="contact-return-wrong-frame" ? 0x500080 : 0x500040;
                observe(0x2e8a8);
            }
            cpu.m_r[2]=0x5cafec;
        }
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
