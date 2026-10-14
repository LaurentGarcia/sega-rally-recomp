// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Adapted from Daytona M2Board/Video and public MAME at
// dddd73680656e355bb2b5beecab1167c9f07bf81: src/mame/sega/model2.cpp
// SHA256359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6;
// model2_v.cpp SHA25664511d032b7382df5ded6829b3f3e618c8f09fb602016dc7be35a7cd924e6892;
// segaic24.cpp SHA256d6e2283b1e02e27fe4e28c78d8c86f6e3c2177e6c7db396e7b666d397a037b96.
// Changes: exact range/mirror decoding replaces the coarse Daytona page map;
// palette/tile/char notifications reuse Video; external callbacks supply frame
// and geometry state. Luma remains CPU-strided for existing VideoMem consumers.
// Framebuffer storage is supported; framebuffer test-mode presentation is not.
#include "runtime/model2a_video.h"

#include <utility>

namespace rt {
namespace {
bool between(uint32_t address, uint32_t start, uint32_t end) { return address >= start && address <= end; }
}

Model2AVideo::Model2AVideo(Hooks hooks)
    : hooks_(std::move(hooks)), tile_(0x10000), character_(0x80000), palette_(0x4000),
      translation_(0xc000), luma_(0x20000), frame_a_(0x80000), frame_b_(0x80000),
      texture_{std::vector<uint32_t>(0x80000), std::vector<uint32_t>(0x80000)},
      video_(tile_.data(), character_.data()) {
    if (!hooks_.frame_number) throw Fatal("Model2A video requires an explicit frame counter");
    video_.enable_write_tracking();
}

Model2AVideo::Mapping Model2AVideo::mapping(uint32_t a) {
    const uint32_t tile = a & ~0x00110000u;
    if (between(tile, 0x01000000, 0x0100ffff)) return {Kind::Tile, tile - 0x01000000};
    const uint32_t character = a & ~0x00100000u;
    if (between(character, 0x01080000, 0x010fffff)) return {Kind::Character, character - 0x01080000};
    if (between(a, 0x01800000, 0x01803fff)) return {Kind::Palette, a - 0x01800000};
    if (between(a, 0x01810000, 0x0181bfff)) return {Kind::Translation, a - 0x01810000};
    const uint32_t texture = a & ~0x00200000u;
    if (between(texture, 0x12000000, 0x121fffff)) return {Kind::Texture0, texture - 0x12000000};
    if (between(texture, 0x12400000, 0x125fffff)) return {Kind::Texture1, texture - 0x12400000};
    if (between(a, 0x12800000, 0x1281ffff)) return {Kind::Luma, a - 0x12800000};
    if (between(a, 0x11600000, 0x1167ffff)) return {Kind::FrameA, a - 0x11600000};
    if (between(a, 0x11680000, 0x116fffff)) return {Kind::FrameB, a - 0x11680000};
    return {Kind::None, 0};
}

uint32_t Model2AVideo::control_address(uint32_t a) {
    const uint32_t canonical = (a & ~0x00100000u) & ~3u;
    if (canonical == 0x01020000 || canonical == 0x01040000 ||
        canonical == 0x01060000 || canonical == 0x01070000) return canonical;
    return a & ~3u;
}

bool Model2AVideo::claims(uint32_t a) const {
    if (mapping(a).kind != Kind::None) return true;
    const uint32_t reg = control_address(a);
    return reg == 0x01020000 || reg == 0x01040000 || reg == 0x01060000 || reg == 0x01070000 ||
           reg == 0x0181c000 || reg == 0x0098000c || reg == 0x10800000 ||
           between(a, 0x00980030, 0x0098003f) || between(a, 0x10000000, 0x101fffff) ||
           between(a, 0x10400000, 0x105fffff);
}

uint16_t Model2AVideo::flags(uint32_t a) const {
    return mapping(a).kind != Kind::None || control_address(a) == 0x01020000 ? Cpu::BURST : 0;
}

void Model2AVideo::check(uint32_t a) const {
    if ((a & 3) || !claims(a)) throw Fatal("Model2A video access outside an aligned mapped word");
}

uint8_t *Model2AVideo::bytes(Kind kind) {
    switch (kind) {
    case Kind::Tile: return tile_.data();
    case Kind::Character: return character_.data();
    case Kind::Palette: return palette_.data();
    case Kind::Translation: return translation_.data();
    case Kind::Luma: return luma_.data();
    case Kind::FrameA: return frame_a_.data();
    case Kind::FrameB: return frame_b_.data();
    default: throw Fatal("Model2A video requested a non-byte memory region");
    }
}
const uint8_t *Model2AVideo::bytes(Kind kind) const { return const_cast<Model2AVideo *>(this)->bytes(kind); }

DeviceModel::Check Model2AVideo::read(uint32_t a, uint32_t mask, uint32_t &value) {
    check(a);
    const Mapping m = mapping(a);
    value = 0;
    if (m.kind == Kind::Texture0 || m.kind == Kind::Texture1) {
        value = texture_[m.kind == Kind::Texture1][m.offset / 4];
    } else if (m.kind == Kind::Luma) {
        value = luma_[m.offset]; // only the low CPU byte lane is wired
    } else if (m.kind != Kind::None) {
        const auto *source = bytes(m.kind) + m.offset;
        for (unsigned lane = 0; lane < 4; ++lane) value |= uint32_t(source[lane]) << (lane * 8);
    } else if (a == 0x0098000c && mask) {
        const uint64_t frame = hooks_.frame_number();
        value = uint32_t(render_mode_ ? (frame & 1) << 2 : (frame & 2) << 1) | (video_control_ & 3);
    } else if (between(a, 0x00980030, 0x0098003f)) {
        static constexpr uint8_t id[] = {0, 'T', 'A', 'H', 0, 'A', 'K', 'O', 0, 'Z', 'A', 'K', 0, 'M', 'T', 'K'};
        for (unsigned lane = 0; lane < 4; ++lane) value |= uint32_t(id[a - 0x00980030 + lane]) << (lane * 8);
    } else if (between(a, 0x10000000, 0x101fffff)) {
        value = uint32_t(render_unknown_) << 14 | uint32_t(render_mode_) << 2 | uint32_t(render_test_);
    } else if (between(a, 0x10400000, 0x105fffff) && mask) {
        if (!hooks_.polygon_count) throw Fatal("Model2A polygon-count read requires an attached geometrizer");
        value = hooks_.polygon_count();
    }
    // Other mapped read holes are pinned nopr/write-only/unmapped zero.
    value &= mask;
    return Strict;
}

void Model2AVideo::notify(Kind kind, uint32_t offset, uint32_t mask) {
    if (kind == Kind::Tile) video_.tile_memory_w();
    else if (kind == Kind::Character) video_.character_memory_w();
    else if (kind == Kind::Palette || kind == Kind::Translation) {
        for (unsigned lane = 0; lane < 2; ++lane) if ((mask >> (lane * 16)) & 0xffff) {
            const uint32_t word = offset / 2 + lane;
            if (kind == Kind::Palette) video_.palette_w(word, palette_.data(), translation_.data());
            else video_.colorxlat_w(word);
        }
    }
}

void Model2AVideo::write(uint32_t a, uint32_t data, uint32_t mask) {
    check(a);
    if (!mask) return;
    const uint32_t d = data & mask;
    const Mapping m = mapping(a);
    if (m.kind == Kind::Texture0 || m.kind == Kind::Texture1) {
        // Handler takes no mask. A narrow write replaces the entire packed
        // half; unwritten CPU lanes arrive as zero, including upper-only writes.
        const uint32_t word = m.offset / 4;
        const unsigned shift = (word & 1) * 16;
        uint32_t &stored = texture_[m.kind == Kind::Texture1][word / 2];
        stored = (stored & ~(0xffffu << shift)) | ((d & 0xffff) << shift);
        ++texture_generation_;
    } else if (m.kind == Kind::Luma) {
        if (mask & 0xff) luma_[m.offset] = uint8_t(d);
    } else if (m.kind != Kind::None) {
        auto *target = bytes(m.kind) + m.offset;
        for (unsigned lane = 0; lane < 4; ++lane) {
            const uint8_t selected = uint8_t(mask >> (lane * 8));
            target[lane] = (target[lane] & ~selected) | uint8_t((d >> (lane * 8)) & selected);
        }
        notify(m.kind, m.offset, mask); // callbacks see the fully stored word
    } else if (a == 0x0098000c) {
        video_control_ = (video_control_ & ~mask) | d;
    } else if (control_address(a) == 0x01040000) {
        if (mask & 0xffff) video_.xhout_w(uint16_t(d));
    } else if (control_address(a) == 0x01060000) {
        if (mask & 0xffff) video_.xvout_w(uint16_t(d));
    } else if (a == 0x0181c000) {
        zclip_ = uint8_t(d);
        if (hooks_.zclip_write) hooks_.zclip_write(zclip_);
    } else if (between(a, 0x10000000, 0x101fffff)) {
        render_test_ = d & 1;
        render_mode_ = d & 4;
        render_unknown_ = d & 0x4000;
    }
    // Read-only ID/count and documented nop writes discard their data.
}

VideoMem Model2AVideo::memory() const {
    return {palette_.data(), translation_.data(), luma_.data(), texture_[0].data(), texture_[1].data(), texture_generation_};
}
std::span<const uint32_t> Model2AVideo::texture_words(unsigned sheet) const {
    if (sheet > 1) throw Fatal("Model2A texture sheet must be zero or one");
    return texture_[sheet];
}
std::span<const uint8_t> Model2AVideo::framebuffer_ram(unsigned bank) const {
    if (bank > 1) throw Fatal("Model2A framebuffer bank must be zero or one");
    return bank ? frame_b_ : frame_a_;
}
void Model2AVideo::bind_geometry(std::function<uint32_t()> count, std::function<void(uint32_t)> zclip) {
    hooks_.polygon_count = std::move(count);
    hooks_.zclip_write = std::move(zclip);
    if (hooks_.zclip_write) hooks_.zclip_write(zclip_);
}
void Model2AVideo::screen_update(const std::vector<GeoPoly> &polygons, int windows) {
    if (render_test_) throw Fatal("Model2A framebuffer test-mode presentation is not implemented");
    video_.screen_update(polygons, windows, memory());
}
void Model2AVideo::measured_mismatch(uint32_t, uint32_t, uint32_t) {
    throw Divergence("Strict Model2A video unexpectedly received a measured mismatch");
}

} // namespace rt
