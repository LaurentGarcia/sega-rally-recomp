#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace ps2 {
struct Vu1WorldBatch;
struct Vu1WorldGeometry;
struct Vu1WorldState;
class Vu1WorldToken;
class Vu1WorldSubmission;
struct Vu1WorldStats;
enum class Vu1WorldResult : std::uint8_t;
enum class Vu1WorldPrepareResult : std::uint8_t;
enum class Vu1WorldCompletion : std::uint8_t;
enum class Vu1WorldSubmitResult : std::uint8_t;
enum class Vu1WorldPollResult : std::uint8_t;
struct alignas(16) TransformVector4 { float x=0,y=0,z=0,w=1; };
// Native logical496x384 screen coordinates, reverse depth and reciprocal w.
// An all-zero value (q==0) asks the caller to project/clip the retained clip
// vector normally. It is not a vertex at the screen origin.
struct alignas(16) TransformProjectedVertex { float x=0,y=0,depth=0,q=0; };
using TransformMatrix = std::array<float,16>; // column-major, column vectors
inline constexpr std::size_t vu1_transform_capacity=96;
struct TransformErrorBound { std::array<double,4> absolute{}; };
struct TransformProjectionBudget {
    double pixels=0.125; // logical 496x384 pixels
    double depth=2.0/65535.0; // native reverse-depth fraction
};
struct TransformProjectionError { double pixels=0,depth=0; };
enum class TransformGuard { accepted, invalid, near_clip_plane, projection_budget };

// Rendering-only arithmetic enclosure relative to the two binary64 matrix
// products used by WorldScene. Inputs/matrices are normal-or-zero binary32,
// abs<=65536. The first matrix is affine and every input w is exactly1.
// The bound allows two ulps per VU arithmetic operation, flush-to-zero, and
// the reference's binary64 rounding. This conservative allowance is validated
// separately on the executing target; it is not a hardware-fidelity claim.
// Returns false atomically for unsupported/overflowing input. Empty is valid.
bool bound_vu1_transform(const TransformMatrix&,const TransformMatrix&,
                         std::span<const TransformVector4>,TransformErrorBound&) noexcept;
// Guard ALL seven native clip planes before using approximate coordinates.
// The projected-error formula is conservative over the enclosed reference
// coordinate, including perspective division. Outside/near-plane vertices
// return near_clip_plane so the caller can use its reference transform.
TransformGuard guard_vu1_projection(const TransformVector4&,const TransformErrorBound&,
                                   const TransformProjectionBudget&,
                                   TransformProjectionError* =nullptr) noexcept;
// Prepares conservative inward-rounded thresholds for vertices with exact
// w=1. Their hot guard uses encoding/integer comparisons; other w values keep
// the general projection formula. No vertex arithmetic or budget is changed.
// The returned estimate is the accepted unit-w box's worst case, not a tighter
// per-vertex estimate. False preparation preserves the previous plan.
class PreparedProjectionGuard {
public: PreparedProjectionGuard() noexcept=default;
private:
    TransformErrorBound error_{};TransformProjectionBudget budget_{};
    TransformProjectionError estimate_{};
    std::array<std::uint32_t,4> plane_{}; // absX,absY,minZ,maxZ
    std::array<std::uint32_t,3> budget_limit_{}; // absX,absY,maxZ
    bool valid_=false,unit_w_=false,plane_possible_=false,budget_possible_=false;
    friend bool prepare_vu1_projection_guard(const TransformErrorBound&,const TransformProjectionBudget&,PreparedProjectionGuard&) noexcept;
    friend TransformGuard guard_prepared_vu1_projection(const TransformVector4&,const PreparedProjectionGuard&,TransformProjectionError*) noexcept;
};
bool prepare_vu1_projection_guard(const TransformErrorBound&,const TransformProjectionBudget&,PreparedProjectionGuard&) noexcept;
TransformGuard guard_prepared_vu1_projection(const TransformVector4&,const PreparedProjectionGuard&,TransformProjectionError* =nullptr) noexcept;
// Independent binary64 transform, useful for diagnostics and scalar fallback.
// Same bounded input contract. Atomic on rejection; overlap is supported.
bool transform_reference(const TransformMatrix&,const TransformMatrix&,
                         std::span<const TransformVector4>,std::span<TransformVector4>) noexcept;
