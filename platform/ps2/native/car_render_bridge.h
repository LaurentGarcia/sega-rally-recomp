#pragma once
#include "car_pose.h"
#include "rounded_affine_transform.h"
#include <array>

namespace ps2::native {
// The four attachment offsets are immutable model data. Their order is the
// original descriptor order; no model name or axle interpretation is implied.
struct CarRenderGeometry { std::array<AffinePoint3,4> wheel_offsets{}; };
struct CarRenderBridge {
    AffineTransform3 shadow{},body{};
    std::array<AffineTransform3,4> wheels{};
};
// Apply the recovered full-car draw transforms to the supplied view matrix.
// This is the geometric portion only: model choice, textures, visibility and
// the source camera update belong to their explicit callers. Identity view
// produces object-to-world transforms; a camera view preserves original
// translation/rotation order without recomposing rounded world matrices.
// Pose position/view translation: |v|<=2^20; orientation/steering: |v|<=8; spin: |v|<=2^20;
// attachment and suspension values: |v|<=64; view coefficients: |v|<=16.
// The four external pose heights admit |v|<=2^20, but their derived shadow
// rotation must remain within the recovered polynomial domain. Every read
// scalar must be finite. Signed zero/subnormals retain exact arithmetic.
// Rejection leaves output unchanged; inputs may alias output storage.
bool build_car_render_bridge(const RenderPose& pose,const CarRenderGeometry& geometry,
                             const AffineTransform3& world_to_view,
                             CarRenderBridge& output) noexcept;
}
