#include "main_contact_step_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
namespace cases=main_contact_step_cases;
using namespace ps2::native;
MainContactStepState initial(bool consuming,bool negative) {
    MainContactStepState s;auto& v=s.vehicle;
    v.position={negative?0.5f:-0.5f,0,0};
    v.pending_position_delta=consuming?std::array<float,3>{negative?-1.25f:1.25f,negative?-0.25f:0.25f,0}:std::array<float,3>{};
    v.motion={consuming?(negative?0.75f:-0.75f):(negative?0.125f:-0.125f),0.125f,0};
    v.orientation={0.25f,-0.5f,0.125f};v.angular_motion={-0.125f,0.25f,-0.0625f};
    v.response_marker=0xabcdef01u;
    v.candidate_position={cases::value(0x7f800001),2,3};
    v.candidate_orientation={cases::value(0xffc00001),5,6};
    s.feedback={-123,456};s.contact_mask=0xdeadbeefu;s.selected_scalar=cases::value(0x7fc12345);
    for(auto& c:s.contacts)c={{cases::value(0x7f800001),cases::value(0xffc00001),cases::value(0x80000000)},cases::value(0x7fc12345),7};
    return s;
}
MainContactStepState expected(bool consuming,bool negative,bool hit,bool feedback) {
    auto s=initial(consuming,negative);auto& v=s.vehicle;
    // Independently calculated authored geometry. All local points are zero,
    // so rotation leaves them zero. The square's first-edge determinant is32,
    // gap32/1024=1/32 and each unit-parameter response is+X/64. Four add1/16
    // to first motion with zero torque. No original data supplies these values.
    if(consuming) {
        // R32(1.25*0.6)=0.75; R32(1.25*3ecccccc)=0.4999999701976776.
        // Cancelling the half-unit base leaves exactly +/-2^-25.
        // Spell the original binary32 clamp by bits: this target compiler's
        // decimal 0.1f constant is not the nearest-even original encoding.
        v.position={cases::value(negative?0x33000000u:0xb3000000u),
                    cases::value(negative?0xbdcccccdu:0x3dcccccdu),0};
        v.pending_position_delta={0,0,0};v.motion[0]=0;v.response_marker=15;
        v.candidate_position={v.position[0],cases::value(negative?0x3cccccccu:0x3e666666u),0};
    } else {
        v.candidate_position={negative?0.625f:-0.625f,0.125f,0};
    }
    v.candidate_orientation={0.125f,-0.25f,0.0625f};
    s.contact_mask=hit?15u:0u;s.selected_scalar=hit?0.03125f:0;
    if(hit) {
        v.motion[0]=consuming?0.0625f:(negative?0.1875f:-0.0625f);
        v.candidate_position[0]=consuming?cases::value(negative?0x3d800004u:0x3d7ffff8u):(negative?0.6875f:-0.5625f);
        if(feedback)s.feedback={0,0};
        for(auto& c:s.contacts)c={{1,0,0},0.03125f,128};
    }else for(auto& c:s.contacts)c.status=15;
    return s;
}
bool run() {
    auto polygon=cases::square();ContactSegment segment{{&polygon,1}};
    MainContactStepInput input;input.course={&segment,1};
    unsigned controls=0,rejections=0;std::uint64_t hash=14695981039346656037ull;
    for(unsigned flags=0;flags<16;++flags) {
        const bool consuming=flags&1,negative=flags&2,hit=flags&4,feedback=flags&8;
        polygon=cases::square(hit?0:32);input.feedback_enabled=feedback;
        auto s=initial(consuming,negative);const auto want=cases::words(expected(consuming,negative,hit,feedback));
        if(!resolve_main_contact_step(input,s)||cases::words(s)!=want) {
            const auto got=cases::words(s);
            for(unsigned i=0;i<46;++i)if(got[i]!=want[i])std::printf("MAIN_CONTACT_STEP mismatch control=%u field=%u expected=%08x got=%08x\n",flags,i,want[i],got[i]);
            return false;
        }
        cases::hash(hash,cases::words(s));++controls;
    }
    for(unsigned mutation=0;mutation<3;++mutation) {
        polygon=cases::square();input={};input.course={&segment,1};auto s=initial(true,false);
        if(mutation==0)s.vehicle.motion[1]=cases::value(0x7f800000);
        if(mutation==1)input.segment=-1;
        if(mutation==2)input.scalar_divisor=0;
        const auto before=cases::words(s);
        if(resolve_main_contact_step(input,s)||cases::words(s)!=before)return false;
        ++rejections;
    }
    // Make three new contacts before the fourth result's divisor rejects.
    // The whole candidate prefix, all previous contacts and all other state
    // must roll back, not merely the final response aggregation.
    std::array<ContactPolygon,2> polygons{cases::square(),cases::square(8)};
    ContactSegment late_segment{polygons};input={};input.course={&late_segment,1};input.local_points[3].x=8;
    auto s=initial(true,false);s.vehicle.orientation={};s.vehicle.angular_motion={};
    const auto before=cases::words(s);auto control=s;
    if(!resolve_main_contact_step(input,control)||control.contact_mask!=15||control.vehicle.response_marker!=15)return false;
    polygons[1].coefficients[3]=0;
    if(resolve_main_contact_step(input,s)||cases::words(s)!=before)return false;++rejections;
    if(controls!=16||hash!=0x76ae45f11adbe279ull||rejections!=4)return false;
    std::printf("MAIN_CONTACT_STEP_PROBE PASS controls=%u hash=%016llx\n",controls,(unsigned long long)hash);
    std::printf("MAIN_CONTACT_STEP_ATOMIC PASS rejections=%u fourth_contact_control=1\n",rejections);
    return true;
}
}
int main(){if(!run())return 1;std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
