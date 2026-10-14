// Native cabinet inputs and a small Metal overlay; no game-specific patches.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace rally {
enum Button { Left, Right, Gas, Brake, Coin, Start, View, GearDown, GearUp, Handbrake, Count };

class TouchControls {
public:
    std::array<SDL_FRect, Count> boxes{};
    std::array<bool, Count> held{};
    bool visible = false;
    void layout(SDL_Window *window) {
        SDL_Rect safe{};
        int w = 1, h = 1;
        SDL_GetWindowSize(window, &w, &h);
        SDL_GetWindowSafeArea(window, &safe);
        if (safe.w <= 0 || safe.h <= 0) safe = {0, 0, w, h};
        const float s = std::clamp(std::min(safe.w / 11.f, safe.h / 4.f), 42.f, 100.f);
        const float x = float(safe.x), y = float(safe.y), r = x + safe.w, b = y + safe.h;
        boxes[Left] = {x + 10, b - s - 10, s, s};
        boxes[Right] = {x + s + 18, b - s - 10, s, s};
        boxes[Brake] = {r - 2*s - 18, b - s - 10, s, s};
        boxes[Gas] = {r - s - 10, b - s - 10, s, s};
        boxes[Coin] = {x + 10, y + 10, s * .75f, s * .65f};
        boxes[Start] = {x + s * .75f + 18, y + 10, s, s * .65f};
        boxes[View] = {r - s - 10, y + 10, s, s * .65f};
        boxes[GearDown] = {r - 2*s - 18, b - 2*s - 18, s, s * .65f};
        boxes[GearUp] = {r - s - 10, b - 2*s - 18, s, s * .65f};
        boxes[Handbrake] = {x + 10, b - 2*s - 18, s, s * .65f};
        width_ = w; height_ = h;
    }
    void clear() { fingers_.clear(); held.fill(false); }
    void event(const SDL_Event &e) {
        if (e.type != SDL_EVENT_FINGER_DOWN && e.type != SDL_EVENT_FINGER_MOTION && e.type != SDL_EVENT_FINGER_UP) return;
        visible = true;
        auto at = std::find_if(fingers_.begin(), fingers_.end(), [&](const Finger &f) {
            return f.touch == e.tfinger.touchID && f.id == e.tfinger.fingerID;
        });
        if (e.type == SDL_EVENT_FINGER_UP) {
            if (at != fingers_.end()) fingers_.erase(at);
        } else {
            const SDL_FPoint p{e.tfinger.x * width_, e.tfinger.y * height_};
            int button = -1;
            for (int i = 0; i < Count; ++i) if (SDL_PointInRectFloat(&p, &boxes[i])) { button = i; break; }
            if (at != fingers_.end()) at->button = button;
            else if (fingers_.size() < 16) fingers_.push_back({e.tfinger.touchID, e.tfinger.fingerID, button});
        }
        held.fill(false);
        for (const auto &f : fingers_) if (f.button >= 0) held[size_t(f.button)] = true;
    }
    bool init(SDL_GPUDevice *device, SDL_GPUTextureFormat format) {
        dev_ = device;
        static const char vs[] = "#include <metal_stdlib>\nusing namespace metal; struct In{float2 p [[attribute(0)]];float4 c [[attribute(1)]];};struct Out{float4 p [[position]];float4 c;};vertex Out main0(In v [[stage_in]]){return {float4(v.p,0,1),v.c};}";
        static const char fs[] = "#include <metal_stdlib>\nusing namespace metal; struct In{float4 p [[position]];float4 c;};fragment float4 main0(In v [[stage_in]]){return v.c;}";
        SDL_GPUShaderCreateInfo si{};
        si.format = SDL_GPU_SHADERFORMAT_MSL; si.entrypoint = "main0";
        si.stage = SDL_GPU_SHADERSTAGE_VERTEX; si.code = reinterpret_cast<const Uint8 *>(vs); si.code_size = strlen(vs);
        auto *vert = SDL_CreateGPUShader(dev_, &si);
        si.stage = SDL_GPU_SHADERSTAGE_FRAGMENT; si.code = reinterpret_cast<const Uint8 *>(fs); si.code_size = strlen(fs);
        auto *frag = SDL_CreateGPUShader(dev_, &si);
        SDL_GPUVertexBufferDescription vb{0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
        SDL_GPUVertexAttribute attrs[] = {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0},
                                         {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 8}};
        SDL_GPUColorTargetDescription color{}; color.format = format;
        color.blend_state.enable_blend = true;
        color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        SDL_GPUGraphicsPipelineCreateInfo pi{};
        pi.vertex_shader = vert; pi.fragment_shader = frag;
        pi.vertex_input_state = {&vb, 1, attrs, 2};
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.target_info.color_target_descriptions = &color; pi.target_info.num_color_targets = 1;
        if (vert && frag) pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
        if (vert) SDL_ReleaseGPUShader(dev_, vert);
        if (frag) SDL_ReleaseGPUShader(dev_, frag);
        SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_VERTEX, 65536, 0};
        SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, 65536, 0};
        buffer_ = SDL_CreateGPUBuffer(dev_, &bi); transfer_ = SDL_CreateGPUTransferBuffer(dev_, &ti);
        return pipe_ && buffer_ && transfer_;
    }
    void shutdown() {
        if (pipe_) SDL_ReleaseGPUGraphicsPipeline(dev_, pipe_);
        if (buffer_) SDL_ReleaseGPUBuffer(dev_, buffer_);
        if (transfer_) SDL_ReleaseGPUTransferBuffer(dev_, transfer_);
        pipe_ = nullptr; buffer_ = nullptr; transfer_ = nullptr;
    }
    void draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
        if (!visible) return;
        vertices_.clear();
        for (int i = 0; i < Count; ++i) {
            const auto &b = boxes[i];
            rectangle(b.x, b.y, b.w, b.h, held[i] ? .15f : .04f, held[i] ? .65f : .07f, held[i] ? .95f : .1f, .7f);
            const float cx=b.x+b.w/2, cy=b.y+b.h/2, z=std::min(b.w,b.h)/4;
            if (i == Left || i == Right) {
                const float d=i==Left?-1.f:1.f;
                triangle(cx+d*z,cy,cx-d*z,cy-z,cx-d*z,cy+z, 1,1,1,.9f);
            } else if (i == Gas || i == Start) {
                triangle(cx+z,cy,cx-z,cy-z,cx-z,cy+z, .9f,1,.9f,.9f);
            } else if (i == GearUp || i == GearDown) {
                rectangle(cx-z,cy-2,2*z,4,1,1,1,.9f);
                if(i==GearUp) rectangle(cx-2,cy-z,4,2*z,1,1,1,.9f);
            } else if (i == Brake || i == Handbrake) {
                rectangle(cx-z,cy-z,2*z,2*z,1,.7f,.4f,.9f);
            } else if (i == Coin) {
                rectangle(cx-z/2,cy-z,z,2*z,1,.9f,.25f,.9f);
            } else { // view button: an outlined camera frame
                rectangle(cx-z,cy-z,2*z,3,1,1,1,.9f); rectangle(cx-z,cy+z-3,2*z,3,1,1,1,.9f);
                rectangle(cx-z,cy-z,3,2*z,1,1,1,.9f); rectangle(cx+z-3,cy-z,3,2*z,1,1,1,.9f);
            }
        }
        const Uint32 bytes=Uint32(vertices_.size()*sizeof(Vertex));
        void *mapped=SDL_MapGPUTransferBuffer(dev_,transfer_,true);
        if(!mapped) return;
        memcpy(mapped,vertices_.data(),bytes); SDL_UnmapGPUTransferBuffer(dev_,transfer_);
        auto *copy=SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation src{transfer_,0}; SDL_GPUBufferRegion dst{buffer_,0,bytes};
        SDL_UploadToGPUBuffer(copy,&src,&dst,true); SDL_EndGPUCopyPass(copy);
        SDL_GPUColorTargetInfo color{};color.texture=target;color.load_op=SDL_GPU_LOADOP_LOAD;color.store_op=SDL_GPU_STOREOP_STORE;
        auto *pass=SDL_BeginGPURenderPass(cmd,&color,1,nullptr);
        SDL_BindGPUGraphicsPipeline(pass,pipe_);SDL_GPUBufferBinding binding{buffer_,0};
        SDL_BindGPUVertexBuffers(pass,0,&binding,1);SDL_DrawGPUPrimitives(pass,Uint32(vertices_.size()),1,0,0);
        SDL_EndGPURenderPass(pass);
    }
private:
    struct Finger {SDL_TouchID touch; SDL_FingerID id; int button;};
    struct Vertex {float x,y,r,g,b,a;};
    std::vector<Finger> fingers_; std::vector<Vertex> vertices_;
    int width_=1,height_=1;
    SDL_GPUDevice *dev_=nullptr;SDL_GPUGraphicsPipeline *pipe_=nullptr;
    SDL_GPUBuffer *buffer_=nullptr;SDL_GPUTransferBuffer *transfer_=nullptr;
    void vertex(float x,float y,float r,float g,float b,float a) {vertices_.push_back({2*x/width_-1,1-2*y/height_,r,g,b,a});}
    void triangle(float x1,float y1,float x2,float y2,float x3,float y3,float r,float g,float b,float a) {
        vertex(x1,y1,r,g,b,a);vertex(x2,y2,r,g,b,a);vertex(x3,y3,r,g,b,a);
    }
    void rectangle(float x,float y,float w,float h,float r,float g,float b,float a) {
        triangle(x,y,x+w,y,x+w,y+h,r,g,b,a);triangle(x,y,x+w,y+h,x,y+h,r,g,b,a);
    }
};
} // namespace rally
