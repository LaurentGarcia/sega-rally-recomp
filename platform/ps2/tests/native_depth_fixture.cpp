// Original numerical/GS depth study. No game assets or reference runtime.
#include "native_display.h"
#include "native_depth_checks.h"
#include <dma_registers.h>
#include <kernel.h>
#include <array>
#include <cstdio>
#include <stdexcept>

int main() {
    using namespace ps2::native;
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::setvbuf(stderr,nullptr,_IONBF,0);
    try {
        const auto checks=ps2::test::depth_checks();
        std::printf("NATIVE DEPTH QUANT %s checks=%u hash=%016llx\n",checks.pass?"PASS":"FAIL",
                    checks.count,static_cast<unsigned long long>(checks.hash));
        if (!checks.pass) throw std::runtime_error("DEPTH INTEGER QUANTIZATION MISMATCH");
        // Standalone diagnostic owns DMA startup. Some HLE loaders leave DMAE
        // clear while nevertheless executing transfers; real hardware requires
        // the explicit enable. Preserve every other controller setting.
        const u32 dma_before=*DMA_REG_CTRL;
        *DMA_REG_CTRL=dma_before|1u;
        asm volatile("sync.l" ::: "memory");
        const u32 dma_after=*DMA_REG_CTRL;
        std::printf("DEPTH DMA STARTUP before=%08lx after=%08lx\n",
            static_cast<unsigned long>(dma_before),static_cast<unsigned long>(dma_after));
        if (dma_after!=(dma_before|1u)) throw std::runtime_error("DIAGNOSTIC DMA ENABLE FAILED");
        Display display;
        if (!display.initialize()) throw std::runtime_error(display.error());
        std::array<std::uint32_t,64> sample;
        sample.fill(0xa55aa55au);
        // Invalid requests must be rejected before touching DMA or output.
        for (const auto result : {
                display.readback_rect(0,0,0,8,sample),
                display.readback_rect(0,display.output_height(),8,8,sample),
                display.readback_rect(0,0,3,3,sample),
                display.readback_rect(0,0,640,32,sample),
                display.readback_rect(0,0,8,8,std::span(sample).first(63))})
            if (result!=ReadbackStatus::invalid_rectangle)
                throw std::runtime_error("READBACK INVALID REQUEST ACCEPTED");
        for (auto pixel:sample)
            if (pixel!=0xa55aa55au) throw std::runtime_error("READBACK REJECTION MODIFIED OUTPUT");
        unsigned verified_pixels=0;
        std::uint64_t pixel_hash=1469598103934665603ull;
        const auto verify_at=[&](const char *phase,const char *region,unsigned logical_x,
                                 unsigned logical_y,std::uint32_t expected) {
            const unsigned x=logical_x*640/frame_width-4;
            const unsigned y=logical_y*display.output_height()/frame_height-4;
            const auto result=display.readback_rect(x,y,8,8,sample);
            if (result!=ReadbackStatus::ok) {
                std::printf("DEPTH PIXELS TRANSPORT FAIL phase=%s region=%s status=%u: %s\n",
                    phase,region,unsigned(result),display.error());
                throw std::runtime_error("GS READBACK UNAVAILABLE; PIXEL RESULT NOT VERIFIED");
            }
            unsigned mismatch=0;
            for (auto pixel:sample) {
                const auto rgb=pixel&0x00ffffffu;
                if (rgb!=expected) ++mismatch;
                pixel_hash=(pixel_hash^rgb)*1099511628211ull;
            }
            std::printf("DEPTH PIXELS %s phase=%s region=%s samples=64 expected_bgr=%06lx first_raw=%08lx mismatch=%u\n",
                mismatch?"FAIL":"PASS",phase,region,
                static_cast<unsigned long>(expected),static_cast<unsigned long>(sample[0]),mismatch);
            if (mismatch) throw std::runtime_error("GS DEPTH SAMPLE RGB MISMATCH");
            verified_pixels+=sample.size();
        };
        const auto verify=[&](const char *phase,std::uint32_t expected) {
            verify_at(phase,"solid",120,160,expected);
            verify_at(phase,"textured",376,160,expected);
        };
        std::array<std::uint8_t,32> indices{};
        const Texture texture{indices.data(),indices.size(),8,8,1};
        Material solid_near,solid_far,textured_near,textured_far;
        solid_near.color=0xffff2020u; solid_far.color=0xff2050ffu;
        textured_near.texture=textured_far.texture=&texture;
        textured_near.palette.fill(solid_near.color); textured_far.palette.fill(solid_far.color);
        const auto triangle=[](float x,float depth,const Material *material) {
            return Triangle{{{{x,80,0,0,1,depth},{x+170,80,8,0,1,depth},{x+85,300,4,8,1,depth}}},material};
        };
        const Triangle sn=triangle(35,.75f,&solid_near),sf=triangle(35,.25f,&solid_far);
        const Triangle tn=triangle(291,.75f,&textured_near),tf=triangle(291,.25f,&textured_far);
        std::array<Triangle,4> ordered{sn,sf,tf,tn}; // Near-first left, near-last right.
        std::array<Triangle,2> far_only{sf,tf};
        display.set_caption("ORIGINAL Z16S DEPTH FIXTURE\nFINAL EXPECTATION: BOTH PANELS RED");
        for (unsigned frame=0;frame<360;++frame) {
            const bool page=frame>=120 && frame<180;
            display.set_debug_page(page ? "ORIGINAL DEPTH TRANSITION FIXTURE\nTHIS PAGE MASKS DEPTH WRITES\nAND USES ALWAYS WITH ZTE ENABLED\nRETURN MUST RESTORE Z16S\nNO GAME OR PERFORMANCE CLAIM" : nullptr);
            if (frame==120) std::puts("DEPTH PHASE diagnostic page");
            if (frame==180) std::puts("DEPTH PHASE far-only after page; expected blue panels (cleared prior near depth)");
            if (frame==240) {
                ordered={sf,sn,tn,tf}; // Reverse both submission orders.
                std::puts("DEPTH PHASE reversed orders; expected both panels red");
            }
            const Frame submitted=(frame>=180 && frame<240) ? Frame{far_only,0xff101923u,true} : Frame{ordered,0xff101923u,true};
            if (!display.present(submitted)) throw std::runtime_error(display.error());
            if (frame==1) verify("opposite_orders",0x002020ffu);
            if (frame==121) verify("diagnostic_page",0x0018120cu);
            if (frame==181) verify("far_after_page",0x00ff5020u);
            if (frame==241) verify("reversed_orders",0x002020ffu);
        }
        // A constant-depth overlap cannot detect a triangle API that silently
        // repeats v0.z for all vertices. These crossing planes must change the
        // visible color within each triangle, for both native submission paths.
        // At logical y 125, the two sampled x positions give the blue plane depths
        // about 0.225 and 0.622 (or their complements). The entire 8x8 physical
        // sample stays well away from the 0.5 crossing and triangle boundaries.
        display.set_debug_page(nullptr);
        display.set_caption("ORIGINAL SLOPED DEPTH FIXTURE\nLEFT AND RIGHT SAMPLES MUST DIFFER");
        for (unsigned phase=0;phase<4;++phase) {
            const bool falling=phase>=2,reversed=phase&1u;
            auto solid_blue=triangle(35,falling?.875f:.125f,&solid_far);
            auto textured_blue=triangle(291,falling?.875f:.125f,&textured_far);
            solid_blue.vertices[1].depth=textured_blue.vertices[1].depth=falling?.125f:.875f;
            const auto solid_red=triangle(35,.5f,&solid_near);
            const auto textured_red=triangle(291,.5f,&textured_near);
            const std::array<Triangle,4> crossing=reversed?
                std::array<Triangle,4>{solid_red,solid_blue,textured_red,textured_blue}:
                std::array<Triangle,4>{solid_blue,solid_red,textured_blue,textured_red};
            if (!display.present({crossing,0xff101923u,true})) throw std::runtime_error(display.error());
            const char *name=falling?(reversed?"slope_falling_reversed":"slope_falling"):
                                    (reversed?"slope_rising_reversed":"slope_rising");
            const std::uint32_t left=falling?0x00ff5020u:0x002020ffu;
            const std::uint32_t right=falling?0x002020ffu:0x00ff5020u;
            verify_at(name,"solid_left",75,125,left);
            verify_at(name,"solid_right",165,125,right);
            verify_at(name,"textured_left",331,125,left);
            verify_at(name,"textured_right",421,125,right);
        }
        std::printf("DEPTH FIXTURE PIXEL PASS frames=364 samples=%u hash=%016llx format=PSMCT32+PSMZ16S vram_bytes=%lu output_height=%u; scoped to sampled original regions\n",
                    verified_pixels,static_cast<unsigned long long>(pixel_hash),
                    static_cast<unsigned long>(display_vram_bytes(display.output_height())),display.output_height());
        while (true) SleepThread();
    } catch (const std::exception &error) {
        std::printf("DEPTH FIXTURE FAIL: %s\n",error.what());
        while (true) SleepThread();
    }
}
