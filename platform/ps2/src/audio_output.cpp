#include "audio_output.h"
#include "pcm_protocol.h"
#include "pcm_resampler.h"
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <sbv_patches.h>
#include <timer.h>
#include <cstdio>

extern "C" {
extern unsigned char libsd_irx[], rally_pcm_irx[];
extern unsigned int size_libsd_irx, size_rally_pcm_irx;
}

namespace ps2 {
struct AudioOutput::Impl {
    alignas(64) SifRpcClientData_t client{};
    alignas(64) RallyPcmPacket packet{};
    alignas(64) RallyPcmReply reply{};
    PcmResampler resampler;
    Stats stats;
    const char *failure = "audio is not initialized";
    bool ready = false;

    bool fail(const char *message) { failure = message; return false; }
    bool call(unsigned command, unsigned frames = 0) {
        packet.version = RALLY_PCM_VERSION;
        packet.frames = frames;
        if (SifCallRpc(&client, command, 0, &packet, int(8 + frames * 4), &reply, sizeof(reply), nullptr, nullptr) < 0)
            return fail("IOP PCM RPC failed");
        if (reply.version != RALLY_PCM_VERSION || reply.status != 0)
            return fail("IOP PCM server rejected request");
        stats.queued = reply.queued; stats.consumed = reply.consumed;
        stats.silent_frames = reply.silent_frames; stats.blocks = reply.blocks;
        stats.late_service_events = reply.late_service_events;
        return true;
    }
    bool send() {
        const unsigned frames = packet.frames;
        if (!frames) return true;
        const uint64_t begin = GetTimerSystemTime();
        do {
            if (!call(RALLY_PCM_WRITE, frames)) return false;
            if (reply.accepted == frames) {
                stats.submitted_frames += frames;
                packet.frames = 0;
                return true;
            }
            if (reply.accepted) return fail("IOP PCM server accepted a partial packet");
        } while (GetTimerSystemTime() - begin < uint64_t(kBUSCLK));
        return fail("IOP PCM queue remained full for one second");
    }
};

AudioOutput::AudioOutput() : impl_(std::make_unique<Impl>()) {}
AudioOutput::~AudioOutput() {
    if (impl_->ready) impl_->call(RALLY_PCM_STOP);
}
bool AudioOutput::initialize() {
    auto &m = *impl_;
    if (m.ready) return true;
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    int result = 0;
    if (SifExecModuleBuffer(libsd_irx, size_libsd_irx, 0, nullptr, &result) < 0 || result < 0)
        return m.fail("cannot load open SDK libsd module");
    if (SifExecModuleBuffer(rally_pcm_irx, size_rally_pcm_irx, 0, nullptr, &result) < 0 || result < 0)
        return m.fail("cannot load original PCM module");
    const uint64_t begin = GetTimerSystemTime();
    do {
        if (SifBindRpc(&m.client, RALLY_PCM_RPC, 0) < 0) return m.fail("cannot bind PCM RPC");
        if (m.client.server) break;
    } while (GetTimerSystemTime() - begin < uint64_t(kBUSCLK));
    if (!m.client.server) return m.fail("PCM RPC server did not appear within one second");
    if (!m.call(RALLY_PCM_START)) return false;
    m.ready = true;
    m.failure = "";
    std::puts("PS2 audio: open libsd, original IOP PCM queue, SPU2 DMA at 48000 Hz");
    return true;
}
bool AudioOutput::submit(std::span<const float> stereo) {
    auto &m = *impl_;
    if (!m.ready) return m.fail("audio is not initialized");
    if (*m.failure) return false; // A rejected full packet must never be overrun on retry.
    if (stereo.size() & 1) return m.fail("audio requires complete stereo frames");
    for (size_t i = 0; i < stereo.size(); i += 2) {
        if (!m.resampler.frame(stereo[i], stereo[i + 1], [&m](int16_t left, int16_t right) {
                const auto index = m.packet.frames++;
                m.packet.samples[index * 2] = left;
                m.packet.samples[index * 2 + 1] = right;
                return m.packet.frames != RALLY_PCM_PACKET_FRAMES || m.send();
            })) return false;
    }
    return true;
}
bool AudioOutput::flush() { return impl_->ready && impl_->send(); }
AudioOutput::Stats AudioOutput::stats() {
    // Preserve a partially filled packet: stats requests carry no samples.
    const auto frames = impl_->packet.frames;
    if (impl_->ready) impl_->call(RALLY_PCM_STATS);
    impl_->packet.frames = frames;
    return impl_->stats;
}
const char *AudioOutput::error() const { return impl_->failure; }
} // namespace ps2
