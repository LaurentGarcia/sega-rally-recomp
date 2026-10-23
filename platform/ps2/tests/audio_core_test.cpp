// ROM-free checks for paged sound images and original PCM presentation logic.
#include "runtime/model2a_sound_bus.h"
#include "pcm_ring.h"
#include "pcm_resampler.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <limits>
#include <stdexcept>
#include <vector>

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct Images final : snd::SoundImageSource {
    std::vector<uint8_t> program = std::vector<uint8_t>(0x80000);
    std::vector<uint8_t> samples = std::vector<uint8_t>(0x800000);
    unsigned reads = 0;
    uint32_t size(snd::SoundImage image) const override {
        return uint32_t(image == snd::SoundImage::Program ? program.size() : samples.size());
    }
    uint16_t read16_be(snd::SoundImage image, uint32_t offset) override {
        const auto &bytes = image == snd::SoundImage::Program ? program : samples;
        check(!(offset & 1) && uint64_t(offset) + 2 <= bytes.size(), "sound source bounds");
        ++reads;
        return uint16_t(uint16_t(bytes[offset]) << 8 | bytes[offset + 1]);
    }
};
static void bus_checks() {
    Images images;
    for (size_t i = 0; i < images.program.size(); ++i) images.program[i] = uint8_t(i * 17 + i / 257);
    for (size_t i = 0; i < images.samples.size(); ++i) images.samples[i] = uint8_t(i * 31 + i / 4093);
    snd::Model2ASoundBus resident(images.program, images.samples), paged({}, {}, &images);
    check(images.reads == 8, "reset copies exactly 16 bytes through the source");
    check(std::equal(resident.sound_ram().begin(), resident.sound_ram().end(), paged.sound_ram().begin()), "reset RAM parity");
    uint32_t seed = 0x125690ab;
    for (unsigned i = 0; i < 20000; ++i) {
        seed = seed * 1664525 + 1013904223;
        const uint32_t address = i & 1 ? 0x800000 | (seed & 0x7ffffe) : 0x600000 | (seed & 0x7fffe);
        for (uint16_t mask : {uint16_t(0xffff), uint16_t(0xff00), uint16_t(0xff), uint16_t(0)})
            check(resident.read16(address, mask) == paged.read16(address, mask), "paged sound byte order or mask");
    }
    for (uint32_t address : {0x600000u, 0x600ffeu, 0x601000u, 0x67fffeu, 0x800000u, 0x9ffffeu, 0xa00000u, 0xdffffeu, 0xe00000u, 0xfffffeu})
        check(resident.read16(address, 0xffff) == paged.read16(address, 0xffff), "sound bank/page edge parity");
    resident.write16(0x40, 0x1234, 0xffff); paged.write16(0x40, 0x1234, 0xffff);
    resident.write16(0, 0xffff, 0xffff); paged.write16(0, 0xffff, 0xffff);
    resident.reset(); paged.reset();
    check(paged.read16(0x40, 0xffff) == 0x1234, "soft reset preserves RAM outside vectors");
    check(std::equal(resident.sound_ram().begin(), resident.sound_ram().end(), paged.sound_ram().begin()), "soft reset parity");
    for (unsigned target : {0u, 1u}) {
        auto &image = target ? images.samples : images.program;
        image.pop_back();
        bool rejected = false;
        try { snd::Model2ASoundBus invalid({}, {}, &images); } catch (const rt::Fatal &) { rejected = true; }
        check(rejected, "wrong sound image size rejected");
        image.push_back(0);
    }
}
static void ring_checks() {
    RallyPcmRing ring{};
    std::fill(std::begin(ring.samples), std::end(ring.samples), 12345);
    rally_pcm_reset(&ring);
    std::array<int16_t, RALLY_PCM_DMA_FRAMES * 2> dma;
    rally_pcm_render(&ring, dma.data());
    check(std::all_of(dma.begin(), dma.end(), [](int16_t value) { return !value; }), "empty queue repeats stale samples");
    std::deque<std::array<int16_t, 2>> expected;
    std::array<int16_t, RALLY_PCM_PACKET_FRAMES * 2> packet;
    uint32_t seed = 17;
    for (unsigned iteration = 0; iteration < 3000; ++iteration) {
        seed = seed * 1664525 + 1013904223;
        const unsigned count = seed % (RALLY_PCM_PACKET_FRAMES + 1);
        for (unsigned i = 0; i < count; ++i) {
            packet[i * 2] = int16_t(iteration + i); packet[i * 2 + 1] = int16_t(-int(iteration + i));
        }
        const unsigned before = ring.queued;
        const unsigned accepted = rally_pcm_push(&ring, packet.data(), count);
        if (count > RALLY_PCM_RING_FRAMES - before) check(!accepted && ring.queued == before, "full queue changes on rejection");
        else {
            check(accepted == count, "queue accepts exact complete packet");
            for (unsigned i = 0; i < count; ++i) expected.push_back({packet[i * 2], packet[i * 2 + 1]});
        }
        if (iteration % 7 == 0) {
            rally_pcm_render(&ring, dma.data());
            for (unsigned i = 0; i < RALLY_PCM_DMA_FRAMES; ++i) {
                std::array<int16_t, 2> sample{};
                if (!expected.empty()) { sample = expected.front(); expected.pop_front(); }
                check(dma[(i / 256) * 512 + i % 256] == sample[0] &&
                      dma[(i / 256) * 512 + 256 + i % 256] == sample[1], "DMA channel order, wrap or underrun mismatch");
            }
            check(ring.queued == expected.size(), "queue accounting");
        }
    }
    rally_pcm_reset(&ring);
    rally_pcm_render(&ring, dma.data());
    check(ring.silent_frames == 512 && ring.consumed == 0, "reset/underrun accounting");
}
static void resampler_checks() {
    ps2::PcmResampler resampler;
    unsigned outputs = 0;
    for (unsigned i = 0; i <= 44100; ++i) {
        const float ramp = float(i) / 44100 * 0.5f;
        check(resampler.frame(ramp, -ramp, [&](int16_t left, int16_t right) {
            const int reference = int(double(outputs) / 48000 * 16384);
            check(std::abs(int(left) - reference) <= 1 && std::abs(int(right) + reference) <= 1,
                  "resampler phase or channel continuity");
            ++outputs; return true;
        }), "resampler callback failed");
    }
    check(outputs == 48001, "exact rational resampling rate");
    ps2::PcmResampler clipping;
    clipping.frame(9, -9, [](int16_t left, int16_t right) { check(left == 32767 && right == -32768, "PCM saturation"); return true; });
    ps2::PcmResampler nonfinite;
    nonfinite.frame(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                    [](int16_t left, int16_t right) { check(!left && !right, "nonfinite samples must be silence"); return true; });
}
int main() {
    try {
        bus_checks(); ring_checks(); resampler_checks();
        std::puts("PS2 paged sound, bounded PCM queue and resampler checks passed; hardware untested.");
        return 0;
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
