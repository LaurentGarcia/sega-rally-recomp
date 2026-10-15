#include "app/segarally_controls.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>

namespace app::segarally {
namespace {
size_t at(Action a) {return size_t(a);}
bool action_valid(Action a) {return at(a)<action_count;}
bool reserved(SDL_Scancode key) {return key==SDL_SCANCODE_F1 || key==SDL_SCANCODE_F6 || key==SDL_SCANCODE_ESCAPE;}
bool pad_valid(const PadInput &p) {
    if(p.dir!=1 && p.dir!=-1) return false;
    switch(p.kind) {
    case PadInput::None:return p.index==0 && p.dir==1;
    case PadInput::Button:return p.index>=0 && p.index<SDL_GAMEPAD_BUTTON_COUNT && p.index!=SDL_GAMEPAD_BUTTON_NORTH && p.dir==1;
    case PadInput::Axis:return p.index>=0 && p.index<SDL_GAMEPAD_AXIS_COUNT &&
        (p.index<SDL_GAMEPAD_AXIS_LEFT_TRIGGER || p.dir==1);
    }
    return false;
}
float raw_axis(const Snapshot &s,int axis) {
    const int v=s.axes[size_t(axis)];
    return v<0?float(v)/32768.f:float(v)/32767.f;
}
float deadzone(const Settings &s,int axis) {return axis>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER?s.trigger_deadzone:s.stick_deadzone;}
float travel(const Snapshot &s,const PadInput &p,const Settings &settings) {
    if(!s.connected || !settings.gamepad_enabled) return 0;
    if(p.kind==PadInput::Button) return s.buttons[size_t(p.index)]?1.f:0.f;
    if(p.kind!=PadInput::Axis) return 0;
    const float v=std::max(0.f,raw_axis(s,p.index)*float(p.dir));
    const float dz=deadzone(settings,p.index);
    return v<=dz?0:(v-dz)/(1-dz);
}
bool no_extra(std::istringstream &line) {line>>std::ws;return line.eof();}
std::array<float,action_count> action_values(const Snapshot &input,const Settings &s,const std::array<bool,action_count> &touch) {
    std::array<float,action_count> values{};
    for(size_t i=0;i<action_count;++i) {
        const auto &b=s.bindings[i];
        values[i]=std::max(((b.key!=SDL_SCANCODE_UNKNOWN && input.keys[size_t(b.key)]) || touch[i])?1.f:0.f,travel(input,b.pad,s));
    }
    return values;
}
Meters adjusted(const std::array<float,action_count> &v,const Settings &s) {
    const float steering=v[at(Action::Right)]-v[at(Action::Left)];
    return {std::copysign(std::pow(std::abs(steering),s.steering_exponent),steering)*(s.steer_invert?-1.f:1.f),v[at(Action::Gas)],v[at(Action::Brake)]};
}
}
Settings Settings::defaults() {
    Settings s;
    const auto bind=[&](Action a,SDL_Scancode key,PadInput pad) {s.bindings[at(a)]={key,pad};};
    bind(Action::Left,SDL_SCANCODE_LEFT,{PadInput::Axis,SDL_GAMEPAD_AXIS_LEFTX,-1});
    bind(Action::Right,SDL_SCANCODE_RIGHT,{PadInput::Axis,SDL_GAMEPAD_AXIS_LEFTX,1});
    bind(Action::Gas,SDL_SCANCODE_Z,{PadInput::Axis,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,1});
    bind(Action::Brake,SDL_SCANCODE_X,{PadInput::Axis,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,1});
    bind(Action::GearUp,SDL_SCANCODE_S,{PadInput::Button,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,1});
    bind(Action::GearDown,SDL_SCANCODE_A,{PadInput::Button,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,1});
    bind(Action::View,SDL_SCANCODE_V,{PadInput::Button,SDL_GAMEPAD_BUTTON_SOUTH,1});
    bind(Action::Handbrake,SDL_SCANCODE_SPACE,{PadInput::Button,SDL_GAMEPAD_BUTTON_WEST,1});
    bind(Action::Coin,SDL_SCANCODE_C,{PadInput::Button,SDL_GAMEPAD_BUTTON_BACK,1});
    bind(Action::Start,SDL_SCANCODE_RETURN,{PadInput::Button,SDL_GAMEPAD_BUTTON_START,1});
    bind(Action::Test,SDL_SCANCODE_F2,{});bind(Action::Service,SDL_SCANCODE_F3,{});
    return s;
}
const char *action_name(Action a) {
    static constexpr const char *names[]={"Steer left","Steer right","Gas","Brake","Shift up","Shift down","View","Handbrake","Coin","Start","Test","Service"};
    return action_valid(a)?names[at(a)]:"Unknown";
}
std::string describe(const PadInput &p) {
    if(!pad_valid(p) || p.kind==PadInput::None) return "Unbound";
    const char *name=p.kind==PadInput::Button?SDL_GetGamepadStringForButton(SDL_GamepadButton(p.index)):
        SDL_GetGamepadStringForAxis(SDL_GamepadAxis(p.index));
    return std::string(name?name:"Unknown")+(p.kind==PadInput::Axis?(p.dir<0?" -":" +"):"");
}
bool valid(const Settings &s) {
    if(!std::isfinite(s.stick_deadzone) || s.stick_deadzone<0 || s.stick_deadzone>.9f ||
       !std::isfinite(s.trigger_deadzone) || s.trigger_deadzone<0 || s.trigger_deadzone>.9f ||
       !std::isfinite(s.steering_exponent) || s.steering_exponent<.25f || s.steering_exponent>4.f) return false;
    for(const auto &b:s.bindings)
        if(b.key<SDL_SCANCODE_UNKNOWN || b.key>=SDL_SCANCODE_COUNT || reserved(b.key) || !pad_valid(b.pad)) return false;
    return true;
}
bool equal(const Settings &a,const Settings &b) {
    if(a.gamepad_enabled!=b.gamepad_enabled || a.steer_invert!=b.steer_invert || a.stick_deadzone!=b.stick_deadzone ||
       a.trigger_deadzone!=b.trigger_deadzone || a.steering_exponent!=b.steering_exponent) return false;
    for(size_t i=0;i<action_count;++i) {
        const auto &x=a.bindings[i],&y=b.bindings[i];
        if(x.key!=y.key || x.pad.kind!=y.pad.kind || x.pad.index!=y.pad.index || x.pad.dir!=y.pad.dir) return false;
    }
    return true;
}
bool load_settings(const std::filesystem::path &path,Settings &settings,std::string &error) {
    error.clear();std::error_code ec;
    if(!std::filesystem::exists(path,ec) && !ec) return true;
    if(ec || !std::filesystem::is_regular_file(path,ec)) {error="Cannot read controls preferences.";return false;}
    std::ifstream file(path,std::ios::binary);std::string bytes;
    char c;
    while(file.get(c)) {if(bytes.size()==4096) {error="Controls preferences exceed 4096 bytes.";return false;}bytes+=c;}
    if(!file.eof()) {error="Cannot read controls preferences.";return false;}
    std::istringstream in(bytes);in.imbue(std::locale::classic());std::string line;Settings next;int enabled,invert;
    auto fail=[&] {error="Invalid controls preferences; previous settings retained.";return false;};
    if(!std::getline(in,line) || line!="rally-controls 1") return fail();
    if(!std::getline(in,line)) return fail();
    {std::istringstream row(line);row.imbue(std::locale::classic());std::string tag;
        if(!(row>>tag>>enabled>>invert>>next.stick_deadzone>>next.trigger_deadzone>>next.steering_exponent) ||
           tag!="options" || (enabled!=0 && enabled!=1) || (invert!=0 && invert!=1) || !no_extra(row)) return fail();}
    next.gamepad_enabled=enabled;next.steer_invert=invert;
    for(size_t i=0;i<action_count;++i) {
        if(!std::getline(in,line)) return fail();
        std::istringstream row(line);row.imbue(std::locale::classic());std::string tag;size_t index;int key,kind;
        auto &b=next.bindings[i];
        if(!(row>>tag>>index>>key>>kind>>b.pad.index>>b.pad.dir) || tag!="bind" || index!=i || kind<0 || kind>2 ||
           key<0 || key>=SDL_SCANCODE_COUNT || !no_extra(row)) return fail();
        b.key=SDL_Scancode(key);b.pad.kind=PadInput::Kind(kind);
    }
    if(in.peek()!=std::char_traits<char>::eof() || !valid(next)) return fail();
    settings=next;return true;
}
bool store_settings(const std::filesystem::path &path,const Settings &s,std::string &error) {
    error.clear();if(!valid(s)) {error="Invalid controls settings.";return false;}
    std::ostringstream out;out.imbue(std::locale::classic());out<<"rally-controls 1\noptions "<<int(s.gamepad_enabled)<<' '<<int(s.steer_invert)<<' '
        <<std::setprecision(9)<<s.stick_deadzone<<' '<<s.trigger_deadzone<<' '<<s.steering_exponent<<'\n';
    for(size_t i=0;i<action_count;++i) {const auto &b=s.bindings[i];out<<"bind "<<i<<' '<<int(b.key)<<' '<<int(b.pad.kind)<<' '<<b.pad.index<<' '<<b.pad.dir<<'\n';}
    const auto bytes=out.str();const auto temp=path.string()+".tmp";
    FILE *file=std::fopen(temp.c_str(),"wbx");
    if(!file) {error="Cannot create controls preferences temporary file.";return false;}
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();
    if(std::fclose(file)!=0) ok=false;
    if(ok) {std::error_code ec;std::filesystem::rename(temp,path,ec);ok=!ec;}
    if(!ok) {std::remove(temp.c_str());error="Cannot write controls preferences.";}
    return ok;
}
void Mapper::suppress(const Snapshot &s) {
    for(size_t i=0;i<s.keys.size();++i) blocked_.keys[i]=blocked_.keys[i]||s.keys[i];
    if(s.connected) {
        for(size_t i=0;i<s.buttons.size();++i) blocked_.buttons[i]=blocked_.buttons[i]||s.buttons[i];
        for(size_t i=0;i<s.axes.size();++i) if(std::abs(raw_axis(s,int(i)))>deadzone(valid(settings)?settings:Settings::defaults(),int(i))) blocked_.axes[i]=1;
    }
    up_=down_=false;
}
Meters Mapper::preview_meters(const Snapshot &s,const std::array<bool,action_count> &touch) const {
    const auto safe=valid(settings)?settings:Settings::defaults();
    return adjusted(action_values(s,safe,touch),safe);
}
rt::Model2AIO::Inputs Mapper::sample(const Snapshot &physical,const std::array<bool,action_count> &touch,bool active) {
    const Settings safe=valid(settings)?settings:Settings::defaults();
    if(!active || active!=active_) {suppress(physical);for(size_t i=0;i<action_count;++i) blocked_touch_[i]=blocked_touch_[i]||touch[i];}
    active_=active;Snapshot input=physical;
    for(size_t i=0;i<input.keys.size();++i) {if(!physical.keys[i]) blocked_.keys[i]=false;if(blocked_.keys[i]) input.keys[i]=false;}
    for(size_t i=0;i<input.buttons.size();++i) {
        if(!physical.connected || !physical.buttons[i]) blocked_.buttons[i]=false;
        if(!safe.gamepad_enabled && physical.connected && physical.buttons[i]) blocked_.buttons[i]=true;
        if(blocked_.buttons[i]) input.buttons[i]=false;
    }
    for(size_t i=0;i<input.axes.size();++i) {
        if(!physical.connected || std::abs(raw_axis(physical,int(i)))<=deadzone(safe,int(i))) blocked_.axes[i]=0;
        if(!safe.gamepad_enabled && physical.connected && std::abs(raw_axis(physical,int(i)))>deadzone(safe,int(i))) blocked_.axes[i]=1;
        if(blocked_.axes[i]) input.axes[i]=0;
    }
    std::array<bool,action_count> allowed_touch{};
    for(size_t i=0;i<action_count;++i) {
        if(!touch[i]) blocked_touch_[i]=false;
        allowed_touch[i]=touch[i]&&!blocked_touch_[i];
    }
    const auto values=action_values(input,safe,allowed_touch);
    rt::Model2AIO::Inputs result;
    constexpr uint8_t gears[]={0,0x20,0x10,0x60,0x50};
    result.in1=uint8_t(0x8f|gears[gear_]);
    if(!active) {meters_={};return result;}
    const auto value=[&](Action a) {return values[at(a)];};
    // Digital keyboard/touch stays full lock under the analog response curve.
    meters_=adjusted(values,safe);const float steering=meters_.steering;
    result.analog[0]=uint8_t(std::lround(128.f+(steering<0?128.f:127.f)*steering));
    result.analog[1]=uint8_t(std::lround(255.f*meters_.gas));result.analog[2]=uint8_t(std::lround(255.f*meters_.brake));
    const auto low=[&](unsigned bit,Action a) {if(value(a)>.5f) result.in0&=uint8_t(~(1u<<bit));};
    low(0,Action::Coin);low(6,Action::Start);low(5,Action::View);low(2,Action::Test);low(3,Action::Service);
    const bool up=value(Action::GearUp)>.5f,down=value(Action::GearDown)>.5f;
    if(up&&!up_) gear_=std::min(4,gear_+1);if(down&&!down_) gear_=std::max(0,gear_-1);up_=up;down_=down;
    result.in1=uint8_t(0x8f|gears[gear_]);
    result.in2=value(Action::Handbrake)>.5f?0xff:0;
    return result;
}
Controls::~Controls() {shutdown();}
bool Controls::init(const std::filesystem::path &directory) {
    if(initialized_) return true;
    path_=directory/"controls.settings";
    load_settings(path_,mapper.settings,error_);saved_=mapper.settings;
    if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {error_=SDL_GetError();poll();return false;}
    initialized_=true;
    rescan();poll();return true;
}
void Controls::shutdown() {
    if(pad_) SDL_CloseGamepad(pad_);pad_=nullptr;
    if(initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    initialized_=false;raw_={};capturing_=menu_request_=y_held_=false;
}
void Controls::rescan() {
    if(!initialized_) return;
    if(pad_ && !SDL_GamepadConnected(pad_)) {cancel_binding();SDL_CloseGamepad(pad_);pad_=nullptr;}
    if(pad_) return;
    int count=0;auto *ids=SDL_GetGamepads(&count);
    for(int i=0;i<count && !pad_;++i) pad_=SDL_OpenGamepad(ids[i]);
    SDL_free(ids);
    if(pad_) {new_pad_=true;y_held_=true;}
}
bool Controls::connected() const {return pad_ && SDL_GamepadConnected(pad_);}
bool Controls::select_gamepad(SDL_JoystickID id) {
    if(!initialized_ || !SDL_IsGamepad(id)) return false;
    SDL_Gamepad *next=SDL_OpenGamepad(id);if(!next) {error_=SDL_GetError();return false;}
    if(pad_) SDL_CloseGamepad(pad_);pad_=next;new_pad_=true;y_held_=true;cancel_binding();return true;
}
std::string Controls::device_name() const {const char *name=connected()?SDL_GetGamepadName(pad_):nullptr;return name?name:"No controller connected";}
const Snapshot &Controls::poll() {
    rescan();Snapshot next;int count=0;const bool *keys=SDL_GetKeyboardState(&count);
    for(int i=0;i<std::min(count,int(next.keys.size()));++i) next.keys[size_t(i)]=keys[i];
    next.connected=connected();
    if(next.connected) {
        for(size_t i=0;i<next.buttons.size();++i) next.buttons[i]=SDL_GetGamepadButton(pad_,SDL_GamepadButton(i));
        for(size_t i=0;i<next.axes.size();++i) next.axes[i]=SDL_GetGamepadAxis(pad_,SDL_GamepadAxis(i));
    }
    raw_=next;
    if(new_pad_) {Snapshot pad_only=raw_;pad_only.keys={};mapper.suppress(pad_only);new_pad_=false;}
    const bool y=raw_.connected && raw_.buttons[SDL_GAMEPAD_BUTTON_NORTH];
    if(y&&!y_held_ && mapper.settings.gamepad_enabled && !capturing_) menu_request_=true;
    y_held_=y;capture_snapshot();return raw_;
}
bool Controls::process_event(const SDL_Event &e) {
    if(e.type==SDL_EVENT_GAMEPAD_ADDED || e.type==SDL_EVENT_GAMEPAD_REMOVED || e.type==SDL_EVENT_GAMEPAD_REMAPPED) rescan();
    if(e.type==SDL_EVENT_KEY_DOWN && capturing_) {
        if(e.key.scancode==SDL_SCANCODE_ESCAPE) {cancel_binding();return true;}
        if(reserved(e.key.scancode)) return false;
        if(capture_source_==Source::Keyboard && !e.key.repeat && e.key.scancode>SDL_SCANCODE_UNKNOWN && e.key.scancode<SDL_SCANCODE_COUNT) {
            mapper.settings.bindings[at(capture_action_)].key=e.key.scancode;cancel_binding();return true;
        }
        return true;
    }
    if((e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP) && e.gbutton.button==SDL_GAMEPAD_BUTTON_NORTH)
        return true;
    return capturing_ && (e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION || e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP);
}
rt::Model2AIO::Inputs Controls::sample(const std::array<bool,action_count> &touch,bool active) {return mapper.sample(raw_,touch,active&&!capturing_);}
void Controls::suppress_held() {mapper.suppress(raw_);}
bool Controls::take_menu_request() {const bool result=menu_request_;menu_request_=false;return result;}
void Controls::begin_binding(Action a,Source source) {
    if(!action_valid(a)) return;
    capture_action_=a;capture_source_=source;capture_blocked_=raw_;capturing_=true;suppress_held();
}
void Controls::cancel_binding() {capturing_=false;suppress_held();}
void Controls::capture_snapshot() {
    if(!capturing_ || capture_source_!=Source::Gamepad || !raw_.connected || !mapper.settings.gamepad_enabled) return;
    for(size_t i=0;i<raw_.buttons.size();++i) {
        if(!raw_.buttons[i]) capture_blocked_.buttons[i]=false;
        if(raw_.buttons[i] && !capture_blocked_.buttons[i] && i!=SDL_GAMEPAD_BUTTON_NORTH) {
            mapper.settings.bindings[at(capture_action_)].pad={PadInput::Button,int(i),1};cancel_binding();return;
        }
    }
    for(size_t i=0;i<raw_.axes.size();++i) {
        const float value=raw_axis(raw_,int(i));
        if(std::abs(value)<.2f) capture_blocked_.axes[i]=0;
        if(capture_blocked_.axes[i]!=0 || std::abs(value)<.6f || (i>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER && value<0)) continue;
        mapper.settings.bindings[at(capture_action_)].pad={PadInput::Axis,int(i),value<0?-1:1};cancel_binding();return;
    }
}
void Controls::clear_binding(Action a,Source source) {
    if(!action_valid(a)) return;
    auto &b=mapper.settings.bindings[at(a)];if(source==Source::Keyboard) b.key=SDL_SCANCODE_UNKNOWN;else b.pad={};suppress_held();
}
void Controls::defaults() {mapper.settings=Settings::defaults();cancel_binding();}
bool Controls::save() {
    if(equal(saved_,mapper.settings)) return error_.empty();
    saved_=mapper.settings;return store_settings(path_,mapper.settings,error_);
}
} // namespace app::segarally
