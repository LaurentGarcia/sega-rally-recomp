// Presentation only; input ownership and hitboxes live in TouchControls.
#pragma once
#include "app/segarally_touch.h"
#include "imgui.h"
#include <cstdio>

namespace rally {
namespace touch_overlay_detail {
constexpr ImU32 ivory=IM_COL32(244,239,225,245);
constexpr ImU32 muted=IM_COL32(200,199,191,190);
constexpr ImU32 orange=IM_COL32(255,157,76,250);

inline void text(ImDrawList *draw,const char *label,float size,ImVec2 at,ImU32 color=ivory) {
    draw->AddText(ImGui::GetFont(),size,at,color,label);
}
inline void centered(ImDrawList *draw,const char *label,float size,float x,float y,ImU32 color=ivory) {
    const auto extent=ImGui::GetFont()->CalcTextSizeA(size,1000,0,label);
    text(draw,label,size,{x-extent.x*.5f,y-extent.y*.5f},color);
}
inline void surface(ImDrawList *draw,const TouchRect &b,bool active,bool pedal=false) {
    const float radius=std::min(17.f,b.h*.28f);
    const ImVec2 a{b.x,b.y},z{b.x+b.w,b.y+b.h};
    draw->AddRectFilled({a.x,a.y+2},{z.x,z.y+2},IM_COL32(0,0,0,45),radius);
    draw->AddRectFilled(a,z,active?IM_COL32(88,57,34,224):IM_COL32(24,28,29,206),radius);
    draw->AddRectFilled({a.x+1,a.y+1},{z.x-1,a.y+b.h*.45f},
                        active?IM_COL32(255,183,104,18):IM_COL32(255,255,243,8),radius-1,ImDrawFlags_RoundCornersTop);
    draw->AddRect(a,z,active?IM_COL32(255,165,81,220):IM_COL32(233,233,220,55),radius,0,active?1.5f:1.f);
    draw->AddLine({a.x+radius,a.y+1},{z.x-radius,a.y+1},IM_COL32(255,250,233,active?80:35),1);
    if(pedal) {
        const float x=b.x+5,y=b.y+b.h*.28f;
        draw->AddRectFilled({x,y},{x+2,y+b.h*.38f},active?orange:IM_COL32(255,157,76,80),1);
    }
}
inline void chevron(ImDrawList *draw,ImVec2 at,float r,bool right,ImU32 color,float thickness=1.5f) {
    const float sign=right?1.f:-1.f;
    draw->AddLine({at.x-sign*r*.45f,at.y-r},{at.x+sign*r*.45f,at.y},color,thickness);
    draw->AddLine({at.x+sign*r*.45f,at.y},{at.x-sign*r*.45f,at.y+r},color,thickness);
}
inline void steering(ImDrawList *draw,const TouchRect &b,const TouchControls &touch) {
    const bool active=touch.steering_active();
    surface(draw,b,active);
    const float scale=std::clamp(b.w/280.f,.75f,1.35f);
    const float cx=b.x+b.w*.5f,cy=b.y+b.h*.66f,travel=b.w*.37f;
    text(draw,"STEER",11*scale,{b.x+15*scale,b.y+11*scale});
    const char *mode="ANALOG";
    const auto size=ImGui::GetFont()->CalcTextSizeA(9*scale,1000,0,mode);
    text(draw,mode,9*scale,{b.x+b.w-15*scale-size.x,b.y+12*scale},muted);
    draw->AddRectFilled({cx-travel,cy-3*scale},{cx+travel,cy+3*scale},IM_COL32(0,0,0,100),3*scale);
    draw->AddLine({cx-travel,cy+3*scale},{cx+travel,cy+3*scale},IM_COL32(245,239,225,30),1);
    for(int i=-4;i<=4;++i) {
        const float x=cx+float(i)*travel/4;
        draw->AddLine({x,cy-9*scale},{x,cy-(i==0?16.f:12.f)*scale},
                      i==0?IM_COL32(244,239,225,200):IM_COL32(244,239,225,65),i==0?1.5f:1.f);
    }
    const float knob=cx+touch.steering()*travel;
    if(active)draw->AddRectFilled({std::min(cx,knob),cy-2*scale},{std::max(cx,knob),cy+2*scale},orange,2*scale);
    const float half=13*scale;
    draw->AddRectFilled({knob-half,cy-11*scale},{knob+half,cy+11*scale},
                        active?orange:IM_COL32(225,224,211,230),8*scale);
    for(int i=-1;i<=1;++i)
        draw->AddLine({knob+float(i)*4*scale,cy-4*scale},{knob+float(i)*4*scale,cy+4*scale},IM_COL32(30,34,34,155),1);
    chevron(draw,{b.x+11*scale,cy},3*scale,false,muted);
    chevron(draw,{b.x+b.w-11*scale,cy},3*scale,true,muted);
}
inline void pedal(ImDrawList *draw,const TouchRect &b,bool active,bool gas) {
    surface(draw,b,active,true);
    const float scale=std::clamp(b.h/112.f,.75f,1.35f);
    const float cx=b.x+b.w*.5f,cy=b.y+b.h*.42f;
    const float half=(gas?12.f:18.f)*scale,height=(gas?39.f:29.f)*scale;
    const ImU32 pad=active?orange:IM_COL32(239,230,208,220);
    // Raised pedal pad, ribbed tread and a visible hinge distinguish gas/brake.
    draw->AddLine({cx,cy+height*.5f},{cx-3*scale,cy+height*.5f+9*scale},IM_COL32(192,193,184,130),4*scale);
    draw->AddRectFilled({cx-half,cy-height*.5f},{cx+half,cy+height*.5f},pad,5*scale);
    draw->AddRect({cx-half+1,cy-height*.5f+1},{cx+half-1,cy+height*.5f-1},IM_COL32(255,251,237,90),4*scale);
    for(int i=0;i<4;++i) {
        const float y=cy-height*.30f+float(i)*height*.2f;
        draw->AddLine({cx-half+5*scale,y},{cx+half-5*scale,y},IM_COL32(42,40,35,150),2*scale);
    }
    centered(draw,gas?"GAS":"BRAKE",std::min(14*scale,b.w/5.3f),cx,b.y+b.h*.81f,active?ivory:pad);
    const float indicator=gas?15.f:11.f;
    draw->AddRectFilled({cx-indicator*scale,b.y+b.h-7*scale},{cx+indicator*scale,b.y+b.h-5*scale},
                        active?orange:IM_COL32(255,157,76,55),scale);
}
inline void shift(ImDrawList *draw,const TouchRect &b,bool active,bool up) {
    surface(draw,b,active);
    const float scale=std::clamp(b.h/58.7f,.75f,1.35f),cx=b.x+b.w*.5f;
    const float cy=b.y+b.h*.38f,r=5*scale,sign=up?-1.f:1.f;
    draw->AddLine({cx,cy-sign*5*scale},{cx,cy+sign*5*scale},active?orange:ivory,2*scale);
    draw->AddLine({cx-r,cy},{cx,cy+sign*5*scale},active?orange:ivory,2*scale);
    draw->AddLine({cx+r,cy},{cx,cy+sign*5*scale},active?orange:ivory,2*scale);
    centered(draw,up?"SHIFT +":"SHIFT -",9*scale,cx,b.y+b.h*.76f,active?ivory:muted);
}
inline void compact_icon(ImDrawList *draw,Button button,ImVec2 c,float scale,ImU32 color) {
    const float r=6*scale;
    if(button==Settings) {
        for(int i=-1;i<=1;++i) {
            const float y=c.y+float(i)*4*scale,x=c.x+float(i==0?3:-2)*scale;
            draw->AddLine({c.x-r,y},{c.x+r,y},color,1.2f*scale);
            draw->AddCircleFilled({x,y},1.8f*scale,color);
        }
    } else if(button==Coin) {
        draw->AddCircle(c,r,color,24,1.3f*scale);
        draw->AddLine({c.x-2.5f*scale,c.y},{c.x+2.5f*scale,c.y},color,1.2f*scale);
        draw->AddLine({c.x,c.y-2.5f*scale},{c.x,c.y+2.5f*scale},color,1.2f*scale);
    } else if(button==Start) {
        draw->AddTriangleFilled({c.x-r*.65f,c.y-r},{c.x+r,c.y},{c.x-r*.65f,c.y+r},color);
    } else if(button==View) {
        draw->AddRect({c.x-r-1,c.y-r*.65f},{c.x+r+1,c.y+r*.65f},color,2*scale,0,1.2f*scale);
        draw->AddCircle(c,2.4f*scale,color,20,1.2f*scale);
        draw->AddLine({c.x-3*scale,c.y-r*.65f-2*scale},{c.x+2*scale,c.y-r*.65f-2*scale},color,1.2f*scale);
    } else if(button==Handbrake) {
        draw->AddRectFilled({c.x-7*scale,c.y+5*scale},{c.x+7*scale,c.y+8*scale},color,1.4f*scale);
        draw->AddLine({c.x-4*scale,c.y+5*scale},{c.x+2*scale,c.y-5*scale},color,2*scale);
        draw->AddLine({c.x+1*scale,c.y-5*scale},{c.x+7*scale,c.y-5*scale},color,3*scale);
    }
}
inline void compact(ImDrawList *draw,const TouchRect &b,Button button,bool active) {
    surface(draw,b,active);
    const float scale=std::clamp(std::min(b.w/48.f,b.h/44.f),.85f,1.f);
    const char *label=button==Settings?"MENU":button==Coin?"COIN":button==Start?"START":button==View?"VIEW":"HANDBRAKE";
    compact_icon(draw,button,{b.x+b.w*.5f,b.y+b.h*.34f},scale,active?orange:ivory);
    centered(draw,label,button==Handbrake?std::clamp(b.w/11.f,8.f,10.f):9*scale,
             b.x+b.w*.5f,b.y+b.h*.77f,active?ivory:muted);
}
} // namespace touch_overlay_detail

inline void draw_touch_overlay(const TouchControls &touch,int gear) {
    using namespace touch_overlay_detail;
    auto *draw=ImGui::GetForegroundDrawList();
    for(int i=0;i<Count;++i) {
        const auto &b=touch.boxes[size_t(i)];
        if(b.w<=0||b.h<=0)continue;
        const bool active=touch.pressed(Button(i));
        if(i==Steering)steering(draw,b,touch);
        else if(i==Gas||i==Brake)pedal(draw,b,active,i==Gas);
        else if(i==GearDown||i==GearUp)shift(draw,b,active,i==GearUp);
        else compact(draw,b,Button(i),active);
    }
    const auto &b=touch.boxes[GearUp];
    if(b.w>0&&b.h>0) {
        char label[16];
        if(gear<=0)std::snprintf(label,sizeof(label),"N");
        else std::snprintf(label,sizeof(label),"%d",gear);
        const float width=70,y=b.y-30,x=b.x+b.w-width;
        draw->AddRectFilled({x,y},{b.x+b.w,y+24},IM_COL32(22,26,27,200),8);
        text(draw,"GEAR",9,{x+9,y+8},muted);
        centered(draw,label,18,b.x+b.w-13,y+12,orange);
    }
}
} // namespace rally
