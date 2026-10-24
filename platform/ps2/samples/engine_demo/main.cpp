// Original hardware sample. All geometry, texture and PCM are generated here.
// This executable imports no game systems, captured scene or external assets.
#include "native_display.h"
#include "native_geometry.h"
#include "pad_input.h"
#include "audio_output.h"
#include "debug_controls.h"
#include <dma_registers.h>
#include <kernel.h>
#include <timer.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
using namespace ps2::native;
constexpr std::array<std::array<float,3>,8> corners{{
    {{-1,-1,-1}},{{1,-1,-1}},{{1,1,-1}},{{-1,1,-1}},
    {{-1,-1,1}},{{1,-1,1}},{{1,1,1}},{{-1,1,1}}
}};
constexpr std::array<std::array<unsigned,4>,6> faces{{
    {{0,1,2,3}},{{4,7,6,5}},{{0,3,7,4}},
    {{1,5,6,2}},{{3,2,6,7}},{{0,4,5,1}}
}};
constexpr std::array<std::uint32_t,6> colors{
    0xff22c4d6u,0xffffa340u,0xffbe78e8u,0xff72cf81u,0xfff17887u,0xff729ee8u
};
constexpr std::array<std::array<float,2>,4> texels{{{{0,0}},{{32,0}},{{32,32}},{{0,32}}}};
constexpr std::array<std::array<unsigned,3>,2> fan{{{{0,1,2}},{{0,2,3}}}};
constexpr std::size_t max_triangles=faces.size()*2*clip_triangle_capacity;

std::size_t cube(float yaw,float distance,const std::array<Material,6> &materials,
                 std::array<Triangle,max_triangles> &output) {
    // Sample camera and animation only. The reusable clipper receives ordinary
    // homogeneous clip coordinates; it has no knowledge of this cube or camera.
    constexpr float focal=1.7320508f, near_plane=.25f, far_plane=12.f;
    constexpr float pitch=.40f;
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    std::array<ClipVertex,8> projected;
    for (unsigned i=0;i<corners.size();++i) {
        const auto &v=corners[i];
        const float x=cy*v[0]+sy*v[2], z=-sy*v[0]+cy*v[2];
        const float y=cp*v[1]-sp*z, view_z=sp*v[1]+cp*z+distance;
        projected[i]={x*focal*float(frame_height)/frame_width,y*focal,
            far_plane*(view_z-near_plane)/(far_plane-near_plane),view_z,0,0};
    }
    std::size_t used=0;
    for (unsigned face=0;face<faces.size();++face) {
        for (const auto &indices:fan) {
            ClipTriangle source;
            source.material=&materials[face];
            for (unsigned i=0;i<3;++i) {
                source.vertices[i]=projected[faces[face][indices[i]]];
                source.vertices[i].u=texels[indices[i]][0];
                source.vertices[i].v=texels[indices[i]][1];
            }
            const auto result=clip_project_triangle(source,std::span(output).subspan(used));
            if (result.status!=ClipStatus::success)
                throw std::runtime_error("ORIGINAL CUBE CLIP FAILED");
            used+=result.count;
        }
    }
    return used;
}
}

