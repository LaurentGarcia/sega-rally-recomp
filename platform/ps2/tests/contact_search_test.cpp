#include "contact_search_cases.h"
#include <cstdio>
#include <vector>
using namespace ps2::native;
int main(){
    unsigned checks=0;std::uint64_t hash=14695981039346656037ull;
    const auto test=[&](const contact_search_cases::Case &c){
        std::array<ContactSegment,3> course;
        for(unsigned i=0;i<3;++i)course[i].polygons=c.polygons[i];
        ContactSearchResult result;
        if(!search_course_contact(course,c.current,c.point,result)||result.matched!=c.matched)return false;
        const auto expected=c.matched?contact_search_cases::payload(c.polygons[c.segment][c.polygon]):std::array<std::uint32_t,17>{};
        if(contact_search_cases::payload(result.data)!=expected||result.segment!=(c.matched?c.segment:0)||result.polygon!=(c.matched?c.polygon:0))return false;
        hash=(hash^result.matched)*1099511628211ull;hash=(hash^result.segment)*1099511628211ull;hash=(hash^result.polygon)*1099511628211ull;
        for(auto value:expected)hash=(hash^value)*1099511628211ull;++checks;return true;
    };
    if(!contact_search_cases::each(test,false))return 1;
    std::printf("CONTACT_SEARCH_PROBE PASS cases=%u hash=%016llx\n",checks,(unsigned long long)hash);
#ifndef _EE
    checks=0;hash=14695981039346656037ull;
    if(!contact_search_cases::each(test,true))return 1;
    std::printf("CONTACT_SEARCH_HOST PASS cases=%u hash=%016llx\n",checks,(unsigned long long)hash);
#endif
    // Repeated neighbors and one-/two-segment wrap remain valid. A zero
    // attribute word is still a geometric match, though a later CPU adapter
    // may interpret its reply as no contact.
    ContactPolygon hit;hit.vertices={ContactVertex{-1,0,-1},ContactVertex{1,0,-1},ContactVertex{1,0,1},ContactVertex{-1,0,1}};
    std::array<ContactSegment,2> course{ContactSegment{std::span(&hit,1)},ContactSegment{std::span(&hit,1)}};
    unsigned edge_checks=0;
    for(unsigned n:{1u,2u})for(unsigned i=0;i<n;++i){ContactSearchResult out;
        if(!search_course_contact(std::span(course.data(),n),static_cast<std::int16_t>(i),{0.5f,0,-0.5f},out)||!out.matched||out.segment!=i||out.data.attributes)return 1;++edge_checks;}
    ContactSearchResult sentinel;sentinel.matched=true;sentinel.segment=23;sentinel.polygon=44;sentinel.data=hit;
    const auto reject=[&](std::span<const ContactSegment> input,std::int16_t index,ContactVertex point){
        auto out=sentinel;const auto ok=search_course_contact(input,index,point,out);
        ++edge_checks;return !ok&&out.matched==sentinel.matched&&out.segment==sentinel.segment&&out.polygon==sentinel.polygon&&contact_search_cases::payload(out.data)==contact_search_cases::payload(sentinel.data);
    };
    if(!reject({},0,{})||!reject(course,-1,{})||!reject(course,2,{})||!reject(course,0,{std::bit_cast<float>(0x7fc00000u),0,0}))return 1;
    ContactSegment empty{};if(!reject(std::span(&empty,1),0,{}))return 1;
    std::array<ContactPolygon,256> many{};ContactSegment oversized{many};if(!reject(std::span(&oversized,1),0,{}))return 1;
    many[254]=hit;ContactSegment maximum{std::span(many.data(),255)};
    // Degenerate zero geometry itself matches; mark every earlier polygon far away.
    for(unsigned i=0;i<254;++i){many[i]=hit;for(auto &v:many[i].vertices)v.x+=10;}
    ContactSearchResult out;if(!search_course_contact(std::span(&maximum,1),0,{0.5f,0,-0.5f},out)||!out.matched||out.polygon!=254)return 1;++edge_checks;
#ifndef _EE
    std::vector<ContactSegment> large(65536,course[0]);if(!reject(large,0,{}))return 1;
    if(!search_course_contact(std::span(large.data(),65535),32767,{0.5f,0,-0.5f},out)||!out.matched||out.segment!=32767)return 1;++edge_checks;
#endif
    std::printf("CONTACT_SEARCH_BOUNDARIES PASS checks=%u\n",edge_checks);
}
