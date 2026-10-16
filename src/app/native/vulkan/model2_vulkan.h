// Synchronous offscreen Vulkan consumption of the common Model2Frame packet.
// No SDL, window, game CPU, device audio or platform presentation is owned here.
#pragma once
#include "app/gpu/model2_frame.h"
#include <memory>
#include <string>
#include <vector>
namespace rt {class Video;}
namespace app::native {
class VulkanModel2 {
public:
    VulkanModel2();~VulkanModel2();
    VulkanModel2(const VulkanModel2 &)=delete;
    VulkanModel2 &operator=(const VulkanModel2 &)=delete;
    bool init(bool require_software=false,bool validation=false);
    // Correctness prototype: full per-frame snapshots, submit/five-second fence
    // wait/readback. Returns packed native BGRA8, not an asynchronous window API.
    std::vector<uint32_t> render(const rt::Video &,int width,int height,int scale=1,bool scanlines=false);
    const Model2Frame &frame() const;
    const std::string &device_name() const;
    uint32_t validation_errors() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
