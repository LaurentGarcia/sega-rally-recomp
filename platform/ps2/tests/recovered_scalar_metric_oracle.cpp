// Offline original-firmware comparison; excluded from native applications.
#include "recovered_scalar_metric_cases.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

namespace {
namespace cases=recovered_scalar_metric_cases;
constexpr std::uint32_t sentinel=0x7fc12345u;
std::vector<std::uint8_t> read(const char *path) {
    std::ifstream f(path,std::ios::binary);if(!f)throw rt::TgpFatal("cannot open private metric input");
    return {std::istreambuf_iterator<char>(f),{}};
}
std::uint32_t word(const std::vector<std::uint8_t>&v,std::size_t offset) {
    return v.at(offset)|(std::uint32_t(v.at(offset+1))<<8)|(std::uint32_t(v.at(offset+2))<<16)|(std::uint32_t(v.at(offset+3))<<24);
}
struct Bus final:rt::TgpBus {
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t &value) override {
        if(consumed==input.size())return false;value=input[consumed++];return true;
    }
    void fifo_push(std::uint32_t value) override {if(output.size())throw rt::TgpFatal("excess metric output");output.push_back(value);}
    std::uint32_t mem_r(std::uint32_t) override {throw rt::TgpFatal("metric external read");}
    void mem_w(std::uint32_t,std::uint32_t) override {throw rt::TgpFatal("metric external write");}
    void bank_w(std::uint32_t v) override {if(v)throw rt::TgpFatal("metric bank change");}
};
void constants(const rt::Tgp &t) {
    constexpr std::array<unsigned,7> address{1,3,4,0x220,0x23a,0x23b,0x23c};
    constexpr std::array<std::uint32_t,7> expected{1,0,0x3f800000u,0x3f000000u,0x3f1714bau,0x3ed5a9a8u,0x3f3504f4u};
    for(unsigned i=0;i<address.size();++i)if(t.data[address[i]]!=expected[i])throw rt::TgpFatal("metric startup constant differs");
}
struct Done{};
unsigned expected_pc=0,steps=0;std::uint32_t squared=0;
void hook(rt::Tgp &t,std::uint16_t pc) {
    if(pc!=expected_pc||t.count!=++steps)throw rt::TgpFatal("metric instruction sequence differs");
    auto &bus=*static_cast<Bus*>(t.bus);
    const unsigned reads=pc>=0x51c?pc<0x51d?1:pc<0x51e?2:pc<0x520?3:pc<0x521?4:pc<0x523?5:6:6;
    if(bus.consumed!=reads||bus.output.size()!=unsigned(pc==0x52e||pc==0x52f))throw rt::TgpFatal("metric FIFO boundary differs");
    if(pc==0x52c)squared=t.d;
    expected_pc=pc+1;
    if(pc==0x52d)expected_pc=0x1ab;
    if(pc==0x1ad&&(t.st&rt::Tgp::F_ZRD))expected_pc=0x1ca;
    if(pc==0x1c1&&(t.st&rt::Tgp::F_ZRD))expected_pc=0x1c5;
    if(pc==0x1ca){if(t.pc!=0x52e)throw rt::TgpFatal("metric primitive return differs");expected_pc=0x52e;}
    if(pc==0x52f){if(t.pc!=0xfff)throw rt::TgpFatal("metric command return differs");throw Done{};}
}
unsigned cases_count=0,accepted=0,original_calls=0,live=0,subnormal_rejections=0,coordinate_rejections=0;
std::uint64_t fingerprint=14695981039346656037ull,instructions=0;
std::set<cases::Case> unique;
bool compare(const rt::Tgp &initial,const cases::Case &input,const std::uint32_t *captured=nullptr) {
    float output=std::bit_cast<float>(sentinel);
    const bool ok=ps2::native::recovered_scalar_metric(cases::point(input,0),cases::point(input,3),output);
    const auto result=std::bit_cast<std::uint32_t>(output);
    bool coordinate_domain=true;
    for(auto v:input)coordinate_domain&=(v&0x7fffffffu)<=0x49800000u;
    if(!coordinate_domain){
        if(ok||result!=sentinel||captured)throw rt::TgpFatal("metric invalid-coordinate policy differs");
        ++coordinate_rejections;
    }else{
        constants(initial);Bus bus;bus.input={input[0],input[3],input[1],input[4],input[2],input[5]};
        rt::Tgp t=initial;t.bus=&bus;t.pc=0x51c;t.pcs[0]=0xfff;t.hook=hook;t.count=0;
        expected_pc=0x51c;steps=0;squared=0;
        try{rt::tgpgen::run(t,53);throw rt::TgpFatal("metric original instruction bound exceeded");}catch(Done&){}
        const bool expected=squared==0||squared>=0x00800000u;
        if(ok!=expected||bus.consumed!=6||bus.output.size()!=1||
           (ok&&result!=bus.output[0])||(!ok&&result!=sentinel)||
           (captured&&(!ok||*captured!=bus.output[0]))) {
            std::fprintf(stderr,"METRIC ORACLE MISMATCH case=%u squared=%08x native=%08x original=%08x\n",cases_count,squared,result,bus.output.at(0));
            throw rt::TgpFatal("metric native/original/captured result differs");
        }
        if(steps!=24&&steps!=49&&steps!=52)throw rt::TgpFatal("unexpected metric branch count");
        constants(t);
        for(unsigned i=0;i<0x400;++i)if(i!=0x34&&i!=0x35&&i!=0x4e&&i!=0x4f&&t.data[i]!=initial.data[i])
            throw rt::TgpFatal("unexpected metric scratch change");
        if(t.tables||t.bank_reg||t.sincos_base!=initial.sincos_base||t.inv_base!=initial.inv_base||
           t.isqrt_base!=initial.isqrt_base||!std::equal(std::begin(t.atan_base),std::end(t.atan_base),std::begin(initial.atan_base)))
            throw rt::TgpFatal("metric touched a table port or bank");
        instructions+=steps;++original_calls;subnormal_rejections+=!ok;
    }
    cases::hash(fingerprint,input,ok,result);unique.insert(input);++cases_count;accepted+=ok;live+=captured!=nullptr;return true;
}
} // namespace
int main(int argc,char**argv){try{
    if(argc!=2&&argc!=3)return 2;
    if(std::fegetround()!=FE_TONEAREST)throw rt::TgpFatal("metric reference requires nearest/even host mode");
    const auto firmware=read(argv[1]);if(firmware.size()!=6660)throw rt::TgpFatal("wrong private firmware size");
    rt::Tgp initial;Bus startup;initial.reset();initial.bus=&startup;
    for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,100000);
    if(initial.count>=100000||startup.consumed!=1||startup.output!=std::vector<std::uint32_t>{8}||initial.tables||initial.bank_reg)
        throw rt::TgpFatal("metric verified startup failed");
    constants(initial);
    if(!cases::each([&](const auto &c){return compare(initial,c);},true))return 1;
    if(cases_count!=156644||accepted!=146736||fingerprint!=0x807252722fa67a0cull)
        throw rt::TgpFatal("metric synthetic coverage changed");
    if(argc==3){
        const auto records=read(argv[2]);const std::array<std::uint8_t,8> magic{'R','X','M','E','T','R','1',0};
        if(records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::TgpFatal("invalid metric record magic");
        const auto count=word(records,8);if(!count||count>16384||records.size()!=12ull+28ull*count)throw rt::TgpFatal("invalid metric record length");
        for(unsigned i=0;i<count;++i){cases::Case input;for(unsigned j=0;j<6;++j)input[j]=word(records,12+28*i+4*j);
            const auto captured=word(records,12+28*i+24);compare(initial,input,&captured);}
    }
    std::printf("RECOVERED_METRIC_ORACLE PASS cases=%u unique_inputs=%zu accepted=%u original_calls=%u live=%u instructions=%llu coordinate_rejections=%u subnormal_rejections=%u hash=%016llx\n",
        cases_count,unique.size(),accepted,original_calls,live,static_cast<unsigned long long>(instructions),coordinate_rejections,subnormal_rejections,static_cast<unsigned long long>(fingerprint));
}catch(const std::exception &e){std::fprintf(stderr,"metric oracle failed: %s\n",e.what());return 1;}return 0;}
