#include "segment_selection_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif
namespace {
namespace cases=segment_selection_cases;
using namespace ps2::native;
bool run(bool extended){
    unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const cases::Case& c){SegmentSelection r;
        if(!select_course_segment(c.vertices,c.segments,c.point,c.progression,r))return false;
#ifndef _EE
        if(cases::words(r)!=cases::words(cases::mathematical(c)))return false;
#endif
        cases::hash(hash,r);++count;return true;
    },extended))return false;
    if(count!=(extended?69170u:3634u)||hash!=(extended?0xb4ab429abeb08285ull:0x1c1c549c187f8285ull))return false;
    std::printf("SEGMENT_SELECTION_%s PASS cases=%u hash=%016llx\n",extended?"HOST":"PROBE",count,(unsigned long long)hash);return true;
}
bool guards(){
    unsigned count=0;const SegmentSelection sentinel{123,234,345};
    auto test=[&](const cases::Case& c){auto r=sentinel;++count;
        return !select_course_segment(c.vertices,c.segments,c.point,c.progression,r)&&cases::words(r)==cases::words(sentinel);};
    auto c=cases::boxes(1,0,0);auto saved=c;c.vertices.clear();if(!test(c))return false;c=saved;c.segments.clear();if(!test(c))return false;
    c=saved;c.segments.resize(65536);if(!test(c))return false;c=saved;c.vertices.resize(65537);if(!test(c))return false;
    c=saved;c.segments[0][2]=8;if(!test(c))return false;
    for(auto raw:{0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7fc12345u}){
        for(unsigned i=0;i<10;++i){c=saved;auto& v=i<8?(i%2?c.vertices[4+i/2].z:c.vertices[4+i/2].x):(i%2?c.point.z:c.point.x);
            v=std::bit_cast<float>(raw);if(!test(c))return false;}}
    // Later malformed records are not read after a first match.
    c=cases::boxes(2,0,0);c.segments[1]={65535,65535,65535,65535};SegmentSelection r;
    if(!select_course_segment(c.vertices,c.segments,c.point,0,r)||r.selected!=0)return false;
    // If both adjacent candidates accept, the positive neighbor wins.
    c=cases::boxes(3,1,0);c.segments[2]={4,5,6,7};if(!select_course_segment(c.vertices,c.segments,c.point,0,r)||r.selected!=1||r.progression!=1)return false;
    std::printf("SEGMENT_SELECTION_REJECTION PASS checks=%u controls=2\n",count);return true;
}
}
int main(){if(!run(false)||!guards())return 1;
#ifndef _EE
if(!run(true))return 1;
#endif
std::fflush(stdout);
#ifdef _EE
SleepThread();
#endif
}
