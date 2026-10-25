#include "contact_normalization_cases.h"
#include <cstdio>
#ifndef _EE
#include <cfenv>
#include <cmath>
#else
#include <kernel.h>
#endif
namespace {
namespace cases=contact_normalization_cases;
using namespace ps2::native;
#ifndef _EE
cases::Words reference(const cases::Words &w) {
    const auto v=cases::vector(w);
    const volatile float xx=v.x*v.x,yy=v.y*v.y,zz=v.z*v.z;
    const volatile float xy=xx+yy,total=xy+zz;
    const float length=float(std::sqrt(double(total)));
    if(double(length)<=std::bit_cast<double>(0x380fffffff9fdba8ull))return {0,0,0x3f800000u};
    const volatile float x=v.x/length,y=v.y/length,z=v.z/length;
    return cases::bits({x,y,z});
}
#endif
bool run(bool extended) {
    unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const cases::Words &c){
        const auto input=cases::vector(c);ContactDirection3 output;
        if(!normalize_contact_direction(input,output))return false;
        const auto result=cases::bits(output);
#ifndef _EE
        if(result!=reference(c)) {
            std::printf("NORMALIZATION MISMATCH case=%u input=%08lx,%08lx,%08lx\n",count,
                (unsigned long)c[0],(unsigned long)c[1],(unsigned long)c[2]);return false;
        }
#endif
        auto alias=input;
        if(!normalize_contact_direction(alias,alias)||cases::bits(alias)!=result)return false;
        cases::hash(hash,c,result);++count;return true;
    },extended))return false;
    if(count!=(extended?59912u:4064u)||hash!=(extended?0x53b71c272e0a45eaull:0x2f11e06fc7af1915ull))return false;
    std::printf("CONTACT_NORMALIZATION_%s PASS cases=%u hash=%016llx alias=1\n",
        extended?"HOST":"PROBE",count,(unsigned long long)hash);
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
    ContactDirection3 result;
    if(!normalize_contact_direction(cases::vector({0x1a000000u,0,0}),result)||
       cases::bits(result)!=cases::Words{0,0,0x3f800000u})return 1;
    if(!normalize_contact_direction(cases::vector({0x1a000001u,0,0}),result)||
       cases::bits(result)==cases::Words{0,0,0x3f800000u})return 1;
    const cases::Words sentinel{0x7f800001u,0x80000000u,0xffffffffu};unsigned rejects=0;
    for(auto bad:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u})
        for(unsigned axis=0;axis<3;++axis) {
            cases::Words c{};c[axis]=bad;
            auto input=cases::vector(c),output=cases::vector(sentinel);
            if(normalize_contact_direction(input,output)||cases::bits(output)!=sentinel||
               normalize_contact_direction(input,input)||cases::bits(input)!=c)return 1;
            rejects+=2;
        }
    std::printf("CONTACT_NORMALIZATION_REJECTION PASS cases=%u underflow_guard=1\n",rejects);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