// Encoding-only native input validation, available to portable tests/tools.
bool valid_native_vu1_transform_input(const TransformMatrix&,const TransformMatrix&,
                                      std::span<const TransformVector4>) noexcept;

struct Vu1TransformStats { std::uint64_t batches=0,vertices=0,dma_qwords=0;
    std::size_t persistent_bytes=0; };
// Exclusive, caller-serialized VIF1/VU1 lease. No VU0 registers are touched.
// Caller must not submit other VIF1/VU1 work during this owner's lifetime.
// Each transform fences FLUSHE/MARK and DMA before reading VU memory or
// returning; therefore subsequent VU0 packing and GS transfers are separate
// synchronous operations. No simultaneous-execution/speedup claim is implied.
// Initialization is idempotent on the same owner; another owner is rejected.
// A transport failure quarantines the lease until process restart, and retains
// DMA source storage if stopping its owned channel cannot be established.
class Vu1Transform {
public:
    Vu1Transform() noexcept; ~Vu1Transform();
    Vu1Transform(const Vu1Transform&)=delete;Vu1Transform& operator=(const Vu1Transform&)=delete;
    bool initialize() noexcept;
    bool transform(const TransformMatrix&,const TransformMatrix&,
                   std::span<const TransformVector4>,std::span<TransformVector4>,
                   TransformErrorBound&) noexcept;
    // Native binary32 presentation: same VU kernel and atomic result checks,
    // without binary64 enclosure/projection guards. Normal-or-zero inputs and
    // matrices have abs<=65536, object matrix affine, inputw1, count<=96.
    // These bounds keep all intermediates below2^52 (no exponent overflow).
    // Native underflow flush is intentional. Returned clip values may lie
    // outside the frustum; they remain normal-or-zero with abs<=65536.
    // Numerical output rejection preserves both output and the usable lease.
    bool transform_native(const TransformMatrix&,const TransformMatrix&,
                          std::span<const TransformVector4>,std::span<TransformVector4>) noexcept;
    // Optional native presentation continuation. Preserves the exact existing
    // clip outputs; a separate, lazily loaded kernel projects interior vertices
    // with one VU reciprocal. All seven planes use inclusive boundaries:
    // w>=1/1024, -w<=x,y<=w, 0<=z<=w. Outside or unusable projected results are
    // all-zero; clip coordinates remain available for ordinary ordered fallback.
    // Nonzero-q results are normal-or-zero, x[0,496], y[0,384], depth[0,1],
    // q[2^-16,1024]. Presentation allowances relative to scalar projection of
    // the same clip vector are1/32pixel,2^-20 absolute depth and2^-20 relative q;
    // authored/scene-corpus validation is separate from hardware fidelity and
    // full-frame raster equivalence. No recovered game arithmetic is changed.
    // Both output prefixes publish atomically after completion. Each span must
    // hold input.size() elements. Input may overlap clip output; projected
    // output may overlap neither. Nonempty buffers must be16-byte aligned.
    // Invalid input/overlap rejects before DMA. Empty succeeds on a ready
    // owner without uploading the projection kernel. No allocation per call.
    bool transform_project_native(const TransformMatrix&,const TransformMatrix&,
                                  std::span<const TransformVector4>,std::span<TransformVector4>,
                                  std::span<TransformProjectedVertex>) noexcept;
    // Explicit optional setup allocates persistent direct-GS staging and loads
    // a separate kernel. Uses this same exclusive VIF1/VU1 lease. The Display
    // owns GS/GIF and must retire its PATH3 state/uploads before draw_world().
    // No per-call allocation or concurrent producer. The submit/finish API
    // permits EE-only gathering while one owned hardware operation is pending.
    bool initialize_world() noexcept;
    Vu1WorldResult draw_world(const Vu1WorldBatch&,std::span<TransformVector4> fallback_clip) noexcept;
    // Geometry before residency. No GS packet is emitted by preparation.
    // Ready writes token only and retains ST/XYZ; fallback writes clip only.
    // Rejected/failed calls preserve both caller outputs. Empty is rejected.
    // Requires idle VIF/DMA/GIF transport, but not completed GS raster work.
    Vu1WorldPrepareResult prepare_world(const Vu1WorldGeometry&,Vu1WorldToken&,
                                       std::span<TransformVector4> fallback_clip) noexcept;
    // A successful emit or cancel consumes every copy of the token. Invalid
    // token/state/output arguments reject before mutation and leave a current
    // ticket reusable. Fatal ownership/transport errors invalidate it.
    // Display may upload textures between prepare and emit, retaining the same
    // exclusive renderer ownership and retiring PATH3 before either operation.
    Vu1WorldResult emit_world(const Vu1WorldToken&,std::span<const Vu1WorldState>,
                              Vu1WorldCompletion) noexcept;
    Vu1WorldResult cancel_world(const Vu1WorldToken&,std::span<TransformVector4>) noexcept;
    // Nonblocking submission under the same exclusive lease. The owner copies
    // all inputs into persistent DMA storage before returning submitted. Caller
    // may then gather another batch in separate EE memory, but must not submit
    // PATH3, VIF/VU work or mutate retained GS resources before finish/drain
    // consumes the submission (even when poll has already reported complete).
    // At most one hardware operation is in flight; up to two completed ready
    // bank tokens may coexist. Async preparation preserves the other bank.
    Vu1WorldSubmitResult submit_prepare_world(const Vu1WorldGeometry&,Vu1WorldSubmission&) noexcept;
    Vu1WorldSubmitResult submit_emit_world(const Vu1WorldToken&,std::span<const Vu1WorldState>,
                                          Vu1WorldCompletion,Vu1WorldSubmission&) noexcept;
    // Poll does not wait or read mapped VU memory; complete means it is safe
    // for finish to inspect status. Modular deadlines cover the whole job.
    Vu1WorldPollResult poll_world(const Vu1WorldSubmission&) noexcept;
    Vu1WorldPrepareResult finish_prepare_world(const Vu1WorldSubmission&,Vu1WorldToken&,
                                               std::span<TransformVector4>) noexcept;
    Vu1WorldResult finish_emit_world(const Vu1WorldSubmission&) noexcept;
    // Bounded retirement without publishing a ready token or clips. A pending
    // deferred emit still leaves GS FINISH to Display before resource reuse.
    // Destruction also drains owned pending work before releasing its buffers.
    bool drain_world(const Vu1WorldSubmission&) noexcept;
    // Bad finish output arguments retain the live submission. Completed
    // numerical rejection consumes it without publishing clips. Fatal errors
    // quarantine the owner. Synchronous mutating calls reject BUSY while a job
    // is live, including after poll reports complete but before finish/drain.
    // With no live submission, every synchronous transform/draw/prepare attempt
    // invalidates older preparations, including empty/invalid attempts. BUSY
    // rejection preserves the in-flight operation and both banks. Async submit
    // preparation preserves other ready banks. Idempotent initialization retains
    // them; actual initialization/program uploads invalidate them. Calls are serialized
    // process-wide; the global ticket allocator rejects exhaustion, never wraps.
    Vu1WorldStats world_stats() const noexcept;
    bool available() const noexcept;
    const char* error() const noexcept;
    Vu1TransformStats stats() const noexcept;
private:
    void invalidate_world() noexcept;
    bool world_pending() noexcept;
    bool transform_validated(const TransformMatrix&,const TransformMatrix&,
                             std::span<const TransformVector4>,std::span<TransformVector4>,
                             std::span<TransformProjectedVertex> = {}) noexcept;
    struct Storage;std::unique_ptr<Storage> storage_;
};
} // namespace ps2
