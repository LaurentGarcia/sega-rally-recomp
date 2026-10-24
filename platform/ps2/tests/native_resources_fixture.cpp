// Original GS resource/lifetime fixture. SDK link wrappers exist only in this
// executable: production Display has no fault-injection or allocator API.
#include "native_display.h"
#include <dma.h>
#include <dma_registers.h>
#include <graph.h>
#include <kernel.h>
#include <malloc.h>
#include <packet.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>

namespace {
enum class Fault { none, packet, staging, dma, vram, video };
Fault fault = Fault::none;
unsigned vram_fault = 0, vram_attempt = 0, checks = 0, injected = 0;
unsigned live_packets = 0, live_staging = 0;
std::array<void *, 64> packets{}, staging{};
bool tracking_overflow = false;
struct Calls {
    unsigned packet = 0, staging = 0, dma = 0, vram = 0, clear = 0, video = 0;
    bool operator==(const Calls &) const = default;
} calls;
void require(bool result, const char *message) {
    ++checks;
    if (!result) throw std::runtime_error(message);
}
void track(std::array<void *,64> &slots, unsigned &live, void *pointer) {
    if (!pointer) return;
    ++live;
    for (auto &slot : slots) if (!slot) { slot = pointer; return; }
    tracking_overflow = true;
}
void release(std::array<void *,64> &slots, unsigned &live, void *pointer) {
    if (!pointer) return;
    for (auto &slot : slots) if (slot == pointer) { slot = nullptr; --live; return; }
}
}

extern "C" {
packet_t *__real_packet_init(int, int);
void __real_packet_free(packet_t *);
void *__real_memalign(std::size_t, std::size_t);
void __real_free(void *);
int __real_dma_channel_initialize(int, void *, int);
int __real_graph_vram_allocate(int, int, int, int);
void __real_graph_vram_clear();
int __real_graph_initialize(int, int, int, int, int, int);
packet_t *__wrap_packet_init(int qwords, int type) {
    ++calls.packet;
    if (fault == Fault::packet) { ++injected; return nullptr; }
    auto *p = __real_packet_init(qwords, type);
    track(packets, live_packets, p);
    return p;
}
void __wrap_packet_free(packet_t *packet) {
    release(packets, live_packets, packet);
    __real_packet_free(packet);
}
void *__wrap_memalign(std::size_t alignment, std::size_t bytes) {
    const bool display_staging = alignment == 128 && bytes == 65536;
    if (display_staging) {
        ++calls.staging;
        if (fault == Fault::staging) { ++injected; return nullptr; }
    }
    auto *p = __real_memalign(alignment, bytes);
    if (display_staging) track(staging, live_staging, p);
    return p;
}
void __wrap_free(void *pointer) {
    release(staging, live_staging, pointer);
    __real_free(pointer);
}
int __wrap_dma_channel_initialize(int channel, void *handler, int flags) {
    ++calls.dma;
    if (fault == Fault::dma) { ++injected; return -1; }
    return __real_dma_channel_initialize(channel, handler, flags);
}
int __wrap_graph_vram_allocate(int width, int height, int psm, int alignment) {
    ++calls.vram;
    ++vram_attempt;
    if (fault == Fault::vram && vram_attempt == vram_fault) { ++injected; return -1; }
    return __real_graph_vram_allocate(width, height, psm, alignment);
}
void __wrap_graph_vram_clear() {
    ++calls.clear;
    __real_graph_vram_clear();
}
int __wrap_graph_initialize(int base, int width, int height, int psm, int x, int y) {
    ++calls.video;
    if (fault == Fault::video) { ++injected; return -1; }
    return __real_graph_initialize(base, width, height, psm, x, y);
}
}

