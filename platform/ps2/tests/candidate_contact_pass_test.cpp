#include "candidate_contact_pass.h"
#include "contact_pass_response_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
using namespace ps2::native;
namespace cases=contact_pass_response_cases;
using Words=std::array<std::uint32_t,33>;
Words words(const CandidateContactPassState& s) {
    Words output{};const auto state=cases::words(s.response);
    for(unsigned i=0;i<state.size();++i)output[i]=state[i];
    for(unsigned i=0;i<4;++i) {
        const auto& c=s.contacts[i];
        for(unsigned j=0;j<3;++j)output[13+5*i+j]=cases::bits(c.direction[j]);
        output[16+5*i]=cases::bits(c.gap);output[17+5*i]=c.status;
    }
    return output;
}
ContactPolygon quad() {
    ContactPolygon p;p.vertices={ContactVertex{-4,0,-4},{4,0,-4},{4,0,4},{-4,0,4}};
    p.coefficients={1,0,0,1024};p.attributes=0x00800004u;return p;
}
bool run() {
    auto polygon=quad();ContactSegment segment{{&polygon,1}};
    CandidateContactPassInput input;input.course={&segment,1};input.feedback_enabled=true;input.bases={2,3,0.25f};
    CandidateContactPassState state;state.response.car.motion={0.5f,-0.5f,0.125f};
    state.response.car.feedback={-123,456};
    if(!resolve_candidate_contact_pass(input,state))return false;
    // At the centre of this authored square, the first edge determinant is32,
    // giving gap1/32. With zero offsets and unit parameters each response is
    // +X/64; four responses add1/16 to the first motion component, no torque.
    const cases::Words expected{0x3f100000,0xbf000000,0x3e000000,0x40240000,0,0x40200000,
                                0,0x3ec00000,0,0,0,15,0x3d000000};
    if(cases::words(state.response)!=expected)return false;
    for(const auto& c:state.contacts)if(c.direction!=std::array<float,3>{1,0,0}||c.gap!=0.03125f||c.status!=128)return false;
    unsigned cases_count=0,active=0,misses=0;std::uint64_t hash=14695981039346656037ull;
    for(unsigned i=0;i<768;++i) {
        polygon=quad();input.feedback_enabled=(i&1)!=0;input.bases={0,0,0};
        input.local_points={AffinePoint3{-1,1,-1},{1,1,-1},{1,1,1},{-1,1,1}};
        state={};state.response.car.candidate_orientation={float(int(i%5)-2)*0.125f,float(int(i%7)-3)*0.0625f,float(int(i%3)-1)*0.125f};
        state.response.car.motion={0.125f,-0.25f,0.0625f};state.response.car.feedback={-123,456};
        state.response.contact_mask=0xdeadbeefu;state.response.selected_scalar=cases::value(0x7fc12345);
        for(auto& c:state.contacts)c={{cases::value(0x7f800001),cases::value(0xffc00001),cases::value(0x80000000)},cases::value(0x7fc12345),7};
        // Two-thirds active square contacts; remaining cases miss the course
        // and must preserve prior per-slot payload and the whole car state.
        const bool miss=i%3==0;if(miss)state.response.car.candidate_position[0]=32;
        const auto before=words(state);
        if(!resolve_candidate_contact_pass(input,state)||state.response.contact_mask!=(miss?0u:15u))return false;
        const auto after=words(state);
        if(miss) {
            if(cases::bits(state.response.selected_scalar)!=0)return false;
            for(unsigned j=0;j<11;++j)if(after[j]!=before[j])return false;
            for(unsigned j=0;j<4;++j) {
                for(unsigned k=0;k<4;++k)if(after[13+5*j+k]!=before[13+5*j+k])return false;
                if(state.contacts[j].status!=15)return false;
            }
            ++misses;
        }else {for(const auto& c:state.contacts)if(c.status!=128||c.direction!=std::array<float,3>{1,0,0}||!(c.gap>0))return false;++active;}
        for(auto w:after)hash=(hash^w)*1099511628211ull;++cases_count;
    }
    if(cases_count!=768||active!=512||misses!=256||hash!=0xb8ed3cad65bf6be4ull)return false;
    // Rejections both before work and after all four new contacts must leave
    // candidate state and each previous contact untouched.
    unsigned rejected=0;
    for(unsigned mutation=0;mutation<5;++mutation) {
        polygon=quad();input={};input.course={&segment,1};state={};state.response.car.feedback={-123,456};
        for(auto& c:state.contacts)c={{3,4,5},6,7};
        if(mutation==0)input.segment=-1;
        if(mutation==1)input.local_points[3].x=cases::value(0x7fc00000);
        if(mutation==2)polygon.coefficients[3]=0;
        if(mutation==3)input.scalar_divisor=0;
        if(mutation==4)state.response.car.motion[2]=cases::value(0x7f800000);
        const auto before=words(state);
        if(resolve_candidate_contact_pass(input,state)||words(state)!=before)return false;++rejected;
    }
    // Three valid new slots precede a fourth hit whose result interpretation
    // fails. This exercises rollback across the contact array itself.
    std::array<ContactPolygon,2> late_polygons{quad(),quad()};
    for(auto& v:late_polygons[1].vertices)v.x+=8;
    ContactSegment late_segment{late_polygons};input={};input.course={&late_segment,1};
    input.local_points[3].x=8;state={};
    for(auto& c:state.contacts)c={{3,4,5},6,7};
    state.response.contact_mask=0xdeadbeefu;state.response.selected_scalar=cases::value(0x7fc12345);
    const auto before=words(state);
    auto control=state;
    if(!resolve_candidate_contact_pass(input,control)||control.response.contact_mask!=15)return false;
    for(const auto& c:control.contacts)if(c.status!=128)return false;
    late_polygons[1].coefficients[3]=0;
    if(resolve_candidate_contact_pass(input,state)||words(state)!=before)return false;++rejected;
    std::printf("CANDIDATE_CONTACT_PASS_PROBE PASS cases=%u active=%u misses=%u hash=%016llx\n",cases_count,active,misses,(unsigned long long)hash);
    std::printf("CANDIDATE_CONTACT_PASS_GUARDS PASS known_geometry=1 atomic_rejections=%u\n",rejected);return true;
}
}
int main() {
    if(!run())return 1;std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
