// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Model2A video bus adapter for the existing native Daytona Video/Geo path.
// Register/memory contracts: pinned MAME dddd73680656e355bb2b5beecab1167c9f07bf81.
#pragma once

#include "runtime/m2_replay_bus.h"
#include "runtime/video.h"

#include <functional>
#include <span>

namespace rt {

class Model2AVideo final : public DeviceModel {
public:
    struct Hooks {
        std::function<uint64_t()> frame_number; // required; caller owns screen scheduling
        std::function<uint32_t()> polygon_count; // raster poly_list_index, not a ready substitute
        std::function<void(uint32_t)> zclip_write;
    };
    explicit Model2AVideo(Hooks hooks);
    Model2AVideo(const Model2AVideo &) = delete;
    Model2AVideo &operator=(const Model2AVideo &) = delete;
    Model2AVideo(Model2AVideo &&) = delete;
    Model2AVideo &operator=(Model2AVideo &&) = delete;

    Video &video() { return video_; }
    const Video &video() const { return video_; }
    VideoMem memory() const;
    std::span<const uint8_t> tile_ram() const { return tile_; }
    std::span<const uint8_t> character_ram() const { return character_; }
    std::span<const uint8_t> palette_ram() const { return palette_; }
    std::span<const uint8_t> color_translation_ram() const { return translation_; }
    std::span<const uint8_t> luma_ram() const { return luma_; } // byte0 per CPU dword
    std::span<const uint32_t> texture_words(unsigned sheet) const;
    std::span<const uint8_t> framebuffer_ram(unsigned bank) const;
    uint32_t video_control() const { return video_control_; }
    uint8_t zclip() const { return zclip_; }
    bool render_mode() const { return render_mode_; }
    bool render_test_mode() const { return render_test_; }
    void bind_geometry(std::function<uint32_t()> count, std::function<void(uint32_t)> zclip);
    void frame_start() { video_.frame_start(); }
    // Existing renderer is reused. Direct framebuffer test-mode presentation
    // is not implemented in Video and remains an explicit frontier here.
    void screen_update(const std::vector<GeoPoly> &polygons, int windows);

    bool claims(uint32_t address) const override;
    uint16_t flags(uint32_t address) const override;
    Check read(uint32_t address, uint32_t mask, uint32_t &value) override;
    void write(uint32_t address, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t address, uint32_t ours, uint32_t mame) override;

private:
    enum class Kind { None, Tile, Character, Palette, Translation, Texture0, Texture1, Luma, FrameA, FrameB };
    struct Mapping { Kind kind; uint32_t offset; };
    static Mapping mapping(uint32_t address);
    uint8_t *bytes(Kind kind);
    const uint8_t *bytes(Kind kind) const;
    static uint32_t control_address(uint32_t address);
    void check(uint32_t address) const;
    void notify(Kind kind, uint32_t offset, uint32_t mask);

    Hooks hooks_;
    // Stable storage shared with the renderer, never duplicate shadow RAM.
    std::vector<uint8_t> tile_, character_, palette_, translation_, luma_, frame_a_, frame_b_;
    std::vector<uint32_t> texture_[2];
    Video video_;
    uint64_t texture_generation_ = 0;
    uint32_t video_control_ = 0;
    uint8_t zclip_ = 0;
    bool render_mode_ = false, render_test_ = false, render_unknown_ = false;
};

} // namespace rt
