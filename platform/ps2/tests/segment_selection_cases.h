#pragma once
#include "segment_selection.h"
#include "contact_projection_cases.h"
#include <bit>
#include <vector>
namespace segment_selection_cases {
using namespace ps2::native;
struct Case {
    std::vector<ContactPointXZ> vertices;
    std::vector<CourseSegmentCorners> segments;
    ContactPointXZ point{};
    std::int16_t progression = 0;
};
inline std::array<std::uint32_t,3> words(const SegmentSelection& r) {
    return {static_cast<std::uint32_t>(r.selected),std::bit_cast<std::uint16_t>(r.current),
        std::bit_cast<std::uint16_t>(r.progression)};
}
inline bool accepts(const std::array<ContactPointXZ,4>& v,const ContactPointXZ& p) {
    std::uint32_t all=0x80000000u,any=0;
    std::array<float,4> x{},z{};
    for(unsigned i=0;i<4;++i){volatile float dx=v[i].x-p.x,dz=v[i].z-p.z;x[i]=dx;z[i]=dz;}
    for(unsigned i=0;i<4;++i){const auto j=(i+1)%4;
        volatile float a=x[i]*z[j],b=x[j]*z[i],cross=a-b;
        const auto sign=std::bit_cast<std::uint32_t>(float(cross))&0x80000000u;all&=sign;any|=sign;}
    return all||!any;
}
inline SegmentSelection mathematical(const Case& c) {
    const int n=int(c.segments.size());int center=int(c.progression);
    while(center<0)center+=n;center%=n;
    for(int rank=0;rank<n;++rank){const int delta=rank%2?(rank+1)/2:-rank/2;
        const int index=(center+n+delta)%n;std::array<ContactPointXZ,4> q;
        for(unsigned j=0;j<4;++j)q[j]=c.vertices[c.segments[index][j]];
        if(accepts(q,c.point))return {index,std::bit_cast<std::int16_t>(std::uint16_t(index)),
            std::bit_cast<std::int16_t>(std::uint16_t(int(c.progression)+delta))};
    }return {};
}
inline Case boxes(unsigned count,int hit,std::int16_t progression) {
    Case c{{{8,8},{9,8},{9,9},{8,9},{-1,-1},{1,-1},{1,1},{-1,1}},
        std::vector<CourseSegmentCorners>(count,{0,1,2,3}),{0,0},progression};
    if(hit>=0)c.segments[unsigned(hit)]={4,5,6,7};return c;
}
template<class Test> bool each(Test test,bool extended) {
    // Search all offsets and misses for odd/even record counts and both ends
    // of the signed progression domain, including modulo16 output wrap.
    for(unsigned n=1;n<=17;++n)for(std::int16_t start:{std::int16_t(-32768),std::int16_t(-17),
        std::int16_t(-1),std::int16_t(0),std::int16_t(1),std::int16_t(17),std::int16_t(32767)})
        for(int hit=-1;hit<int(n);++hit)if(!test(boxes(n,hit,start)))return false;
    // Reuse original test shapes, edge/corner and signed-zero/subnormal cases.
    if(!contact_projection_cases::each([&](const contact_projection_cases::Case& p){
        if(p.count!=4)return true;Case c;c.segments.push_back({0,1,2,3});
        for(unsigned i=0;i<4;++i)c.vertices.push_back({std::bit_cast<float>(p.values[2*i]),std::bit_cast<float>(p.values[2*i+1])});
        c.point={std::bit_cast<float>(p.values[8]),std::bit_cast<float>(p.values[9])};return test(c);
    },false))return false;
    for(unsigned n:{32767u,32768u,65535u})for(std::int16_t start:{std::int16_t(-32768),std::int16_t(-1),std::int16_t(32767)}){
        auto c=boxes(n,0,start);for(auto& q:c.segments)q={4,5,6,7};if(!test(c))return false;
    }
    if(extended)for(unsigned raw=0;raw<65536;++raw)
        if(!test(boxes(1,0,std::bit_cast<std::int16_t>(std::uint16_t(raw)))))return false;
    return true;
}
inline void hash(std::uint64_t& h,const SegmentSelection& r){for(auto v:words(r))h=(h^v)*1099511628211ull;}
} // namespace segment_selection_cases
