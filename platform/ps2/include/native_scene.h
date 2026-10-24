#pragma once
#include "native_display.h"
#include "native_mesh.h"
#include <vector>

namespace ps2::native {
inline constexpr std::size_t study_triangle_limit = 16384;
// Explicit diagnostic camera in fitted reference-view coordinates. These are
// not recovered game camera parameters. Angles are radians; distance is in
// units of the fitted bounding sphere. No physics or game state is advanced.
struct StudyCamera {
    float yaw = 0, pitch = 0, distance = 2.75f;
    bool diagnostic_lighting = true;
};
enum class StudyError { none, invalid_mesh, invalid_camera, degenerate_bounds,
                        transform_failed, output_limit, clipping_failed, allocation_failed };
const char *study_error_message(StudyError error) noexcept;
struct StudyStats {
    std::uint32_t source_vertices = 0, source_faces = 0, source_objects = 0;
    std::uint32_t output_triangles = 0, clipped_faces = 0;
    std::size_t persistent_bytes = 0;
};
class StudyScene {
public:
    StudyScene() = default;
    StudyScene(const StudyScene &) = delete;
    StudyScene &operator=(const StudyScene &) = delete;
    StudyScene(StudyScene &&) = delete;
    StudyScene &operator=(StudyScene &&) = delete;
    // Applies each captured object->reference-view transform, fits its bounds,
    // then performs diagnostic orbit/projection and native reference clipping.
    // Caller mesh need not outlive the result. On failure the last frame stays
    // intact. At most16384 output triangles; excess rejects without truncation.
    // Rebuild is reference work with software-double cost on EE, not a VU path.
    bool prepare(const Mesh &mesh, const StudyCamera &camera, StudyError &error) noexcept;
    Frame frame() const noexcept { return {triangles_, 0xff101923u, true}; }
    const StudyStats &stats() const noexcept { return stats_; }
private:
    std::vector<Material> materials_;
    std::vector<Triangle> triangles_;
    StudyStats stats_{};
};
} // namespace ps2::native
