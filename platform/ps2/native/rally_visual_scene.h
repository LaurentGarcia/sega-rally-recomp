#pragma once
#include "car_render_bridge.h"
#include "driving_render_view.h"
#include "world_scene.h"
#include <memory>

namespace ps2::native {
inline constexpr std::size_t rally_visual_asset_limit=7;
// Roles are course, shadow, body, then four attachments in descriptor order.
// Several roles may explicitly share one immutable model asset.
struct RallyVisualAssetIdentity { std::uint32_t bytes=0,crc32=0; };
struct RallyVisualConfig {
    std::uint32_t initial_game_frame=0;
    RenderPose initial_pose{};
    DrivingRenderViewState initial_view_state{};
    DrivingRenderViewConfig view{};
    CarRenderGeometry geometry{};
    // Native presentation clip planes, not a recovered arcade clipping claim.
    float near_z=.125f,far_z=2048;
    std::uint32_t clear_color=0xff101923u,asset_count=0;
    std::array<std::uint32_t,7> role_assets{};
    std::array<RallyVisualAssetIdentity,7> assets{};
};
enum class RallyVisualError {
    none,not_loaded,io,config,frame_mismatch,asset_identity,asset,capacity,
    allocation,initial_pose,pose,presentation
};
struct RallyVisualTimings { std::uint64_t pose_ticks=0,scene_ticks=0; };
const char* rally_visual_error_message(RallyVisualError) noexcept;
// Fixed 320-byte RLYVIEW1 little-endian config. See rally_visual_scene.md for
// the complete field map. Decode failure leaves output unchanged.
bool decode_rally_visual_config(std::span<const std::uint8_t>,std::uint32_t expected_game_frame,
                               RallyVisualConfig&,RallyVisualError&) noexcept;
class RallyVisualScene {
public:
    RallyVisualScene() noexcept;~RallyVisualScene();
    RallyVisualScene(const RallyVisualScene&)=delete;
    RallyVisualScene&operator=(const RallyVisualScene&)=delete;
    // Fixed filenames view.bin and model0.bin..model6.bin. Bounded sizes/CRCs
    // bind the model set. Failure preserves prior assets, history and frame.
    bool load(const char* directory,std::uint32_t expected_game_frame,RallyVisualError&) noexcept;
    // Typed owner-data entry used by authored tests and offline conversion.
    // Each model is deep-copied once; role aliases share the owned snapshot.
    bool configure(const RallyVisualConfig&,std::span<const WorldModel>,RallyVisualError&) noexcept;
    // First pose after load/reset must exactly match initial_pose, which is
    // the first native update output at initial_game_frame. Subsequent calls
    // advance recovered chase history. No allocation; failure is atomic.
    bool update(const RenderPose&,RallyVisualError&) noexcept;
    // Reset history/guard and clear the published frame without reallocating.
    void reset() noexcept;
    void set_profile_clock(WorldSceneClock,void* context=nullptr) noexcept;
    void set_transform_provider(WorldSceneTransformBatch,void* context=nullptr) noexcept;
    void set_transform_project_provider(WorldSceneTransformProjectBatch,void* context=nullptr) noexcept;
    void set_numeric_mode(WorldNumericMode) noexcept;
    Frame frame() const noexcept;
    WorldSceneStats stats() const noexcept;
    RallyVisualTimings timings() const noexcept;
    std::size_t persistent_bytes() const noexcept;
private:
    struct Storage;std::unique_ptr<Storage> storage_;
    WorldSceneClock clock_=nullptr;void* clock_context_=nullptr;
    WorldSceneTransformBatch provider_=nullptr;void* provider_context_=nullptr;
    WorldSceneTransformProjectBatch project_provider_=nullptr;
    WorldNumericMode mode_=WorldNumericMode::native_float;
    RallyVisualTimings timings_{};
};
}
