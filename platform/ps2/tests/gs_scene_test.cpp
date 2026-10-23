#include "gs_scene.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
std::uint64_t hash(const unsigned char *bytes, std::size_t size) {
    std::uint64_t value = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) value = (value ^ bytes[i]) * 1099511628211ull;
    return value;
}
void put(unsigned char *bytes, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes[i] = std::uint8_t(value >> (8 * i));
}
template<class T> bool equal(const std::vector<T> &a, const std::vector<T> &b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0;
}
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        const char *path = argv[1];
        ps2::GsScene scene;
        scene.frame = 1634; scene.texture_generation = 0x12345678abcdef00ull;
        scene.vertices.resize(3); scene.batches = {{0, 3, {0, 0, 496, 384}}};
        scene.vertices[0].x = -0.05f; scene.vertices[1].y = 383.9375f;
        scene.vertices[2].info[3] = 0x5480;
        scene.palette.resize(0x4000); scene.translation.resize(0xc000); scene.luma.resize(0x20000);
        scene.texture0.resize(0x80000); scene.texture1.resize(0x80000);
        scene.background.resize(496 * 384); scene.foreground.resize(496 * 384); scene.reference.resize(496 * 384);
        scene.palette[17] = 0xa7; scene.translation[0xbfff] = 31; scene.luma[0x1ffff] = 63;
        scene.texture0[0x7ffff] = 0x1234abcd; scene.texture1[0x78654] = 0xfedc4321;
        scene.background[19] = 0xff123456; scene.foreground[23] = 0xff654321; scene.reference[29] = 0xff999aaa;
        std::string error;
        check(ps2::save_gs_scene(path, scene, error), error.c_str());
        ps2::GsScene loaded;
        check(ps2::load_gs_scene(path, loaded, error), error.c_str());
        check(loaded.frame == scene.frame && loaded.texture_generation == scene.texture_generation &&
            equal(loaded.vertices, scene.vertices) && equal(loaded.batches, scene.batches) &&
            equal(loaded.palette, scene.palette) && equal(loaded.translation, scene.translation) &&
            equal(loaded.luma, scene.luma) && equal(loaded.texture0, scene.texture0) &&
            equal(loaded.texture1, scene.texture1) && equal(loaded.background, scene.background) &&
            equal(loaded.foreground, scene.foreground) && equal(loaded.reference, scene.reference), "round-trip differs");
        check(loaded.memory().tex0 == loaded.texture0.data() && loaded.memory().tex_generation == scene.texture_generation,
              "video pointers are not tied to reader-owned memory");
        check(!ps2::save_gs_scene("platform/ps2/forbidden-scene.bin", scene, error), "capture escaped build");
        check(!ps2::save_gs_scene("build/forbidden-scene.txt", scene, error), "capture used an exportable suffix");
        scene.foreground.clear();
        check(!ps2::save_gs_scene(path, scene, error), "short foreground accepted");
        check(ps2::load_gs_scene(path, loaded, error), "invalid save replaced previous file");
        std::array<unsigned char, 80> original{}, header{};
        { std::ifstream file(path, std::ios::binary); file.read(reinterpret_cast<char *>(original.data()), original.size()); }
        auto change_header = [&] {
            put(header.data() + 72, hash(header.data(), 72), 8);
            std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
            file.write(reinterpret_cast<const char *>(header.data()), header.size());
        };
        header = original; put(header.data() + 28, 0xffffffffu, 4); change_header();
        check(!ps2::load_gs_scene(path, loaded, error) && error == "scene counts exceed bounded capacities" &&
              loaded.frame == 1634, "oversized count allocated or replaced destination");
        header = original; put(header.data() + 48, 0xffffffffu, 8); change_header();
        check(!ps2::load_gs_scene(path, loaded, error) && error == "invalid scene payload size", "oversized payload accepted");
        header = original; header[56] ^= 1; change_header();
        check(!ps2::load_gs_scene(path, loaded, error) && error == "scene payload checksum mismatch" &&
              loaded.reference[29] == 0xff999aaa, "corruption replaced destination");
        header = original; change_header();
        { std::ofstream file(path, std::ios::binary | std::ios::app); file.put('x'); }
        check(!ps2::load_gs_scene(path, loaded, error) && error == "scene file length mismatch", "trailing data accepted");
        { std::ofstream file(path, std::ios::binary | std::ios::trunc); file.write(reinterpret_cast<const char *>(original.data()), 79); }
        check(!ps2::load_gs_scene(path, loaded, error) && error == "short scene header", "truncation accepted");
        std::puts("PS2 scene round-trip, bounds, checksums, atomicity and private output checks passed");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "PS2 scene tests: %s\n", error.what());
        return 1;
    }
}
