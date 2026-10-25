// Offline original-firmware oracle. Never linked into a native application.
#include "rounded_affine_transform_cases.h"
#include "runtime/tgp.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
struct Complete {};
void hook(rt::Tgp &t,std::uint16_t pc) {
    if(pc==0x46b)throw Complete{};
    if(pc<0x44c||pc>=0x46c)t.fatal(pc,"outside isolated affine-transform interval");
}
struct Bus final:rt::TgpBus {
    unsigned reads=0;
    bool fifo_pop(std::uint32_t &v) override {
        if(reads++)throw rt::TgpFatal("unexpected extra FIFO read");
        v=0;return true;
    }
    void fifo_push(std::uint32_t)override{throw rt::TgpFatal("unexpected FIFO write");}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("unexpected external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("unexpected external write");}
};
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    try {
        std::ifstream stream(argv[1],std::ios::binary);
        const std::vector<std::uint8_t> firmware{std::istreambuf_iterator<char>(stream),{}};
        if(firmware.size()!=6660)throw rt::TgpFatal("wrong private firmware size");
        unsigned count=0;std::uint64_t hash=14695981039346656037ull,steps=0;
        if(!rounded_affine_cases::each([&](const rounded_affine_cases::Case &c){
            ps2::native::AffinePoint3 result;
            if(!ps2::native::rounded_affine_transform(rounded_affine_cases::transform(c),
                                                     rounded_affine_cases::point(c,12),result))return false;
            Bus bus;rt::Tgp t;t.bus=&bus;t.hook=hook;t.pc=0x44c;t.pcs[0]=0x46c;
            for(unsigned i=0;i<firmware.size()/4;++i)
                t.prog[i]=firmware[4*i]|(std::uint32_t(firmware[4*i+1])<<8)|
                    (std::uint32_t(firmware[4*i+2])<<16)|(std::uint32_t(firmware[4*i+3])<<24);
            t.data[7]=0x80;
            for(unsigned i=0;i<12;++i)t.data[0x280+i]=c[i];
            for(unsigned i=0;i<3;++i)t.data[0x70+4*i]=c[12+i];
            try {rt::tgpgen::run(t,40);throw rt::TgpFatal("affine instruction bound exceeded");}
            catch(const Complete &) {}
            if(t.count!=32||bus.reads!=1||t.pc!=0x46c)throw rt::TgpFatal("wrong instruction, return or FIFO count");
            const rounded_affine_cases::Words expected{t.data[0x70],t.data[0x74],t.data[0x78]};
            if(expected!=rounded_affine_cases::bits(result)) {
                std::printf("AFFINE ORACLE MISMATCH case=%u expected=%08x,%08x,%08x actual=%08x,%08x,%08x\n",
                    count,expected[0],expected[1],expected[2],rounded_affine_cases::bits(result)[0],
                    rounded_affine_cases::bits(result)[1],rounded_affine_cases::bits(result)[2]);return false;
            }
            rounded_affine_cases::hash(hash,c,expected);steps+=t.count;++count;return true;
        },true))return 1;
        std::printf("ROUNDED_AFFINE_ORACLE PASS cases=%u instructions=%llu fifo_reads=%u hash=%016llx\n",
                    count,static_cast<unsigned long long>(steps),count,static_cast<unsigned long long>(hash));
    } catch(const std::exception &e) {std::fprintf(stderr,"affine oracle failed: %s\n",e.what());return 1;}
}
