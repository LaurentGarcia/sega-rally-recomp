#pragma once
#include "runtime/rom_source.h"
#include "runtime/sound_image_source.h"
#include "platform/ps2/tools/pack_reader.h"

namespace rally::ps2 {
class RomCache final : public rt::RomSource, public snd::SoundImageSource {
public:
    explicit RomCache(const char *path, uint32_t pages = 512,
                      PackReader::Progress progress = nullptr, void *context = nullptr)
        : pack_(path, pages, progress, context) {}
    uint32_t size(rt::RomRegion r) const override { return pack_.size(region(r)); }
    const uint8_t *page(rt::RomRegion r, uint32_t index) override {
        // A page load may evict any remembered region pointer. This is also
        // required on failure: PackReader invalidates a victim before I/O.
        forget();
        return pack_.page(region(r), index);
    }
    std::vector<uint8_t> tgp_tables() {
        // This is the only resident ROM region in the paged machine. Check
        // its hardware size before trusting pack metadata with an allocation.
        constexpr uint32_t bytes = 0x40000;
        if (pack_.size(Region::TgpTables) != bytes)
            throw std::runtime_error("invalid TGP table size in asset pack");
        std::vector<uint8_t> result(bytes);
        forget();
        pack_.read(Region::TgpTables, 0, result.data(), result.size());
        return result;
    }
    uint64_t misses() const { return pack_.misses(); }
    uint32_t size(snd::SoundImage image) const override { return pack_.size(sound_region(image)); }
    uint16_t read16_be(snd::SoundImage image, uint32_t offset) override {
        const auto region = sound_region(image);
        if ((offset & 1u) || uint64_t(offset) + 2 > pack_.size(region))
            throw std::runtime_error("sound ROM word outside asset pack");
        // Sound reads share the same cache and can invalidate main/TGP pages.
        forget();
        const uint8_t *bytes = pack_.page(region, offset / PackReader::page_bytes)
                             + offset % PackReader::page_bytes;
        return uint16_t(uint16_t(bytes[0]) << 8 | bytes[1]);
    }
private:
    static Region sound_region(snd::SoundImage image) {
        switch (image) {
        case snd::SoundImage::Program: return Region::SoundProgram;
        case snd::SoundImage::Samples: return Region::SoundSamples;
        }
        throw std::runtime_error("invalid sound ROM region");
    }
    static Region region(rt::RomRegion r) {
        switch (r) {
        case rt::RomRegion::Program: return Region::Program;
        case rt::RomRegion::MainData: return Region::MainData;
        case rt::RomRegion::Polygons: return Region::Polygons;
        case rt::RomRegion::Textures: return Region::Textures;
        case rt::RomRegion::CoproData: return Region::CoproData;
        }
        throw std::runtime_error("invalid machine ROM region");
    }
    PackReader pack_;
};
} // namespace rally::ps2
