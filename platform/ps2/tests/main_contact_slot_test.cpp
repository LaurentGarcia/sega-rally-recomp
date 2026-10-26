#include "main_contact_slot_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=main_slot_cases;using namespace ps2::native;
bool run(bool extended){
    static const auto table=cases::table();unsigned count=0,active=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](auto c){c.input.normal_table=table;MainContactSlotOutput output;
        if(!update_main_contact_slot(c.input,c.state,output))return false;
        cases::hash(hash,cases::words(c.state,output));++count;active+=output.active;return true;
    },extended))return false;

    if(count!=(extended?25852u:2876u)||active!=(extended?7248u:947u)||hash!=(extended?0x121b47e419609ea7ull:0x32ef92cd4f0d4583ull))return false;
    std::printf("MAIN_CONTACT_SLOT_%s PASS cases=%u active=%u hash=%016llx\n",extended?"HOST":"PROBE",count,active,(unsigned long long)hash);
    return true;
}
bool guards(){
    static auto table=cases::table();unsigned rejected=0;
    auto reject=[&](cases::Case c){MainContactSlotOutput output{{1,2,3},true};const auto before=cases::words(c.state,output);
        if(update_main_contact_slot(c.input,c.state,output)||cases::words(c.state,output)!=before){std::printf("REJECTION FAILED %u\n",rejected);return false;}++rejected;return true;};
    for(auto bad:{0x7f800000u,0xff800000u,0x7f800001u,0x7fc12345u,0xffc12345u})for(unsigned field=0;field<12;++field){
        auto c=cases::base(0);c.input.normal_table=table;const auto v=cases::value(bad);
        switch(field){case 0:c.input.contact.gap=v;break;case 1:c.input.reference_middle=v;break;case 2:c.input.parameters[1]=v;break;case 3:c.input.parameters[3]=v;break;
        case 4:c.input.control=v;break;case 5:c.state.history[0]=v;break;case 6:c.input.contact.direction[1]=v;break;case 7:c.input.vector.x=v;break;
        case 8:c.input.axis_direction.z=v;break;case 9:c.input.contact.gap=1;c.input.factor=v;break;case 10:c.input.contact.gap=1;c.input.object_parameter=v;break;
        case 11:c.input.contact.status=128;c.state.history[1]=v;break;}
        if(!reject(c))return false;
    }
    for(unsigned slot=4;slot<256;++slot){auto c=cases::base(0);c.input.slot=std::uint8_t(slot);if(!reject(c))return false;}
    for(unsigned mode=0;mode<4;++mode){auto c=cases::base(0);c.input.normal_table=table;
        if(mode==0)c.input.normal_table={};if(mode==1)c.input.vector={cases::value(0x30800000),1,0};if(mode==2)c.input.vector={cases::value(0x1c800000),0,0};if(mode==3)c.input.contact.gap=cases::value(0x41800001);
        if(!reject(c))return false;}
    // A minimal active update with a completely unused table and coefficient
    // payloads has independently calculable values and preserves other slots.
    auto c=cases::base(2);c.input.contact={{1,0,-1},0.125f,6};c.input.parameters={cases::value(0x7f800001),cases::value(0x7f800001),2,cases::value(0x7f800001),0};
    c.input.factor=cases::value(0x7f800001);c.input.object_parameter=cases::value(0x7f800001);c.input.vector={};c.input.axis_direction={cases::value(0x7f800001),0,cases::value(0x7f800001)};
    const auto before=c.state;MainContactSlotOutput output;
    if(!update_main_contact_slot(c.input,c.state,output)||!output.active||output.response.x!=25||cases::bits(output.response.y)!=0||output.response.z!=-25||c.state.gain[2]!=25||c.state.history[2]!=0.125f||c.state.published_middle[2]!=2.125f||cases::bits(c.state.metric[2])||cases::bits(c.state.lateral[2]))return false;
    for(unsigned i:{0u,1u,3u})if(cases::bits(c.state.gain[i])!=cases::bits(before.gain[i])||cases::bits(c.state.metric[i])!=cases::bits(before.metric[i])||cases::bits(c.state.lateral[i])!=cases::bits(before.lateral[i])||cases::bits(c.state.published_middle[i])!=cases::bits(before.published_middle[i])||cases::bits(c.state.history[i])!=cases::bits(before.history[i]))return false;
    // Disabled slot with a nonpositive sibling never reads these payloads.
    c=cases::base(0);c.input.contact.status=128;c.state.history[1]=-0.0f;c.input.factor=c.input.object_parameter=c.input.control=cases::value(0x7f800001);c.input.contact.direction={cases::value(0x7f800001),cases::value(0x7f800001),cases::value(0x7f800001)};
    if(!update_main_contact_slot(c.input,c.state,output)||output.active||cases::bits(output.response.x)||cases::bits(output.response.y)||cases::bits(output.response.z))return false;
    std::printf("MAIN_CONTACT_SLOT_REJECTION PASS checks=%u known_controls=2\n",rejected);return true;
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
