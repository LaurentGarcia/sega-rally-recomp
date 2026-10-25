#include "contact_aggregate_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=contact_aggregate_cases;
bool run(bool extended){
    unsigned steps=0,updates=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::steps([&](const cases::Step& step){
        float output=step.accumulator;
        if(!ps2::native::accumulate_contact_scalar(output,step.metric,step.sign,output))return false;
#ifndef _EE
        if(cases::bits(output)!=cases::bits(cases::step_math(step)))return false;
#endif
        cases::hash(hash,cases::bits(output));++steps;return true;
    },extended))return false;
    if(!cases::each([&](const cases::Case& c){
        auto output=c.state;if(!ps2::native::apply_contact_aggregate(c.input,output))return false;
#ifndef _EE
        if(cases::words(output)!=cases::words(cases::mathematical(c)))return false;
#endif
        cases::hash(hash,cases::words(output));++updates;return true;
    },extended))return false;
    if(steps!=(extended?25728u:2240u)||updates!=(extended?18400u:912u)||
       hash!=(extended?0x390cbdf6408065cdull:0x63248df7338a0797ull))return false;
    std::printf("CONTACT_AGGREGATE_%s PASS steps=%u updates=%u hash=%016llx\n",extended?"HOST":"PROBE",steps,updates,(unsigned long long)hash);
    return true;
}
bool rejection(){
    cases::Case c{};c.input.response_divisor=1;c.input.scalar_divisor=1;
    c.state.candidate_position={cases::value(0x7f800001u),cases::value(0xffc00001u),0};
    c.state.candidate_orientation={cases::value(0x80000000u),0,cases::value(0xff800000u)};
    c.state.feedback={-123,456};const auto before=cases::words(c.state);unsigned count=0;
    const auto reject=[&](const auto& input){++count;auto out=c.state;return !ps2::native::apply_contact_aggregate(input,out)&&cases::words(out)==before;};
    for(auto bad:{0x7f800000u,0xff800000u,0x7f800001u,0x49800001u,0xc9800001u}){
        auto in=c.input;in.signed_scalar=cases::value(bad);if(!reject(in))return false;
        in=c.input;in.responses[3][2]=cases::value(bad);if(!reject(in))return false;
        in=c.input;in.bases[1]=cases::value(bad);if(!reject(in))return false;
        float output=cases::value(0x7f800001u);++count;
        if(ps2::native::accumulate_contact_scalar(0,cases::value(bad),0,output)||cases::bits(output)!=0x7f800001u)return false;
    }
    for(auto bad:{0u,0x80000000u,0x357fffffu,0xb57fffffu}){
        auto in=c.input;in.response_divisor=cases::value(bad);if(!reject(in))return false;
        in=c.input;in.scalar_divisor=cases::value(bad);if(!reject(in))return false;
    }
    for(auto sign:{1.0f,-1.0f}){
        auto in=c.input;in.signed_scalar=sign*1048576.0f;in.scalar_divisor=cases::value(0x35800000u);in.feedback_enabled=true;
        if(!reject(in))return false;
        in=c.input;in.feedback_enabled=true;in.signed_scalar=sign*268435.46875f;
        if(!reject(in))return false;
        in.signed_scalar=sign*268435.4375f;auto out=c.state;
        if(!ps2::native::apply_contact_aggregate(in,out)||out.feedback[0]!=268435437||
           out.feedback[1]!=(sign<0?-2147483500:2147483500))return false;
    }
    float scalar=17;++count;if(ps2::native::accumulate_contact_scalar(0,-1,0,scalar)||scalar!=17)return false;
    // A disabled feedback path accepts the same large update and preserves both integers.
    auto in=c.input;in.signed_scalar=1048576.0f;in.scalar_divisor=cases::value(0x35800000u);
    auto out=c.state;if(!ps2::native::apply_contact_aggregate(in,out)||out.feedback!=c.state.feedback)return false;
    std::printf("CONTACT_AGGREGATE_REJECTION PASS checks=%u\n",count);return true;
}
}
int main(){
    if(!run(false)||!rejection())return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
}
