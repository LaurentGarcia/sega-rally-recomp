#pragma once
#include "native_geometry.h"
#include "vu1_transform.h"
#include <memory>

namespace ps2::native {
using WorldMatrix=std::array<float,16>; // column-major, column vectors
struct WorldPoint { float x=0,y=0,z=0; };
struct WorldUV { float u=0,v=0; }; // Texel coordinates, per face corner.
// Optional authored facing data, independent of triangle winding. Both fan
// triangles of a source quad retain the same original point/vector. The
// vector uses the instance's linear transform (no inverse transpose).
struct WorldFacing { WorldPoint normal{},point{};bool enabled=false; };
inline constexpr std::uint32_t world_flat_material=0xffffffffu;
inline constexpr std::size_t world_material_limit=4096;
inline constexpr std::size_t world_prepared_texture_limit=1024;
inline constexpr std::size_t world_prepared_texture_bytes_limit=4*1024*1024;
struct WorldFace {
    std::array<std::uint32_t,3> indices{};std::uint32_t argb=0xffffffffu;
    std::array<WorldUV,3> uv{};
    // The sentinel retains the opaque argb path. Otherwise this selects the
    // complete material, including its palette, cutout and texture identity.
    std::uint32_t material_index=world_flat_material;
    WorldFacing facing{};
};
struct WorldFaceGroup { std::uint32_t first_face=0,face_count=0; };
struct WorldModel { std::span<const WorldPoint> vertices;std::span<const WorldFace> faces;
    // Optional nonempty, contiguous partition of all faces. Bounds are derived
    // from validated vertices each call; no caller-supplied bound is trusted.
    std::span<const WorldFaceGroup> groups{};
    std::span<const Material> materials{}; };
struct WorldInstance { WorldModel model;WorldMatrix object_to_world{}; };
struct WorldView { WorldMatrix world_to_clip{};std::uint32_t clear_color=0xff101923u;
    // Explicit pre-projection-space origin. Facing uses original-ordered
    // binary32 arithmetic in both geometry modes, retaining dot==0. All read
    // inputs and arithmetic intermediates must be finite normal-or-zero.
    bool use_facing=false;WorldPoint facing_origin{}; };
enum class WorldNumericMode { reference, native_float };
enum class WorldSceneError { none,not_configured,invalid_capacity,invalid_model,invalid_transform,
    input_limit,output_limit,clipping_failed,allocation_failed,generation_exhausted };
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
    WorldNumericMode numeric_mode=WorldNumericMode::reference;
    std::uint32_t backface_rejected_faces=0;
    // Projection values consumed from the optional provider on cache misses;
    // cache hits and prefetched but unused vertices are excluded.
    std::uint32_t provider_projected_vertices=0; };
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
// Native-float continuation of the same transform contract. Clip acceptance
// remains independent of projection: q=0 requests ordinary EE projection.
// Supplied x/y/depth/q must describe the same clip vector within 1/32 logical
// pixel, 2^-20 depth and 2^-20 relative q. Only wholly inside, bounded outputs
// are used. UVs remain per-corner. Invalid/missing projected outputs fall back
// to EE projection without discarding a valid transform. Reference mode never
// calls this provider. All spans are separate and live only during the call.
using WorldSceneTransformProjectBatch=bool (*)(void*,const WorldMatrix&,const WorldMatrix&,
    std::span<const ps2::TransformVector4>,std::span<ps2::TransformVector4>,
    std::span<std::uint8_t>,std::span<ps2::TransformProjectedVertex>) noexcept;
const char* world_scene_error_message(WorldSceneError error) noexcept;

// Immutable, owned model snapshot. Assignment validates and copies all source
// vertices/faces/groups, referenced materials and texture bytes, and derives
// exact local group extrema once. Sources may then change or die. Failed
// assignment retains the previous snapshot. Assignment and prepared updates
// are serialized process-wide: copied textures receive monotonically unique
// lifetime generations, with exhaustion rejected before publication.
class PreparedWorldModel {
public:
    PreparedWorldModel() noexcept;~PreparedWorldModel();
    PreparedWorldModel(const PreparedWorldModel&)=delete;
    PreparedWorldModel& operator=(const PreparedWorldModel&)=delete;
    PreparedWorldModel(PreparedWorldModel&&)=delete;
    PreparedWorldModel& operator=(PreparedWorldModel&&)=delete;
    // Same model domain as update(), at most65536 vertices and32768 faces.
    // Copies only referenced materials and unique Texture descriptor pointers:
    // at most1024 textures/4MiB of indices. Supplied generations are replaced by
    // private lifetime IDs; prepared pixels are immutable. A published frame
    // retains its snapshot even after this owner is replaced or destroyed.
    bool assign(WorldModel,WorldSceneError&) noexcept;
    bool valid() const noexcept;
    // Owned storage and vector capacities, excluding allocator bookkeeping.
    std::size_t persistent_bytes() const noexcept;
private:
    struct Storage;std::shared_ptr<const Storage> storage_;
    friend class WorldScene;
};
struct PreparedWorldInstance {
    const PreparedWorldModel* model=nullptr;
    WorldMatrix object_to_world{};
};

