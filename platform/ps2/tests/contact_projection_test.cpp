#include "contact_projection.h"
#include "contact_projection_cases.h"
#include <bit>
#include <cstdio>
#include <cstdint>
#include <span>
using namespace ps2::native;
namespace {
#ifndef _EE
ContactProjection reference(std::span<const ContactPointXZ> vertices, ContactPointXZ point) {
    const auto round=[](double x){return static_cast<float>(x);};
    ContactProjection result;
    bool negative=false,positive=false;
    for(unsigned i=0;i<vertices.size();++i){
        const auto &a=vertices[i], &b=vertices[(i+1)%vertices.size()];
        const float ax=round(double(a.x)-point.x), az=round(double(a.z)-point.z);
        const float bx=round(double(b.x)-point.x), bz=round(double(b.z)-point.z);
        const float left=round(double(ax)*bz),right=round(double(bx)*az);
        const float cross=round(double(left)-right);
        result.edge_cross[i]=cross;
        if(std::bit_cast<std::uint32_t>(cross)>>31)negative=true;else positive=true;
    }
    result.accepted=!(negative&&positive); return result;
}
#endif
}
int main(){
    unsigned count=0,rejections=0;std::uint64_t hash=14695981039346656037ull;
    const auto test=[&](const contact_projection_cases::Case &c){
        std::array<ContactPointXZ,4> vertices;
        for(unsigned i=0;i<4;++i)vertices[i]={std::bit_cast<float>(c.values[2*i]),std::bit_cast<float>(c.values[2*i+1])};
        ContactPointXZ point{std::bit_cast<float>(c.values[8]),std::bit_cast<float>(c.values[9])};
        ContactProjection out;
        const auto span=std::span<const ContactPointXZ>(vertices.data(),c.count);
        if(!contact_projection(span,point,out))return false;
#ifndef _EE
        const auto expected=reference(span,point);
        for(unsigned i=0;i<4;++i)if(std::bit_cast<std::uint32_t>(out.edge_cross[i])!=std::bit_cast<std::uint32_t>(expected.edge_cross[i])) {
            std::printf("CONTACT CROSS MISMATCH case=%u edge=%u\n",count,i);return false;
        }
        if(out.accepted!=expected.accepted)return false;
#endif
        hash=(hash^c.count)*1099511628211ull;
        for(auto x:c.values)hash=(hash^x)*1099511628211ull;
        for(auto x:out.edge_cross)hash=(hash^std::bit_cast<std::uint32_t>(x))*1099511628211ull;
        hash=(hash^out.accepted)*1099511628211ull;++count;return true;
    };
    if(!contact_projection_cases::each(test,false))return 1;
    std::printf("CONTACT_PROJECTION_PROBE PASS cases=%u hash=%016llx\n",count,(unsigned long long)hash);
#ifndef _EE
    count=0;hash=14695981039346656037ull;
    if(!contact_projection_cases::each(test,true))return 1;
    std::printf("CONTACT_PROJECTION_HOST PASS cases=%u hash=%016llx\n",count,(unsigned long long)hash);
#endif
    const ContactProjection sentinel{{1,2,3,4},true};
    for(auto bad:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u})
        for(unsigned field=0;field<10;++field){
            std::array<ContactPointXZ,4> vertices{};ContactPointXZ point{};
            if(field<8){if(field&1)vertices[field/2].z=std::bit_cast<float>(bad);else vertices[field/2].x=std::bit_cast<float>(bad);}
            else if(field==8)point.x=std::bit_cast<float>(bad);else point.z=std::bit_cast<float>(bad);
            auto out=sentinel;if(contact_projection(vertices,point,out)||out.edge_cross!=sentinel.edge_cross||out.accepted!=sentinel.accepted)return 1;
            ++rejections;
        }
    std::array<ContactPointXZ,5> vertices{};
    for(unsigned n:{0u,1u,2u,5u}){auto out=sentinel;if(contact_projection(std::span(vertices.data(),n),{},out)||out.edge_cross!=sentinel.edge_cross||!out.accepted)return 1;++rejections;}
    std::printf("CONTACT_PROJECTION_REJECTION PASS cases=%u\n",rejections);
}
