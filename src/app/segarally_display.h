// Optional presentation controls. No Machine, game-save or recording access.
#pragma once
#include "app/gpu/crt_settings.h"
#ifdef SEGARALLY_NATIVE_VULKAN
#include "app/native/vulkan/vulkan_presenter.h"
#endif
#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace app {
namespace segarally { class Controls; }
struct DisplaySettings {
    CrtSettings crt{};
    bool linear_filter = true; // Preserve the existing linear presentation.
    bool integer_scale = false;
    bool accurate_textures = false;
};

class DisplayPanel {
public:
    DisplayPanel();
    ~DisplayPanel();
    DisplayPanel(const DisplayPanel &) = delete;
    DisplayPanel &operator=(const DisplayPanel &) = delete;
    // CPU-only registration/load. Invalid/missing preferences retain settings;
    // invalid preferences supply a warning through error(). No ImGui GPU work.
#ifdef SEGARALLY_NATIVE_METAL
    bool init_metal(SDL_Window *, void *metal_device,
                    const std::filesystem::path &save_directory, DisplaySettings &);
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
    bool init_vulkan(SDL_Window *, const native::VulkanPresenter::UIContext &,
                     const std::filesystem::path &save_directory, DisplaySettings &);
#endif
    bool init(SDL_Window *, SDL_GPUDevice *, SDL_GPUTextureFormat,
              const std::filesystem::path &save_directory, DisplaySettings &);
    // Borrowed; detach before destroying Controls. Existing display-only hosts
    // and tests may leave this null.
    void attach_controls(segarally::Controls *);
    // Presentation-only touch layout; desktop hosts keep the compact panel.
    void set_touch_interface(bool);
    void set_open(bool);
    bool open() const;
    bool capture_active() const;
    // Host calls Controls::process_event once first, then always forwards its
    // consumed result here. A consumed binding event never reaches ImGui or
    // host Escape handling. Returns true only for explicit capture/isolation,
    // not ordinary ImGui handling. Host owns pause and F1/Y toggles.
    bool process_event(const SDL_Event &, bool controls_consumed=false);
    // Closed with no overlay does no backend/frame/GPU work. An optional
    // presentation-only overlay lazily shares the Settings context and frame.
    // Callback runs only when Settings is closed, with that context current.
#ifdef SEGARALLY_NATIVE_METAL
    // Borrow the actual UI render-pass descriptor, with its BGRA8 target, before
    // opening its encoder. The host renders this draw data last in that pass.
    bool new_frame_metal(DisplaySettings &, void *render_pass_descriptor,
                         const std::function<void()> &overlay = {});
    bool render_metal(void *command_buffer, void *render_encoder);
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
    // Query the presenter's current context before each UI frame. Record draw
    // data only after its slot fence is waited, inside its active render pass.
    bool new_frame_vulkan(DisplaySettings &, const native::VulkanPresenter::UIContext &,
                          const std::function<void()> &overlay = {});
    bool render_vulkan(VkCommandBuffer);
#endif
    bool new_frame(DisplaySettings &, const std::function<void()> &overlay = {});
    // Prepare before opening the render pass, then render last in that pass.
    // Both are no-ops without current draw data.
    void prepare(SDL_GPUCommandBuffer *);
    void render(SDL_GPUCommandBuffer *, SDL_GPURenderPass *);
    // For external presentation changes such as F6. Writes only on actual change.
    bool save(const DisplaySettings &);
    const std::string &error() const;
    void shutdown(); // Call before destroying the SDL window/device.
private:
    bool new_frame_impl(DisplaySettings &, const std::function<void()> &, void *render_pass_descriptor);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace app
