// Offline comparison with the owner's verified original firmware. Excluded
// from native programs; original generated instructions remain under build/.
#include "contact_point_preparation_cases.h"
#include "runtime/tgp.h"
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>
namespace {
namespace cases=contact_point_cases;
struct Bus final:rt::TgpBus{
    std::vector<std::uint32_t> input,output;unsigned consumed=0;
    bool fifo_pop(std::uint32_t&v)override{if(consumed==input.size())return false;v=input[consumed++];return true;}
    void fifo_push(std::uint32_t v)override{if(output.size()>=12)throw rt::TgpFatal("point-preparation excess output");output.push_back(v);}
    std::uint32_t mem_r(std::uint32_t)override{throw rt::TgpFatal("point-preparation external read");}
    void mem_w(std::uint32_t,std::uint32_t)override{throw rt::TgpFatal("point-preparation external write");}
    void bank_w(std::uint32_t v)override{if(v)throw rt::TgpFatal("point-preparation bank change");}
};
std::vector<std::uint8_t> read(const char*path){std::ifstream f(path,std::ios::binary);if(!f)throw rt::TgpFatal("cannot open private point input");return {std::istreambuf_iterator<char>(f),{}};}
std::uint32_t word(const std::vector<std::uint8_t>&v,std::size_t n){return v.at(n)|(std::uint32_t(v.at(n+1))<<8)|(std::uint32_t(v.at(n+2))<<16)|(std::uint32_t(v.at(n+3))<<24);}
void constants(const rt::Tgp&t){
    constexpr std::array<unsigned,12> addresses{3,4,5,6,0x3b,0x3c,0x220,0x221,0x222,0x223,0x224,0x225};
    constexpr std::array<std::uint32_t,12> values{0,0x3f800000u,0xbf800000u,0x40490fdbu,0x3fc90fdbu,0x3ea2f983u,0x3f000000u,0x3d2aaaabu,0x3ab60b61u,0x37d00d01u,0x3493f27eu,0x310f76c7u};
    for(unsigned i=0;i<addresses.size();++i)if(t.data[addresses[i]]!=values[i])throw rt::TgpFatal("point-preparation startup constants differ");
    if(t.tables||t.bank_reg||t.sincos_base||t.inv_base||t.isqrt_base||t.atan_base[0]||t.atan_base[1]||t.atan_base[2]||t.atan_base[3])throw rt::TgpFatal("point-preparation table state changed");
}
std::uint64_t hooks=0;
void hook(rt::Tgp&t,std::uint16_t pc){
    // Dispatcher, indexed point storage/readback/transform, push/pop/identity,
    // and exactly the float-angle rotations plus polynomial. Raw-angle table
    // entries are excluded, as are all external-memory firmware operations.
    const bool dispatch=pc==0xcf||pc==0xd0||pc==0xd4||pc==0xd8||pc==0xd9||pc==0xda||pc==0xf1||pc==0xf2||pc==0xf7;
    const bool allowed=dispatch||(pc>=0x77&&pc<=0xae)||(pc>=0x1e6&&pc<=0x21c)||
        (pc>=0x25b&&pc<=0x27d)||(pc>=0x327&&pc<=0x32d)||
        (pc>=0x363&&pc<=0x37f)||(pc>=0x384&&pc<=0x39f)||
        (pc>=0x3a4&&pc<=0x3bf)||(pc>=0x404&&pc<=0x413)||(pc>=0x44c&&pc<=0x46b);
    if(!allowed){std::fprintf(stderr,"unexpected original point PC=%x\n",pc);throw rt::TgpFatal("outside verified point-preparation instruction regions");}
    if(t.tables||t.bank_reg||t.sincos_base||t.inv_base||t.isqrt_base)throw rt::TgpFatal("point-preparation table state changed");++hooks;
}
std::uint64_t instructions=0,fingerprint=14695981039346656037ull;
unsigned evaluated=0,live=0;std::set<cases::Case> unique;
void execute(rt::Tgp&t,Bus&bus){const auto start=t.count;const auto before=hooks;rt::tgpgen::run(t,2048);if(t.count-start>=2048||hooks-before!=t.count-start||bus.consumed!=bus.input.size()||t.pc!=0x77)throw rt::TgpFatal("point command sequence did not return to bounded FIFO stall");instructions+=t.count-start;}
bool compare(const rt::Tgp&initial,const cases::Case&input,const std::uint32_t*captured=nullptr){
    Bus bus;rt::Tgp t=initial;t.bus=&bus;t.hook=hook;constants(t);
    const unsigned oldbase=t.data[7],olddepth=t.data[9];if(olddepth>=t.data[10])throw rt::TgpFatal("matrix stack lacks capacity");
    std::array<std::uint32_t,12> previous;
    for(unsigned i=0;i<12;++i){previous[i]=0x3f000000u+((i+evaluated)&0x7fffffu);t.data[0x200+oldbase+i]=previous[i];}
    for(unsigned i=0;i<4;++i){bus.input.push_back(0x21004242);bus.input.push_back(i);for(unsigned j=0;j<3;++j)bus.input.push_back(input[6+3*i+j]);}
    bus.input.insert(bus.input.end(),{0x10002020,0x12802525,0x15002a2a,input[1],0x14802929,input[0],0x15802b2b,input[2]});
    execute(t,bus);
    if(!bus.output.empty()||t.data[9]!=olddepth+1||t.data[7]!=oldbase+12)throw rt::TgpFatal("matrix push/rotation boundary differs");
    cases::Result expected;
    for(unsigned i=0;i<12;++i)expected[i]=t.data[0x200+t.data[7]+i];
    bus.input.clear();bus.consumed=0;
    for(unsigned i=0;i<4;++i)bus.input.insert(bus.input.end(),{0x24004848,i,0x21804343,i});
    bus.input.push_back(0x10802121);execute(t,bus);
    if(bus.output.size()!=12||t.data[7]!=oldbase||t.data[9]!=olddepth)throw rt::TgpFatal("matrix pop/output differs");
    for(unsigned i=0;i<12;++i){
        if(t.data[0x200+oldbase+i]!=previous[i])throw rt::TgpFatal("point preparation changed saved matrix");
        expected[12+i]=bus.output[i];
        // The CPU consumer adds two binary32 operands at nearest/even. This
        // independent host operation rounds once, exactly as its addr contract.
        volatile float absolute=std::bit_cast<float>(bus.output[i])+std::bit_cast<float>(input[3+i%3]);
        expected[24+i]=std::bit_cast<std::uint32_t>(float(absolute));
    }
    constants(t);
    ps2::native::ContactPointPreparation result;
    if(!ps2::native::prepare_contact_points(cases::point(input,0),cases::point(input,3),cases::local(input),result))throw rt::TgpFatal("native point preparation rejected bounded case");
    const auto actual=cases::words(result);
    if(actual!=expected){for(unsigned j=0;j<36;++j)if(actual[j]!=expected[j])std::fprintf(stderr,"POINT ORACLE MISMATCH case=%u lane=%u native=%08x original=%08x\n",evaluated,j,actual[j],expected[j]);throw rt::TgpFatal("native/original point preparation differs");}
    if(captured)for(unsigned j=0;j<24;++j)if(captured[j]!=expected[12+j])throw rt::TgpFatal("live prepared point differs");
    unique.insert(input);cases::hash(fingerprint,input,actual);++evaluated;live+=captured!=nullptr;return true;
}
}
int main(int argc,char**argv){try{
    if(argc!=2&&argc!=3)return 2;if(std::fegetround()!=FE_TONEAREST)throw rt::TgpFatal("point oracle requires nearest/even");
    const auto firmware=read(argv[1]);if(firmware.size()!=6660)throw rt::TgpFatal("wrong private firmware length");
    rt::Tgp initial;Bus startup;initial.reset();initial.bus=&startup;
    for(unsigned i=0;i<firmware.size()/4;++i)initial.prog[i]=word(firmware,4*i);
    startup.input={0x04000000};rt::tgpgen::run(initial,1000);
    if(initial.count!=108||initial.pc!=0x77||startup.consumed!=1||startup.output!=std::vector<std::uint32_t>{8})throw rt::TgpFatal("point verified startup differs");constants(initial);
    if(!cases::each([&](const auto&input){return compare(initial,input);},true))return 1;
    if(evaluated!=49295||unique.size()!=49287||fingerprint!=0xf05d0d03253121d8ull||instructions!=46675549)throw rt::TgpFatal("point synthetic coverage changed");
    if(argc==3){const auto records=read(argv[2]);const std::array<std::uint8_t,8> magic{'R','X','P','O','I','N','1',0};if(records.size()<12||!std::equal(magic.begin(),magic.end(),records.begin()))throw rt::TgpFatal("invalid point record magic");const auto count=word(records,8);if(!count||count>32||records.size()!=12ull+168ull*count)throw rt::TgpFatal("invalid point record count");for(unsigned i=0;i<count;++i){cases::Case input;std::array<std::uint32_t,24> out;for(unsigned j=0;j<18;++j)input[j]=word(records,12+168*i+4*j);for(unsigned j=0;j<24;++j)out[j]=word(records,12+168*i+72+4*j);compare(initial,input,out.data());}}
    std::printf("CONTACT_POINT_ORACLE PASS cases=%u unique_inputs=%zu live=%u instructions=%llu hash=%016llx external_accesses=0 saved_matrices=%u\n",evaluated,unique.size(),live,(unsigned long long)instructions,(unsigned long long)fingerprint,evaluated);
}catch(const std::exception&e){std::fprintf(stderr,"point oracle failed: %s\n",e.what());return 1;}return 0;}
