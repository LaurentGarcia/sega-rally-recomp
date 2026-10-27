#include "main_contact_step.h"
#include <cstring>

namespace ps2::native {
bool resolve_main_contact_step(const MainContactStepInput& input,
                               MainContactStepState& state) noexcept {
    MainContactStepState result;
    std::memcpy(&result,&state,sizeof result);
    auto& vehicle=result.vehicle;
    if(!prepare_main_candidate(vehicle))return false;
    CandidateContactPassInput request;
    request.course=input.course;request.normal_table=input.normal_table;
    request.local_points=input.local_points;request.segment=input.segment;
    request.response_parameter=input.response_parameter;
    request.gain_parameter=input.gain_parameter;
    request.scalar_divisor=input.scalar_divisor;
    request.bases={vehicle.position[0],vehicle.position[2],vehicle.orientation[1]};
    request.feedback_enabled=input.feedback_enabled;
    CandidateContactPassState contact;
    contact.contacts=result.contacts;
    contact.response.car.motion={vehicle.motion[0],vehicle.motion[2],vehicle.angular_motion[1]};
    contact.response.car.candidate_position=vehicle.candidate_position;
    contact.response.car.candidate_orientation=vehicle.candidate_orientation;
    contact.response.car.feedback=result.feedback;
    contact.response.contact_mask=result.contact_mask;
    contact.response.selected_scalar=result.selected_scalar;
    if(!resolve_candidate_contact_pass(request,contact))return false;
    vehicle.motion[0]=contact.response.car.motion[0];
    vehicle.motion[2]=contact.response.car.motion[1];
    vehicle.angular_motion[1]=contact.response.car.motion[2];
    vehicle.candidate_position=contact.response.car.candidate_position;
    vehicle.candidate_orientation=contact.response.car.candidate_orientation;
    result.feedback=contact.response.car.feedback;
    result.contact_mask=contact.response.contact_mask;
    result.selected_scalar=contact.response.selected_scalar;
    result.contacts=contact.contacts;
    std::memcpy(&state,&result,sizeof result);
    return true;
}
} // namespace ps2::native
