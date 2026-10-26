#include "contact_response_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=contact_response_cases;
bool run(bool extended){
    unsigned count=0,accepted=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const cases::Case& c){
        auto out=cases::sentinel();const auto before=cases::words(out);
        const bool ok=ps2::native::make_contact_response(c.input,c.previous,out);
        if(!ok&&cases::words(out)!=before)return false;
#ifndef _EE
        auto expected=cases::sentinel();const bool valid=cases::mathematical(c,expected);
        if(ok!=valid||(ok&&cases::words(out)!=cases::words(expected))){
            std::printf("RESPONSE MISMATCH case=%u accepted=%u expected=%u\n",count,unsigned(ok),unsigned(valid));return false;
        }
#endif
        cases::hash(hash,cases::words(out),ok);++count;accepted+=ok;return true;
    },extended))return false;
    if(count!=(extended?18432u:1200u)||accepted!=(extended?18360u:1128u)||
       hash!=(extended?0x3684bd3cf078fbc4ull:0xcd1b0a7946e70d88ull))return false;
    std::printf("CONTACT_RESPONSE_%s PASS cases=%u accepted=%u hash=%016llx\n",extended?"HOST":"PROBE",count,accepted,(unsigned long long)hash);return true;
}
bool rejection(){
    cases::Case c{};c.input.contact.status=128;c.input.contact.direction={1,0,0};c.input.offset={0,0,1};c.input.contact.gap=1;
    c.input.parameter50=1000;c.input.parameter54=1146;c.previous=0;
    unsigned count=0;
    const auto reject=[&](const cases::Case& c){auto out=cases::sentinel();const auto before=cases::words(out);++count;return !ps2::native::make_contact_response(c.input,c.previous,out)&&cases::words(out)==before;};
    for(auto bad:{0x7f800000u,0xff800000u,0x7f800001u,0x41800001u,0xc1800001u}){
        auto changed=c;changed.input.offset.y=cases::value(bad);if(!reject(changed))return false;
        changed=c;changed.input.contact.direction[1]=cases::value(bad);if(!reject(changed))return false;
    }
    for(auto bad:{0x7f800000u,0xff800000u,0x7f800001u,0x44800001u,0xc4800001u}){
        auto changed=c;changed.input.contact.gap=cases::value(bad);if(!reject(changed))return false;
        changed=c;changed.previous=cases::value(bad);if(!reject(changed))return false;
    }
    for(auto bad:{0u,0x80000000u,0x3a7fffffu,0x47800001u,0xbf800000u,0x7fc00000u}){
        auto changed=c;changed.input.parameter50=cases::value(bad);if(!reject(changed))return false;
        changed=c;changed.input.parameter54=cases::value(bad);if(!reject(changed))return false;
    }
    // The fallback direction dotted with a negative parallel offset is negative.
    auto changed=c;changed.input.contact.direction={0,0,1};changed.input.offset={0,0,-1};if(!reject(changed))return false;
    std::printf("CONTACT_RESPONSE_REJECTION PASS checks=%u\n",count);return true;
}
}
int main(){if(!run(false)||!rejection())return 1;
#ifndef _EE
if(!run(true))return 1;
#endif
std::fflush(stdout);
#ifdef _EE
SleepThread();
#endif
}