int main() {
    using namespace ps2::native;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    try {
        const u32 dma_before = *DMA_REG_CTRL;
        *DMA_REG_CTRL = dma_before | 1u;
        asm volatile("sync.l" ::: "memory");
        require(*DMA_REG_CTRL == (dma_before | 1u), "STANDALONE DMA ENABLE FAILED");
        Display replacement;
        replacement.invalidate_textures(); // Also safe before initialization.
        unsigned sampled = 0;
        std::uint64_t fingerprint = 1469598103934665603ull;
        const auto pixels = [&](Display &display, const char *phase, std::uint32_t bgr) {
            std::array<std::uint32_t,64> sample{};
            const auto status = display.readback_rect(316, 160 * display.output_height() / frame_height - 4,
                                                     8, 8, sample);
            if (status != ReadbackStatus::ok) {
                std::printf("RESOURCE READBACK UNVERIFIED phase=%s status=%u error=%s\n",
                            phase, unsigned(status), display.error());
                throw std::runtime_error("RESOURCE PIXELS NOT VERIFIED");
            }
            for (auto pixel : sample) {
                require((pixel & 0x00ffffffu) == bgr, "RESOURCE PIXEL RGB MISMATCH");
                fingerprint = (fingerprint ^ (pixel & 0x00ffffffu)) * 1099511628211ull;
                ++sampled;
            }
            std::printf("RESOURCE PIXELS PASS phase=%s samples=64 expected_bgr=%06lx\n",
                        phase, static_cast<unsigned long>(bgr));
        };
        {
            Display display;
            struct Case { Fault kind; unsigned allocation; const char *error; };
            const Case cases[]{
                {Fault::packet,0,"NATIVE GS BUFFER ALLOCATION FAILED"},
                {Fault::staging,0,"NATIVE GS BUFFER ALLOCATION FAILED"},
                {Fault::dma,0,"NATIVE GIF INITIALIZATION FAILED"},
                {Fault::vram,1,"NATIVE FRAMEBUFFER VRAM EXHAUSTED"},
                {Fault::vram,2,"NATIVE FRAMEBUFFER VRAM EXHAUSTED"},
                {Fault::vram,3,"NATIVE DEPTH VRAM EXHAUSTED"},
                {Fault::vram,4,"NATIVE FONT VRAM EXHAUSTED"},
                {Fault::vram,5,"NATIVE TEXTURE VRAM EXHAUSTED"},
                {Fault::vram,18,"NATIVE TEXTURE VRAM EXHAUSTED"},
                {Fault::vram,19,"NATIVE CLUT VRAM EXHAUSTED"},
                {Fault::vram,274,"NATIVE CLUT VRAM EXHAUSTED"},
                {Fault::video,0,"NATIVE VIDEO MODE FAILED"},
            };
            // Every failure retries the same object twice. Failure must release
            // both CPU allocations, including the successfully allocated peer.
            for (const auto &item : cases) for (unsigned repeat=0; repeat<2; ++repeat) {
                fault=item.kind; vram_fault=item.allocation; vram_attempt=0;
                const unsigned before=injected;
                require(!display.initialize(), "INJECTED INITIALIZATION FAILURE ACCEPTED");
                require(injected==before+1, "FAILURE SITE NOT REACHED EXACTLY ONCE");
                require(std::strcmp(display.error(),item.error)==0, "WRONG INITIALIZATION ERROR");
                require(!live_packets && !live_staging && !tracking_overflow,
                        "INITIALIZATION FAILURE RETAINED CPU ALLOCATIONS");
                require(!display.present(Frame{}), "FAILED DISPLAY ACCEPTED PRESENT");
            }
            fault=Fault::none;
            // A different object can acquire the released lease after failure.
            {
                Display after_failure;
                require(after_failure.initialize(), after_failure.error());
                require(live_packets==1 && live_staging==1, "RETRY OWNERSHIP ALLOCATION COUNT");
            }
            require(!live_packets && !live_staging, "DESTROYED RETRY OWNER RETAINED BUFFERS");
            require(display.initialize(), display.error());
            require(live_packets==1 && live_staging==1, "SUCCESS ALLOCATION COUNT");
            const auto initialized_calls=calls;
            require(display.initialize(), "REPEATED INITIALIZE REJECTED");
            require(calls==initialized_calls, "REPEATED INITIALIZE TOUCHED SDK OR ALLOCATIONS");

            using Indices=std::array<std::uint8_t,32>;
            alignas(Indices) std::byte storage[sizeof(Indices)];
            auto *indices=new (storage) Indices{};
            Texture texture{indices->data(),indices->size(),8,8,7};
            Material material;
            material.texture=&texture;
            material.palette.fill(0xffff2020u);
            material.palette[1]=0xff2050ffu;
            const std::array<Triangle,1> triangle{{{{{{48,60,0,0,1,0},
                {448,60,8,0,1,0},{248,340,4,8,1,0}}},&material}}};
            const Frame frame{triangle};
            require(display.present(frame), display.error());
            require(display.stats().texture_uploads==1 && display.stats().texture_bytes==32 &&
                    display.stats().palette_uploads==1, "FIRST TEXTURE NOT UPLOADED");
            pixels(display,"first_asset",0x002020ffu);
            require(display.present(frame), display.error());
            require(display.stats().texture_hits==1 && !display.stats().texture_uploads &&
                    display.stats().palette_hits==1, "UNCHANGED ASSET NOT CACHED");
            const auto owned_calls=calls;
            require(!replacement.initialize(), "SECOND DISPLAY STOLE GS OWNERSHIP");
            require(calls==owned_calls, "REJECTED OWNER TOUCHED SDK OR ALLOCATIONS");
            pixels(display,"rejected_owner_preserved_frame",0x002020ffu);
            const auto saved_stats=display.stats();
            const char *saved_error=display.error();
            display.invalidate_textures();
            display.invalidate_textures();
            require(calls==owned_calls && display.stats().texture_hits==saved_stats.texture_hits &&
                    display.stats().texture_uploads==saved_stats.texture_uploads && display.error()==saved_error,
                    "INVALIDATION MODIFIED TRANSFER/STATS/ERROR STATE");
            const auto *old_address=texture.indices;
            indices->~Indices();
            indices=new (storage) Indices{}; // New asset, same storage and generation.
            indices->fill(0x11);
            texture.indices=indices->data();
            require(texture.indices==old_address && texture.generation==7, "ADDRESS REUSE FIXTURE CHANGED KEY");
            require(display.present(frame), display.error());
            require(display.stats().texture_uploads==1 && !display.stats().texture_hits &&
                    display.stats().texture_bytes==32 && display.stats().palette_hits==1 &&
                    !display.stats().palette_uploads, "INVALIDATED ASSET NOT REUPLOADED WITH RETAINED CLUT");
            pixels(display,"reused_storage_after_invalidation",0x00ff5020u);
            indices->fill(0);
            ++texture.generation;
            require(display.present(frame), display.error());
            require(display.stats().texture_uploads==1 && !display.stats().texture_hits,
                    "GENERATION UPDATE DID NOT UPLOAD");
            pixels(display,"generation_update",0x002020ffu);
            display.invalidate_textures();
            indices->~Indices();
        }
        require(!live_packets && !live_staging && !tracking_overflow, "DESTROYED OWNER RETAINED BUFFERS");
        require(replacement.initialize(), replacement.error());
        require(live_packets==1 && live_staging==1, "REPLACEMENT ALLOCATION COUNT");
        require(replacement.present(Frame{{},0xff2050ffu}), replacement.error());
        pixels(replacement,"replacement_owner",0x00ff5020u);
        std::printf("RESOURCE FIXTURE PASS checks=%u injected=%u pixel_samples=%u hash=%016llx\n",
                    checks,injected,sampled,static_cast<unsigned long long>(fingerprint));
        std::puts("Scoped to original assets, controlled setup failures and sampled GS pixels; physical hardware unverified.");
        while (true) SleepThread();
    } catch (const std::exception &error) {
        std::printf("RESOURCE FIXTURE FAIL: %s\n",error.what());
        while (true) SleepThread();
    }
}
