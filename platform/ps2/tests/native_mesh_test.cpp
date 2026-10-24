#include "native_mesh.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Fail each typed-vector allocation independently; the nonempty destination
// must survive every partially constructed replacement.
static int allocation_failure = -1;
void *operator new(std::size_t size) {
    if (allocation_failure == 0) throw std::bad_alloc();
    if (allocation_failure > 0) --allocation_failure;
    if (void *p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace ps2::native;
using Bytes = std::vector<std::uint8_t>;
unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    std::printf("NATIVE MESH FAIL line=%u: %s\n", unsigned(__LINE__), #expr); return false; } } while (0)
void word(Bytes &bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = std::uint8_t(value >> (8 * i));
}
std::uint32_t word(const Bytes &bytes, std::size_t at) {
    return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 |
           std::uint32_t(bytes[at + 2]) << 16 | std::uint32_t(bytes[at + 3]) << 24;
}
void checksum(Bytes &bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 64; i < bytes.size(); ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    word(bytes, 28, crc ^ 0xffffffffu);
}
bool same(const Mesh &a, const Mesh &b) {
    return a.vertices.size() == b.vertices.size() && a.faces.size() == b.faces.size() &&
        a.objects.size() == b.objects.size() && a.payload_crc32 == b.payload_crc32 &&
        !std::memcmp(a.vertices.data(), b.vertices.data(), a.vertices.size() * sizeof(MeshVertex)) &&
        !std::memcmp(a.faces.data(), b.faces.data(), a.faces.size() * sizeof(MeshFace)) &&
        !std::memcmp(a.objects.data(), b.objects.data(), a.objects.size() * sizeof(MeshObject));
}
bool reject(const Bytes &bytes, MeshError expected, Mesh &output, const Mesh &original) {
    const auto *vertices = output.vertices.data();
    const auto *faces = output.faces.data();
    const auto *objects = output.objects.data();
    MeshError error = MeshError::none;
    CHECK(!decode_mesh(bytes, output, error));
    CHECK(error == expected);
    CHECK(same(output, original));
    CHECK(output.vertices.data() == vertices && output.faces.data() == faces && output.objects.data() == objects);
    return true;
}
bool write_file(const char *path, const Bytes &bytes) {
    auto *file = std::fopen(path, "wb");
    CHECK(file);
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    const int closed = std::fclose(file);
    CHECK(written == bytes.size() && closed == 0);
    return true;
}
Bytes maximum(std::uint32_t vertices, std::uint32_t faces, std::uint32_t objects) {
    Bytes bytes(64 + vertices * 12 + faces * 16 + objects * 80);
    std::memcpy(bytes.data(), "RLYMESH", 8);
    const std::uint32_t header[] = {1, 64, vertices, faces, objects, 0,
        std::uint32_t(bytes.size() - 64), 12, 16, 80};
    for (unsigned i = 0; i < 10; ++i) word(bytes, 8 + i * 4, header[i]);
    std::uint32_t first_vertex = 0, first_face = 0;
    for (std::uint32_t i = 0; i < objects; ++i) {
        const auto vertex_count = i + 1 == objects ? vertices - first_vertex : 3;
        const auto face_count = i + 1 == objects ? faces - first_face : 1;
        const std::size_t at = 64 + vertices * 12 + faces * 16 + i * 80;
        word(bytes, at, first_vertex); word(bytes, at + 4, vertex_count);
        word(bytes, at + 8, first_face); word(bytes, at + 12, face_count);
        for (unsigned diagonal = 0; diagonal < 4; ++diagonal) word(bytes, at + 16 + diagonal * 20, 0x3f800000u);
        for (std::uint32_t f = first_face; f < first_face + face_count; ++f) {
            const std::size_t face_at = 64 + vertices * 12 + f * 16;
            word(bytes, face_at, first_vertex); word(bytes, face_at + 4, first_vertex + 1);
            word(bytes, face_at + 8, first_vertex + 2); word(bytes, face_at + 12, 0xff123456u);
        }
        first_vertex += vertex_count; first_face += face_count;
    }
    checksum(bytes);
    return bytes;
}
bool run(const char *fixture_path, const char *scratch_path) {
    // Python struct/zlib creates this fixture independently of our C++ writer.
    Bytes good(384);
    auto *file = std::fopen(fixture_path, "rb");
    CHECK(file);
    const auto read = std::fread(good.data(), 1, good.size(), file);
    const int trailing = std::fgetc(file), closed = std::fclose(file);
    CHECK(read == good.size() && trailing == EOF && closed == 0);
    Mesh output;
    MeshError error = MeshError::invalid_header;
    CHECK(decode_mesh(good, output, error) && error == MeshError::none);
    CHECK(output.vertices.size() == 8 && output.faces.size() == 4 && output.objects.size() == 2);
    CHECK(output.vertices[0].x == -1 && output.vertices[6].y == 2);
    CHECK(output.faces[2].indices[0] == 4 && output.faces[0].argb == 0xffff0000u);
    CHECK(output.objects[1].object_to_reference_view[12] == 10);
    CHECK(output.objects[1].object_to_reference_view[13] == 20);
    CHECK(output.objects[1].object_to_reference_view[14] == 30);
    CHECK(output.payload_crc32 == word(good, 28));
    const Mesh original = output;

    // Successful replacement of a nonempty mesh is observable, then restored.
    Bytes changed = good; word(changed, 64, std::bit_cast<std::uint32_t>(-3.f)); checksum(changed);
    CHECK(decode_mesh(changed, output, error) && output.vertices[0].x == -3);
    CHECK(load_mesh_file(fixture_path, output, error) && same(output, original));

    for (std::size_t length = 0; length < good.size(); ++length) {
        Bytes bytes(good.begin(), good.begin() + length);
        CHECK(reject(bytes, MeshError::wrong_size, output, original));
    }
    Bytes bytes = good; bytes.push_back(0);
    CHECK(reject(bytes, MeshError::wrong_size, output, original));
    for (unsigned at = 64; at < good.size(); ++at) {
        bytes = good; bytes[at] ^= 1;
        CHECK(reject(bytes, MeshError::checksum_mismatch, output, original));
    }
    bytes = good; bytes[0] ^= 1; CHECK(reject(bytes, MeshError::invalid_magic, output, original));
    bytes = good; word(bytes, 8, 2); CHECK(reject(bytes, MeshError::unsupported_version, output, original));
    for (unsigned at : {12u, 32u, 36u, 40u, 44u, 48u, 52u, 56u, 60u}) {
        bytes = good; word(bytes, at, word(bytes, at) + 1);
        CHECK(reject(bytes, MeshError::invalid_header, output, original));
    }
    for (unsigned at : {16u, 20u, 24u}) {
        bytes = good; word(bytes, at, 0); CHECK(reject(bytes, MeshError::invalid_header, output, original));
        word(bytes, at, 0xffffffffu); CHECK(reject(bytes, MeshError::limit_exceeded, output, original));
    }
    for (auto value : {0x7f800000u, 0xff800000u, 0x7fc00001u, 0x7f800001u,
                       0x00000001u, 0x807fffffu, 0x47800001u, 0xc7800001u}) {
        for (unsigned component = 0; component < 3; ++component) {
            bytes = good; word(bytes, 64 + 7 * 12 + component * 4, value); checksum(bytes);
            CHECK(reject(bytes, MeshError::invalid_vertex, output, original));
        }
        for (unsigned component = 0; component < 16; ++component) {
            bytes = good; word(bytes, 224 + 80 + 16 + component * 4, value); checksum(bytes);
            CHECK(reject(bytes, MeshError::invalid_matrix, output, original));
        }
    }
    // Global range checks, repeated indices, opacity, and object isolation.
    for (auto index : {8u, 0xffffffffu, 1u, 4u}) {
        bytes = good; word(bytes, 160, index); checksum(bytes);
        CHECK(reject(bytes, MeshError::invalid_face, output, original));
    }
    bytes = good; word(bytes, 172, 0x7fff0000u); checksum(bytes);
    CHECK(reject(bytes, MeshError::invalid_face, output, original));
    for (unsigned field = 0; field < 4; ++field) for (auto value : {0u, 0xffffffffu}) {
        bytes = good; word(bytes, 304 + field * 4, value); checksum(bytes);
        CHECK(reject(bytes, MeshError::invalid_object, output, original));
    }
    for (unsigned field : {3u, 7u, 11u, 15u}) {
        bytes = good; word(bytes, 240 + field * 4, field == 15 ? 0u : 0x3f800000u); checksum(bytes);
        CHECK(reject(bytes, MeshError::invalid_matrix, output, original));
    }
    // Orphaned vertices and faces after otherwise legal contiguous ranges.
    bytes = good; word(bytes, 308, 3); checksum(bytes);
    // Last face would reference the orphan: use only the remaining three first.
    word(bytes, 208, 4); word(bytes, 212, 5); word(bytes, 216, 6); checksum(bytes);
    CHECK(reject(bytes, MeshError::invalid_object, output, original));
    bytes = good; word(bytes, 316, 1); checksum(bytes);
    CHECK(reject(bytes, MeshError::invalid_object, output, original));

    for (int failure = 0; failure < 3; ++failure) {
        allocation_failure = failure;
        const bool loaded = decode_mesh(good, output, error);
        allocation_failure = -1;
        CHECK(!loaded && error == MeshError::allocation_failed && same(output, original));
    }
    // Malformed headers never reach allocation, even when the next allocation
    // is forced to fail; checksum failures likewise precede typed storage.
    bytes = good; word(bytes, 16, 0xffffffffu);
    allocation_failure = 0; const bool oversized = decode_mesh(bytes, output, error); allocation_failure = -1;
    CHECK(!oversized && error == MeshError::limit_exceeded && same(output, original));
    bytes = good; bytes[64] ^= 1;
    allocation_failure = 0; const bool corrupt = decode_mesh(bytes, output, error); allocation_failure = -1;
    CHECK(!corrupt && error == MeshError::checksum_mismatch && same(output, original));

    CHECK(!load_mesh_file(nullptr, output, error) && error == MeshError::invalid_path && same(output, original));
    CHECK(!load_mesh_file("", output, error) && error == MeshError::invalid_path && same(output, original));
    CHECK(!load_mesh_file(scratch_path, output, error) && error == MeshError::open_failed && same(output, original));
    for (unsigned kind = 0; kind < 4; ++kind) {
        bytes = good;
        if (kind == 0) bytes.pop_back();
        if (kind == 1) bytes.push_back(0);
        if (kind == 2) bytes[64] ^= 1;
        if (kind == 3) bytes[0] ^= 1;
        CHECK(write_file(scratch_path, bytes));
        CHECK(!load_mesh_file(scratch_path, output, error) && same(output, original));
        CHECK(error == (kind < 2 ? MeshError::wrong_size : kind == 2 ? MeshError::checksum_mismatch : MeshError::invalid_magic));
    }
    CHECK(write_file(scratch_path, good));
    for (int failure = 0; failure < 4; ++failure) {
        allocation_failure = failure;
        const bool loaded = load_mesh_file(scratch_path, output, error);
        allocation_failure = -1;
        CHECK(!loaded && error == MeshError::allocation_failed && same(output, original));
    }
    CHECK(load_mesh_file(scratch_path, output, error) && same(output, original));

    // Each individual count maximum and the simultaneous maximum are valid.
    for (const auto counts : {std::array<std::uint32_t, 3>{65536, 1, 1}, {3, 32768, 1},
                              {3072, 1024, 1024}, {65536, 32768, 1024}}) {
        bytes = maximum(counts[0], counts[1], counts[2]);
        CHECK(decode_mesh(bytes, output, error));
        CHECK(output.vertices.size() == counts[0] && output.faces.size() == counts[1] && output.objects.size() == counts[2]);
        CHECK(bytes.size() <= mesh_file_limit);
    }
    CHECK(bytes.size() == mesh_file_limit);
    CHECK(load_mesh_file(fixture_path, output, error) && same(output, original));
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const bool passed = run(argv[1], argv[2]);
    std::printf("NATIVE MESH %s checks=%u\n", passed ? "PASS" : "FAIL", checks);
    return passed ? 0 : 1;
}
