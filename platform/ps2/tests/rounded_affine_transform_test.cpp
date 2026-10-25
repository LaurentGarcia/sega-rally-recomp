#include "rounded_affine_transform_cases.h"
#include <cstdio>
#ifndef _EE
#include <cfenv>
#else
#include <kernel.h>
#endif
namespace {
namespace cases = rounded_affine_cases;
using namespace ps2::native;
#ifndef _EE
cases::Words reference(const cases::Case &c) {
    cases::Words result{};
    for(unsigned axis=0;axis<3;++axis) {
        volatile float sum=std::bit_cast<float>(c[9+axis]);
        for(unsigned column=0;column<3;++column) {
            const volatile float product=std::bit_cast<float>(c[3*column+axis])*std::bit_cast<float>(c[12+column]);
            sum=sum+product;
        }
        result[axis]=std::bit_cast<std::uint32_t>(float(sum));
    }
    return result;
}
#endif
bool run(bool extended) {
    unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const cases::Case &c){
        const auto matrix=cases::transform(c);const auto input=cases::point(c,12);
        AffinePoint3 result;
        if(!rounded_affine_transform(matrix,input,result))return false;
        const auto words=cases::bits(result);
#ifndef _EE
        if(words!=reference(c)) {std::printf("AFFINE MISMATCH case=%u\n",count);return false;}
#endif
        auto alias=input;
        if(!rounded_affine_transform(matrix,alias,alias)||cases::bits(alias)!=words)return false;
        for(unsigned i=0;i<4;++i) {
            auto copy=matrix;auto &out=i==3?copy.translation:copy.columns[i];
            if(!rounded_affine_transform(copy,input,out)||cases::bits(out)!=words)return false;
        }
        cases::hash(hash,c,words);++count;return true;
    },extended))return false;
    if(count!=(extended?84273u:4065u)||hash!=(extended?0x7005cd920cda465eull:0xeb07dfee53139c87ull))return false;
    std::printf("ROUNDED_AFFINE_%s PASS cases=%u hash=%016llx aliases=5\n",
                extended?"HOST":"PROBE",count,static_cast<unsigned long long>(hash));
    return true;
}
} // namespace
int main() {
#ifndef _EE
    if(std::fegetround()!=FE_TONEAREST)return 1;
#endif
    if(!run(false))return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    cases::Case guard{};guard[0]=0xbf800000u;guard[3]=0x3f800000u;
    guard[9]=0x33000000u;guard[12]=guard[13]=0x3f800000u;
    AffinePoint3 checked;
    if(!rounded_affine_transform(cases::transform(guard),cases::point(guard,12),checked)||
       cases::bits(checked)!=cases::Words{})return 1;
    guard={};guard[0]=0x3f800001u;guard[9]=0xbf800000u;guard[12]=0x3f7ffffeu;
    if(!rounded_affine_transform(cases::transform(guard),cases::point(guard,12),checked)||
       cases::bits(checked)!=cases::Words{})return 1;
    constexpr cases::Words sentinel{0x7fc12345u,0x80000000u,0xffffffffu};
    unsigned rejects=0;
    for(auto bad:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,
                  0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u})
        for(unsigned lane=0;lane<15;++lane) {
            cases::Case c{};c[lane]=bad;
            auto matrix=cases::transform(c);auto input=cases::point(c,12);
            AffinePoint3 out{std::bit_cast<float>(sentinel[0]),std::bit_cast<float>(sentinel[1]),std::bit_cast<float>(sentinel[2])};
            if(rounded_affine_transform(matrix,input,out)||cases::bits(out)!=sentinel)return 1;
            const auto before=cases::bits(input);
            if(rounded_affine_transform(matrix,input,input)||cases::bits(input)!=before)return 1;
            for(unsigned i=0;i<4;++i) {
                auto &alias=i==3?matrix.translation:matrix.columns[i];
                const auto bits=cases::bits(alias);
                if(rounded_affine_transform(matrix,input,alias)||cases::bits(alias)!=bits)return 1;
            }
            rejects+=6;
        }
    std::printf("ROUNDED_AFFINE_REJECTION PASS cases=%u order_guard=1 fused_guard=1\n",rejects);std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
