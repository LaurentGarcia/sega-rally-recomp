// Seekable, bounded cache for asset_pack.py output. Portable C++17 + stdio;
// no graphics SDK, mmap, complete ROM-region allocations or platform macros.
// Host `asset_pack.py verify` authenticates reconstructed bytes against the
// manifest copied into the pack. This reader validates structure and bounds,
// but does not independently authenticate that manifest or data.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace rally::ps2 {

enum class Region : unsigned {
    Program, MainData, TgpTables, CoproData, Polygons, Textures, SoundProgram, SoundSamples
};

class PackReader {
public:
    static constexpr uint32_t page_bytes = 4096;
    static constexpr unsigned region_count = 8;
    using Progress = void (*)(const char *, void *);

    explicit PackReader(const char *path, uint32_t cache_pages = 512,
                        Progress progress = nullptr, void *context = nullptr)
        : file_(std::fopen(path, "rb"), &std::fclose) {
        if (!file_) fail("cannot open asset pack");
        if (cache_pages < 16 || cache_pages > 4096) fail("invalid asset cache size");
        if (progress) progress("PACK FILE OPEN\nSEEKING FILE END", context);
        if (std::fseek(file_.get(), 0, SEEK_END)) fail("cannot seek asset pack");
        const long file_size = std::ftell(file_.get());
        if (file_size < 0 || uint64_t(file_size) > 0x7fffffff) fail("invalid asset pack size");
        file_bytes_ = uint32_t(file_size);
        std::array<uint8_t, 32> header{};
        if (progress) progress("PACK SIZE READ\nREADING HEADER", context);
        read_at(0, header.data(), header.size());
        if (std::memcmp(header.data(), "SRPS2PK1", 8) || u32(&header[8]) != 1 ||
            u32(&header[12]) != page_bytes || u32(&header[16]) != region_count ||
            u32(&header[20]) != 32 || u32(&header[24]) != 544)
            fail("invalid asset pack header");
        payload_ = u32(&header[28]);
        if (payload_ < 544 || payload_ % page_bytes || file_bytes_ < payload_ || file_bytes_ % page_bytes)
            fail("invalid asset pack payload");
        constexpr const char *names[region_count] = {"program", "main_data", "tgp_tables",
            "copro_data", "polygons", "textures", "sound_program", "sound_samples"};
        uint32_t cursor = 544, total_pages = 0;
        if (progress) progress("PACK HEADER VALID\nREADING REGION DIRECTORY", context);
        for (unsigned i = 0; i < region_count; ++i) {
            std::array<uint8_t, 64> entry{};
            read_at(32 + i * 64, entry.data(), entry.size());
            std::array<char, 16> expected{};
            std::memcpy(expected.data(), names[i], std::strlen(names[i]));
            const uint32_t size = u32(&entry[16]), index = u32(&entry[20]), pages = u32(&entry[24]);
            if (std::memcmp(entry.data(), expected.data(), 16) || !size || u32(&entry[28]))
                fail("invalid asset pack region");
            if (index != cursor || pages != (uint64_t(size) + page_bytes - 1) / page_bytes ||
                uint64_t(cursor) + uint64_t(pages) * 4 > payload_)
                fail("invalid asset pack index");
            regions_[i] = {size, total_pages, pages};
            cursor += pages * 4;
            total_pages += pages;
        }
        if ((uint64_t(cursor) + page_bytes - 1) / page_bytes * page_bytes != payload_)
            fail("invalid asset pack index padding");
        // Region page maps are bounded by the fixed known retail image sizes.
        // This also prevents a corrupt header reserving an arbitrarily large
        // heap before its page table can be checked.
        constexpr uint32_t max_pages = 26304; // all eight srallycb mapped regions
        if (total_pages > max_pages) fail("asset pack page map exceeds Rally bounds");
        offsets_.resize(total_pages);
        cache_index_.assign(total_pages, -1);
        std::array<uint8_t, page_bytes> index_block{};
        if (progress) progress("PACK REGIONS VALID\nREADING PAGE INDEX", context);
        for (uint32_t first = 0; first < total_pages; first += page_bytes / 4) {
            const uint32_t count = std::min(page_bytes / 4, total_pages - first);
            read_at(544 + first * 4, index_block.data(), count * 4);
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t offset = u32(&index_block[i * 4]);
                if (offset && (offset < payload_ || offset % page_bytes || uint64_t(offset) + page_bytes > file_bytes_))
                    fail("asset page outside payload");
                offsets_[first + i] = offset;
            }
        }
        if (progress) progress("PACK INDEX VALID\nALLOCATING ROM CACHE", context);
        slots_.resize(cache_pages);
        storage_.resize(size_t(cache_pages) * page_bytes);
    }

    uint32_t size(Region region) const { return metadata(region).size; }

    // A returned pointer remains valid through at least eight subsequent page
    // requests, including cache misses. Cache storage never changes size.
    const uint8_t *page(Region region, uint32_t index) {
        const auto &r = metadata(region);
        if (index >= r.pages) fail("asset page outside region");
        const uint32_t logical = r.first + index, offset = offsets_[logical];
        if (!offset) return zero_.data();
        int32_t cached = cache_index_[logical];
        if (cached < 0) {
            size_t victim = 0;
            for (size_t i = 1; i < slots_.size(); ++i)
                if (slots_[i].used < slots_[victim].used) victim = i;
            auto &slot = slots_[victim];
            // Invalidate before I/O: a failed read must never expose a partly
            // overwritten old page on the next request.
            if (slot.logical != UINT32_MAX) cache_index_[slot.logical] = -1;
            slot.logical = UINT32_MAX;
            read_at(offset, &storage_[victim * page_bytes], page_bytes);
            slot.logical = logical;
            cached = int32_t(victim);
            cache_index_[logical] = cached;
            ++misses_;
        }
        // No special pointer cache exists outside this object. Adapter callers
        // with remembered pointers must invalidate them before page() can evict.
        slots_[size_t(cached)].used = ++clock_;
        return &storage_[size_t(cached) * page_bytes];
    }

    void read(Region region, uint32_t offset, void *output, size_t count) {
        const uint32_t length = size(region);
        if (offset > length || count > uint64_t(length) - offset || (count && !output))
            fail("asset read outside region");
        auto *out = static_cast<uint8_t *>(output);
        while (count) {
            const uint32_t within = offset % page_bytes;
            const size_t n = std::min(count, size_t(page_bytes - within));
            std::memcpy(out, page(region, offset / page_bytes) + within, n);
            out += n;
            offset += uint32_t(n);
            count -= n;
        }
    }

    uint64_t misses() const { return misses_; }
    size_t cache_data_bytes() const { return storage_.size(); }
    size_t index_bytes() const { return offsets_.size() * sizeof(uint32_t) + cache_index_.size() * sizeof(int32_t); }

