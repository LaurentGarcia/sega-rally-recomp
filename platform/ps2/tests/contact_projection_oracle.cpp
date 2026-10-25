// Offline original-firmware oracle. Never linked into a native application.
#include "contact_projection.h"
#include "contact_projection_cases.h"
#include "runtime/tgp.h"
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
std::vector<std::uint8_t> read(const char *path){
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw rt::TgpFatal("cannot open private firmware");
    return {std::istreambuf_iterator<char>(stream),{}};
}
std::uint32_t word(const std::vector<std::uint8_t>&v,std::size_t offset){
    return v.at(offset)|(std::uint32_t(v.at(offset+1))<<8)|(std::uint32_t(v.at(offset+2))<<16)|(std::uint32_t(v.at(offset+3))<<24);
}
struct Bus final:rt::TgpBus{
    unsigned count=0,reads=0;
    bool fifo_pop(std::uint32_t&)override{throw rt::TgpFatal("unexpected FIFO read");}
    void fifo_push(std::uint32_t)override{throw rt::TgpFatal("unexpected FIFO write");}
    std::uint32_t mem_r(std::uint32_t address)override{
        if(address!=0x400100)throw rt::TgpFatal("unexpected projected-test data read");
        ++reads;return count==4?4:0;
    }
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("unexpected external write");}
};
struct Outcome { bool accepted; };
std::uint32_t last_cross=0;
void hook(rt::Tgp &t,std::uint16_t pc){
    if(pc==0x60e || pc==0x626)last_cross=t.b;
    if(pc==0x630 || pc==0x632)throw Outcome{pc==0x630};
    if(pc<0x5da||pc>=0x630)t.fatal(pc,"outside isolated projected-test instruction span");
}
} // namespace
int main(int argc,char **argv){
    if(argc!=2)return 2;
    try{
        const auto firmware=read(argv[1]);if(firmware.size()!=6660)throw rt::TgpFatal("wrong firmware size");
        unsigned cases=0;std::uint64_t hash=14695981039346656037ull,steps=0;
        const auto test=[&](const contact_projection_cases::Case &c){
            std::array<ps2::native::ContactPointXZ,4> vertices;
            for(unsigned i=0;i<4;++i)vertices[i]={std::bit_cast<float>(c.values[2*i]),std::bit_cast<float>(c.values[2*i+1])};
            const ps2::native::ContactPointXZ point{std::bit_cast<float>(c.values[8]),std::bit_cast<float>(c.values[9])};
            ps2::native::ContactProjection native;
            if(!ps2::native::contact_projection(std::span(vertices.data(),c.count),point,native))return false;
            Bus bus;bus.count=c.count;
            rt::Tgp t;t.bus=&bus;t.hook=hook;t.pc=0x5da;t.bank_reg=0x400000;
            for(unsigned i=0;i<firmware.size()/4;++i)t.prog[i]=word(firmware,4*i);
            t.data[0x37]=0x80000000u;t.data[0x3f]=0x400100u;
            t.data[0x45]=c.values[8];t.data[0x47]=c.values[9];
            for(unsigned i=0;i<4;++i){t.data[0x256+3*i]=c.values[2*i];t.data[0x258+3*i]=c.values[2*i+1];}
            last_cross=0;
            bool accepted=false;
            try { rt::tgpgen::run(t,1000); throw rt::TgpFatal("projected-test instruction bound exceeded"); }
            catch(const Outcome &result){accepted=result.accepted;}
            if(bus.reads!=1)throw rt::TgpFatal("wrong projected-test data access count");
            std::array<std::uint32_t,4> edges{t.data[0x40],t.data[0x41],c.count==4?t.data[0x42]:last_cross,c.count==4?last_cross:0};
            for(unsigned i=0;i<4;++i)if(edges[i]!=std::bit_cast<std::uint32_t>(native.edge_cross[i])){
                std::printf("PROJECTION ORACLE MISMATCH case=%u edge=%u expected=%08x actual=%08x\n",cases,i,edges[i],std::bit_cast<std::uint32_t>(native.edge_cross[i]));return false;
            }
            if(accepted!=native.accepted){std::printf("PROJECTION ACCEPT MISMATCH case=%u\n",cases);return false;}
            hash=(hash^c.count)*1099511628211ull;
            for(auto x:c.values)hash=(hash^x)*1099511628211ull;
            for(auto x:edges)hash=(hash^x)*1099511628211ull;
            hash=(hash^native.accepted)*1099511628211ull;steps+=t.count;++cases;
            return true;
        };
        if(!contact_projection_cases::each(test,true))return 1;
        std::printf("CONTACT_PROJECTION_ORACLE PASS cases=%u instructions=%llu reads=%u hash=%016llx\n",cases,(unsigned long long)steps,cases,(unsigned long long)hash);
    }catch(const std::exception&e){std::fprintf(stderr,"contact projection oracle failed: %s\n",e.what());return 1;}
}
