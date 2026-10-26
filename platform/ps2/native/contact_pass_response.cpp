#include "contact_pass_response.h"
#include "contact_response.h"
#include "rounded_cross_product.h"
#include "recovered_scalar_metric.h"
#include <bit>
#include <cstring>
namespace ps2::native {
bool resolve_contact_pass_response(const ContactPassResponseInput& input,
                                   ContactPassResponseState& state) noexcept {
    ContactPassResponseState result;
    std::memcpy(&result,&state,sizeof result);
    result.contact_mask=0;result.selected_scalar=0;
    std::array<ContactVertex,4> responses{};
    for(unsigned i=0;i<4;++i) {
        ContactResponseOutput response;
        const ContactResponseInput request{input.contacts[i],input.offsets[i],
                                           input.response_parameter,input.gain_parameter};
        if(!make_contact_response(request,result.selected_scalar,response))return false;
        responses[i]=response.response;result.selected_scalar=response.selected_scalar;
        if(response.active)result.contact_mask|=1u<<i;
    }
    if(std::bit_cast<std::uint32_t>(result.selected_scalar)&0x7fffffffu) {
        float scalar=0;
        ContactAggregateInput aggregate;
        for(unsigned i=0;i<4;++i) {
            // The original active aggregate path overwrites both middle
            // components with positive zero before the cross operation.
            const CrossVector3 offset{input.offsets[i].x,0,input.offsets[i].z};
            const CrossVector3 response{responses[i].x,0,responses[i].z};
            CrossVector3 cross;
            if(!rounded_cross_product(offset,response,cross))return false;
            // The original wire order is 0,0,0,cross.x,cross.y,cross.z;
            // command58 interleaves the coordinates of its two points.
            float metric;
            if(!recovered_scalar_metric({0,0,cross.y},{0,cross.x,cross.z},metric)||
               !accumulate_contact_scalar(scalar,metric,cross.y,scalar))return false;
            aggregate.responses[i]={response.x,response.y,response.z};
        }
        aggregate.signed_scalar=scalar;aggregate.bases=input.bases;
        aggregate.response_divisor=input.response_parameter;
        aggregate.scalar_divisor=input.scalar_divisor;
        aggregate.feedback_enabled=input.feedback_enabled;
        if(!apply_contact_aggregate(aggregate,result.car))return false;
    }
    std::memcpy(&state,&result,sizeof result);return true;
}
} // namespace ps2::native
