// Original SPU2 PCM transport using the open PS2SDK libsd and kernel APIs.
#include <irx.h>
#include <loadcore.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <sifrpc.h>
#include <libsd.h>
#include "pcm_ring.h"

IRX_ID("rally_pcm", 1, 0);
static RallyPcmRing ring;
static int16_t dma_buffer[RALLY_PCM_DMA_FRAMES * 4] __attribute__((aligned(64)));
static RallyPcmPacket request __attribute__((aligned(16)));
static RallyPcmReply reply __attribute__((aligned(16)));
static SifRpcDataQueue_t rpc_queue;
static SifRpcServerData_t rpc_server;
static int transfer_sema = -1, audio_tid = -1, started = 0;

static int transfer_done(void *unused) {
    (void)unused;
    iSignalSema(transfer_sema);
    return 1;
}

static void audio_thread(void *unused) {
    (void)unused;
    for (;;) {
        int state;
        unsigned idle;
        WaitSema(transfer_sema);
        CpuSuspendIntr(&state);
        idle = 1u - ((sceSdBlockTransStatus(1, 0) >> 24) & 1u);
        rally_pcm_render(&ring, dma_buffer + idle * RALLY_PCM_DMA_FRAMES * 2);
        CpuResumeIntr(state);
    }
}

static int start_audio(void) {
    iop_sema_t semaphore = {0, 0, 0, 1};
    iop_thread_t thread = {TH_C, 0, audio_thread, 2048, 36};
    int state;
    CpuSuspendIntr(&state);
    rally_pcm_reset(&ring);
    CpuResumeIntr(state);
    if (started) return 0;
    if (sceSdInit(0) < 0) return -1; // Cold initialization of the open driver.
    transfer_sema = CreateSema(&semaphore);
    if (transfer_sema < 0) return -2;
    audio_tid = CreateThread(&thread);
    if (audio_tid < 0) { DeleteSema(transfer_sema); transfer_sema = -1; return -3; }
    if (StartThread(audio_tid, 0) < 0) {
        DeleteThread(audio_tid); DeleteSema(transfer_sema); transfer_sema = -1; return -4;
    }
    sceSdSetParam(0 | SD_PARAM_MVOLL, 0);
    sceSdSetParam(0 | SD_PARAM_MVOLR, 0);
    sceSdSetParam(0 | SD_PARAM_BVOLL, 0);
    sceSdSetParam(0 | SD_PARAM_BVOLR, 0);
    sceSdSetParam(1 | SD_PARAM_MVOLL, 0x3fff);
    sceSdSetParam(1 | SD_PARAM_MVOLR, 0x3fff);
    sceSdSetParam(1 | SD_PARAM_AVOLL, 0x7fff);
    sceSdSetParam(1 | SD_PARAM_AVOLR, 0x7fff);
    sceSdSetParam(1 | SD_PARAM_BVOLL, 0x3fff);
    sceSdSetParam(1 | SD_PARAM_BVOLR, 0x3fff);
    sceSdSetTransCallback(1, transfer_done);
    if (sceSdBlockTrans(1, SD_TRANS_WRITE | SD_TRANS_LOOP, (u8 *)dma_buffer, sizeof(dma_buffer), 0) < 0) {
        sceSdSetTransCallback(1, 0); TerminateThread(audio_tid);
        DeleteThread(audio_tid); DeleteSema(transfer_sema); transfer_sema = -1; return -5;
    }
    started = 1;
    return 0;
}

static void *dispatch(int function, void *buffer, int bytes) {
    RallyPcmPacket *packet = (RallyPcmPacket *)buffer;
    int state;
    reply.version = RALLY_PCM_VERSION;
    reply.status = 0;
    reply.accepted = 0;
    if (bytes < 8 || packet->version != RALLY_PCM_VERSION) reply.status = -10;
    else if (function == RALLY_PCM_START) reply.status = start_audio();
    else if (function == RALLY_PCM_WRITE) {
        if (!started || packet->frames > RALLY_PCM_PACKET_FRAMES ||
            bytes != (int)(8 + packet->frames * 4)) reply.status = -11;
        else {
            CpuSuspendIntr(&state);
            reply.accepted = rally_pcm_push(&ring, packet->samples, packet->frames);
            CpuResumeIntr(state);
        }
    } else if (function == RALLY_PCM_STOP) {
        CpuSuspendIntr(&state);
        ring.read = ring.write = ring.queued = 0;
        CpuResumeIntr(state);
        /* Both DMA halves become zero within two transfer completions. */
    } else if (function != RALLY_PCM_STATS) reply.status = -12;
    CpuSuspendIntr(&state);
    reply.queued = ring.queued; reply.consumed = ring.consumed;
    reply.silent_frames = ring.silent_frames; reply.blocks = ring.blocks;
    CpuResumeIntr(state);
    return &reply;
}

static void rpc_thread(void *unused) {
    (void)unused;
    SifInitRpc(0);
    SifSetRpcQueue(&rpc_queue, GetThreadId());
    SifRegisterRpc(&rpc_server, RALLY_PCM_RPC, dispatch, &request, 0, 0, &rpc_queue);
    SifRpcLoop(&rpc_queue);
}

int _start(int argc, char **argv) {
    iop_thread_t thread = {TH_C, 0, rpc_thread, 2048, 40};
    int tid;
    (void)argc; (void)argv;
    rally_pcm_reset(&ring);
    tid = CreateThread(&thread);
    if (tid < 0) return MODULE_NO_RESIDENT_END;
    if (StartThread(tid, 0) < 0) { DeleteThread(tid); return MODULE_NO_RESIDENT_END; }
    return MODULE_RESIDENT_END;
}
