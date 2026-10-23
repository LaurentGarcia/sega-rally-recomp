#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>

namespace ps2 {
// Presentation diagnostics only. These observations never feed the guest
// clock or change its instruction budget. Timings describe completed work;
// submission waits are not a measurement of GS utilization.
struct FlowSample {
    enum class Renderer { Cpu, Native, Fallback } renderer = Renderer::Cpu;
    std::uint32_t frame = 0;
    std::uint64_t frequency = 0, total = 0, machine = 0, tiles = 0;
    std::uint64_t raster = 0, composite = 0, submit = 0, packets = 0;
    std::uint64_t cache_misses = 0, cache_delta = 0;
    std::uint64_t texture_bytes = 0, palette_bytes = 0, layer_bytes = 0;
    std::uint32_t texture_hits = 0, triangles = 0;
    bool vu0 = false, sound = false, sound_output = false;
    std::uint32_t queued_frames = 0, silent_frames = 0;
};

class FlowDiagram {
public:
    static constexpr std::size_t capacity = 2048;
    const char *text() const { return text_.data(); }
    void update(const FlowSample &s) {
        const auto video = s.tiles + s.raster + s.composite;
        const auto core = s.machine > video ? s.machine - video : 0;
        const auto layers = s.tiles + s.composite;
        const std::array<std::uint64_t, 5> costs{core, layers, s.raster, s.packets, s.submit};
        const char *names[] = {"EE CORE+DEVICES", "TILE LAYERS", "CPU RASTER", "GS PACKETS", "SUBMIT+WAIT"};
        const auto top = std::size_t(std::max_element(costs.begin(), costs.end()) - costs.begin());
        const char *mode = s.renderer == FlowSample::Renderer::Native ? "GS NEAREST - EXPERIMENTAL" :
                           s.renderer == FlowSample::Renderer::Fallback ? "CPU REFERENCE - GS FALLBACK" : "CPU REFERENCE";
        auto ms = [&](std::uint64_t ticks) -> unsigned long long {
            // Split before multiplication, including diagnostic counter wrap
            // or long stalls. Values beyond the display bound are saturated.
            if (!s.frequency) return 0;
            const auto whole = ticks / s.frequency;
            if (whole >= 999999) return 999999999;
            return whole * 1000 + (ticks % s.frequency) * 1000 / s.frequency;
        };
        char bar[25];
        const auto measured = core + layers + s.raster + s.packets + s.submit;
        // At most 24 characters, with relative costs from measured CPU stages.
        // A denominator rounded upward avoids overflow in the multiplication.
        const auto divisor = measured / 24 + (measured % 24 != 0);
        const auto top_columns = divisor ? std::min<std::uint64_t>(24, costs[top] / divisor) : 0;
        for (unsigned i = 0; i < 24; ++i) bar[i] = i < top_columns ? '=' : '.';
        bar[24] = 0;
        std::snprintf(text_.data(), text_.size(),
            "PS2 DIAGNOSTICS - LAST COMPLETE FRAME\n"
            "FRAME %lu / %llu MS\n"
            "[ROM] -> [512K CACHE] -> [EE]\n"
            "MISSES %llu (+%llu)\n"
            "                         |\n"
            "EE CORE + DEVICES  %llu MS\n"
            "                         V\n"
            "TILES + COMPOSE    %llu MS\n"
            "                         |\n"
            "[FRAME/TRIANGLES] -> [GIF] -> [GS] -> TV\n"
            "RASTER %llu MS / PACK %llu MS\n"
            "SUBMIT + WAIT %llu MS\n"
            "VU0 %s / VU1 INACTIVE\n"
            "%s\n"
            "GS TRI %lu / TEX %llu KB\n"
            "CLUT %llu B / LAYERS %llu KB\n"
            "[AUDIO %s] -> [IOP] -> [SPU2 %s]\n"
            "QUEUE %lu / SILENCE %lu FR\n"
            "TOP %s\n"
            "[%s] COST SHARE\n"
            "EE BUS CLOCK / GS BUSY UNMEASURED\n"
            "EMULATOR SPEED IS NOT CONSOLE SPEED\n"
            "SELECT + START : RETURN TO GAME",
            static_cast<unsigned long>(s.frame), ms(s.total),
            static_cast<unsigned long long>(s.cache_misses),
            static_cast<unsigned long long>(s.cache_delta), ms(core), ms(layers),
            ms(s.raster), ms(s.packets), ms(s.submit),
            s.vu0 && s.renderer == FlowSample::Renderer::Native ? "XYZ PACK" : "INACTIVE",
            mode, static_cast<unsigned long>(s.triangles),
            static_cast<unsigned long long>(s.texture_bytes / 1024),
            static_cast<unsigned long long>(s.palette_bytes),
            static_cast<unsigned long long>(s.layer_bytes / 1024),
            s.sound ? "ON" : "OFF", s.sound_output ? "ON" : "OFF",
            static_cast<unsigned long>(s.queued_frames), static_cast<unsigned long>(s.silent_frames),
            s.frame && s.frequency ? names[top] : "WAITING FOR DATA", bar);
    }
private:
    std::array<char, capacity> text_{};
};
} // namespace ps2
