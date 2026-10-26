#include "contact_pass_response_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace cases=contact_pass_response_cases;
using namespace ps2::native;
bool run(bool extended){
    unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const auto& c){auto state=c.state;if(!resolve_contact_pass_response(c.input,state))return false;
        if(state.contact_mask!=count%16)return false;
        cases::hash(hash,cases::words(state));++count;return true;},extended))return false;
    if(count!=(extended?4224u:640u)||hash!=(extended?0xf99dcd3347d0f255ull:0x3a490cc705f0b6f2ull))return false;
    std::printf("CONTACT_PASS_RESPONSE_%s PASS cases=%u hash=%016llx\n",extended?"HOST":"PROBE",count,(unsigned long long)hash);return true;
}
int main(){
    if(!run(false))return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    ContactPassResponseInput input{};ContactPassResponseState state{};
    state.car.candidate_position={cases::value(0x7f800001u),cases::value(0xffc01234u),cases::value(0x80000000u)};
    state.car.motion=state.car.candidate_position;state.car.candidate_orientation=state.car.candidate_position;
    state.car.feedback={-123,456};state.contact_mask=0xffffffffu;state.selected_scalar=7;
    input.response_parameter=cases::value(0x7fc00000u);input.gain_parameter=0;input.scalar_divisor=0;
    for(auto& p:input.offsets)p={cases::value(0x7f800001u),0,0};
    auto expected=cases::words(state);expected[11]=expected[12]=0;
    if(!resolve_contact_pass_response(input,state)||cases::words(state)!=expected)return 1;
    const auto saved=cases::words(state);input.contacts[3].status=0x80;input.contacts[3].gap=1;
    if(resolve_contact_pass_response(input,state)||cases::words(state)!=saved)return 1;
    std::puts("CONTACT_PASS_RESPONSE_GUARDS PASS inactive_preservation=1 late_atomic_rejection=1");std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
}
