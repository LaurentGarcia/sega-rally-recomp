// Separate offline host executable. It changes cabinet inputs, never game RAM.
#include "driving_capture.h"
#include "full_step_capture.h"
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
unsigned number(const char *argument) {
    unsigned result=0;const std::string text(argument);
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())
        throw std::runtime_error("invalid bounded frame argument");
    return result;
}
void image(const std::string &path,const rt::Video &video) {
    std::ofstream out(path,std::ios::binary);
    out << "P6\n" << video.width() << ' ' << rt::Video::H << "\n255\n";
    for (const auto pixel:video.screen()) {
        out.put(char(pixel>>16));out.put(char(pixel>>8));out.put(char(pixel));
    }
    out.close();if (!out) throw std::runtime_error("cannot write private driving reference image");
}
}
int main(int argc,char **argv) {
    try {
        if (argc!=9) throw std::runtime_error("usage: driving-reference PACK INPUTS FRAMES CAPTURE-DIR|- FIRST LAST SAMPLE-IMAGES0|1 FULL-STEP0|1");
        const auto frames=number(argv[3]),first=number(argv[5]),last=number(argv[6]);
        if (!frames || frames>10000 || last>=frames || first>last)
            throw std::runtime_error("invalid host capture frame bound");
        const bool capture=std::string(argv[4])!="-",images=number(argv[7])!=0;
        std::ifstream inputs(argv[2]);if (!inputs) throw std::runtime_error("cannot open input replay");
        srally::InputReplay replay(inputs);
        auto cache=std::make_unique<rally::ps2::RomCache>(argv[1],128);
        rt::Model2AMachine::Images assets;assets.rom=cache.get();assets.tgp_tables=cache->tgp_tables();
        rt::Model2AMachine::Options options;options.instruction_boundary_hook=true;options.sound=false;
        rt::Model2AMachine machine(std::move(assets),options);
        if (capture) ps2::offline::begin_driving_capture(argv[4],first,last);
        if (capture && number(argv[8])) ps2::offline::begin_full_step_capture(argv[4]);
        std::puts("frame,instructions,cycles,board_ns,pc,register_hash,screen_hash");
        for (unsigned frame=1;frame<=frames;++frame) {
            if (replay.apply(frame-1,machine.inputs()))
                std::fprintf(stderr,"INPUT completed_frame=%u events=%zu in0=%02x axis=%02x gas=%02x brake=%02x\n",
                    frame-1,replay.applied(),machine.inputs().in0,machine.inputs().analog[0],
                    machine.inputs().analog[1],machine.inputs().analog[2]);
            const auto &pins=machine.inputs();
            std::array<std::uint8_t,12> controls{pins.in0,pins.in1,pins.in2,pins.switches};
            std::copy(pins.analog.begin(),pins.analog.end(),controls.begin()+4);
            ps2::offline::full_step_controls(frame-1,controls);
            rt::Model2AMachine::RunResult result;
            for (unsigned remaining=50000000;remaining;) {
                const unsigned budget=std::min(remaining,4096u);
                result=machine.run_until_frame(budget);
                if (result.reason!=rt::Model2AMachine::Reason::Budget) break;
                remaining-=budget;
            }
            if (result.reason!=rt::Model2AMachine::Reason::Frame)
                throw std::runtime_error("reference stopped: "+result.message+" PC="+std::to_string(result.pc));
            const auto stats=machine.stats(true);
            std::uint64_t registers=0xcbf29ce484222325ull;
            for (auto word:machine.cpu().m_r) for (unsigned shift=0;shift<32;shift+=8)
                registers=(registers^std::uint8_t(word>>shift))*0x100000001b3ull;
            std::printf("%u,%llu,%llu,%llu,%08x,%016llx,%016llx\n",frame,
                (unsigned long long)stats.instructions,(unsigned long long)stats.reference_cycles,
                (unsigned long long)stats.board_time_ns,unsigned(stats.pc),
                (unsigned long long)registers,(unsigned long long)stats.screen_hash);
            if (capture && images && (frame==2301 || frame==2501 || frame==2701))
                image(std::string(argv[4])+"/frame-"+std::to_string(frame)+".ppm",machine.video());
        }
        if (replay.applied()!=replay.size()) throw std::runtime_error("reference ended before all input events");
        if (capture) { ps2::offline::end_full_step_capture();ps2::offline::end_driving_capture(); }
        std::fprintf(stderr,"COMPLETE frames=%u input_events=%zu cache_misses=%llu\n",frames,replay.applied(),
                     (unsigned long long)cache->misses());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr,"Driving reference failed: %s\n",error.what());return 1;
    }
}
