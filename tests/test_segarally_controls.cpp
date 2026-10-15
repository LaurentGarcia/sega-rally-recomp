// ROM-free cabinet mapping, preferences, and real SDL virtual-gamepad coverage.
#include "app/segarally_controls.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace c=app::segarally;
namespace {
int checks=0;
void check(bool value,const char *message) {++checks;if(!value) throw std::runtime_error(message);}
bool neutral(const rt::Model2AIO::Inputs &in) {
    return in.in0==255 && in.in1==0x8f && in.in2==0 && in.switches==255 &&
        in.analog==std::array<uint8_t,8>{128,0,0,255,255,255,255,255};
}
void write(const std::filesystem::path &p,const std::string &s) {std::ofstream f(p,std::ios::binary);f<<s;}
std::string read(const std::filesystem::path &p) {std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
SDL_JoystickID attach(const char *name) {
    SDL_VirtualJoystickDesc d;SDL_INIT_INTERFACE(&d);d.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes=SDL_GAMEPAD_AXIS_COUNT;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;d.name=name;
    return SDL_AttachVirtualJoystick(&d);
}
void pump(c::Controls &controls) {SDL_UpdateJoysticks();SDL_Event e;while(SDL_PollEvent(&e)) controls.process_event(e);controls.poll();}
void pure() {
    c::Mapper m;c::Snapshot s;
    check(c::valid(m.settings),"valid defaults");check(neutral(m.sample(s)),"all twelve neutral pins");
    s.keys[SDL_SCANCODE_LEFT]=s.keys[SDL_SCANCODE_Z]=s.keys[SDL_SCANCODE_X]=true;
    auto in=m.sample(s);check(in.analog[0]==0 && in.analog[1]==255 && in.analog[2]==255,"keyboard ADC full ranges");
    s.keys[SDL_SCANCODE_RIGHT]=true;check(m.sample(s).analog[0]==128,"opposed keys centered");
    s={};std::array<bool,c::action_count> touch{};touch[size_t(c::Action::Right)]=touch[size_t(c::Action::Gas)]=true;
    in=m.sample(s,touch);check(in.analog[0]==255 && in.analog[1]==255,"existing digital touch ranges");
    s.keys[SDL_SCANCODE_C]=s.keys[SDL_SCANCODE_RETURN]=s.keys[SDL_SCANCODE_V]=s.keys[SDL_SCANCODE_F2]=s.keys[SDL_SCANCODE_F3]=true;
    s.keys[SDL_SCANCODE_SPACE]=true;in=m.sample(s);check(in.in0==0x92 && in.in2==255,"active low coin/start/view/test/service and handbrake");
    s={};s.keys[SDL_SCANCODE_S]=true;
    check(m.sample(s).in1==0xaf && m.gear()==1,"neutral to first gear");
    for(int i=0;i<20;++i) m.sample(s);check(m.gear()==1,"held shift is one shot");
    for(int i=0;i<6;++i) {s.keys[SDL_SCANCODE_S]=false;m.sample(s);s.keys[SDL_SCANCODE_S]=true;m.sample(s);}
    check(m.gear()==4,"shift upper clamp");
    for(int i=0;i<6;++i) {s={};m.sample(s);s.keys[SDL_SCANCODE_A]=true;m.sample(s);}
    check(m.gear()==0,"downshift lower clamp includes neutral");
    s={};s.connected=true;m.settings.stick_deadzone=0;m.settings.trigger_deadzone=0;
    int previous=-1;
    for(int v=-32768;v<=32767;v+=257) {s.axes[SDL_GAMEPAD_AXIS_LEFTX]=Sint16(v);in=m.sample(s);check(in.analog[0]>=previous,"analog steering monotonic entire range");previous=in.analog[0];}
    s.axes[SDL_GAMEPAD_AXIS_LEFTX]=-32768;check(m.sample(s).analog[0]==0,"negative endpoint exact");
    s.axes[SDL_GAMEPAD_AXIS_LEFTX]=32767;check(m.sample(s).analog[0]==255,"positive endpoint exact");
    s.axes[SDL_GAMEPAD_AXIS_LEFTX]=0;s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=16384;
    check(std::abs(int(m.sample(s).analog[1])-128)<=1,"analog half trigger");
    m.settings.stick_deadzone=.2f;m.settings.trigger_deadzone=.1f;
    s.axes[SDL_GAMEPAD_AXIS_LEFTX]=6000;s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=3000;
    check(m.sample(s).analog[0]==128 && m.sample(s).analog[1]==0,"separate axis deadzones");
    s.axes[SDL_GAMEPAD_AXIS_LEFTX]=19660;m.settings.steering_exponent=2;
    auto meter=m.preview_meters(s);check(meter.steering>.24f && meter.steering<.26f,"rescaled half travel then response curve");
    m.settings.steer_invert=true;check(m.preview_meters(s).steering<-.24f,"steering inversion");
    s={};s.connected=true;s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=32767;s.buttons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=true;
    s.keys[SDL_SCANCODE_C]=true;touch={};touch[size_t(c::Action::Handbrake)]=true;
    m.sample(s,touch,false);check(m.gear()==0 && neutral(m.sample(s,touch,false)),"inactive preserves gear and returns neutral");
    check(m.preview_meters(s).gas==1,"preview remains live while gated");
    check(neutral(m.sample(s,touch,true)),"all held keyboard/pad/axis/touch gated across resume");
    s={};s.connected=true;touch={};m.sample(s,touch);
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=32767;s.buttons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=true;
    s.keys[SDL_SCANCODE_C]=true;touch[size_t(c::Action::Handbrake)]=true;in=m.sample(s,touch);
    check(in.analog[1]==255 && in.in0==254 && in.in2==255 && m.gear()==1,"each released input rearms on fresh press");
    in=m.sample(s,touch,false);check(in.in1==0xaf && in.in0==255 && in.in2==0 && in.analog[1]==0 && m.gear()==1,"focus suppression releases controls but retains physical first gear pins");
    s={};s.connected=true;touch={};m.sample(s,touch,true);
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=32767;m.sample(s);
    m.settings.gamepad_enabled=false;check(m.sample(s).analog[1]==0,"gamepad disable leaves keyboard active");
    m.settings.gamepad_enabled=true;check(m.sample(s).analog[1]==0,"re-enable cannot inherit held trigger");
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=0;m.sample(s);s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=32767;check(m.sample(s).analog[1]==255,"disabled trigger rearms only after release");
    s.connected=false;check(m.sample(s).analog[1]==0,"disconnection releases pedal despite stale snapshot bytes");
    m.settings.bindings[size_t(c::Action::Gas)].key=SDL_SCANCODE_B;s={};s.keys[SDL_SCANCODE_Z]=true;
    check(m.sample(s).analog[1]==0,"old key ceases after remap");s.keys[SDL_SCANCODE_B]=true;check(m.sample(s).analog[1]==255,"new key works");
    m.settings.bindings[0].pad.index=100000;check(!c::valid(m.settings),"invalid axis rejected");
    check(m.sample({}).analog[0]==128,"malformed live settings cannot index out of bounds");
}
void preferences(const std::filesystem::path &dir) {
    const auto path=dir/"controls.settings";auto s=c::Settings::defaults();std::string error;
    check(c::load_settings(path,s,error),"missing preferences retain defaults");
    s.steering_exponent=1.2345678f;s.trigger_deadzone=.1234567f;s.gamepad_enabled=false;
    check(c::store_settings(path,s,error),"store bounded settings");auto next=c::Settings::defaults();
    check(c::load_settings(path,next,error) && c::equal(s,next),"exact float/key/pad preferences roundtrip");
    const auto bytes=read(path);
    for(size_t n:{size_t(0),size_t(18),bytes.size()-20}) {write(path,bytes.substr(0,n));check(!c::load_settings(path,next,error) && c::equal(s,next),"truncation rejected without partial apply");}
    write(path,bytes+"extra\n");check(!c::load_settings(path,next,error),"trailing records rejected");
    write(path,std::string(4097,'x'));check(!c::load_settings(path,next,error),"oversize bounded read rejected");
    for(auto key:{SDL_SCANCODE_F1,SDL_SCANCODE_F6,SDL_SCANCODE_ESCAPE}) {auto bad=s;bad.bindings[0].key=key;check(!c::valid(bad),"host shortcut reserved");}
    auto bad=s;bad.bindings[0].pad={c::PadInput::Button,SDL_GAMEPAD_BUTTON_NORTH,1};check(!c::valid(bad),"Y settings reserved");
    bad=s;bad.stick_deadzone=std::numeric_limits<float>::quiet_NaN();check(!c::valid(bad),"nonfinite tuning rejected");
    bad=s;bad.bindings[0].pad={c::PadInput::Axis,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-1};check(!c::valid(bad),"negative trigger half rejected");
    write(path,bytes);write(path.string()+".tmp","preserve");check(!c::store_settings(path,s,error) && read(path)==bytes && read(path.string()+".tmp")=="preserve","existing temp refused without touching either file");
    std::filesystem::remove(path.string()+".tmp");
}
void virtual_pad(const std::filesystem::path &dir) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    check(SDL_Init(SDL_INIT_GAMEPAD),"SDL gamepad init");
    const auto id=attach("Rally virtual Xbox");check(id!=0 && SDL_IsGamepad(id),"virtual recognized gamepad");
    c::Controls controls;check(controls.init(dir),"controls initialize without window/audio/GPU");
    check(controls.select_gamepad(id),"explicit virtual pad selection isolates physical devices");pump(controls);controls.sample();
    auto *joy=SDL_GetJoystickFromID(id);check(joy!=nullptr,"virtual joystick handle");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);pump(controls);
    check(controls.sample().analog[1]==255,"real SDL RT gas");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,true);pump(controls);controls.sample();controls.sample();
    check(controls.mapper.gear()==1,"SDL bumper shift edge exactly once");
    controls.mapper.settings.gamepad_enabled=false;check(controls.sample().analog[1]==0,"SDL disable releases trigger");
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_NORTH,true);pump(controls);check(!controls.take_menu_request(),"disabled Y cannot open settings");
    controls.mapper.settings.gamepad_enabled=true;SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_NORTH,false);pump(controls);
    SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_NORTH,true);pump(controls);check(controls.take_menu_request() && !controls.take_menu_request(),"Y one-shot settings request");
    pump(controls);check(!controls.take_menu_request(),"held Y does not toggle repeatedly");
    controls.begin_binding(c::Action::Brake,c::Source::Gamepad);pump(controls);
    check(controls.binding(),"held RT/Y/paddle excluded from binding capture");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-26000);pump(controls);
    auto b=controls.mapper.settings.bindings[size_t(c::Action::Brake)].pad;
    check(!controls.binding() && b.kind==c::PadInput::Axis && b.index==SDL_GAMEPAD_AXIS_LEFTY && b.dir==-1,"gamepad half-axis captured from live SDL");
    check(controls.sample().analog[2]==0,"captured axis blocked until neutral");
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,0);pump(controls);controls.sample();
    SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-32768);pump(controls);check(controls.sample().analog[2]==255,"captured axis works after neutral");
    controls.begin_binding(c::Action::View,c::Source::Keyboard);SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_ESCAPE;
    check(controls.process_event(e) && !controls.binding(),"Escape capture cancel consumed before host quit");
    controls.begin_binding(c::Action::View,c::Source::Keyboard);e.key.scancode=SDL_SCANCODE_F1;
    check(!controls.process_event(e) && controls.binding(),"F1 remains panel close and cannot bind");
    e.key.scancode=SDL_SCANCODE_B;check(controls.process_event(e) && !controls.binding() && controls.mapper.settings.bindings[size_t(c::Action::View)].key==SDL_SCANCODE_B,"keyboard remap event consumed");
    controls.begin_binding(c::Action::Coin,c::Source::Gamepad);SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,true);pump(controls);
    check(!controls.binding() && controls.mapper.settings.bindings[size_t(c::Action::Coin)].pad.index==SDL_GAMEPAD_BUTTON_EAST,"gamepad button capture");
    const auto second=attach("Rally replacement Xbox");check(second!=0,"second virtual pad attached");
    check(SDL_DetachVirtualJoystick(id),"unplug active virtual gamepad");pump(controls);
    check(controls.connected() && SDL_GetGamepadID(controls.selected_gamepad())!=id,"hotplug selects another recognized controller");
    check(controls.select_gamepad(second),"select second virtual controller");pump(controls);
    check(controls.sample().analog[1]==0,"unplugged held trigger released");
    check(!controls.select_gamepad(0) && SDL_GetGamepadID(controls.selected_gamepad())==second,"invalid selection preserves current device");
    controls.defaults();controls.clear_binding(c::Action::Gas,c::Source::Keyboard);controls.clear_binding(c::Action::Gas,c::Source::Gamepad);
    check(controls.mapper.settings.bindings[size_t(c::Action::Gas)].key==SDL_SCANCODE_UNKNOWN && controls.mapper.settings.bindings[size_t(c::Action::Gas)].pad.kind==c::PadInput::None,"independent binding clear");
    check(controls.save(),"wrapper preferences store");controls.shutdown();check(SDL_DetachVirtualJoystick(second),"detach replacement");SDL_Quit();
}
}
int main() {
    std::filesystem::path dir;
    try {
        dir=std::filesystem::temp_directory_path()/("segarally-controls-"+std::to_string(SDL_GetTicksNS()));
        check(std::filesystem::create_directory(dir),"exclusive test directory");pure();preferences(dir);
        std::filesystem::remove(dir/"controls.settings");virtual_pad(dir);
        std::filesystem::remove_all(dir);std::printf("test_segarally_controls: %d checks PASS\n",checks);return 0;
    } catch(const std::exception &e) {std::fprintf(stderr,"FAIL after %d checks: %s\n",checks,e.what());if(!dir.empty()) std::filesystem::remove_all(dir);SDL_Quit();return 1;}
}
