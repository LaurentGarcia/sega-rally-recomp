#include "app/segarally_display.h"
#include "app/segarally_display_preferences.h"
#include "app/segarally_controls.h"
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
    segarally::Controls *controls=nullptr;
    std::array<bool,SDL_SCANCODE_COUNT> held_keys{};
    bool wait_pad_neutral=true,capture_was_active=false;
    std::filesystem::path preferences;
    DisplaySettings saved{};
    std::string error;
    void isolate_input() {
        wait_pad_neutral=true;
        if (controls) held_keys=controls->snapshot().keys;
        if (context) {
            CurrentContext current(context);
            ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearEventsQueue();
            if (platform) ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,nullptr,0);
        }
    }
    void navigation() {
        const bool capturing=controls && controls->binding();
        if (capturing!=capture_was_active) isolate_input();
        capture_was_active=capturing;
        SDL_Gamepad *pad=nullptr;
        if (controls && controls->connected() && controls->mapper.settings.gamepad_enabled && !capturing) {
            const auto &raw=controls->snapshot();
            bool neutral=true;
            for (bool button:raw.buttons) neutral &= !button;
            for (size_t i=0;i<raw.axes.size();++i)
                neutral &= i>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? raw.axes[i]<=3000 : std::abs(int(raw.axes[i]))<=8000;
            if (neutral) wait_pad_neutral=false;
            if (!wait_pad_neutral) pad=controls->selected_gamepad();
        }
        auto &io=ImGui::GetIO();
        if (pad) io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
        else io.ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;
        // Borrow exactly the cabinet's selected device. The SDL backend must
        // not open or poll an unrelated pad, including during binding capture.
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,pad?&pad:nullptr,pad?1:0);
    }
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
            ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,nullptr,0);
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
void DisplayPanel::attach_controls(segarally::Controls *controls) {
    auto &s=*impl_;
    if (s.controls) s.controls->cancel_binding();
    s.controls=controls;s.capture_was_active=false;s.isolate_input();
}
void DisplayPanel::set_open(bool value) {
    auto &s=*impl_;s.visible=value;s.draw=nullptr;
    if (!value && s.controls) s.controls->cancel_binding();
    s.capture_was_active=false;s.isolate_input();
}
bool DisplayPanel::open() const {return impl_->visible;}
bool DisplayPanel::capture_active() const {return impl_->controls && impl_->controls->binding();}
bool DisplayPanel::process_event(const SDL_Event &event,bool controls_consumed) {
    auto &s=*impl_;
    const bool key=event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_KEY_UP;
    const auto code=key?event.key.scancode:SDL_SCANCODE_UNKNOWN;
    if (key && event.type==SDL_EVENT_KEY_UP && code>=0 && code<SDL_SCANCODE_COUNT) s.held_keys[size_t(code)]=false;
    if (!s.visible) return controls_consumed;
    if (controls_consumed) {
        s.isolate_input();
        if (key && event.type==SDL_EVENT_KEY_DOWN && code>=0 && code<SDL_SCANCODE_COUNT) s.held_keys[size_t(code)]=true;
        return true;
    }
    // Global shortcuts remain the host's responsibility. Escape is consumed by
    // Controls above when it cancels capture, otherwise it closes Settings.
    if (key && (code==SDL_SCANCODE_F1 || code==SDL_SCANCODE_F6 || code==SDL_SCANCODE_ESCAPE)) return false;
    if (capture_active() && (key || event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
        event.type==SDL_EVENT_GAMEPAD_BUTTON_UP || event.type==SDL_EVENT_GAMEPAD_AXIS_MOTION)) return true;
    if (key && code>=0 && code<SDL_SCANCODE_COUNT && s.held_keys[size_t(code)]) return true;
    if (s.context) {CurrentContext current(s.context);ImGui_ImplSDL3_ProcessEvent(&event);}
    // This result describes explicit isolation, not ImGui's broad event types.
    return false;
}
bool DisplayPanel::new_frame(DisplaySettings &settings) {
    auto &s=*impl_;s.draw=nullptr;if (!s.visible) return true;
    if (!s.backend()) {s.visible=false;return false;}
    CurrentContext current(s.context);
    s.navigation();
    ImGui_ImplSDLGPU3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
    const auto *viewport=ImGui::GetMainViewport();
    const ImVec2 available(std::max(1.0f,viewport->WorkSize.x-24.0f),
                           std::max(1.0f,viewport->WorkSize.y-24.0f));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+12.0f,viewport->WorkPos.y+12.0f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(std::min(560.0f,available.x),std::min(660.0f,available.y)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(320.0f,available.x),std::min(260.0f,available.y)),available);
    if (ImGui::Begin("Settings",&s.visible,ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("Paused  |  F1 or Xbox Y to resume");
        // Only tab content scrolls. Resume stays reachable even at 640x480.
        const float footer=ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y*2;
        const float tab_height=std::max(1.0f,ImGui::GetContentRegionAvail().y-footer);
        ImGui::BeginChild("Settings tabs",ImVec2(0,tab_height),ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        if (ImGui::BeginTabBar("Settings pages")) {
        if (ImGui::BeginTabItem("Display")) {
        ImGui::BeginChild("Display settings",ImVec2(0,0));
        ImGui::PushItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x*.52f));
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
        ImGui::PopItemWidth();ImGui::EndChild();ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls")) {
            ImGui::BeginChild("Control settings",ImVec2(0,0));
            if (!s.controls) ImGui::TextWrapped("Controller settings are unavailable.");
            else {
                auto &controls=*s.controls;auto &options=controls.mapper.settings;
                const bool capturing=controls.binding();
                if (controls.connected()) ImGui::TextWrapped("Connected: %s",controls.device_name().c_str());
                else ImGui::TextWrapped("No controller connected. Keyboard controls are available.");
                if (capturing) {
                    ImGui::TextWrapped("%s: %s",segarally::action_name(controls.binding_action()),
                        controls.binding_source()==segarally::Source::Keyboard ? "press a key" : "press a button or move an axis");
                    ImGui::TextWrapped("Escape cancels. F1 and Xbox Y are reserved for Settings.");
                    if (ImGui::Button("Cancel binding",ImVec2(-1,0))) {controls.cancel_binding();s.isolate_input();}
                }
                ImGui::BeginDisabled(capturing);
                ImGui::Checkbox("Enable controller",&options.gamepad_enabled);
                ImGui::PushItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x*.50f));
                ImGui::SliderFloat("Stick deadzone",&options.stick_deadzone,0,.5f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderFloat("Trigger deadzone",&options.trigger_deadzone,0,.5f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderFloat("Steering response",&options.steering_exponent,.5f,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SetItemTooltip("1.0 is linear. Higher values soften steering near the center.");
                ImGui::Checkbox("Invert steering",&options.steer_invert);
                ImGui::PopItemWidth();
                ImGui::Separator();ImGui::TextUnformatted("Live input");
                const auto meters=controls.mapper.preview_meters(controls.snapshot());
                ImGui::ProgressBar((meters.steering+1)*.5f,ImVec2(-1,0),"Steering: left / center / right");
                ImGui::ProgressBar(meters.gas,ImVec2(-1,0),"Accelerator");
                ImGui::ProgressBar(meters.brake,ImVec2(-1,0),"Brake");
                ImGui::Separator();ImGui::TextWrapped("Select a binding to change it. Start remains the cabinet Start button.");
                if (ImGui::Button("Restore control defaults",ImVec2(-1,0))) {controls.defaults();s.isolate_input();}
                for (size_t i=0;i<segarally::action_count;++i) {
                    const auto action=segarally::Action(i);const auto &binding=options.bindings[i];
                    ImGui::PushID(int(i));ImGui::SeparatorText(segarally::action_name(action));
                    if (ImGui::BeginTable("Bindings",2,ImGuiTableFlags_SizingStretchSame)) {
                        ImGui::TableNextColumn();ImGui::PushID("Keyboard");ImGui::TextUnformatted("Keyboard");
                        const char *keyname=binding.key==SDL_SCANCODE_UNKNOWN?"Unbound":SDL_GetScancodeName(binding.key);
                        if (ImGui::Button(keyname && *keyname?keyname:"Unknown key",ImVec2(-1,0))) {
                            controls.begin_binding(action,segarally::Source::Keyboard);s.isolate_input();
                        }
                        if (ImGui::SmallButton("Clear##key")) controls.clear_binding(action,segarally::Source::Keyboard);
                        ImGui::PopID();ImGui::TableNextColumn();ImGui::PushID("Controller");ImGui::TextUnformatted("Controller");
                        const auto padname=segarally::describe(binding.pad);
                        const bool can_bind_pad=options.gamepad_enabled && controls.connected();
                        ImGui::BeginDisabled(!can_bind_pad);
                        if (ImGui::Button(padname.c_str(),ImVec2(-1,0))) {
                            controls.begin_binding(action,segarally::Source::Gamepad);s.isolate_input();
                        }
                        if (!can_bind_pad && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Connect and enable a controller before assigning a binding.");
                        ImGui::EndDisabled();
                        if (ImGui::SmallButton("Clear##pad")) controls.clear_binding(action,segarally::Source::Gamepad);
                        ImGui::PopID();
                        ImGui::EndTable();
                    }
                    ImGui::PopID();
                }
                ImGui::EndDisabled();
                if (!controls.error().empty()) ImGui::TextWrapped("Control preferences: %s",controls.error().c_str());
            }
            ImGui::EndChild();ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        }
        ImGui::EndChild();
        ImGui::Separator();if (ImGui::Button("Resume",ImVec2(-1,0))) s.visible=false;
    }
    ImGui::End();ImGui::Render();
    if (s.visible) s.draw=ImGui::GetDrawData();
    else set_open(false);
    // UI changes persist separately; a failed save does not stop game playback.
    save(settings);if (s.controls) s.controls->save();return true;
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
    if (s.controls) s.controls->cancel_binding();
    s.controls=nullptr;s.held_keys={};s.capture_was_active=false;s.wait_pad_neutral=true;
    if (s.context) {
        auto *previous=ImGui::GetCurrentContext();ImGui::SetCurrentContext(s.context);
        if (s.renderer) ImGui_ImplSDLGPU3_Shutdown();
        if (s.platform) ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(s.context);ImGui::SetCurrentContext(previous==s.context?nullptr:previous);
    }
    s.context=nullptr;s.platform=s.renderer=false;s.window=nullptr;s.device=nullptr;s.format=SDL_GPU_TEXTUREFORMAT_INVALID;
}
} // namespace app
