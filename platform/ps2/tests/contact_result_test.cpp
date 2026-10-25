#include "contact_result_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
using namespace contact_result_cases;
bool run(bool extended){
    const auto pairs=table();unsigned cases=0;std::uint64_t fingerprint=14695981039346656037ull;
    if(!each([&](const Case& c){
        auto output=c.previous;
        if(!interpret_contact_result(c.polygon,c.point,pairs,output)){std::printf("REJECT case=%u\n",cases);return false;}
#ifndef _EE
        const auto expected=mathematical(c,pairs);
        if(words(output)!=words(expected)){
            const auto a=words(output),b=words(expected);
            for(unsigned i=0;i<a.size();++i)std::printf("MISMATCH case=%u field=%u actual=%08lx expected=%08lx\n",cases,i,(unsigned long)a[i],(unsigned long)b[i]);
            return false;
        }
#endif
        hash(fingerprint,words(output));++cases;return true;
    },extended))return false;
    if(cases!=(extended?40392u:2440u)||fingerprint!=(extended?0x385ddd84352f0d5eull:0x82d33d3b52e3a7d4ull))return false;
    std::printf("CONTACT_RESULT_%s PASS cases=%u hash=%016llx\n",extended?"HOST":"PROBE",cases,(unsigned long long)fingerprint);
    return true;
}
bool reject(){
    auto pairs=table();ContactPolygon p{};p.attributes=1;p.coefficients={0,1,0,0};ContactVertex point{};
    ContactResult state{{value(0x7f800001u),value(0x80000000u),value(1)},value(0xffc12345u),123};
    const auto before=words(state);unsigned checks=0;
    const auto check=[&](const ContactPolygon& polygon,const ContactVertex& q,std::span<const ContactNormalPair> t){
        ++checks;return !interpret_contact_result(polygon,q,t,state)&&words(state)==before;
    };
    for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u,0x49800001u,0xc9800001u}){
        auto q=point;q.x=value(bad);if(!check(p,q,pairs))return false;
        auto changed=p;changed.coefficients[0]=value(bad);if(!check(changed,point,pairs))return false;
    }
    for(auto bad:{0u,0x80000000u,0x357fffffu,0xb57fffffu}){
        auto changed=p;changed.coefficients[1]=value(bad);if(!check(changed,point,pairs))return false;
    }
    if(!check(p,point,{})||!check(p,point,std::span(pairs).first(8191)))return false;
    // Squared length1 chooses row0. Corrupt only the actually used row.
    for(auto bad:{0u,0x80000000u,0x7fc00000u,0x3dffffffu,0x3f800001u}){
        pairs[0].slope=value(bad);if(!check(p,point,pairs))return false;
    }
    pairs=table();for(auto bad:{0u,0x7fc00000u,0x3f7fffffu,0x40800001u}){
        pairs[0].intercept=value(bad);if(!check(p,point,pairs))return false;
    }
    p.attributes=0x800000u;p.coefficients={value(0x7f800001u),value(0x80000000u),value(0xff800000u),1};
    point.y=value(0x7fc12345u);
    for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u,0x49800001u,0xc9800001u}){
        auto changed=p;changed.vertices[1].z=value(bad);if(!check(changed,point,{}))return false;
    }
    for(auto bad:{0u,0x80000000u,0x357fffffu,0xb57fffffu}){
        auto changed=p;changed.coefficients[3]=value(bad);if(!check(changed,point,{}))return false;
    }
    // Edge replies copy arbitrary direction words and ignore pointY/table data.
    if(!interpret_contact_result(p,point,{},state)||words(state)!=Words{0x7f800001u,0x80000000u,0xff800000u,0,0x80})return false;
    std::printf("CONTACT_RESULT_REJECTION PASS checks=%u\n",checks);return true;
}
bool interpolation(){
    auto pairs=table();ContactPolygon polygon{};polygon.attributes=1;
    ContactResult result;const ContactVertex point{};
    polygon.coefficients={0,-1,0,0};pairs[0]={1,1};
    if(!interpret_contact_result(polygon,point,pairs,result)||words(result)!=Words{0,0x80000000u,0,0,0})return false;
    // Squared length2.25 has mantissa1.125 and scale1/2. The admitted
    // synthetic pair below deliberately produces a negative interpolation.
    polygon.coefficients={0,1.5f,0,0};pairs[4608]={1,1};
    if(!interpret_contact_result(polygon,point,pairs,result)||words(result)!=Words{0x80000000u,0xbdc00000u,0x80000000u,0x80000000u,0})return false;
    pairs[4608]={1,1.125f};
    if(!interpret_contact_result(polygon,point,pairs,result)||words(result)!=Words{0,0,0,0x80000000u,0})return false;
    std::puts("CONTACT_RESULT_INTERPOLATION PASS cases=3");return true;
}
}
int main(){if(!run(false)||!reject()||!interpolation())return 1;
#ifndef _EE
if(!run(true))return 1;
#endif
std::fflush(stdout);
#ifdef _EE
SleepThread();
#endif
}
