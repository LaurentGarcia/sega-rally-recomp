#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace ps2 {
struct alignas(16) TransformVector4 { float x=0,y=0,z=0,w=1; };
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
    bool available() const noexcept;
    const char* error() const noexcept;
    Vu1TransformStats stats() const noexcept;
private:
    bool transform_validated(const TransformMatrix&,const TransformMatrix&,
                             std::span<const TransformVector4>,std::span<TransformVector4>) noexcept;
    struct Storage;std::unique_ptr<Storage> storage_;
};
} // namespace ps2
