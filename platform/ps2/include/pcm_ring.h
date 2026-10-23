// Original bounded IOP queue. Callers serialize access against the DMA thread.
// Empty input produces zero samples, never a replay of old ring contents.
#pragma once
#include "pcm_protocol.h"

typedef struct {
    int16_t samples[RALLY_PCM_RING_FRAMES * 2];
    uint32_t read, write, queued, consumed, silent_frames, blocks;
} RallyPcmRing;

static inline void rally_pcm_reset(RallyPcmRing *ring) {
    ring->read = ring->write = ring->queued = 0;
    ring->consumed = ring->silent_frames = ring->blocks = 0;
}
static inline unsigned rally_pcm_push(RallyPcmRing *ring, const int16_t *samples, unsigned frames) {
    unsigned i;
    if (frames > RALLY_PCM_RING_FRAMES - ring->queued) return 0;
    for (i = 0; i < frames; ++i) {
        ring->samples[ring->write * 2] = samples[i * 2];
        ring->samples[ring->write * 2 + 1] = samples[i * 2 + 1];
        ring->write = (ring->write + 1) % RALLY_PCM_RING_FRAMES;
    }
    ring->queued += frames;
    return frames;
}
static inline void rally_pcm_render(RallyPcmRing *ring, int16_t *dma) {
    unsigned i;
    for (i = 0; i < RALLY_PCM_DMA_FRAMES; ++i) {
        int16_t left = 0, right = 0;
        if (ring->queued) {
            left = ring->samples[ring->read * 2];
            right = ring->samples[ring->read * 2 + 1];
            ring->read = (ring->read + 1) % RALLY_PCM_RING_FRAMES;
            --ring->queued;
            ++ring->consumed;
        } else {
            ++ring->silent_frames;
        }
        /* SPU2 block DMA: alternating 256-sample left and right planes. */
        dma[(i / 256) * 512 + i % 256] = left;
        dma[(i / 256) * 512 + 256 + i % 256] = right;
    }
    ++ring->blocks;
}
