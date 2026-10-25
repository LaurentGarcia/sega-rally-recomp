// Private original-firmware comparison; excluded from native applications.
#include "rounded_cross_product_cases.h"
#include "runtime/tgp.h"
#include <array>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

namespace {
namespace cases = rounded_cross_product_cases;
std::vector<std::uint8_t> read(const char *path) {
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw rt::TgpFatal("cannot open private cross-product input");
    return {std::istreambuf_iterator<char>(stream),{}};
}
std::uint32_t word(const std::vector<std::uint8_t> &v,std::size_t offset) {
    return v.at(offset) | (std::uint32_t(v.at(offset+1))<<8) |
           (std::uint32_t(v.at(offset+2))<<16) | (std::uint32_t(v.at(offset+3))<<24);
}
struct Bus final : rt::TgpBus {
    cases::Case input{};
    cases::Words output{};
    unsigned consumed=0,produced=0;
    bool fifo_pop(std::uint32_t &value) override {
        if (consumed>=input.size()) throw rt::TgpFatal("cross product requested excess input");
        value=input[consumed++];return true;
    }
    void fifo_push(std::uint32_t value) override {
        if (produced>=output.size()) throw rt::TgpFatal("cross product returned excess output");
        output[produced++]=value;
    }
    std::uint32_t mem_r(std::uint32_t) override { throw rt::TgpFatal("unexpected cross-product external read"); }
    void mem_w(std::uint32_t,std::uint32_t) override { throw rt::TgpFatal("unexpected cross-product external write"); }
    void bank_w(std::uint32_t) override { throw rt::TgpFatal("unexpected cross-product bank change"); }
};
struct Finished {};
unsigned hook_count=0;
void hook(rt::Tgp &t,std::uint16_t pc) {
    if (pc != 0x55f + hook_count || t.count != hook_count+1)
        throw rt::TgpFatal("cross product escaped original instruction sequence");
    ++hook_count;
    const auto &bus=*static_cast<const Bus*>(t.bus);
    const unsigned expected_input = pc<0x561?1:pc<0x563?2:pc<0x564?3:pc<0x567?4:pc<0x56a?5:6;
    const unsigned expected_output = pc<0x569?0:pc<0x56f?1:pc<0x575?2:3;
    if (bus.consumed!=expected_input || bus.produced!=expected_output)
        throw rt::TgpFatal("cross-product FIFO access boundary differs");
    if (pc==0x576) {
        if (t.pc!=0xfff) throw rt::TgpFatal("cross product returned to wrong caller");
        throw Finished{};
    }
}
unsigned comparisons=0,live_comparisons=0;
std::uint64_t fingerprint=14695981039346656037ull,instructions=0;
std::set<cases::Case> distinct;
bool compare(const std::vector<std::uint8_t> &firmware,const cases::Case &input,
             const cases::Words *captured=nullptr) {
    ps2::native::CrossVector3 native;
    if (!ps2::native::rounded_cross_product(cases::vector(input,0),cases::vector(input,3),native))
        throw rt::TgpFatal("captured or synthetic cross-product input exceeds native domain");
    const auto actual=cases::bits(native);
    Bus bus;
    // Explicit semantic adapter: six transport words encode two ordinary
    // vectors in this order. This arrangement has no native-runtime meaning.
    bus.input={input[1],input[5],input[2],input[4],input[3],input[0]};
    rt::Tgp t;t.reset();t.bus=&bus;t.pc=0x55f;t.pcs[0]=0xfff;t.hook=hook;
    t.a=0x12345678;t.b=0x87654321;t.d=0xabcdef01;t.p=0xfedcba98;
    for (unsigned i=0;i<firmware.size()/4;++i)t.prog[i]=word(firmware,4*i);
    for (unsigned i=0;i<0x400;++i)t.data[i]=0x53a91d00u^i;
    hook_count=0;
    try { rt::tgpgen::run(t,25);throw rt::TgpFatal("cross-product instruction bound exceeded"); }
    catch(const Finished&) {}
    if (hook_count!=24 || bus.consumed!=6 || bus.produced!=3 || actual!=bus.output ||
        (captured && *captured!=bus.output)) {
        std::fprintf(stderr,"CROSS ORACLE MISMATCH case=%u native=%08x,%08x,%08x original=%08x,%08x,%08x\n",
                     comparisons,actual[0],actual[1],actual[2],bus.output[0],bus.output[1],bus.output[2]);
        throw rt::TgpFatal("native/original/captured cross-product values differ");
    }
    for (unsigned i=0;i<0x400;++i) {
        auto expected=0x53a91d00u^i;
        if (i==0x34)expected=bus.input[1];
        if (i==0x35)expected=bus.input[3];
        if (i==0x36)expected=bus.input[0];
        if (i==0x37)expected=bus.input[4];
        if (t.data[i]!=expected)throw rt::TgpFatal("unexpected cross-product scratch state change");
    }
    distinct.insert(input);cases::hash(fingerprint,input,bus.output);
    instructions+=t.count;++comparisons;if(captured)++live_comparisons;
    return true;
}
} // namespace
int main(int argc,char **argv) {
    if (argc!=2 && argc!=3)return 2;
    try {
        if(std::fegetround()!=FE_TONEAREST)throw rt::TgpFatal("host rounding mode is not nearest/even");
        const auto firmware=read(argv[1]);
        if(firmware.size()!=6660)throw rt::TgpFatal("wrong private firmware size");
        if(!cases::each([&](const auto &c){return compare(firmware,c);},true))return 1;
        if(comparisons!=110048 || fingerprint!=0x428bbf5526cd241bull)
            throw rt::TgpFatal("synthetic cross-product coverage/fingerprint changed");
        if(argc==3) {
            const auto records=read(argv[2]);
            const std::array<std::uint8_t,8> magic{'R','X','C','R','O','S','1',0};
            if(records.size()<12 || !std::equal(magic.begin(),magic.end(),records.begin()))
                throw rt::TgpFatal("invalid private cross-product records");
            const auto count=word(records,8);
            if(!count || count>16384 || records.size()!=12ull+36ull*count)
                throw rt::TgpFatal("invalid private cross-product record length");
            for(unsigned i=0;i<count;++i) {
                cases::Case input;cases::Words output;
                for(unsigned j=0;j<6;++j)input[j]=word(records,12+i*36+j*4);
                for(unsigned j=0;j<3;++j)output[j]=word(records,12+i*36+24+j*4);
                compare(firmware,input,&output);
            }
        }
        std::printf("ROUNDED_CROSS_ORACLE PASS cases=%u unique_inputs=%zu live=%u instructions=%llu reads=%u writes=%u hash=%016llx\n",
                    comparisons,distinct.size(),live_comparisons,static_cast<unsigned long long>(instructions),
                    comparisons*6,comparisons*3,static_cast<unsigned long long>(fingerprint));
    }catch(const std::exception &error){std::fprintf(stderr,"cross-product oracle failed: %s\n",error.what());return 1;}
    return 0;
}