// Ordered, untransformed geometry borrowed from immutable prepared snapshots.
// Only WorldScene can publish a plan. Its instance views cannot be replaced
// through this API; source UVs and material indices remain per face/corner.
struct WorldDrawInstance {
    std::span<const WorldPoint> vertices;
    std::span<const WorldFace> faces;
    std::span<const Material> materials;
    WorldMatrix object_to_world{};
    std::span<const std::uint32_t> visible_face_indices;
};
class WorldDrawPlan {
public:
    WorldDrawPlan() noexcept=default;
    std::span<const WorldDrawInstance> instances() const noexcept{return instances_;}
    const WorldMatrix& world_to_clip() const noexcept{return world_to_clip_;}
    std::uint32_t clear_color() const noexcept{return clear_color_;}
    bool depth_test() const noexcept{return depth_test_;}
    std::size_t output_triangle_capacity() const noexcept{return output_triangle_capacity_;}
private:
    friend class WorldScene;
    WorldDrawPlan(std::span<const WorldDrawInstance> instances,const WorldMatrix& view,
                  std::uint32_t clear,std::size_t capacity) noexcept
        :instances_(instances),world_to_clip_(view),clear_color_(clear),output_triangle_capacity_(capacity){}
    std::span<const WorldDrawInstance> instances_{};
    WorldMatrix world_to_clip_{};
    std::uint32_t clear_color_=0xff101923u;
    bool depth_test_=true;
    std::size_t output_triangle_capacity_=0;
};

// Explicit fixed-world presentation adapter. It does not fit/recenter bounds,
// advance game state, recover a game camera, or read reference-view transforms.
// Caller supplies the proven model-local assembly and world/view transforms.
// Materials and UVs are explicit assets; lighting/texture identity is not inferred.
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
    // Replaces the ordinary provider; either setter clears the other one.
    // A null callback restores scalar work. No projection-provider work runs
    // in reference mode; changing back to native-float enables it again.
    void set_transform_project_provider(WorldSceneTransformProjectBatch,void* context=nullptr) noexcept;
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
    // UVs are normal-or-zero with abs<=65536. At most4096 materials, with
    // PSMT4 Texture dimensions8..256 and opaque/transparent palette entries.
    // Failure retains the previous frame. Geometry/views/material tables need
    // only survive this call; the frame copies materials/vertices. Ordinary
    // Texture descriptors/pixels remain borrowed through the frame's lifetime;
    // Display's generation/invalidation contract still applies to those assets.
    // Returned frame views
    // expire at the next successful update/configure or owner destruction.
    bool update(std::span<const WorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    // Identical presentation and transactional limits, using owned snapshots.
    // Owners must be valid and survive this call; instance/view transforms are
    // checked every time. No allocations occur, and the resulting frame does
    // not retain pointers to a model owner. Its preallocated bank leases retain
    // textured snapshots; flat snapshots require no lease. Snapshot storage is separate from
    // WorldSceneStats::persistent_bytes and is reported by each model owner.
    bool update_prepared(std::span<const PreparedWorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    // Separate optional storage: no Triangle/material bank or transformed
    // vertices are built by the planner.1..32768 source faces,1..16384 maximum
    // emitted triangles. Allocation failure preserves the prior plan. Ordinary
    // configure/update/Frame storage is independent and remains unchanged.
    bool configure_plan(std::size_t source_faces,std::size_t output_triangles,WorldSceneError&) noexcept;
    // Validate instance/view inputs, then select source faces in original order
    // using the native-float group bounds and authored facing rules. No vertex
    // transform/project/clip, provider callback or per-call allocation occurs.
    // Planning does not prove transformed output is in domain or fits the final
    // triangle cap: the renderer must reject numerical/expansion failures.
    // Success retains every referenced snapshot, including flat geometry. The
    // borrowed plan survives owner reassignment/destruction and normal Frame
    // updates, until next successful plan/configure_plan or scene destruction.
    // Failure retains the prior plan and its statistics.
    bool plan_prepared(std::span<const PreparedWorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    WorldDrawPlan draw_plan() const noexcept;
    // Source/group/facing counts only: output_triangles stays zero until a
    // consumer actually emits geometry. Storage includes both plan banks.
    WorldSceneStats plan_stats() const noexcept;
    Frame frame() const noexcept;
    WorldSceneStats stats() const noexcept;
private:
    template<class Instance>
    bool update_impl(std::span<const Instance>,const WorldView&,WorldSceneError&) noexcept;
    struct Storage;std::unique_ptr<Storage> storage_;
    struct PlanStorage;std::unique_ptr<PlanStorage> plan_storage_;
    WorldSceneClock clock_=nullptr;void* clock_context_=nullptr;
    WorldSceneTransformBatch transform_batch_=nullptr;void* transform_context_=nullptr;
    WorldSceneTransformProjectBatch transform_project_batch_=nullptr;
    WorldNumericMode numeric_mode_=WorldNumericMode::reference;
};
} // namespace ps2::native
