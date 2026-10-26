#include "main_candidate_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#else
#include <cfenv>
#endif
namespace cases=main_candidate_cases;
bool run(bool extended){
    unsigned count=0,accepted=0,consumed=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const auto& before){
        auto s=before;const bool ok=ps2::native::prepare_main_candidate(s);
#ifndef _EE
        auto expected=before;if(cases::mathematical(expected)!=ok||cases::words(expected)!=cases::words(s))return false;
#endif
        if(!ok&&cases::words(s)!=cases::words(before))return false;
        accepted+=ok;consumed+=ok&&s.response_marker==15;++count;
        cases::hash(hash,cases::words(before));cases::hash(hash,cases::words(s));hash=(hash^ok)*1099511628211ull;return true;
    },extended))return false;
    if(count!=(extended?17044u:1684u)||accepted!=(extended?16315u:1641u)||consumed!=(extended?5807u:971u)||
       hash!=(extended?0x966c10f581152becull:0xd782474a4f893dc8ull))return false;
    std::printf("MAIN_CANDIDATE_%s PASS cases=%u accepted=%u consumed=%u hash=%016llx\n",extended?"HOST":"PROBE",count,accepted,consumed,(unsigned long long)hash);return true;
}
int main(){
#ifndef _EE
    if(std::fegetround()!=FE_TONEAREST)return 1;
#endif
    if(!run(false))return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    unsigned rejected=0;
    for(unsigned axis=0;axis<15;++axis)for(auto bad:{0x7f800000u,0xff800000u,0x7fc00001u,0x49800001u,0xc9800001u}){
        cases::Words w{};w[axis]=bad;w[15]=0xabcdef01u;w[21]=0x7f800001u;auto state=cases::state(w);
        if(ps2::native::prepare_main_candidate(state)||cases::words(state)!=w)return 1;++rejected;
    }
    std::printf("MAIN_CANDIDATE_REJECTION PASS cases=%u\n",rejected);std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
