#include "candidate_contact_pass.h"
#include "contact_point_preparation.h"
#include <cstring>
namespace ps2::native {
bool resolve_candidate_contact_pass(const CandidateContactPassInput& input,
                                    CandidateContactPassState& state) noexcept {
    CandidateContactPassState result;
    std::memcpy(&result,&state,sizeof result);
    const auto& position=state.response.car.candidate_position;
    const auto& orientation=state.response.car.candidate_orientation;
    ContactPointPreparation prepared;
    if(!prepare_contact_points({orientation[0],orientation[1],orientation[2]},
                               {position[0],position[1],position[2]},input.local_points,prepared))return false;
    ContactPassResponseInput response;
    response.response_parameter=input.response_parameter;response.gain_parameter=input.gain_parameter;
    response.scalar_divisor=input.scalar_divisor;response.bases=input.bases;response.feedback_enabled=input.feedback_enabled;
    for(unsigned i=0;i<4;++i){
        const auto& p=prepared.points[i];const ContactVertex point{p.x,p.y,p.z};ContactSearchResult selected;
        if(!search_course_contact(input.course,input.segment,point,selected)||
           !interpret_contact_result(selected.data,point,input.normal_table,result.contacts[i]))return false;
        const auto& offset=prepared.offsets[i];response.offsets[i]={offset.x,offset.y,offset.z};
        response.contacts[i]=result.contacts[i];
    }
    if(!resolve_contact_pass_response(response,result.response))return false;
    std::memcpy(&state,&result,sizeof result);return true;
}
} // namespace ps2::native
