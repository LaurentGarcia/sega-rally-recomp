// Presentation only; input ownership lives in TouchControls.
#pragma once
#include "app/segarally_touch.h"
#include "imgui.h"
#include <cstdio>
namespace rally {
inline void draw_touch_overlay(const TouchControls &touch,int gear) {
    auto *draw=ImGui::GetForegroundDrawList();
    const char *labels[Count]={"STEERING","GAS","BRAKE","COIN","START","VIEW","SHIFT -","SHIFT +","HANDBRAKE","SETTINGS"};
    for(int i=0;i<Count;++i) {
        const auto &b=touch.boxes[size_t(i)];if(b.w<=0||b.h<=0)continue;
        const bool active=touch.pressed(Button(i));
        const ImU32 accent=i==Brake||i==Handbrake?IM_COL32(255,166,88,230):IM_COL32(92,224,218,230);
        draw->AddRectFilled({b.x,b.y},{b.x+b.w,b.y+b.h},active?IM_COL32(24,96,104,215):IM_COL32(14,23,32,170),12);
        draw->AddRect({b.x,b.y},{b.x+b.w,b.y+b.h},active?accent:IM_COL32(207,228,240,90),12,0,1.5f);
        const float fs=std::clamp(b.w/7.f,10.f,16.f);
        const auto size=ImGui::GetFont()->CalcTextSizeA(fs,1000,0,labels[i]);
        draw->AddText(ImGui::GetFont(),fs,{b.x+(b.w-size.x)/2,b.y+(i==Steering?10:(b.h-size.y)/2)},IM_COL32(240,248,252,245),labels[i]);
        if(i==Steering){
            const float cy=b.y+b.h*.65f,cx=b.x+b.w/2,half=b.w*.4f;
            draw->AddLine({cx-half,cy},{cx+half,cy},IM_COL32(185,209,221,145),4);
            draw->AddRectFilled({cx-half*.08f,cy-7},{cx+half*.08f,cy+7},IM_COL32(185,209,221,70),4);
            draw->AddCircleFilled({cx+touch.steering()*half,cy},10,accent);
        }
    }
    const auto &b=touch.boxes[GearUp];if(b.w>0){char label[16];std::snprintf(label,sizeof(label),"GEAR %s",gear==0?"N":"");if(gear>0)std::snprintf(label,sizeof(label),"GEAR %d",gear);const auto size=ImGui::CalcTextSize(label);draw->AddText({b.x+b.w-size.x,b.y-20},IM_COL32(245,250,255,230),label);}
}
} // namespace rally
