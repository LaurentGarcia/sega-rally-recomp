// Optional presentation controls. No Machine, game-save or recording access.
#pragma once
#include "app/gpu/crt_settings.h"
#include <SDL3/SDL.h>
#include <filesystem>
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
    bool init(SDL_Window *, SDL_GPUDevice *, SDL_GPUTextureFormat,
              const std::filesystem::path &save_directory, DisplaySettings &);
    // Borrowed; detach before destroying Controls. Existing display-only hosts
    // and tests may leave this null.
    void attach_controls(segarally::Controls *);
    void set_open(bool);
    bool open() const;
    bool capture_active() const;
    // Host calls Controls::process_event once first, then always forwards its
    // consumed result here. A consumed binding event never reaches ImGui or
    // host Escape handling. Returns true only for explicit capture/isolation,
    // not ordinary ImGui handling. Host owns pause and F1/Y toggles.
    bool process_event(const SDL_Event &, bool controls_consumed=false);
    // Closed returns true without backend/frame/GPU work. Open initializes lazily.
    bool new_frame(DisplaySettings &);
    // Prepare before opening the render pass, then render last in that pass.
    // Both are no-ops without current draw data.
    void prepare(SDL_GPUCommandBuffer *);
    void render(SDL_GPUCommandBuffer *, SDL_GPURenderPass *);
    // For external presentation changes such as F6. Writes only on actual change.
    bool save(const DisplaySettings &);
    const std::string &error() const;
    void shutdown(); // Call before destroying the SDL window/device.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace app
