#include "native_mesh.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace ps2::native {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::radix == 2 &&
              std::numeric_limits<float>::digits == 24);
static_assert(sizeof(MeshVertex) == 12 && sizeof(MeshFace) == 16 && sizeof(MeshObject) == 80);
static_assert(mesh_payload_limit == std::size_t(mesh_vertex_limit) * 12 +
              std::size_t(mesh_face_limit) * 16 + std::size_t(mesh_object_limit) * 80);
struct Header { std::uint32_t vertices, faces, objects, crc, payload; };
struct File {
    std::FILE *handle = nullptr;
    ~File() { if (handle) std::fclose(handle); }
};
std::uint32_t word(const std::uint8_t *p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 |
           std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
float scalar(const std::uint8_t *p) { return std::bit_cast<float>(word(p)); }
bool valid_scalar(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits <= 0x47800000u);
}
bool fail(MeshError value, MeshError &error) { error = value; return false; }
bool header(std::span<const std::uint8_t> bytes, Header &result, MeshError &error) {
    if (bytes.size() < mesh_header_bytes) return fail(MeshError::wrong_size, error);
    constexpr char magic[8] = {'R','L','Y','M','E','S','H','\0'};
    if (std::memcmp(bytes.data(), magic, sizeof magic)) return fail(MeshError::invalid_magic, error);
    if (word(bytes.data() + 8) != 1) return fail(MeshError::unsupported_version, error);
    if (word(bytes.data() + 12) != mesh_header_bytes || word(bytes.data() + 36) != 12 ||
        word(bytes.data() + 40) != 16 || word(bytes.data() + 44) != 80)
        return fail(MeshError::invalid_header, error);
    for (unsigned i = 48; i < mesh_header_bytes; ++i)
        if (bytes[i]) return fail(MeshError::invalid_header, error);
    result = {word(bytes.data() + 16), word(bytes.data() + 20), word(bytes.data() + 24),
              word(bytes.data() + 28), word(bytes.data() + 32)};
    if (!result.vertices || !result.faces || !result.objects) return fail(MeshError::invalid_header, error);
    if (result.vertices > mesh_vertex_limit || result.faces > mesh_face_limit || result.objects > mesh_object_limit)
        return fail(MeshError::limit_exceeded, error);
    // Count limits are enforced before products; this arithmetic fits32-bit
    // size_t on EE as well as the host. No file-supplied offsets are trusted.
    const std::size_t expected = std::size_t(result.vertices) * 12 +
        std::size_t(result.faces) * 16 + std::size_t(result.objects) * 80;
    if (result.payload != expected) return fail(MeshError::invalid_header, error);
    return true;
}
std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = 0xffffffffu;
    for (auto byte : bytes) {
        value ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
    }
    return value ^ 0xffffffffu;
}
} // namespace

const char *mesh_error_message(MeshError error) noexcept {
    switch (error) {
    case MeshError::none: return "NONE";
    case MeshError::invalid_path: return "MESH PATH IS EMPTY";
    case MeshError::open_failed: return "MESH FILE OPEN FAILED";
    case MeshError::read_failed: return "MESH FILE READ FAILED";
    case MeshError::allocation_failed: return "MESH ALLOCATION FAILED";
    case MeshError::invalid_magic: return "INVALID MESH MAGIC";
    case MeshError::unsupported_version: return "UNSUPPORTED MESH VERSION";
    case MeshError::invalid_header: return "INVALID MESH HEADER";
    case MeshError::limit_exceeded: return "MESH COUNT LIMIT EXCEEDED";
    case MeshError::wrong_size: return "MESH FILE SIZE MISMATCH";
    case MeshError::checksum_mismatch: return "MESH CHECKSUM MISMATCH";
    case MeshError::invalid_vertex: return "INVALID MESH VERTEX";
    case MeshError::invalid_face: return "INVALID MESH FACE";
    case MeshError::invalid_object: return "INVALID MESH OBJECT RANGES";
    case MeshError::invalid_matrix: return "INVALID MESH REFERENCE MATRIX";
    }
    return "UNKNOWN MESH ERROR";
}

