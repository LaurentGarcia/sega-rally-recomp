// Direct Metal presentation over an SDL-owned window. No SDL GPU objects.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

struct SDL_Window;
namespace rt { class Video; }
namespace app { struct DisplaySettings; class DisplayPanel; }
namespace app::native {
class MetalPresenter {
public:
    struct Stats { uint64_t game_frames = 0, presented_frames = 0, skipped_drawables = 0; };
    MetalPresenter();
    ~MetalPresenter();
    MetalPresenter(const MetalPresenter &) = delete;
    MetalPresenter &operator=(const MetalPresenter &) = delete;
    // Main-thread API; window must outlive shutdown. SDL still owns events,
    // input and audio. Only SDL_Metal_* connects its view to CAMetalLayer.
    bool init(SDL_Window *);
    void shutdown();
    void *metal_device() const; // Borrowed id<MTLDevice>, valid until shutdown.
    const std::string &error() const;
    Stats stats() const;
    // Acquires the actual drawable/UI pass before building Settings. Updated
    // controls affect this frame. A hidden/minimized/unavailable drawable still
    // permits offscreen game updates. Settings render last, outside CRT effects.
    bool render(const rt::Video &, DisplaySettings &, bool update_game, bool has_game,
                DisplayPanel &, const std::function<void()> &overlay = {});
    bool wait_idle(); // Drain before the panel/device is destroyed; errors propagate.
    // Same 496x384 P6 RGB capture as the SDL path, before CRT and Settings.
    bool capture(const std::filesystem::path &);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace app::native
