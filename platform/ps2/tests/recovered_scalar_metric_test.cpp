#include "recovered_scalar_metric_cases.h"
#include <cstdio>
#ifndef _EE
#include <cfenv>
#else
#include <kernel.h>
#endif

namespace {
namespace cases=recovered_scalar_metric_cases;
using namespace ps2::native;
constexpr std::uint32_t sentinel=0x7fc12345u;
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
#ifndef _EE
// Independent host floating evaluation; volatile temporaries prohibit fused
// arithmetic and preserve every specified binary32 boundary.
bool reference(const cases::Case &c,std::uint32_t &result) {
    for(auto value:c)if((value&0x7fffffffu)>0x49800000u)return false;
    float square[3];
    for(unsigned i=0;i<3;++i){const volatile float d=std::bit_cast<float>(c[3+i])-std::bit_cast<float>(c[i]);square[i]=d*d;}
    const volatile float partial=square[2]+square[1];
    const float squared=partial+square[0];const auto s=bits(squared);
    if(s && s<0x00800000u)return false;
    if(!s){result=0;return true;}
    const float m=std::bit_cast<float>((s&0x7fffffu)|0x3f000000u);
    const volatile float product=m*std::bit_cast<float>(0x3f1714bau);
    const float seed=std::bit_cast<float>(0x3ed5a9a8u)+product;
    const volatile float sum=1.0f+seed;
    const float halfway=sum*0.5f;
    const volatile float quotient=m/halfway;
    const volatile float corrected=quotient+halfway;
    float q=corrected*0.5f;int adjustment=int(s>>23)-126;
    if(adjustment%2){q=q*std::bit_cast<float>(0x3f3504f4u);++adjustment;}
    result=(bits(q)&0x7fffffu)|(std::uint32_t(int(bits(q)>>23)+adjustment/2)<<23);return true;
}
#endif
bool run(bool extended) {
    unsigned count=0,accepted=0;
    std::uint64_t hash=14695981039346656037ull;
    const auto test=[&](const cases::Case &c){
        const auto a=cases::point(c,0),b=cases::point(c,3);
        float output=std::bit_cast<float>(sentinel);
        const bool ok=recovered_scalar_metric(a,b,output);const auto result=bits(output);
#ifndef _EE
        std::uint32_t expected=sentinel;const auto wanted=reference(c,expected);
        if(ok!=wanted||result!=expected){std::printf("METRIC MISMATCH case=%u native=%08lx expected=%08lx\n",count,static_cast<unsigned long>(result),static_cast<unsigned long>(expected));return false;}
#endif
        for(unsigned lane=0;lane<6;++lane){
            auto first=a,second=b;
            float *coordinate=lane<3?(lane==0?&first.x:lane==1?&first.y:&first.z):
                                      (lane==3?&second.x:lane==4?&second.y:&second.z);
            const auto old=bits(*coordinate);
            if(recovered_scalar_metric(first,second,*coordinate)!=ok||bits(*coordinate)!=(ok?result:old))return false;
        }
        cases::hash(hash,c,ok,result);++count;accepted+=ok;return true;
    };
    if(!cases::each(test,extended))return false;
    if(count!=(extended?156644u:11972u) || accepted!=(extended?146736u:11234u) ||
       hash!=(extended?0x807252722fa67a0cull:0x46407e33dcf00f1bull))return false;
    std::printf("RECOVERED_METRIC_%s PASS cases=%u accepted=%u hash=%016llx aliases=6\n",
        extended?"HOST":"PROBE",count,accepted,static_cast<unsigned long long>(hash));
    return true;
}
} // namespace
int main(){
#ifndef _EE
    if(std::fegetround()!=FE_TONEAREST)return 1;
#endif
    if(!run(false))return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    float unit=0;
    if(!recovered_scalar_metric({}, {1,0,0},unit)||bits(unit)!=0x3f825985u)return 1;
    MetricPoint3 same{1,-2,3};
    if(!recovered_scalar_metric(same,same,same.y)||bits(same.y)!=0||same.x!=1||same.z!=3)return 1;
    unsigned rejected=0;
    for(auto bad:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u})
        for(unsigned lane=0;lane<6;++lane){
            cases::Case c{};c[lane]=bad;auto a=cases::point(c,0),b=cases::point(c,3);
            float out=std::bit_cast<float>(sentinel);
            if(recovered_scalar_metric(a,b,out)||bits(out)!=sentinel)return 1;
            float *coordinate=lane<3?(lane==0?&a.x:lane==1?&a.y:&a.z):(lane==3?&b.x:lane==4?&b.y:&b.z);
            if(recovered_scalar_metric(a,b,*coordinate)||bits(*coordinate)!=bad)return 1;
            rejected+=2;
        }
    std::printf("RECOVERED_METRIC_REJECTION PASS cases=%u unit_discriminator=1 identical_alias=1\n",rejected);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
