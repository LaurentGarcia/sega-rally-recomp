#include "gs_scene.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#ifndef _EE
#include <filesystem>
#endif

namespace ps2 {
namespace {
constexpr std::size_t kHeader = 80, kPixels = 496 * 384, kTextureWords = 0x80000;
constexpr std::size_t kPalette = 0x4000, kTranslation = 0xc000, kLuma = 0x20000;
constexpr std::size_t kFixedPayload = kPalette + kTranslation + kLuma + 2 * kTextureWords * 4 + 3 * kPixels * 4;
constexpr std::size_t kMaxFile = 16 * 1024 * 1024, kMaxVertices = 65535, kMaxBatches = 4096;
constexpr std::uint64_t kHashSeed = 14695981039346656037ull;
using Vertex = app::Model2Frame::PolyVertex;
using Batch = app::Model2Frame::Batch;
// The wire records contain no pointers or padding. Reject a compiler/target
// ABI that cannot read the fixed LE32 records directly.
static_assert(std::endian::native == std::endian::little && sizeof(float) == 4 && sizeof(int) == 4);
static_assert(std::is_trivially_copyable_v<Vertex> && sizeof(Vertex) == 72);
static_assert(offsetof(Vertex, x) == 0 && offsetof(Vertex, info) == 40 &&
              offsetof(Vertex, more) == 56 && offsetof(Vertex, edges) == 64);
static_assert(std::is_trivially_copyable_v<Batch> && sizeof(Batch) == 24 && offsetof(Batch, clip) == 8);

std::uint64_t hash_bytes(const void *data, std::size_t bytes, std::uint64_t hash = kHashSeed) {
    const auto *p = static_cast<const std::uint8_t *>(data);
    for (std::size_t i = 0; i < bytes; ++i) hash = (hash ^ p[i]) * 1099511628211ull;
    return hash;
}
std::uint64_t get(const std::uint8_t *p, unsigned bytes) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= std::uint64_t(p[i]) << (i * 8);
    return value;
}
#ifndef _EE
void put(std::uint8_t *p, std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) p[i] = std::uint8_t(value >> (i * 8));
}
#endif
using File = std::unique_ptr<FILE, int (*)(FILE *)>;
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Scene, class Fn> void sections(Scene &s, Fn visit) {
    visit(s.vertices); visit(s.batches); visit(s.palette); visit(s.translation); visit(s.luma);
    visit(s.texture0); visit(s.texture1); visit(s.background); visit(s.foreground); visit(s.reference);
}
#ifndef _EE
bool valid_sizes(const GsScene &s) {
    return s.vertices.size() <= kMaxVertices && s.vertices.size() % 3 == 0 && s.batches.size() <= kMaxBatches &&
        s.palette.size() == kPalette && s.translation.size() == kTranslation && s.luma.size() == kLuma &&
        s.texture0.size() == kTextureWords && s.texture1.size() == kTextureWords &&
        s.background.size() == kPixels && s.foreground.size() == kPixels && s.reference.size() == kPixels;
}
#endif
std::size_t payload_size(std::size_t vertices, std::size_t batches) {
    return kFixedPayload + vertices * sizeof(Vertex) + batches * sizeof(Batch);
}
void valid_batches(const GsScene &s) {
    std::size_t next = 0;
    for (const auto &b : s.batches) {
        require(b.first == next && b.count % 3 == 0 && b.count <= s.vertices.size() - next,
                "invalid scene triangle batch");
        next += b.count;
    }
    require(next == s.vertices.size(), "scene vertices are not completely batched");
}
} // namespace

rt::VideoMem GsScene::memory() const {
    return {palette.data(), translation.data(), luma.data(), texture0.data(), texture1.data(), texture_generation};
}

