#pragma once
#include "native_geometry.h"
#include "vu1_transform.h"
#include <memory>

namespace ps2::native {
using WorldMatrix=std::array<float,16>; // column-major, column vectors
struct WorldPoint { float x=0,y=0,z=0; };
struct WorldFace { std::array<std::uint32_t,3> indices{};std::uint32_t argb=0xffffffffu; };
struct WorldFaceGroup { std::uint32_t first_face=0,face_count=0; };
struct WorldModel { std::span<const WorldPoint> vertices;std::span<const WorldFace> faces;
    // Optional nonempty, contiguous partition of all faces. Bounds are derived
    // from validated vertices each call; no caller-supplied bound is trusted.
    std::span<const WorldFaceGroup> groups{}; };
struct WorldInstance { WorldModel model;WorldMatrix object_to_world{}; };
struct WorldView { WorldMatrix world_to_clip{};std::uint32_t clear_color=0xff101923u; };
enum class WorldNumericMode { reference, native_float };
enum class WorldSceneError { none,not_configured,invalid_capacity,invalid_model,invalid_transform,
    input_limit,output_limit,clipping_failed,allocation_failed };
struct WorldSceneStats { std::uint32_t instances=0,vertices=0,source_faces=0,output_triangles=0;
    std::size_t persistent_bytes=0;
    std::uint32_t groups=0,culled_groups=0,transformed_vertices=0,vertex_cache_hits=0;
    std::uint32_t batch_calls=0,batch_vertices=0,accepted_batch_vertices=0,failed_batches=0,clipped_batch_fallbacks=0;
    // Consumed approximate input corners of nondegenerate output triangles,
    // after any clipping fallback; distinct from prefetched/accepted vertices.
    std::uint32_t provider_triangle_corners=0;
    // Optional caller-clock units, reset per successful frame. Clock overhead
    // is included; these diagnostics are not an uninstrumented timing claim.
    std::uint64_t validation_ticks=0,bounds_ticks=0,transform_ticks=0,clipping_ticks=0;
    WorldNumericMode numeric_mode=WorldNumericMode::reference; };
using WorldSceneClock=std::uint64_t (*)(void* context) noexcept;
// Optional rendering-only provider, never a game-state transform. Every call
// has1..96 distinct input vertices and equally sized output/accepted spans.
// Inputs have w=1; arrays are separate, nonoverlapping and16-byte aligned.
// Return true for completed work, marking usable results1;
//0 means ordinary numerical/visual-budget rejection and uses scalar fallback.
// Return false for unavailable/failed transport: every output is ignored,
// failed_batches increments, and all vertices use the selected mode's scalar path.
// Reference mode requires individually guarded inside results (e.g. the VU1
// projection guard). Native-float mode accepts bounded outside results too;
// the provider must implement the supplied two-stage binary32 transform with
// at most two ulps error per multiply/add, including flush-to-zero. It must not
// supply arbitrary deformations: conservative group culling assumes this bound.
// Multiplication by zero or +/-1 and addition of zero must remain exact for
// normal-or-zero operands, apart from the sign of a zero result.
using WorldSceneTransformBatch=bool (*)(void*,const WorldMatrix&,const WorldMatrix&,
    std::span<const ps2::TransformVector4>,std::span<ps2::TransformVector4>,
    std::span<std::uint8_t>) noexcept;
const char* world_scene_error_message(WorldSceneError error) noexcept;

// Immutable, owned model snapshot. Assignment validates and copies all source
// vertices/faces/groups and derives exact local group extrema once. Sources may
// then change or die. Failed assignment retains the previous snapshot. Calls
// are serialized with update_prepared(); no borrowed mutable model is trusted.
class PreparedWorldModel {
public:
    PreparedWorldModel() noexcept;~PreparedWorldModel();
    PreparedWorldModel(const PreparedWorldModel&)=delete;
    PreparedWorldModel& operator=(const PreparedWorldModel&)=delete;
    PreparedWorldModel(PreparedWorldModel&&)=delete;
    PreparedWorldModel& operator=(PreparedWorldModel&&)=delete;
    // Same model domain as update(), at most65536 vertices and32768 faces.
    bool assign(WorldModel,WorldSceneError&) noexcept;
    bool valid() const noexcept;
    // Owned storage and vector capacities, excluding allocator bookkeeping.
    std::size_t persistent_bytes() const noexcept;
private:
    struct Storage;std::unique_ptr<Storage> storage_;
    friend class WorldScene;
};
struct PreparedWorldInstance {
    const PreparedWorldModel* model=nullptr;
    WorldMatrix object_to_world{};
};

// Explicit fixed-world presentation adapter. It does not fit/recenter bounds,
// advance game state, recover a game camera, or read reference-view transforms.
// Caller supplies the proven model-local assembly and world/view transforms.
// Color is opaque flat material; original lighting/textures are not inferred.
class WorldScene {
public:
    WorldScene() noexcept;~WorldScene();
    WorldScene(const WorldScene&)=delete;WorldScene& operator=(const WorldScene&)=delete;
    WorldScene(WorldScene&&)=delete;WorldScene& operator=(WorldScene&&)=delete;
    // Allocates two bounded frame banks once.1..32768 source faces and
    //1..16384 output triangles. Reconfiguration is atomic on failure and
    // discards the old frame on success. The default transform/projection cache
    // holds up to4096 entries (224 KiB), reset logically for every instance.
    // Disable it only for the uncached diagnostic comparison. No allocation
    // occurs in update(). Clock configuration survives configure().
    bool configure(std::size_t source_faces,std::size_t output_triangles,WorldSceneError&,
                   bool cache_vertices=true) noexcept;
    // Null disables profiling. Caller retains context through update(), and
    // supplies a monotonic clock; calls and setting changes are serialized.
    void set_profile_clock(WorldSceneClock clock,void* context=nullptr) noexcept;
    // Null restores the selected mode's scalar path. The callback/context must survive
    // update() and may not reenter this scene or retain any provided span.
    // Changes are serialized outside update(); subsequent instances/frames
    // always start fresh cache serials, including after provider changes.
    void set_transform_provider(WorldSceneTransformBatch,void* context=nullptr) noexcept;
    // Serialized between updates, retained across configure(). Reference is
    // the default and keeps binary64 preparation/clipping. Native-float uses
    // binary32 transforms, conservative bounds and clipping without a hidden
    // binary64 fallback or clipped-corner restoration. It is a presentation
    // mode, not a pixel-equivalence claim or a change to game-state arithmetic.
    void set_numeric_mode(WorldNumericMode) noexcept;
    // <=1024 instances, <=65536 total declared vertices. All vertex/transform
    // values must be normal-or-zero finite with abs<=65536. Object transforms
    // are affine; the explicit world_to_clip may be projective. Positive-w and
    // clip/depth conventions are native_geometry's; depth testing is enabled.
    // Failure retains the previous frame. Models/views need only survive this
    // call; the prepared frame owns its materials/vertices. Returned frame views
    // expire at the next successful update/configure or owner destruction.
    bool update(std::span<const WorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    // Identical presentation and transactional limits, using owned snapshots.
    // Owners must be valid and survive this call; instance/view transforms are
    // checked every time. No allocations occur, and the resulting frame does
    // not retain pointers to a model owner. Snapshot storage is separate from
    // WorldSceneStats::persistent_bytes and is reported by each model owner.
    bool update_prepared(std::span<const PreparedWorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    Frame frame() const noexcept;
    WorldSceneStats stats() const noexcept;
private:
    template<class Instance>
    bool update_impl(std::span<const Instance>,const WorldView&,WorldSceneError&) noexcept;
    struct Storage;std::unique_ptr<Storage> storage_;
    WorldSceneClock clock_=nullptr;void* clock_context_=nullptr;
    WorldSceneTransformBatch transform_batch_=nullptr;void* transform_context_=nullptr;
    WorldNumericMode numeric_mode_=WorldNumericMode::reference;
};
} // namespace ps2::native
