#include "app/segarally_display.h"
#include "app/segarally_display_preferences.h"
#include "app/segarally_controls.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#ifdef SEGARALLY_NATIVE_METAL
#include "app/segarally_display_metal.h"
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
#include "app/segarally_display_vulkan.h"
#endif
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
#ifdef SEGARALLY_NATIVE_METAL
    void *metal_device=nullptr;
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
    native::VulkanPresenter::UIContext vulkan_context{};
    display_vulkan::Backend vulkan;
#endif
    SDL_GPUDevice *device=nullptr;
    SDL_GPUTextureFormat format=SDL_GPU_TEXTUREFORMAT_INVALID;
    ImGuiContext *context=nullptr;
    ImDrawData *draw=nullptr;
    bool visible=false,platform=false,renderer=false,touch_interface=false;
    segarally::Controls *controls=nullptr;
    std::array<bool,SDL_SCANCODE_COUNT> held_keys{};
    bool wait_pad_neutral=true,capture_was_active=false;
    std::filesystem::path preferences;
    DisplaySettings saved{};
    std::string error;
    bool using_metal() const {
#ifdef SEGARALLY_NATIVE_METAL
        return metal_device!=nullptr;
#else
        return false;
#endif
    }
    bool using_vulkan() const {
#ifdef SEGARALLY_NATIVE_VULKAN
        return vulkan_context.device!=VK_NULL_HANDLE;
#else
        return false;
#endif
    }
    bool using_native() const {return using_metal() || using_vulkan();}
    void stop_renderer() {
#ifdef SEGARALLY_NATIVE_VULKAN
        if (using_vulkan()) {vulkan.shutdown();return;}
#endif
#ifdef SEGARALLY_NATIVE_METAL
        if (using_metal()) {display_metal::shutdown();return;}
#endif
        ImGui_ImplSDLGPU3_Shutdown();
    }
    void isolate_input() {
        wait_pad_neutral=true;
        if (controls) held_keys=controls->snapshot().keys;
        if (context) {
            CurrentContext current(context);
            ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearInputMouse();ImGui::GetIO().ClearEventsQueue();
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
    void style() {
        auto &ui=ImGui::GetStyle();
        ui=ImGuiStyle{};
        ImGui::StyleColorsDark();
        ui.FontSizeBase=17.0f;
        ui.FramePadding=ImVec2(7,5);
        if (!touch_interface) return;
        // All dimensions are logical points. A normal framed control is 45pt
        // high; framebuffer density must never shrink the touch target.
        ui.FramePadding=ImVec2(12,14);
        ui.WindowPadding=ImVec2(16,12);
        ui.ItemSpacing=ImVec2(12,10);
        ui.WindowRounding=18;ui.ChildRounding=12;ui.FrameRounding=9;
        ui.ScrollbarSize=28;ui.GrabMinSize=28;
        ui.Colors[ImGuiCol_WindowBg]=ImVec4(.055f,.065f,.075f,.98f);
        ui.Colors[ImGuiCol_Text]=ImVec4(.96f,.94f,.88f,1);
        ui.Colors[ImGuiCol_Button]=ImVec4(.20f,.23f,.25f,1);
        ui.Colors[ImGuiCol_ButtonHovered]=ImVec4(.36f,.25f,.15f,1);
        ui.Colors[ImGuiCol_ButtonActive]=ImVec4(.70f,.33f,.10f,1);
        ui.Colors[ImGuiCol_FrameBg]=ImVec4(.14f,.17f,.19f,1);
        ui.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.26f,.22f,.18f,1);
        ui.Colors[ImGuiCol_FrameBgActive]=ImVec4(.34f,.25f,.17f,1);
        ui.Colors[ImGuiCol_Header]=ImVec4(.18f,.21f,.23f,1);
        ui.Colors[ImGuiCol_HeaderHovered]=ImVec4(.36f,.25f,.15f,1);
        ui.Colors[ImGuiCol_HeaderActive]=ImVec4(.46f,.29f,.15f,1);
        ui.Colors[ImGuiCol_CheckMark]=ImVec4(1,.57f,.24f,1);
        ui.Colors[ImGuiCol_CheckboxSelectedBg]=ImVec4(.34f,.25f,.17f,1);
        ui.Colors[ImGuiCol_SliderGrab]=ImVec4(1,.57f,.24f,1);
        ui.Colors[ImGuiCol_SliderGrabActive]=ImVec4(1,.72f,.44f,1);
        ui.Colors[ImGuiCol_Tab]=ImVec4(.14f,.17f,.19f,1);
        ui.Colors[ImGuiCol_TabSelected]=ImVec4(.35f,.23f,.13f,1);
        ui.Colors[ImGuiCol_TabHovered]=ImVec4(.50f,.30f,.14f,1);
        ui.Colors[ImGuiCol_TabSelectedOverline]=ImVec4(1,.57f,.24f,1);
    }
    bool backend() {
        if (context) return true;
        if (!window || (!device && !using_native())) {error="Display panel is not initialized.";return false;}
        const auto previous=ImGui::GetCurrentContext();
        IMGUI_CHECKVERSION();context=ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetIO().LogFilename=nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // SDL supplies logical window points; its backend handles framebuffer
        // scaling. Do not multiply fonts by the Retina framebuffer ratio.
        style();
        platform=using_vulkan()?ImGui_ImplSDL3_InitForVulkan(window):
            using_metal()?ImGui_ImplSDL3_InitForMetal(window):ImGui_ImplSDL3_InitForSDLGPU(window);
        if (platform) {
            ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,nullptr,0);
#ifdef SEGARALLY_NATIVE_VULKAN
            if (using_vulkan()) renderer=vulkan.init(vulkan_context,error);
            else
#endif
#ifdef SEGARALLY_NATIVE_METAL
            if (using_metal()) renderer=display_metal::init(metal_device);
            else
#endif
            {
                ImGui_ImplSDLGPU3_InitInfo info{};info.Device=device;info.ColorTargetFormat=format;
                renderer=ImGui_ImplSDLGPU3_Init(&info);
            }
        }
        if (!platform || !renderer) {
            if (!using_vulkan() || error.empty()) {
                error="Cannot open display controls: ";error+=SDL_GetError();
            }
            if (renderer) stop_renderer();
            if (platform) ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(context);context=nullptr;platform=renderer=false;
        }
        ImGui::SetCurrentContext(previous);
        return context!=nullptr;
    }
};
DisplayPanel::DisplayPanel():impl_(std::make_unique<Impl>()) {}
DisplayPanel::~DisplayPanel() {shutdown();}
void DisplayPanel::set_touch_interface(bool enabled) {
    auto &s=*impl_;
    if (s.touch_interface==enabled) return;
    s.touch_interface=enabled;
    if (s.context) {CurrentContext current(s.context);s.style();}
}
#ifdef SEGARALLY_NATIVE_METAL
bool DisplayPanel::init_metal(SDL_Window *window,void *device,
        const std::filesystem::path &save_directory,DisplaySettings &settings) {
    shutdown();auto &s=*impl_;s.error.clear();
    if (!window || !device) {s.error="Display panel requires a window and Metal device.";return false;}
    s.window=window;s.metal_device=device;
    s.preferences=save_directory.empty()?std::filesystem::path{}:save_directory/"display.settings";
    if (!s.preferences.empty()) display_preferences::load(s.preferences,settings,s.error);
    s.saved=settings;return true;
}
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
bool DisplayPanel::init_vulkan(SDL_Window *window,const native::VulkanPresenter::UIContext &context,
        const std::filesystem::path &save_directory,DisplaySettings &settings) {
    shutdown();auto &s=*impl_;s.error.clear();
    if (!window) {s.error="Display panel requires a window.";return false;}
    if (!display_vulkan::valid(context,s.error)) return false;
    s.window=window;s.vulkan_context=context;
    s.preferences=save_directory.empty()?std::filesystem::path{}:save_directory/"display.settings";
    if (!s.preferences.empty()) display_preferences::load(s.preferences,settings,s.error);
    s.saved=settings;return true;
}
#endif
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
#ifdef SEGARALLY_NATIVE_METAL
bool DisplayPanel::new_frame_metal(DisplaySettings &settings,void *pass,const std::function<void()> &overlay) {
    if (!impl_->using_metal()) {impl_->error="Display panel is not using Metal.";return false;}
    return new_frame_impl(settings,overlay,pass);
}
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
bool DisplayPanel::new_frame_vulkan(DisplaySettings &settings,const native::VulkanPresenter::UIContext &context,
        const std::function<void()> &overlay) {
    auto &s=*impl_;s.draw=nullptr;
    if (!s.using_vulkan()) {s.error="Display panel is not using Vulkan.";return false;}
    if (!display_vulkan::valid(context,s.error)) return false;
    if (!display_vulkan::same_device(s.vulkan_context,context)) {
        s.error="Display panel Vulkan device changed; reinitialize the panel.";return false;
    }
    s.vulkan_context=context;
    return new_frame_impl(settings,overlay,nullptr);
}
bool DisplayPanel::render_vulkan(VkCommandBuffer command) {
    auto &s=*impl_;
    if (!s.using_vulkan()) {s.error="Display panel is not using Vulkan.";return false;}
    if (!s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return true;
    CurrentContext current(s.context);
    return s.vulkan.render(s.draw,command,s.error);
}
#endif
bool DisplayPanel::new_frame(DisplaySettings &settings,const std::function<void()> &overlay) {
    if (impl_->using_native()) {impl_->error="Display panel requires its native frame context.";return false;}
    return new_frame_impl(settings,overlay,nullptr);
}
bool DisplayPanel::new_frame_impl(DisplaySettings &settings,const std::function<void()> &overlay,void *pass) {
    auto &s=*impl_;s.draw=nullptr;if (!s.visible && !overlay) return true;
    if (!s.backend()) {s.visible=false;return false;}
    CurrentContext current(s.context);
    if(s.visible) s.navigation();
    else ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,nullptr,0);
    (void)pass;
#ifdef SEGARALLY_NATIVE_VULKAN
    if (s.using_vulkan()) {
        if (!s.vulkan.new_frame(s.vulkan_context,s.error)) return false;
    } else
#endif
#ifdef SEGARALLY_NATIVE_METAL
    if (s.using_metal()) {
        if (!display_metal::new_frame(s.metal_device,pass)) {
            s.error="Display panel requires the current Metal render pass.";return false;
        }
    } else
#endif
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
    if(!s.visible) {overlay();ImGui::Render();s.draw=ImGui::GetDrawData();return true;}
    const auto *viewport=ImGui::GetMainViewport();
    const ImVec2 available(std::max(1.0f,viewport->WorkSize.x-24.0f),
                           std::max(1.0f,viewport->WorkSize.y-24.0f));
    ImGuiWindowFlags flags=ImGuiWindowFlags_NoCollapse;
    if (s.touch_interface) {
        SDL_Rect safe{};int width=0,height=0;SDL_GetWindowSize(s.window,&width,&height);
        if (!SDL_GetWindowSafeArea(s.window,&safe) || safe.x<0 || safe.y<0 || safe.w<=0 || safe.h<=0 ||
            double(safe.x)+safe.w>width || double(safe.y)+safe.h>height) safe={0,0,width,height};
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x+safe.x+8,viewport->Pos.y+safe.y+8));
        ImGui::SetNextWindowSize(ImVec2(std::max(1,safe.w-16),std::max(1,safe.h-16)));
        flags|=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize;
    } else {
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+12.0f,viewport->WorkPos.y+12.0f),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(std::min(560.0f,available.x),std::min(660.0f,available.y)),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(320.0f,available.x),std::min(260.0f,available.y)),available);
    }
    if (ImGui::Begin("Settings",&s.visible,flags)) {
        if (s.touch_interface) {
            ImGui::TextColored(ImVec4(1,.57f,.24f,1),"PIT STOP");
            ImGui::SameLine();ImGui::TextDisabled("Your drive is paused");
        } else ImGui::TextUnformatted("Paused  |  F1 or Xbox Y to resume");
        // Only tab content scrolls. Resume stays reachable even at 640x480.
        const float footer=ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y*2;
        const float tab_height=std::max(1.0f,ImGui::GetContentRegionAvail().y-footer);
        ImGui::BeginChild("Settings tabs",ImVec2(0,tab_height),ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        if (ImGui::BeginTabBar("Settings pages")) {
        if (ImGui::BeginTabItem("Display")) {
        ImGui::BeginChild("Display settings",ImVec2(0,0));
        ImGui::PushItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x*.52f));
        const float preset_width=s.touch_interface?std::max(1.f,(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f):0.f;
        if (ImGui::Button("Original",ImVec2(preset_width,0))) settings.crt.enabled=false;
        ImGui::SameLine();
        if (ImGui::Button("Arcade CRT",ImVec2(preset_width,0))) {settings.crt=CrtSettings{};settings.crt.enabled=true;}
        const bool tune=!s.touch_interface || ImGui::CollapsingHeader("Tune the screen");
        if (tune) {
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
        }
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
                else ImGui::TextWrapped("%s",s.touch_interface?"Touch controls are ready. Connect a controller to customize its buttons.":"No controller connected. Keyboard controls are available.");
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
                        if (s.touch_interface?ImGui::Button("Clear##key",ImVec2(-1,0)):ImGui::SmallButton("Clear##key"))
                            controls.clear_binding(action,segarally::Source::Keyboard);
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
                        if (s.touch_interface?ImGui::Button("Clear##pad",ImVec2(-1,0)):ImGui::SmallButton("Clear##pad"))
                            controls.clear_binding(action,segarally::Source::Gamepad);
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
        ImGui::Separator();if (ImGui::Button(s.touch_interface?"Back to the stage":"Resume",ImVec2(-1,0))) s.visible=false;
    }
    ImGui::End();ImGui::Render();
    if (s.visible) s.draw=ImGui::GetDrawData();
    else set_open(false);
    // UI changes persist separately; a failed save does not stop game playback.
    save(settings);if (s.controls) s.controls->save();return true;
}
#ifdef SEGARALLY_NATIVE_METAL
bool DisplayPanel::render_metal(void *command,void *encoder) {
    auto &s=*impl_;
    if (!s.using_metal()) {s.error="Display panel is not using Metal.";return false;}
    if (!s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return true;
    CurrentContext current(s.context);
    if (!display_metal::render(s.draw,s.metal_device,command,encoder)) {
        s.error="Display panel requires a live Metal command and encoder.";return false;
    }
    return true;
}
#endif
void DisplayPanel::prepare(SDL_GPUCommandBuffer *command) {
    auto &s=*impl_;if (s.using_native() || !command || !s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return;
    CurrentContext current(s.context);ImGui_ImplSDLGPU3_PrepareDrawData(s.draw,command);
}
void DisplayPanel::render(SDL_GPUCommandBuffer *command,SDL_GPURenderPass *pass) {
    auto &s=*impl_;if (s.using_native() || !command || !pass || !s.draw || s.draw->DisplaySize.x<=0 || s.draw->DisplaySize.y<=0) return;
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
        if (s.renderer) s.stop_renderer();
        if (s.platform) ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(s.context);ImGui::SetCurrentContext(previous==s.context?nullptr:previous);
    }
    s.context=nullptr;s.platform=s.renderer=false;s.window=nullptr;
#ifdef SEGARALLY_NATIVE_METAL
    s.metal_device=nullptr;
#endif
#ifdef SEGARALLY_NATIVE_VULKAN
    s.vulkan_context={};
#endif
    s.device=nullptr;s.format=SDL_GPU_TEXTUREFORMAT_INVALID;
}
} // namespace app