bool load_gs_scene(const char *path, GsScene &destination, std::string &error) {
    try {
        require(path && *path, "missing scene path");
        File file(std::fopen(path, "rb"), std::fclose);
        require(bool(file), "cannot open scene");
        std::array<std::uint8_t, kHeader> h{};
        require(std::fread(h.data(), 1, h.size(), file.get()) == h.size(), "short scene header");
        require(std::memcmp(h.data(), "SRPS2SC1", 8) == 0 && get(h.data() + 8, 4) == 1 &&
                get(h.data() + 12, 4) == kHeader, "unsupported scene format");
        require(get(h.data() + 16, 4) == 496 && get(h.data() + 20, 4) == 384 &&
                get(h.data() + 36, 4) == 0 && get(h.data() + 64, 8) == 0, "invalid scene dimensions or flags");
        require(hash_bytes(h.data(), 72) == get(h.data() + 72, 8), "scene header checksum mismatch");
        const auto vertices = get(h.data() + 28, 4), batches = get(h.data() + 32, 4);
        require(vertices <= kMaxVertices && vertices % 3 == 0 && batches <= kMaxBatches,
                "scene counts exceed bounded capacities");
        const auto payload = payload_size(std::size_t(vertices), std::size_t(batches));
        require(payload + kHeader <= kMaxFile && get(h.data() + 48, 8) == payload, "invalid scene payload size");
        // Validate actual file length before allocating up to the bounded size.
        require(std::fseek(file.get(), 0, SEEK_END) == 0, "scene transport is not seekable");
        const long file_bytes = std::ftell(file.get());
        require(file_bytes >= 0 && std::uint64_t(file_bytes) == payload + kHeader, "scene file length mismatch");
        require(std::fseek(file.get(), long(kHeader), SEEK_SET) == 0, "cannot seek scene payload");
        GsScene s;
        s.frame = std::uint32_t(get(h.data() + 24, 4));
        s.texture_generation = get(h.data() + 40, 8);
        s.vertices.resize(std::size_t(vertices)); s.batches.resize(std::size_t(batches));
        s.palette.resize(kPalette); s.translation.resize(kTranslation); s.luma.resize(kLuma);
        s.texture0.resize(kTextureWords); s.texture1.resize(kTextureWords);
        s.background.resize(kPixels); s.foreground.resize(kPixels); s.reference.resize(kPixels);
        std::uint64_t checksum = kHashSeed;
        sections(s, [&](auto &values) {
            auto *p = reinterpret_cast<std::uint8_t *>(values.data());
            auto remaining = values.size() * sizeof(values[0]);
            while (remaining) {
                const auto bytes = std::min<std::size_t>(remaining, 16384);
                require(std::fread(p, 1, bytes, file.get()) == bytes, "short scene payload");
                checksum = hash_bytes(p, bytes, checksum);
                p += bytes; remaining -= bytes;
            }
        });
        require(checksum == get(h.data() + 56, 8), "scene payload checksum mismatch");
        valid_batches(s);
        destination = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception &failure) {
        error = failure.what();
        return false;
    }
}

#ifndef _EE
bool save_gs_scene(const char *path, const GsScene &scene, std::string &error) {
    std::filesystem::path temporary;
    try {
        require(path && *path, "missing scene output path");
        require(valid_sizes(scene), "invalid fixed scene buffer sizes");
        valid_batches(scene);
        // Capture runs from this checkout's root. Canonicalize existing parents
        // before writing so a symlink cannot escape the private build tree.
        const auto root = std::filesystem::current_path();
        require(std::filesystem::exists(root / "rules.md") && std::filesystem::exists(root / "platform/ps2"),
                "scene capture requires the repository working directory");
        const auto output = std::filesystem::weakly_canonical(path);
        const auto build = std::filesystem::weakly_canonical(root / "build");
        const auto relative = output.lexically_relative(build);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".." && output.extension() == ".bin",
                "scene output must be a .bin below ignored build/");
        std::filesystem::create_directories(output.parent_path());
        std::array<std::uint8_t, kHeader> h{};
        std::memcpy(h.data(), "SRPS2SC1", 8);
        put(h.data() + 8, 1, 4); put(h.data() + 12, kHeader, 4);
        put(h.data() + 16, 496, 4); put(h.data() + 20, 384, 4);
        put(h.data() + 24, scene.frame, 4); put(h.data() + 28, scene.vertices.size(), 4);
        put(h.data() + 32, scene.batches.size(), 4); put(h.data() + 40, scene.texture_generation, 8);
        put(h.data() + 48, payload_size(scene.vertices.size(), scene.batches.size()), 8);
        std::uint64_t checksum = kHashSeed;
        sections(scene, [&](const auto &values) { checksum = hash_bytes(values.data(), values.size() * sizeof(values[0]), checksum); });
        put(h.data() + 56, checksum, 8); put(h.data() + 72, hash_bytes(h.data(), 72), 8);
        temporary = output;
        temporary += ".tmp";
        File file(std::fopen(temporary.c_str(), "wb"), std::fclose);
        require(bool(file), "cannot create scene output");
        require(std::fwrite(h.data(), 1, h.size(), file.get()) == h.size(), "cannot write scene header");
        sections(scene, [&](const auto &values) {
            const auto bytes = values.size() * sizeof(values[0]);
            require(std::fwrite(values.data(), 1, bytes, file.get()) == bytes, "cannot write scene payload");
        });
        require(std::fclose(file.release()) == 0, "cannot close scene output");
        require(std::rename(temporary.c_str(), output.c_str()) == 0, "cannot publish private scene output");
        temporary.clear();
        error.clear();
        return true;
    } catch (const std::exception &failure) {
        if (!temporary.empty()) std::remove(temporary.c_str());
        error = failure.what();
        return false;
    }
}
#endif
} // namespace ps2
