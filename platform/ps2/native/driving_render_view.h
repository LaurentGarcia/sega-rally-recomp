#pragma once
#include "car_pose.h"
#include "rounded_affine_transform.h"

namespace ps2::native {
struct DrivingRenderViewState { float yaw_history=0; };
struct DrivingRenderViewConfig { float distance_adjustment=0; };
struct DrivingRenderView {
    AffineTransform3 world_to_view{};
    AffinePoint3 position{},orientation{};
    float focus_x=300,focus_y=300;
};
// Recovered chase-view branch. State is initialized once, then retained across
// calls. This is not the alternative close/intro view or a view-mode selector.
// Position is finite |v|<=2^20, pose angles/history |v|<=round32(pi), and distance
// adjustment |v|<=64. Exact one-fold and intermediate value domains must hold.
// Output/state publish together only on success. The two output objects must
// be distinct; pose/config values are consumed before publication.
bool update_driving_render_view(const RenderPose&,const DrivingRenderViewConfig&,
                                DrivingRenderViewState&,DrivingRenderView&) noexcept;
}
