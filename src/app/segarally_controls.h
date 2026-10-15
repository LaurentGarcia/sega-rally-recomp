// Rally cabinet inputs. SDL gamepad bindings use the inherited half-axis model;
// Rally ADC/gear wiring and keyboard defaults remain distinct from Daytona.
#pragma once
#include "runtime/model2a_io.h"
#include <SDL3/SDL.h>
#include <array>
#include <filesystem>
#include <string>

namespace app::segarally {
enum class Action { Left, Right, Gas, Brake, GearUp, GearDown, View, Handbrake, Coin, Start, Test, Service, Count };
constexpr size_t action_count=size_t(Action::Count);
enum class Source { Keyboard, Gamepad };
struct PadInput {
    enum Kind { None, Button, Axis } kind=None;
    int index=0, dir=1; // One signed half-axis; triggers use positive travel.
};
struct Binding { SDL_Scancode key=SDL_SCANCODE_UNKNOWN; PadInput pad{}; };
struct Settings {
    bool gamepad_enabled=true, steer_invert=false;
    float stick_deadzone=.12f, trigger_deadzone=.05f, steering_exponent=1.f;
    std::array<Binding,action_count> bindings{};
    static Settings defaults();
};
const char *action_name(Action);
std::string describe(const PadInput &);
bool valid(const Settings &);
bool equal(const Settings &,const Settings &);
// Strict bounded versioned preferences; malformed files never partially apply.
bool load_settings(const std::filesystem::path &,Settings &,std::string &error);
bool store_settings(const std::filesystem::path &,const Settings &,std::string &error);
struct Snapshot {
    std::array<bool,SDL_SCANCODE_COUNT> keys{};
    std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{};
    std::array<Sint16,SDL_GAMEPAD_AXIS_COUNT> axes{};
    bool connected=false;
};
struct Meters { float steering=0,gas=0,brake=0; };
// Pure snapshot mapper: no SDL polling, files, time, or guest access.
class Mapper {
public:
    Settings settings=Settings::defaults();
    int gear() const {return gear_;} // Neutral=0, then 1..4.
    const Meters &meters() const {return meters_;}
    Meters preview_meters(const Snapshot &,const std::array<bool,action_count> &touch={}) const;
    // Block each currently held physical input until released/neutral. Call on
    // menu/focus/script transitions; keep sampling inactive snapshots meanwhile.
    void suppress(const Snapshot &);
    rt::Model2AIO::Inputs sample(const Snapshot &,const std::array<bool,action_count> &touch={},bool active=true);
private:
    Snapshot blocked_{};
    std::array<bool,action_count> blocked_touch_{};
    bool up_=false,down_=false,active_=true;
    int gear_=0;
    Meters meters_{};
};
// SDL wrapper. Initialize only AFTER offline replay's early return. No rumble,
// GPU, audio, or window API. Opens the first recognized gamepad; rescans on
// removal so a second pad can take over. Devices never change cabinet defaults.
class Controls {
public:
    Controls()=default;
    ~Controls();
    Controls(const Controls &)=delete;
    Controls &operator=(const Controls &)=delete;
    Mapper mapper;
    bool init(const std::filesystem::path &save_directory);
    void shutdown();
    // Consumes reserved Y Settings, and binding keys (Escape cancels, F1 is
    // reserved for closing the panel). Host still owns F1/focus/menu toggles.
    bool process_event(const SDL_Event &);
    const Snapshot &poll();
    const Snapshot &snapshot() const {return raw_;}
    rt::Model2AIO::Inputs sample(const std::array<bool,action_count> &touch={},bool active=true);
    void suppress_held();
    bool connected() const;
    SDL_Gamepad *selected_gamepad() const {return pad_;} // Borrowed until next poll/event/shutdown.
    bool select_gamepad(SDL_JoystickID); // Explicit recognized device; failure retains current pad.
    std::string device_name() const;
    bool take_menu_request(); // Y rising edge, never cabinet Start/Back.
    void begin_binding(Action,Source);
    void cancel_binding();
    bool binding() const {return capturing_;}
    Action binding_action() const {return capture_action_;}
    Source binding_source() const {return capture_source_;}
    void clear_binding(Action,Source);
    void defaults();
    bool save(); // Writes only changed preferences, error accessible below.
    const std::string &error() const {return error_;}
private:
    void rescan();
    void capture_snapshot();
    SDL_Gamepad *pad_=nullptr;
    bool initialized_=false,capturing_=false,menu_request_=false,y_held_=false;
    bool new_pad_=false;
    Action capture_action_=Action::Left;
    Source capture_source_=Source::Keyboard;
    Snapshot raw_{},capture_blocked_{};
    std::filesystem::path path_;
    Settings saved_=Settings::defaults();
    std::string error_;
};
} // namespace app::segarally
