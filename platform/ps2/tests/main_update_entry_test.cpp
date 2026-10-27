#include "main_update_entry.h"
#include <array>
#include <bit>
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
using namespace ps2::native;
auto words(const MainEntryState &s) {
    return std::array<std::uint32_t,7>{s.flags,std::bit_cast<std::uint32_t>(s.steering_history),
        std::bit_cast<std::uint32_t>(s.steering),std::bit_cast<std::uint16_t>(s.segment),
        std::bit_cast<std::uint16_t>(s.progression),std::bit_cast<std::uint32_t>(s.published_segment),
        std::bit_cast<std::uint32_t>(s.published_progression)};
}
bool tests() {
    const std::array<ContactPointXZ,4> vertices{{{-1,-1},{1,-1},{1,1},{-1,1}}};
    const std::array<CourseSegmentCorners,1> faces{{{0,1,2,3}}};
    MainEntryInput input;input.vertices=vertices;input.segments=faces;input.position={0,0};
    MainEntryState initial{0xdeadbeefu,0,1,-17,0,71,72};unsigned checks=0;
    auto check=[&](bool result) { ++checks;return result; };
    // The flag input is a bitfield, not a boolean. Other bits survive intact.
    for(unsigned sample:{0u,2u,3u,254u,255u}) {
        auto s=initial;input.flag_sample=sample;
        if(!check(begin_main_update(input,s)&&s.flags==((initial.flags&~0x80u)|((sample&1u)<<7))&&
                  s.segment==0&&s.progression==0&&s.published_segment==0&&s.published_progression==0))return false;
    }
    // Screen9 deliberately bypasses the filter: even an invalid history is
    // preserved raw, and steering becomes positive zero.
    input.screen=9;input.steering_sample=0;auto s=initial;
    s.steering_history=std::bit_cast<float>(0x7fc12345u);
    if(!check(begin_main_update(input,s)&&std::bit_cast<std::uint32_t>(s.steering_history)==0x7fc12345u&&
              std::bit_cast<std::uint32_t>(s.steering)==0))return false;
    input.position={10,10};s=initial;
    if(!check(begin_main_update(input,s)&&s.segment==0&&s.progression==-1&&
              s.published_segment==0&&s.published_progression==-1))return false;
    auto rejected=[&](const MainEntryInput &bad,MainEntryState before) {
        const auto original=words(before);return check(!begin_main_update(bad,before)&&words(before)==original);
    };
    auto bad=input;bad.surrounding_car_update=true;if(!rejected(bad,initial))return false;
    bad=input;bad.segments={};if(!rejected(bad,initial))return false;
    bad=input;bad.vertices={};if(!rejected(bad,initial))return false;
    const std::array<CourseSegmentCorners,1> broken{{{0,1,2,4}}};bad=input;bad.segments=broken;
    if(!rejected(bad,initial))return false;
    bad=input;bad.position.x=std::bit_cast<float>(0x7f800000u);if(!rejected(bad,initial))return false;
    bad=input;bad.screen=0;s=initial;s.steering_history=std::bit_cast<float>(0x3f800001u);
    if(!rejected(bad,s))return false;
    // Malformed course data fails after a valid filter computation without
    // committing that new history or the flag bit to the caller's state.
    bad=input;bad.screen=0;bad.steering_sample=0;bad.segments=broken;
    if(!rejected(bad,initial))return false;
    std::printf("MAIN_UPDATE_ENTRY_TEST PASS checks=%u\n",checks);return checks==14;
}
}
int main() {if(!tests())return 1;std::fflush(stdout);
#ifdef _EE
SleepThread();
#endif
return 0;}
