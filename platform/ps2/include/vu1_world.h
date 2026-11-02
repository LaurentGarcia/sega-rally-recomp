#pragma once
#include "vu1_transform.h"

namespace ps2 {
inline constexpr std::size_t vu1_world_triangle_capacity=16;
inline constexpr std::size_t vu1_world_corner_capacity=3*vu1_world_triangle_capacity;
// Model-space corners preserve per-face UV seams. The two texture scales are
// reciprocal power-of-two dimensions (8..256), or one for an untextured face.
struct alignas(16) Vu1WorldCorner {
    TransformVector4 position;
    float u=0,v=0,inv_width=1,inv_height=1;
};
// Renderer-owned, already resolved GS context-1 values. This is an internal
// engine handoff: the Display retains texture/CLUT residency until completion.
// rgba uses GS byte order and its 0..128 alpha convention, not asset ARGB.
struct Vu1WorldState {
    std::uint64_t test=0,tex0=0,clamp=0,prim=0;
    std::uint32_t rgba=0;
};
enum class Vu1WorldCompletion : std::uint8_t {
    gs_finished,
    // Internal renderer handoff: PATH1 is fully retired, but GS drawing may
    // remain queued. The Display must finish it before VRAM overwrite, flip,
    // readback, rejection return or owner release. No FINISH event is emitted.
    path1_retired
};
struct Vu1WorldBatch {
    TransformMatrix object_to_world{},world_to_clip{};
    std::span<const Vu1WorldCorner> corners;
    std::span<const Vu1WorldState> states;
    unsigned output_height=448;
    std::uint32_t sequence=0;
    Vu1WorldCompletion completion=Vu1WorldCompletion::gs_finished;
};
struct Vu1WorldGeometry {
    TransformMatrix object_to_world{},world_to_clip{};
    std::span<const Vu1WorldCorner> corners;
    unsigned output_height=448;
    std::uint32_t sequence=0;
};
// A copy is another handle to the SAME single-use preparation, not a new lease.
// The process-wide nonwrapping ticket prevents owner-address reuse from making
// an old handle valid. Only the exclusive, caller-serialized owner may use it.
class Vu1WorldToken {
public:
    Vu1WorldToken() noexcept=default;
private:
    const Vu1Transform* owner_=nullptr;
    std::uint64_t ticket_=0;
    friend class Vu1Transform;
};
// One in-flight DMA/VU operation. Copies identify the same single-use job.
// A submission is distinct from the ready-bank token produced by preparation.
class Vu1WorldSubmission {
public:
    Vu1WorldSubmission() noexcept=default;
private:
    const Vu1Transform* owner_=nullptr;
    std::uint64_t ticket_=0;
    friend class Vu1Transform;
};
enum class Vu1WorldSubmitResult : std::uint8_t { submitted,rejected,failed };
enum class Vu1WorldPollResult : std::uint8_t { pending,complete,rejected,failed };
enum class Vu1WorldPrepareResult : std::uint8_t { ready,fallback,rejected,failed };
enum class Vu1WorldResult : std::uint8_t {
    drawn,     // Packet retired to requested completion; clip stays untouched.
    fallback,  // No GIF emission; complete valid clip output is published.
    rejected,  // No GIF emission; clip output untouched; scalar retry allowed.
    failed     // Transport/ownership fault; no further drawing or flip allowed.
};
struct Vu1WorldStats {
    std::uint32_t batches=0,drawn_batches=0,fallback_batches=0,rejected_batches=0;
    std::uint64_t corners=0,dma_qwords=0;
    std::size_t persistent_bytes=0;
    std::uint32_t prepared_batches=0,ready_batches=0,cancelled_batches=0;
};
// Bounded numerical and packet-shape admission, independent of hardware.
// Resolved TEST/TEX0/CLAMP values remain the Display owner's responsibility.
bool valid_native_vu1_world_input(const Vu1WorldBatch&) noexcept;
// Same numerical admission as fused input, without requiring resident state.
// Empty geometry is invalid for preparation; fused empty draws remain valid.
bool valid_native_vu1_world_geometry(const Vu1WorldGeometry&) noexcept;
} // namespace ps2
