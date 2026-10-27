#pragma once
#include "native_geometry.h"
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
enum class WorldSceneError { none,not_configured,invalid_capacity,invalid_model,invalid_transform,
    input_limit,output_limit,clipping_failed,allocation_failed };
struct WorldSceneStats { std::uint32_t instances=0,vertices=0,source_faces=0,output_triangles=0;
    std::size_t persistent_bytes=0;
    std::uint32_t groups=0,culled_groups=0,transformed_vertices=0; };
const char* world_scene_error_message(WorldSceneError error) noexcept;

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
    // discards the old frame on success. No allocation occurs in update().
    bool configure(std::size_t source_faces,std::size_t output_triangles,WorldSceneError&) noexcept;
    // <=1024 instances, <=65536 total declared vertices. All vertex/transform
    // values must be normal-or-zero finite with abs<=65536. Object transforms
    // are affine; the explicit world_to_clip may be projective. Positive-w and
    // clip/depth conventions are native_geometry's; depth testing is enabled.
    // Failure retains the previous frame. Models/views need only survive this
    // call; the prepared frame owns its materials/vertices. Returned frame views
    // expire at the next successful update/configure or owner destruction.
    bool update(std::span<const WorldInstance>,const WorldView&,WorldSceneError&) noexcept;
    Frame frame() const noexcept;
    WorldSceneStats stats() const noexcept;
private:
    struct Storage;std::unique_ptr<Storage> storage_;
};
} // namespace ps2::native
