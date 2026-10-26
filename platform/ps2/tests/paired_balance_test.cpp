#include "paired_balance_cases.h"
#include "vehicle_parameter_selection.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=paired_balance_cases;
using namespace ps2::native;
bool run(bool extended){
    unsigned count=0;std::uint64_t fingerprint=14695981039346656037ull;
    if(!cases::each([&](const cases::Input& input){
        auto output=cases::sentinel();if(!make_paired_balance(input,output))return false;
#ifndef _EE
        if(cases::words(output)!=cases::words(cases::mathematical(input))){
            std::printf("PAIR MISMATCH case=%u input=%08x,%08x,%08x,%08x\n",count,cases::bits(input[0]),cases::bits(input[1]),cases::bits(input[2]),cases::bits(input[3]));return false;}
#endif
        cases::hash(fingerprint,cases::words(output));++count;return true;
    },extended))return false;
    if(count!=(extended?101130u:2154u)||fingerprint!=(extended?0x205c18818a38f128ull:0x1fd698b609c8f960ull))return false;
    std::printf("PAIRED_BALANCE_%s PASS cases=%u hash=%016llx\n",extended?"HOST":"PROBE",count,(unsigned long long)fingerprint);
    return true;
}
bool guards(){
    unsigned count=0;
    for(auto invalid:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7f800001u,0x7fc12345u})for(unsigned i=0;i<4;++i){
        cases::Input input{1,2,3,4};input[i]=cases::value(invalid);auto output=cases::sentinel();const auto before=cases::words(output);
        if(make_paired_balance(input,output)||cases::words(output)!=before)return false;++count;
    }
    std::array<VehicleParameterRecord,10> records{};
    std::uint32_t state=0x1976ad25u;for(auto& record:records)for(auto& word:record.words)word=cases::next(state);
    std::uint64_t fingerprint=14695981039346656037ull;unsigned selections=0;
    for(std::uint32_t flags=0;flags<1024;++flags)for(auto high:{0u,0xfffffc00u,0xaaaaa800u}){
        const auto result=select_vehicle_parameters(flags|high,records);
        const unsigned expected=(flags&0x3feu)?unsigned(std::bit_width(flags&0x3feu)-1):0;
        if(result.index!=expected||result.record.words!=records[expected].words)return false;
        for(auto word:result.record.words)fingerprint=(fingerprint^word)*1099511628211ull;
        fingerprint=(fingerprint^result.index)*1099511628211ull;++selections;
    }
    if(count!=24||selections!=3072||fingerprint!=0xcf0278eb2ab1a5a1ull)return false;
    std::printf("PAIRED_BALANCE_REJECTION PASS checks=%u\n",count);
    std::printf("VEHICLE_PARAMETER_SELECTION PASS cases=%u hash=%016llx\n",selections,(unsigned long long)fingerprint);return true;
}
}
int main(){if(!run(false)||!guards())return 1;
#ifndef _EE
if(!run(true))return 1;
#endif
std::fflush(stdout);
#ifdef _EE
SleepThread();
#endif
}
