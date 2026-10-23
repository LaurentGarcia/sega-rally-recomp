// Exercise the actual EE transport against deterministic SDK/RPC mocks.
#include "audio_output.h"
#include "pcm_ring.h"
#include <sifrpc.h>
#include <timer.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
static RallyPcmRing ring;
static unsigned stops, writes, modules, late_services;
static uint64_t ticks;
static bool full, partial, module_failure, bind_missing, rpc_failure;
static void reset() {
    rally_pcm_reset(&ring); stops = writes = modules = late_services = 0; ticks = 0;
    full = partial = module_failure = bind_missing = rpc_failure = false;
}
extern "C" {
unsigned char libsd_irx[1]{}, rally_pcm_irx[1]{};
unsigned int size_libsd_irx = 1, size_rally_pcm_irx = 1;
}
void SifInitRpc(int) {}
int sbv_patch_enable_lmb() { return 0; }
int SifExecModuleBuffer(void *, int, int, const char *, int *result) {
    ++modules; *result = module_failure ? -1 : 0; return *result;
}
uint64_t GetTimerSystemTime() { ticks += kBUSCLK / 100; return ticks; }
int SifBindRpc(SifRpcClientData_t *client, unsigned id, int) {
    check(id == RALLY_PCM_RPC, "wrong PCM RPC identifier");
    if (!bind_missing) client->server = &ring;
    return 0;
}
int SifCallRpc(SifRpcClientData_t *, unsigned command, int, void *request, int request_bytes,
               void *response, int response_bytes, void (*)(void *), void *) {
    if (rpc_failure) return -1;
    const auto &packet = *static_cast<RallyPcmPacket *>(request);
    check(packet.version == RALLY_PCM_VERSION && packet.frames <= RALLY_PCM_PACKET_FRAMES,
          "malformed client packet");
    check(request_bytes == int(8 + packet.frames * 4) && response_bytes == sizeof(RallyPcmReply),
          "incorrect RPC wire length");
    RallyPcmReply reply{}; reply.version = RALLY_PCM_VERSION;
    if (command == RALLY_PCM_START) rally_pcm_reset(&ring);
    else if (command == RALLY_PCM_STOP) { ++stops; ring.queued = 0; }
    else if (command == RALLY_PCM_WRITE) {
        ++writes;
        reply.accepted = partial ? 1 : full ? 0 : rally_pcm_push(&ring, packet.samples, packet.frames);
    } else check(command == RALLY_PCM_STATS, "unknown PCM operation");
    reply.queued = ring.queued; reply.consumed = ring.consumed;
    reply.silent_frames = ring.silent_frames; reply.blocks = ring.blocks;
    reply.late_service_events = late_services;
    std::memcpy(response, &reply, sizeof(reply));
    return 0;
}
int main() {
    try {
        std::array<float, 2000> stereo;
        for (unsigned i = 0; i < 1000; ++i) { stereo[i * 2] = 0.5f; stereo[i * 2 + 1] = -0.5f; }
        reset();
        {
            ps2::AudioOutput audio;
            check(audio.initialize() && audio.initialize() && modules == 2, "initialization must be idempotent");
            check(audio.submit(stereo), "submit real transport");
            check(audio.stats().submitted_frames == 1024, "partial packet should stay bounded on EE");
            check(audio.flush(), "flush after querying stats");
            check(audio.stats().submitted_frames == 1088 && ring.queued == 1088, "stats lost partial packet");
            late_services = 3;
            check(audio.stats().late_service_events == 3 && audio.stats().silent_frames == 0,
                  "DMA worker lateness must remain distinct from ring starvation");
            for (unsigned i = 0; i < ring.queued; ++i)
                check(ring.samples[i * 2] == 16384 && ring.samples[i * 2 + 1] == -16384, "PCM content/channel order");
        }
        check(stops == 1 && !ring.queued, "destructor must stop queued output");
        reset(); full = true;
        {
            ps2::AudioOutput audio;
            check(audio.initialize() && !audio.submit(stereo), "full queue must time out");
            check(writes > 1 && writes <= 101, "queue backpressure is bounded");
            const auto before = writes;
            check(!audio.submit(stereo) && writes == before, "failed full packet retry must not overrun storage");
        }
        reset(); partial = true;
        { ps2::AudioOutput audio; check(audio.initialize() && !audio.submit(stereo), "partial acceptance must fail visibly"); }
        reset(); module_failure = true;
        { ps2::AudioOutput audio; check(!audio.initialize(), "module load failure must propagate"); }
        check(stops == 0, "uninitialized transport must not call stop");
        reset(); bind_missing = true;
        { ps2::AudioOutput audio; check(!audio.initialize() && ticks < kBUSCLK * 2, "bind timeout must be bounded"); }
        reset();
        { ps2::AudioOutput audio; check(audio.initialize(), "RPC fixture init"); rpc_failure = true;
          check(!audio.submit(stereo), "RPC send failure must propagate"); }
        reset();
        { ps2::AudioOutput audio; check(audio.initialize() && !audio.submit(std::span(stereo).first(3)), "odd channel count must fail"); }
        std::puts("PS2 actual EE audio transport mock checks passed; hardware untested.");
        return 0;
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
