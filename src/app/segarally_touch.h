// Host-only cabinet controls. Coordinates are logical window points, not pixels.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace rally {
enum Button { Steering, Gas, Brake, Coin, Start, View, GearDown, GearUp, Handbrake, Settings, Count };
struct TouchRect {
    float x=0,y=0,w=0,h=0;
    bool contains(float px,float py) const {
        return std::isfinite(px)&&std::isfinite(py)&&w>0&&h>0&&px>=x&&py>=y&&px<x+w&&py<y+h;
    }
};
class TouchControls {
public:
    std::array<TouchRect,Count> boxes{};
    bool visible=false;
    // Resizing, pause, focus loss and script ownership all require a new DOWN.
    // Keep blocked contacts until UP so the host can isolate synthesized mouse events.
    void clear() {
        released_.fill(false);menu_=false;
        for(auto &f:fingers_) if(f.used) {f.button=-1;f.pending=false;f.inside=false;}
        steering_=0;
    }
    void cancel_all() {clear();fingers_={};}
    void layout(float width,float height,TouchRect safe={}) {
        clear();boxes={};
        if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return;
        if(!std::isfinite(safe.x)||!std::isfinite(safe.y)||!std::isfinite(safe.w)||!std::isfinite(safe.h)||
           safe.x<0||safe.y<0||safe.w<=0||safe.h<=0||safe.x+safe.w>width||safe.y+safe.h>height)
            safe={0,0,width,height};
        if(safe.w<320||safe.h<220)return;
        const float u=std::clamp(std::min(safe.w/844.f,safe.h/390.f),.75f,1.35f);
        const float p=12*u,g=10*u,b=safe.y+safe.h-p,r=safe.x+safe.w-p,l=safe.x+p;
        const float gas=std::max(76.f,92*u),brake=std::max(60.f,76*u),pedal=std::max(88.f,112*u),small=44*u/.75f;
        const float sw=std::min(280*u,(r-l)-gas-brake-3*g);
        const float sh=std::max(68.f,78*u);
        boxes[Steering]={l,b-sh,sw,sh};
        boxes[Gas]={r-gas,b-pedal,gas,pedal};boxes[Brake]={r-gas-g-brake,b-pedal,brake,pedal};
        boxes[GearDown]={boxes[Brake].x,b-pedal-g-small,brake,small};
        boxes[GearUp]={boxes[Gas].x,b-pedal-g-small,gas,small};
        boxes[Handbrake]={l,b-sh-g-small,std::min(sw,110*u),small};
        const float top=44.f;
        const float menu=std::max(60.f,76*u),coin=std::max(44.f,48*u),start=std::max(44.f,56*u);
        boxes[Settings]={l,safe.y+p,menu,top};
        boxes[Coin]={l+menu+g,safe.y+p,coin,top};
        boxes[Start]={l+menu+coin+2*g,safe.y+p,start,top};
        boxes[View]={r-start,safe.y+p,start,top};
        // Small windows may not fit both rows. Never create overlapping targets.
        for(int i=0;i<Count;++i)for(int j=i+1;j<Count;++j)
            if(overlap(boxes[i],boxes[j])) {boxes={};return;}
    }
    bool down(uint64_t device,uint64_t id,float x,float y,bool gameplay=true) {
        if(find(device,id))return false;
        Finger *f=nullptr;for(auto &at:fingers_)if(!at.used){f=&at;break;}
        if(!f)return false;
        *f={true,device,id,-1,false,false};
        int hit=-1;for(int i=0;i<Count;++i)if(boxes[i].contains(x,y)){hit=i;break;}
        if(hit==Settings) {clear();menu_=true;return true;}
        if(!gameplay||hit<0)return false;
        if(hit==Steering&&steering_active())return true;
        f->button=hit;f->inside=true;f->pending=hit!=Steering;
        if(hit==Steering)update_steering(x);
        return true;
    }
    void move(uint64_t device,uint64_t id,float x,float y) {
        auto *f=find(device,id);if(!f||f->button<0)return;
        if(!std::isfinite(x)||!std::isfinite(y)){f->inside=false;f->pending=false;if(f->button==Steering)steering_=0;return;}
        if(f->button==Steering){update_steering(x);return;}
        f->inside=boxes[size_t(f->button)].contains(x,y);if(!f->inside)f->pending=false;
    }
    void up(uint64_t device,uint64_t id,bool canceled=false) {
        auto *f=find(device,id);if(!f)return;
        if(f->button==Steering)steering_=0;
        else if(!canceled&&f->button>=0&&f->pending)released_[size_t(f->button)]=true;
        *f={};
    }
    bool pressed(Button b) const {
        if(released_[size_t(b)])return true;
        for(const auto &f:fingers_)if(f.used&&f.button==b&&(f.inside||f.pending))return true;
        return false;
    }
    void consume() {released_.fill(false);for(auto &f:fingers_)f.pending=false;}
    bool take_settings_request(){const bool result=menu_;menu_=false;return result;}
    bool steering_active() const {for(const auto &f:fingers_)if(f.used&&f.button==Steering)return true;return false;}
    float steering() const{return steering_;}
    uint8_t steering_adc() const{return uint8_t(std::lround(128.f+(steering_<0?128.f:127.f)*steering_));}
    bool blocked_contacts() const {for(const auto &f:fingers_)if(f.used&&f.button<0)return true;return false;}
    static bool overlap(TouchRect a,TouchRect b){return a.w>0&&a.h>0&&b.w>0&&b.h>0&&a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
private:
    struct Finger {bool used=false;uint64_t device=0,id=0;int button=-1;bool pending=false,inside=false;};
    std::array<Finger,16> fingers_{};
    std::array<bool,Count> released_{};
    float steering_=0;bool menu_=false;
    Finger *find(uint64_t device,uint64_t id){for(auto &f:fingers_)if(f.used&&f.device==device&&f.id==id)return &f;return nullptr;}
    void update_steering(float x){const auto &b=boxes[Steering];float v=std::clamp(2*(x-b.x)/b.w-1,-1.f,1.f);steering_=std::abs(v)<=.08f?0:std::copysign((std::abs(v)-.08f)/.92f,v);}
};
} // namespace rally