int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    Display display;
    char page[1200]{};
    try {
        // This standalone application owns startup. Preserve DMAC settings and
        // enable its global gate explicitly, including under HLE boot loaders.
        *DMA_REG_CTRL=*DMA_REG_CTRL|1u;
        asm volatile("sync.l" ::: "memory");
        if (!display.initialize()) throw std::runtime_error(display.error());
        display.set_vu0_packing(true); // Final textured XYZ packing, not transforms.
        display.present_status("ORIGINAL PS2 ENGINE SAMPLE\nINITIALIZING OWNED IOP MODULES");
        ps2::PadInput pad;
        // No host filesystem is used. Reset once before audio is initialized;
        // no device may reset the IOP during this application's remaining life.
        if (!pad.initialize(true)) std::printf("ENGINE DEMO PAD: %s\n",pad.status());
        ps2::AudioOutput audio;
        const bool sound=audio.initialize();
        if (!sound) std::printf("ENGINE DEMO AUDIO UNAVAILABLE: %s\n",audio.error());

        std::array<std::uint8_t,32*32/2> indices{};
        for (unsigned y=0;y<32;++y)
            for (unsigned x=0;x<32;x+=2) {
                const auto shade=(x/8+y/8)&1u;
                indices[y*16+x/2]=std::uint8_t(shade|(shade<<4));
            }
        const Texture texture{indices.data(),indices.size(),32,32,1};
        std::array<Material,6> materials;
        for (unsigned i=0;i<materials.size();++i) {
            materials[i].texture=&texture;
            materials[i].palette.fill(colors[i]);
            materials[i].palette[0]=0xff000000u|((colors[i]>>1)&0x007f7f7fu);
        }
        std::array<Triangle,max_triangles> triangles;
        std::array<float,4096> pcm{};
        constexpr unsigned target_audio_frames=1536;
        static_assert(((target_audio_frames*44100u+47999u)/48000u)*2<=pcm.size());
        ps2::AudioOutput::Stats audio_stats{};
        ps2::DebugPageToggle debug_toggle;
        bool debug=false,animate=true;
        std::uint16_t previous_buttons=0;
        unsigned tone_phase=0;
        float yaw=.55f,distance=3.8f;
        auto previous_time=GetTimerSystemTime();
        display.set_caption("ORIGINAL PS2 ENGINE SAMPLE\nANIMATED CHECKER CUBE / DIRECT GS");
        std::puts("ENGINE DEMO: original cube, indexed texture and quiet PCM; no game assets or game systems");
        std::puts("ENGINE DEMO: EE clip/project, VU0 final XYZ, GIF/GS Z16S, optional pad and IOP/SPU2; VU1 inactive");
        for (unsigned frame=1;;++frame) {
            auto now=GetTimerSystemTime();
            auto elapsed=now-previous_time;
            previous_time=now;
            if (elapsed>kBUSCLK/10) elapsed=kBUSCLK/10; // Bound sample animation after pauses.
            const float seconds=float(elapsed)/float(kBUSCLK);
            auto input=pad.poll();
            if (debug_toggle.update(input)) debug=!debug;
            const auto pressed=input.buttons&~previous_buttons;
            previous_buttons=input.buttons;
            if (pressed&ps2::Square) animate=!animate;
            float manual=0;
            if (input.connected) {
                const int axis=int(input.left_x)-128;
                if (axis < -12 || axis > 12) manual=float(axis)/128.f;
                manual+=float(bool(input.buttons&ps2::Right))-float(bool(input.buttons&ps2::Left));
                distance+=seconds*2.f*(float(bool(input.buttons&ps2::R1))-float(bool(input.buttons&ps2::L1)));
            }
            if (distance<2.75f) distance=2.75f;
            if (distance>7.5f) distance=7.5f;
            yaw+=seconds*((animate?.6f:0.f)+manual*2.f);
            if (yaw>3.141593f) yaw-=6.2831853f;
            if (yaw< -3.141593f) yaw+=6.2831853f;
            if (sound) {
                audio_stats=audio.stats();
                if (*audio.error()) throw std::runtime_error(audio.error());
                const auto missing=audio_stats.queued<target_audio_frames ? target_audio_frames-audio_stats.queued : 0;
                const unsigned input_frames=(missing*44100u+47999u)/48000u;
                const bool tone=input.connected && (input.buttons&ps2::Cross);
                for (unsigned i=0;i<input_frames;++i) {
                    tone_phase=(tone_phase+440)%44100;
                    const float value=tone ? (tone_phase<22050?.04f:-.04f) : 0.f;
                    pcm[2*i]=pcm[2*i+1]=value;
                }
                if (!audio.submit(std::span<const float>(pcm.data(),input_frames*2)) || !audio.flush())
                    throw std::runtime_error(audio.error());
            }
            const auto count=cube(yaw,distance,materials,triangles);
            std::snprintf(page,sizeof(page),
                "ORIGINAL PS2 ENGINE SAMPLE\n"
                "NO GAME ASSETS OR GAME SYSTEMS\n\n"
                "EE SAMPLE ROTATION / CLIP / PROJECT\n"
                "VU0 FINAL TEXTURED XYZ PACKING\n"
                "GIF DMA / GS PSMT4 / Z16S\n"
                "VU1 INACTIVE\n\n"
                "FRAME %u / TRIANGLES %lu\n"
                "PAD %s / LEFT X %u\n"
                "AUDIO %s / QUEUE %lu FR\n\n"
                "LEFT STICK X OR LEFT/RIGHT: ROTATE\n"
                "SQUARE: TOGGLE AUTOMATIC ROTATION\n"
                "L1/R1: NEARER/FARTHER\n"
                "HOLD CROSS: QUIET 440 HZ TONE\n"
                "SELECT+START: TOGGLE THIS PAGE\n\n"
                "EMULATOR SPEED IS NOT CONSOLE SPEED",
                frame,static_cast<unsigned long>(count),input.connected?"CONNECTED":"ABSENT",
                unsigned(input.left_x),sound?"ON":"OFF",static_cast<unsigned long>(audio_stats.queued));
            display.set_debug_page(debug?page:nullptr);
            if (!display.present(Frame{std::span<const Triangle>(triangles.data(),count),0xff101923u,true}))
                throw std::runtime_error(display.error());
            if (frame<=3 || frame%60==0) {
                const auto &stats=display.stats();
                std::printf("ENGINE_DEMO_FRAME frame=%u triangles=%lu texture_uploads=%lu palette_uploads=%lu audio=%u\n",
                    frame,static_cast<unsigned long>(stats.triangles),static_cast<unsigned long>(stats.texture_uploads),
                    static_cast<unsigned long>(stats.palette_uploads),unsigned(sound));
            }
        }
    } catch (const std::exception &error) {
        display.set_caption(nullptr);
        display.set_debug_page(nullptr);
        std::printf("ENGINE DEMO ERROR: %s\n",error.what());
        if (display.present_status(error.what())) while (true) SleepThread();
        return 1;
    }
}
