#pragma once
#include "rounded_affine_transform.h"
#include "contact_search.h"
#include "world_scene.h"
#include <memory>

namespace ps2::native {
struct RallyBodyPose { AffinePoint3 position{}, orientation{}; };
enum class RallyBodyError {
    none, invalid_file, invalid_header, invalid_geometry, invalid_bridge,
    invalid_pose, capacity, allocation_failed, presentation_failed, not_loaded
};
const char* rally_body_error_message(RallyBodyError) noexcept;

// Body-only presentation of externally advanced native game state. Loading
// copies model-local data and an explicit fixed model-to-car transform; it
// never imports captured view matrices or fits/recenters the object each frame.
// A diagnostic bridge/camera must not be labelled recovered arcade rendering.
class RallyBodyScene {
public:
    RallyBodyScene() noexcept;
    ~RallyBodyScene();
    RallyBodyScene(const RallyBodyScene&) = delete;
    RallyBodyScene& operator=(const RallyBodyScene&) = delete;
    // RLYBODY v1: 64-byte little-endian header, then XYZ binary32 vertices and
    // {three u32 local indices,u32 opaque ARGB} faces. Header: magic RLYBODY\0,
    // version1, header64, vertex_count, face_count, payload_bytes, IEEE CRC32,
    // vertex_stride12, face_stride16; bytes40..63 zero. No pose/camera is stored.
    // <=65536 vertices, <=16384 faces; normal-or-zero floats, abs<=65536.
    // Explicit affine bridge is copied, not inferred. Failure retains previous
    // model/frame; success replaces them. Frame views expire on successful load.
    bool decode(std::span<const std::uint8_t>, const WorldMatrix& model_to_car,
                RallyBodyError&) noexcept;
    bool load(const char* path, const WorldMatrix& model_to_car, RallyBodyError&) noexcept;
    // Optional diagnostic collision surface, copied once as two triangles per
    // quad in unchanged course/world coordinates. No arcade material claim.
    // Empty input removes it. Total model vertices<=65536, source faces<=32768;
    // output capacity<=16384. Success clears the prior frame, failure retains it.
    bool attach_course(std::span<const ContactSegment>, RallyBodyError&) noexcept;
    // No allocation or simulation. Uses the recovered middle/first/third
    // orientation basis, then the fixed bridge and caller-owned camera.
    // Failure retains the previous frame. Successful update expires old views.
    bool update(const RallyBodyPose&, const WorldView&, RallyBodyError&) noexcept;
    Frame frame() const noexcept;
    WorldSceneStats stats() const noexcept;
    std::size_t persistent_bytes() const noexcept;
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
} // namespace ps2::native
