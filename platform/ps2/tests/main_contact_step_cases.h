#pragma once
#include "main_contact_step.h"
#include "main_candidate_cases.h"

namespace main_contact_step_cases {
using namespace ps2::native;
using main_candidate_cases::bits;
using main_candidate_cases::value;
using Words=std::array<std::uint32_t,46>;
inline Words words(const MainContactStepState& s) {
    Words result{};const auto candidate=main_candidate_cases::words(s.vehicle);
    for(unsigned i=0;i<22;++i)result[i]=candidate[i];
    for(unsigned i=0;i<2;++i)result[22+i]=std::bit_cast<std::uint32_t>(s.feedback[i]);
    result[24]=s.contact_mask;result[25]=bits(s.selected_scalar);
    for(unsigned i=0;i<4;++i) {
        for(unsigned j=0;j<3;++j)result[26+5*i+j]=bits(s.contacts[i].direction[j]);
        result[29+5*i]=bits(s.contacts[i].gap);result[30+5*i]=s.contacts[i].status;
    }
    return result;
}
inline MainContactStepState state(const Words& w) {
    MainContactStepState result;main_candidate_cases::Words candidate{};
    for(unsigned i=0;i<22;++i)candidate[i]=w[i];result.vehicle=main_candidate_cases::state(candidate);
    for(unsigned i=0;i<2;++i)result.feedback[i]=std::bit_cast<std::int32_t>(w[22+i]);
    result.contact_mask=w[24];result.selected_scalar=value(w[25]);
    for(unsigned i=0;i<4;++i) {
        for(unsigned j=0;j<3;++j)result.contacts[i].direction[j]=value(w[26+5*i+j]);
        result.contacts[i].gap=value(w[29+5*i]);result.contacts[i].status=w[30+5*i];
    }
    return result;
}
inline void hash(std::uint64_t& h,const Words& w){for(auto v:w)h=(h^v)*1099511628211ull;}
inline ContactPolygon square(float translation=0) {
    ContactPolygon p;p.vertices={ContactVertex{-4+translation,0,-4},{4+translation,0,-4},
                                {4+translation,0,4},{-4+translation,0,4}};
    p.coefficients={1,0,0,1024};p.attributes=0x00800004u;return p;
}
} // namespace main_contact_step_cases