private:
    struct Metadata { uint32_t size = 0, first = 0, pages = 0; };
    struct Slot { uint64_t used = 0; uint32_t logical = UINT32_MAX, reserved = 0; };
    static_assert(sizeof(Slot) == 16);
    const Metadata &metadata(Region region) const {
        const unsigned index = unsigned(region);
        if (index >= region_count) fail("invalid asset region");
        return regions_[index];
    }
    [[noreturn]] static void fail(const char *message) { throw std::runtime_error(message); }
    static uint32_t u32(const uint8_t *p) {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    void read_at(uint32_t offset, void *output, size_t count) {
        if (offset > file_bytes_ || count > uint64_t(file_bytes_) - offset ||
            std::fseek(file_.get(), long(offset), SEEK_SET) || std::fread(output, 1, count, file_.get()) != count)
            fail("asset pack read failed");
    }
    std::unique_ptr<FILE, int (*)(FILE *)> file_;
    std::array<Metadata, region_count> regions_{};
    std::vector<uint32_t> offsets_;
    std::vector<int32_t> cache_index_;
    std::vector<Slot> slots_;
    std::vector<uint8_t> storage_;
    std::array<uint8_t, page_bytes> zero_{};
    uint32_t file_bytes_ = 0, payload_ = 0;
    uint64_t clock_ = 0, misses_ = 0;
};

} // namespace rally::ps2