bool decode_mesh(std::span<const std::uint8_t> bytes, Mesh &output, MeshError &error) noexcept {
    Header h{};
    if (!header(bytes, h, error)) return false;
    if (bytes.size() != mesh_header_bytes + h.payload) return fail(MeshError::wrong_size, error);
    if (crc32(bytes.subspan(mesh_header_bytes)) != h.crc) return fail(MeshError::checksum_mismatch, error);
    try {
        Mesh staged;
        staged.vertices.resize(h.vertices); staged.faces.resize(h.faces); staged.objects.resize(h.objects);
        std::size_t offset = mesh_header_bytes;
        for (auto &v : staged.vertices) {
            v = {scalar(bytes.data() + offset), scalar(bytes.data() + offset + 4), scalar(bytes.data() + offset + 8)};
            if (!valid_scalar(v.x) || !valid_scalar(v.y) || !valid_scalar(v.z))
                return fail(MeshError::invalid_vertex, error);
            offset += 12;
        }
        for (auto &face : staged.faces) {
            face = {{word(bytes.data() + offset), word(bytes.data() + offset + 4), word(bytes.data() + offset + 8)},
                    word(bytes.data() + offset + 12)};
            const auto &i = face.indices;
            if (i[0] >= h.vertices || i[1] >= h.vertices || i[2] >= h.vertices ||
                i[0] == i[1] || i[1] == i[2] || i[2] == i[0] || (face.argb >> 24) != 255)
                return fail(MeshError::invalid_face, error);
            offset += 16;
        }
        std::uint32_t next_vertex = 0, next_face = 0;
        for (auto &object : staged.objects) {
            object.first_vertex = word(bytes.data() + offset);
            object.vertex_count = word(bytes.data() + offset + 4);
            object.first_face = word(bytes.data() + offset + 8);
            object.face_count = word(bytes.data() + offset + 12);
            if (object.first_vertex != next_vertex || object.first_face != next_face ||
                object.vertex_count < 3 || object.face_count < 1 ||
                object.vertex_count > h.vertices - next_vertex || object.face_count > h.faces - next_face)
                return fail(MeshError::invalid_object, error);
            for (unsigned i = 0; i < object.object_to_reference_view.size(); ++i) {
                const float value = scalar(bytes.data() + offset + 16 + i * 4);
                if (!valid_scalar(value)) return fail(MeshError::invalid_matrix, error);
                object.object_to_reference_view[i] = value;
            }
            const auto &m = object.object_to_reference_view;
            if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
                return fail(MeshError::invalid_matrix, error);
            next_vertex += object.vertex_count; next_face += object.face_count;
            for (std::uint32_t i = object.first_face; i < next_face; ++i)
                for (const auto index : staged.faces[i].indices)
                    if (index < object.first_vertex || index >= next_vertex) return fail(MeshError::invalid_face, error);
            offset += 80;
        }
        if (next_vertex != h.vertices || next_face != h.faces) return fail(MeshError::invalid_object, error);
        staged.payload_crc32 = h.crc;
        output = std::move(staged);
        error = MeshError::none;
        return true;
    } catch (const std::bad_alloc &) {
        return fail(MeshError::allocation_failed, error);
    } catch (const std::length_error &) {
        return fail(MeshError::allocation_failed, error);
    }
}

bool load_mesh_file(const char *path, Mesh &output, MeshError &error) noexcept {
    if (!path || !*path) return fail(MeshError::invalid_path, error);
    File file{std::fopen(path, "rb")};
    if (!file.handle) return fail(MeshError::open_failed, error);
    std::array<std::uint8_t, mesh_header_bytes> prefix{};
    if (std::fread(prefix.data(), 1, prefix.size(), file.handle) != prefix.size())
        return fail(std::ferror(file.handle) ? MeshError::read_failed : MeshError::wrong_size, error);
    Header h{};
    if (!header(prefix, h, error)) return false;
    try {
        std::vector<std::uint8_t> bytes(mesh_header_bytes + h.payload);
        std::memcpy(bytes.data(), prefix.data(), prefix.size());
        if (std::fread(bytes.data() + mesh_header_bytes, 1, h.payload, file.handle) != h.payload)
            return fail(std::ferror(file.handle) ? MeshError::read_failed : MeshError::wrong_size, error);
        if (std::fgetc(file.handle) != EOF) return fail(MeshError::wrong_size, error);
        if (std::ferror(file.handle)) return fail(MeshError::read_failed, error);
        return decode_mesh(bytes, output, error);
    } catch (const std::bad_alloc &) {
        return fail(MeshError::allocation_failed, error);
    } catch (const std::length_error &) {
        return fail(MeshError::allocation_failed, error);
    }
}
} // namespace ps2::native
