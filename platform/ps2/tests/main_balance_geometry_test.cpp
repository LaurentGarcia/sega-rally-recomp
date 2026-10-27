#include "main_balance_geometry_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=main_geometry_cases;
bool run(bool extended){unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](auto c){const auto input=cases::words(c);if(!ps2::native::publish_main_balance_geometry(c.input,c.state))return false;cases::hash(hash,input);cases::hash(hash,cases::words(c.state));++count;return true;},extended))return false;
    if(count!=(extended?20480u:1504u)||hash!=(extended?0x56b32fd92694a6a0ull:0x3e2a507aab74e9d4ull))return false;
    std::printf("MAIN_BALANCE_GEOMETRY_%s PASS cases=%u hash=%016llx\n",extended?"HOST":"PROBE",count,(unsigned long long)hash);return true;
}
bool guards(){unsigned count=0;
    for(unsigned i=0;i<16;++i)if(i!=8&&i!=15){const auto limit=i<4?0x47800000u:i<6?0x41800000u:i<8?0x41000000u:0x49800000u;
        for(auto bad:{0x7f800000u,0xff800000u,0x7f800001u,0x7fc12345u,limit+1,(limit+1)|0x80000000u}){
            auto w=cases::words(cases::base());w[i]=bad;auto c=cases::input(w);const auto before=cases::words(c.state);
            if(ps2::native::publish_main_balance_geometry(c.input,c.state)||cases::words(c.state)!=before)return false;++count;
        }
    }
    for(unsigned folded=0;folded<2;++folded){auto c=cases::base();c.input.longitudinal={folded?-1.0f:0,folded?1.0f:0};c.input.lateral={0,0};c.input.balance={0,0};c.input.steering=8;c.input.middle_orientation=-8;
        auto expected=cases::words(c.state);expected[6]=c.input.selected_index;
        if(!ps2::native::publish_main_balance_geometry(c.input,c.state)||cases::words(c.state)!=expected)return false;
    }
    std::printf("MAIN_BALANCE_GEOMETRY_REJECTION PASS checks=%u known_controls=2\n",count);return true;
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
