// Compile the real original IOP server against deterministic public-API mocks.
// Callback order models the pinned open driver; no hardware timing is claimed.
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _start rally_pcm_module_start
#include "../iop/pcm_server.c"
#undef _start

static struct {
    int failure, masked, active_half, starts, sema_deletes, thread_deletes, terminations;
    int signals, wait_calls, init_calls, volumes[0x602];
    int (*callback)(void *);
} fake;
static jmp_buf worker_yield;
static void check(int condition, const char *message) {
    if (!condition) { fprintf(stderr, "IOP mock: %s\n", message); exit(1); }
}
static void reset(void) {
    memset(&fake, 0, sizeof(fake));
    memset(dma_buffer, 0, sizeof(dma_buffer));
    rally_pcm_reset(&ring);
    transfer_sema = audio_tid = -1;
    started = 0;
    dma_completions = dma_serviced = late_service_events = 0;
}
int iSignalSema(int id) { check(id == 11, "callback semaphore"); ++fake.signals; return fake.signals > 1 ? -1 : 0; }
int WaitSema(int id) {
    check(id == 11, "worker semaphore");
    if (fake.wait_calls++) longjmp(worker_yield, 1);
    fake.signals = 0;
    return 0;
}
int CreateSema(iop_sema_t *s) { check(s->initial == 0 && s->maximum == 1, "bounded completion semaphore"); return fake.failure == 2 ? -1 : 11; }
int DeleteSema(int id) { check(id == 11, "deletes owned semaphore"); ++fake.sema_deletes; return 0; }
int CreateThread(iop_thread_t *t) { check(t->stack == 2048 && (t->priority == 36 || t->priority == 40), "IOP thread budget"); return fake.failure == 3 ? -1 : 12; }
int StartThread(int id, void *arg) { check(id == 12 && !arg, "starts owned thread"); ++fake.starts; return fake.failure == 4 ? -1 : 0; }
int TerminateThread(int id) { check(id == 12, "terminates owned thread"); ++fake.terminations; return 0; }
int DeleteThread(int id) { check(id == 12, "deletes owned thread"); ++fake.thread_deletes; return 0; }
int GetThreadId(void) { return 12; }
int CpuSuspendIntr(int *previous) { *previous = fake.masked; fake.masked = 1; return 0; }
int CpuResumeIntr(int previous) { check(fake.masked, "balanced critical section"); fake.masked = previous; return 0; }
int sceSdInit(int flag) { check(!flag, "cold open-driver initialization"); ++fake.init_calls; return fake.failure == 1 ? -1 : 0; }
void sceSdSetParam(unsigned reg, unsigned value) { check(reg < 0x602, "known volume register"); fake.volumes[reg] = (int)value; }
void sceSdSetTransCallback(int core, int (*callback)(void *)) { check(core == 1, "SPU2 core 1 callback"); fake.callback = callback; }
int sceSdBlockTrans(int core, unsigned mode, u8 *buffer, unsigned bytes, ...) {
    check(core == 1 && mode == (SD_TRANS_WRITE | SD_TRANS_LOOP), "looping core 1 DMA");
    check(buffer == (u8 *)dma_buffer && bytes == 4096, "bounded double buffer");
    return fake.failure == 5 ? -1 : 0;
}
unsigned sceSdBlockTransStatus(int core, int flag) {
    check(core == 1 && flag == 0 && fake.masked, "DMA half read in critical section");
    return (unsigned)fake.active_half << 24;
}
void SifInitRpc(int flag) { check(!flag, "RPC initialization"); }
void SifSetRpcQueue(SifRpcDataQueue_t *queue, int thread) { (void)queue; check(thread == 12, "RPC thread"); }
void SifRegisterRpc(SifRpcServerData_t *server, unsigned id, void *(*fn)(int, void *, int),
                    void *buffer, void *cfn, void *cbuffer, SifRpcDataQueue_t *queue) {
    (void)server; (void)fn; (void)buffer; (void)cfn; (void)cbuffer; (void)queue;
    check(id == RALLY_PCM_RPC, "original RPC identity");
}
void SifRpcLoop(SifRpcDataQueue_t *queue) { (void)queue; }
static void complete(void) {
    check(fake.callback != NULL && !fake.masked, "live completion callback");
    // Pinned freesd rearms the other half BEFORE invoking the callback.
    fake.active_half ^= 1;
    fake.callback(NULL);
}
static void service(void) {
    fake.wait_calls = 0;
    if (!setjmp(worker_yield)) audio_thread(NULL);
    check(!fake.masked, "worker restores interrupts");
}
static RallyPcmReply command(unsigned op, unsigned frames) {
    static RallyPcmPacket packet;
    unsigned i;
    packet.version = RALLY_PCM_VERSION;
    packet.frames = frames;
    for (i = 0; i < RALLY_PCM_PACKET_FRAMES; ++i) {
        packet.samples[i * 2] = 1000;
        packet.samples[i * 2 + 1] = -2000;
    }
    return *(RallyPcmReply *)dispatch((int)op, &packet, (int)(8 + frames * 4));
}
int main(void) {
    unsigned i;
    RallyPcmReply result;
    reset();
    check(command(RALLY_PCM_START, 0).status == 0, "starts actual module");
    check(fake.volumes[1 | SD_PARAM_BVOLL] == 0x7fff && fake.volumes[1 | SD_PARAM_BVOLR] == 0x7fff,
          "PCM input near-unity uses Q15");
    check(fake.volumes[1 | SD_PARAM_MVOLL] == 0x3fff && fake.volumes[1 | SD_PARAM_MVOLR] == 0x3fff,
          "master near-unity uses Q14");
    check(command(RALLY_PCM_WRITE, 256).accepted == 256 && command(RALLY_PCM_WRITE, 256).accepted == 256,
          "queues one DMA half of distinct stereo PCM");
    complete(); service();
    result = command(RALLY_PCM_STATS, 0);
    check(result.consumed == 512 && !result.silent_frames && !result.late_service_events,
          "on-time nonempty service has no starvation or late completion");
    for (i = 0; i < RALLY_PCM_DMA_FRAMES; ++i)
        check(dma_buffer[(i / 256) * 512 + i % 256] == 1000 &&
              dma_buffer[(i / 256) * 512 + 256 + i % 256] == -2000, "inactive DMA half channel layout");
    complete(); complete();
    result = command(RALLY_PCM_STATS, 0);
    check(result.late_service_events == 1 && !result.silent_frames,
          "coalesced completion records worker lateness independently of empty-ring frames");
    service();
    result = command(RALLY_PCM_STATS, 0);
    check(result.late_service_events == 1 && result.silent_frames == 512, "worker resumes and zero-fills empty input");
    for (i = 0; i < RALLY_PCM_DMA_FRAMES * 2; ++i) check(!dma_buffer[i], "previously populated half becomes silent");
    check(command(RALLY_PCM_WRITE, 256).accepted == 256, "queue data after delayed service");
    service(); // A previously posted wake remains after catching up to the latest half.
    result = command(RALLY_PCM_STATS, 0);
    check(result.queued == 256 && result.consumed == 512 && result.blocks == 2,
          "stale semaphore wake must not consume PCM or refill the same DMA half twice");
    complete(); service();
    check(command(RALLY_PCM_STATS, 0).late_service_events == 1, "on-time starvation adds no late-service event");
    dma_completions = dma_serviced = UINT32_MAX;
    complete(); service();
    check(command(RALLY_PCM_STATS, 0).late_service_events == 1, "completion sequence wrap is harmless");
    check(command(RALLY_PCM_WRITE, 256).accepted == 256, "queue before stop");
    command(RALLY_PCM_STOP, 0);
    complete(); service(); complete(); service();
    for (i = 0; i < RALLY_PCM_DMA_FRAMES * 4; ++i) check(!dma_buffer[i], "stop silences both halves after service");
    check(command(RALLY_PCM_START, 0).status == 0 && fake.init_calls == 1, "restart reuses live transport");
    check(!command(RALLY_PCM_STATS, 0).late_service_events, "restart resets service telemetry");
    {
        RallyPcmPacket old = {1, 0, {0}};
        check(((RallyPcmReply *)dispatch(RALLY_PCM_STATS, &old, 8))->status == -10, "old protocol rejected");
    }
    check(command(RALLY_PCM_WRITE, RALLY_PCM_PACKET_FRAMES + 1).status == -11, "oversized PCM packet rejected");
    for (i = 1; i <= 5; ++i) {
        reset(); fake.failure = (int)i;
        check(command(RALLY_PCM_START, 0).status < 0 && !started, "start failure stays unavailable");
        check(fake.sema_deletes == (i >= 3) && fake.thread_deletes == (i >= 4), "partial startup resources reclaimed");
        check(fake.terminations == (i == 5) && !fake.callback, "failed DMA start clears callback and running thread");
    }
    reset(); fake.failure = 3;
    check(rally_pcm_module_start(0, NULL) == MODULE_NO_RESIDENT_END, "RPC thread allocation failure unloads module");
    reset(); fake.failure = 4;
    check(rally_pcm_module_start(0, NULL) == MODULE_NO_RESIDENT_END && fake.thread_deletes == 1,
          "RPC thread start failure cleans up");
    puts("PS2 actual IOP gain, DMA service telemetry and failure cleanup checks passed; hardware timing untested.");
    return 0;
}
