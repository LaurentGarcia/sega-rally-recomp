#pragma once
#include "contact_search.h"
#include <array>
#include <bit>
#include <cstdint>
namespace contact_search_cases {
using namespace ps2::native;
struct Case {
    std::array<std::array<ContactPolygon, 4>, 3> polygons;
    std::int16_t current;
    ContactVertex point{0.5f, std::bit_cast<float>(0x7fc12345u), -0.5f};
    bool matched;
    unsigned segment, polygon;
};
template<class Test> bool each(Test test, bool extended) {
    const unsigned masks=extended?4096:64;
    for(unsigned mask=0;mask<masks;++mask)for(unsigned current=0;current<3;++current){
        Case c{};c.current=static_cast<std::int16_t>(current);
        for(unsigned segment=0;segment<3;++segment)for(unsigned polygon=0;polygon<4;++polygon){
            auto &p=c.polygons[segment][polygon];
            const bool hit=(mask>>(segment*4+polygon))&1;
            const float left=hit?-1.0f:10.0f,right=hit?1.0f:12.0f;
            p.vertices={ContactVertex{left, std::bit_cast<float>(0x80000000u), -1},
                        ContactVertex{right, std::bit_cast<float>(0x7f800001u), -1},
                        ContactVertex{right, std::bit_cast<float>(0x7fc12345u), 1},
                        ContactVertex{left, std::bit_cast<float>(0xff800000u), 1}};
            p.coefficients={std::bit_cast<float>(0xffc98765u), std::bit_cast<float>(0x80000001u),
                            float(segment),float(polygon)};
            // Every other polygon uses three vertices; the query is strictly
            // inside both original shapes. Other attribute bits are payload.
            p.attributes=((mask+segment*4+polygon)<<8)|((polygon&1)?4u:0u);
        }
        for(auto segment:{current,(current+1)%3,(current+2)%3}){
            for(unsigned polygon=0;polygon<4;++polygon)if((mask>>(segment*4+polygon))&1){
                c.matched=true;c.segment=segment;c.polygon=polygon;break;
            }
            if(c.matched)break;
        }
        if(!test(c))return false;
    }
    return true;
}
inline std::array<std::uint32_t,17> payload(const ContactPolygon &p){
    std::array<std::uint32_t,17> result{};
    for(unsigned i=0;i<4;++i){result[3*i]=std::bit_cast<std::uint32_t>(p.vertices[i].x);
        result[3*i+1]=std::bit_cast<std::uint32_t>(p.vertices[i].y);result[3*i+2]=std::bit_cast<std::uint32_t>(p.vertices[i].z);
        result[12+i]=std::bit_cast<std::uint32_t>(p.coefficients[i]);}
    result[16]=p.attributes;return result;
}
} // namespace contact_search_cases
