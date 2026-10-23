// Original EE/IOP wire protocol: stereo signed PCM16 at 48 kHz.
#pragma once
#include <stdint.h>

#define RALLY_PCM_RPC 0x53525043u
#define RALLY_PCM_VERSION 2u
#define RALLY_PCM_PACKET_FRAMES 256u
#define RALLY_PCM_RING_FRAMES 4096u
#define RALLY_PCM_DMA_FRAMES 512u
enum { RALLY_PCM_START = 1, RALLY_PCM_WRITE, RALLY_PCM_STATS, RALLY_PCM_STOP };
typedef struct {
    uint32_t version, frames;
    int16_t samples[RALLY_PCM_PACKET_FRAMES * 2];
} RallyPcmPacket;
typedef struct {
    uint32_t version;
    int32_t status;
    uint32_t accepted, queued, consumed, silent_frames, blocks, late_service_events;
} RallyPcmReply;
