// Separate offline reference executable. Cabinet replay only, no game-RAM edits.
#include "visual_asset_capture.h"
#include "visual_camera_capture.h"
#include "platform/ps2/game/rom_cache.h"
#include "runtime/model2a_machine.h"
#include "runtime/video.h"
#include "tools/srally_boot/input_replay.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
namespace {
unsigned number(const char*arg){unsigned n=0;const std::string s(arg);const auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())throw std::runtime_error("invalid visual frame argument");return n;}
}
int main(int argc,char**argv){try{
    if(argc!=7)throw std::runtime_error("usage: visual-reference PACK INPUTS FRAMES DIRECTORY|- FIRST LAST");
    const auto frames=number(argv[3]),first=number(argv[5]),last=number(argv[6]);
    if(!first||first>last||last-first>8||last>=frames||frames>10000)throw std::runtime_error("invalid visual interval");
    const bool capture=std::string(argv[4])!="-";
    std::ifstream stream(argv[2]);if(!stream)throw std::runtime_error("cannot read cabinet replay");srally::InputReplay replay(stream);
    auto cache=std::make_unique<rally::ps2::RomCache>(argv[1],128);
    rt::Model2AMachine::Images images;images.rom=cache.get();images.tgp_tables=cache->tgp_tables();
    rt::Model2AMachine::Options options;options.sound=false;options.instruction_boundary_hook=true;
    rt::Model2AMachine machine(std::move(images),options);
    if(capture)ps2::offline::begin_visual_camera_capture((std::string(argv[4])+"/camera").c_str(),first,last);
    std::puts("frame,instructions,cycles,board_ns,pc,register_hash,screen_hash");
    for(unsigned frame=1;frame<=frames;++frame){
        replay.apply(frame-1,machine.inputs());
        const bool sample=capture&&frame-1>=first&&frame-1<=last;
        if(sample)ps2::offline::begin_visual_asset_frame((std::string(argv[4])+"/frame-"+std::to_string(frame-1)).c_str(),frame-1);
        rt::Model2AMachine::RunResult result;
        for(unsigned remaining=50000000;remaining;){const unsigned n=std::min(remaining,4096u);result=machine.run_until_frame(n);if(result.reason!=rt::Model2AMachine::Reason::Budget)break;remaining-=n;}
        if(result.reason!=rt::Model2AMachine::Reason::Frame)throw std::runtime_error("visual reference stopped: "+result.message);
        if(sample)ps2::offline::end_visual_asset_frame(machine.video());
        const auto s=machine.stats(true);std::uint64_t hash=0xcbf29ce484222325ull;
        for(auto word:machine.cpu().m_r)for(unsigned shift=0;shift<32;shift+=8)hash=(hash^std::uint8_t(word>>shift))*0x100000001b3ull;
        std::printf("%u,%llu,%llu,%llu,%08x,%016llx,%016llx\n",frame,(unsigned long long)s.instructions,
            (unsigned long long)s.reference_cycles,(unsigned long long)s.board_time_ns,unsigned(s.pc),
            (unsigned long long)hash,(unsigned long long)s.screen_hash);
    }
    if(replay.applied()!=replay.size())throw std::runtime_error("visual run did not apply complete cabinet replay");
    if(capture)ps2::offline::end_visual_camera_capture();
    std::fprintf(stderr,"COMPLETE frames=%u input_events=%zu\n",frames,replay.applied());return 0;
}catch(const std::exception&e){std::fprintf(stderr,"Visual capture failed: %s\n",e.what());return 1;}}
