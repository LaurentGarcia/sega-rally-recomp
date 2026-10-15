#include "app/segarally_display.h"
#include "app/segarally_display_preferences.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include <algorithm>
#include <cmath>

namespace app {
namespace {
struct CurrentContext {
    ImGuiContext *previous=ImGui::GetCurrentContext();
    explicit CurrentContext(ImGuiContext *next) {ImGui::SetCurrentContext(next);}
    ~CurrentContext() {ImGui::SetCurrentContext(previous);}
};
}
struct DisplayPanel::Impl {
    SDL_Window *window=nullptr;
    SDL_GPUDevice *device=nullptr;
    SDL_GPUTextureFormat format=SDL_GPU_TEXTUREFORMAT_INVALID;
    ImGuiContext *context=nullptr;
    ImDrawData *draw=nullptr;
    bool visible=false,platform=false,renderer=false;
    std::filesystem::path preferences;
    DisplaySettings saved{};
    std::string error;
    bool backend() {
        if (context) return true;
        if (!window || !device) {error="Display panel is not initialized.";return false;}
        const auto previous=ImGui::GetCurrentContext();
        IMGUI_CHECKVERSION();context=ImGui::CreateContext();
        ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetIO().LogFilename=nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        // SDL supplies logical window points; its backend handles framebuffer
        // scaling. Do not multiply fonts by the Retina framebuffer ratio.
        ImGui::GetStyle().FontSizeBase=17.0f;
        ImGui::GetStyle().FramePadding=ImVec2(7,5);
        platform=ImGui_ImplSDL3_InitForSDLGPU(window);
        if (platform) {
            ImGui_ImplSDLGPU3_InitInfo info{};info.Device=device;info.ColorTargetFormat=format;
            renderer=ImGui_ImplSDLGPU3_Init(&info);
        }
        if (!platform || !renderer) {
            error="Cannot open display controls: ";error+=SDL_GetError();
            if (renderer) ImGui_ImplSDLGPU3_Shutdown();
            if (platform) ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(context);context=nullptr;platform=renderer=false;
        }
        ImGui::SetCurrentContext(previous);
        return context!=nullptr;
    }
};
DisplayPanel::DisplayPanel():impl_(std::make_unique<Impl>()) {}
DisplayPanel::~DisplayPanel() {shutdown();}
bool DisplayPanel::init(SDL_Window *window,SDL_GPUDevice *device,SDL_GPUTextureFormat format,
        const std::filesystem::path &save_directory,DisplaySettings &settings) {
    shutdown();auto &s=*impl_;s.error.clear();
    if (!window || !device || format==SDL_GPU_TEXTUREFORMAT_INVALID) {s.error="Display panel requires a window and renderer.";return false;}
    s.window=window;s.device=device;s.format=format;
    s.preferences=save_directory.empty()?std::filesystem::path{}:save_directory/"display.settings";
    // Loading is optional; malformed input never changes the caller's settings.
    if (!s.preferences.empty()) display_preferences::load(s.preferences,settings,s.error);
    s.saved=settings;return true;
}
void DisplayPanel::set_open(bool value) {
    auto &s=*impl_;s.visible=value;s.draw=nullptr;
    if (!value && s.context) {
        CurrentContext current(s.context);ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearEventsQueue();
    }
}
bool DisplayPanel::open() const {return impl_->visible;}
bool DisplayPanel::process_event(const SDL_Event &event) {
    auto &s=*impl_;if (!s.visible || !s.context) return false;
    CurrentContext current(s.context);return ImGui_ImplSDL3_ProcessEvent(&event);
}
bool DisplayPanel::new_frame(DisplaySettings &settings) {
    auto &s=*impl_;s.draw=nullptr;if (!s.visible) return true;
    if (!s.backend()) {s.visible=false;return false;}
    CurrentContext current(s.context);
    ImGui_ImplSDLGPU3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
    const auto *viewport=ImGui::GetMainViewport();
    const ImVec2 available(std::max(1.0f,viewport->WorkSize.x-24.0f),
                           std::max(1.0f,viewport->WorkSize.y-24.0f));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+12.0f,viewport->WorkPos.y+12.0f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(std::min(390.0f,available.x),0),ImGuiCond_FirstUseEver);
    // Cap automatic height to the window work area. ImGui's normal vertical
    // scrolling keeps the final Resume button reachable in smaller windows.
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(320.0f,available.x),std::min(120.0f,available.y)),available);
    if (ImGui::Begin("Display",&s.visible,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("F1 to return to the game");
        if (ImGui::Button("Original")) settings.crt.enabled=false;
        ImGui::SameLine();
        if (ImGui::Button("Arcade CRT")) {settings.crt=CrtSettings{};settings.crt.enabled=true;}
        ImGui::Separator();
        ImGui::Checkbox("CRT effects",&settings.crt.enabled);
        ImGui::BeginDisabled(!settings.crt.enabled);
        ImGui::SliderFloat("Scanlines",&settings.crt.scanlines,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Phosphor mask",&settings.crt.mask,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Glow",&settings.crt.glow,0,.5f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Curvature",&settings.crt.curvature,0,.15f,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Color fringing",&settings.crt.chromatic,0,1,"%.2f px",ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::TextUnformatted("Picture scaling");
        if (ImGui::RadioButton("Smooth",settings.linear_filter)) settings.linear_filter=true;
        ImGui::SameLine();if (ImGui::RadioButton("Sharp",!settings.linear_filter)) settings.linear_filter=false;
        ImGui::SetItemTooltip("Choose smooth or sharp scaling when CRT effects are off.");
        ImGui::Checkbox("Integer scale",&settings.integer_scale);
        ImGui::SetItemTooltip("Use whole-number scaling to keep pixels evenly sized.");
        ImGui::Checkbox("Accurate textures",&settings.accurate_textures);
        ImGui::SetItemTooltip("Improves texture detail and edges. May reduce performance.");
        if (!s.error.empty()) ImGui::TextWrapped("Display preferences could not be saved or loaded.");
        ImGui::Separator();if (ImGui::Button("Resume",ImVec2(-1,0))) s.visible=false;
    }
    ImGui::End();ImGui::Render();
    if (s.visible) s.draw=ImGui::GetDrawData();
    else set_open(false);
    // UI changes persist separately; a failed save does not stop game playback.
    save(settings);return true;
}
void DisplayPanel::prepare(SDL_GPUCommandBuffer *command) {
    auto &s=*impl_;if (!command || !s.visible || !s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return;
    CurrentContext current(s.context);ImGui_ImplSDLGPU3_PrepareDrawData(s.draw,command);
}
void DisplayPanel::render(SDL_GPUCommandBuffer *command,SDL_GPURenderPass *pass) {
    auto &s=*impl_;if (!command || !pass || !s.visible || !s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return;
    CurrentContext current(s.context);ImGui_ImplSDLGPU3_RenderDrawData(s.draw,command,pass);
}
bool DisplayPanel::save(const DisplaySettings &settings) {
    auto &s=*impl_;if (display_preferences::equal(settings,s.saved)) return true;
    // Remember the attempted value too: do not retry/write each frame after an
    // I/O failure. A subsequent actual setting change explicitly retries.
    s.saved=settings;
    return display_preferences::store(s.preferences,settings,s.error);
}
const std::string &DisplayPanel::error() const {return impl_->error;}
void DisplayPanel::shutdown() {
    auto &s=*impl_;s.draw=nullptr;s.visible=false;
    if (s.context) {
        auto *previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(s.context);
        if (s.renderer) ImGui_ImplSDLGPU3_Shutdown();
        if (s.platform) ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(s.context);ImGui::SetCurrentContext(previous==s.context?nullptr:previous);
    }
    s.context=nullptr;s.platform=s.renderer=false;s.window=nullptr;s.device=nullptr;s.format=SDL_GPU_TEXTUREFORMAT_INVALID;
}
} // namespace app
