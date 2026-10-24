#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ps2::native {
inline constexpr std::uint32_t mesh_vertex_limit = 65536;
inline constexpr std::uint32_t mesh_face_limit = 32768;
inline constexpr std::uint32_t mesh_object_limit = 1024;
inline constexpr std::size_t mesh_header_bytes = 64;
inline constexpr std::size_t mesh_payload_limit = 1392640;
inline constexpr std::size_t mesh_file_limit = mesh_header_bytes + mesh_payload_limit;

struct MeshVertex { float x = 0, y = 0, z = 0; };
struct MeshFace {
    std::array<std::uint32_t, 3> indices{}; // Global indices, within owning object.
    std::uint32_t argb = 0xff808080u;      // Opaque diagnostic color, not original shading.
};
struct MeshObject {
    std::uint32_t first_vertex = 0, vertex_count = 0, first_face = 0, face_count = 0;
    // Column-major affine transform. Captured object -> reference VIEW space;
    // this is not a recovered world/camera convention or a native camera.
    std::array<float, 16> object_to_reference_view{};
};
struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<MeshFace> faces;
    std::vector<MeshObject> objects;
    std::uint32_t payload_crc32 = 0;
};
enum class MeshError {
    none,
    invalid_path,
    open_failed,
    read_failed,
    allocation_failed,
    invalid_magic,
    unsupported_version,
    invalid_header,
    limit_exceeded,
    wrong_size,
    checksum_mismatch,
    invalid_vertex,
    invalid_face,
    invalid_object,
    invalid_matrix,
};
const char *mesh_error_message(MeshError error) noexcept;

// Native mesh v1 wire contract, all integers/floats little endian:
// header[0..7]="RLYMESH\0"; u32 fields at offsets8..44 are version1,
// header_bytes64, vertex_count, face_count, object_count, payload_crc32,
// payload_bytes, vertex_stride12, face_stride16, object_stride80. Bytes48..63
// are zero. Payload: XYZ binary32 vertices; three u32 indices + u32 ARGB per
// face; four u32 ranges + sixteen binary32 matrix elements per object.
// CRC32 is IEEE (reflected0xEDB88320, initial/final xor0xffffffff), payload only.
//
// Nonempty object ranges partition both pools contiguously in file order.
// Every object has >=3 vertices and >=1 face; face indices are distinct and
// remain in that object's range. Colors must be opaque. All floats are normal
// binary32 or signed zero, magnitude <=65536. Matrix last row is exactly
// [0,0,0,1]. No game addressing, texture or runtime device types are present.
//
// Both functions leave output unchanged on every failure. decode_mesh checks
// size/checksum before allocating typed storage. load_mesh_file additionally
// stages at most mesh_file_limit bytes, accepts ordinary host:/mass:/ paths,
// and rejects any trailing byte. New raw + typed storage stays below2.7MiB;
// an existing output mesh remains alive until replacement succeeds.
bool decode_mesh(std::span<const std::uint8_t> bytes, Mesh &output,
                 MeshError &error) noexcept;
bool load_mesh_file(const char *path, Mesh &output, MeshError &error) noexcept;
} // namespace ps2::native
