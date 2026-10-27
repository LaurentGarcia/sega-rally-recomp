#pragma once
#include "contact_result.h"
#include "main_balance_update.h"
#include "rounded_affine_transform.h"
#include "segment_selection.h"
#include "vehicle_parameter_selection.h"
#include <memory>

namespace ps2::native {
inline constexpr std::uint32_t driving_vertex_limit=65536, driving_segment_limit=4096,
    driving_polygon_limit=65535, driving_normal_count=8192, driving_sound_row_limit=4096;
inline constexpr std::size_t driving_header_bytes=96;
inline constexpr std::size_t driving_payload_limit=std::size_t(driving_vertex_limit)*8+
    std::size_t(driving_segment_limit)*24+std::size_t(driving_polygon_limit)*68+8192*8+10*52+4*12+501*4+driving_sound_row_limit*3;
struct DrivingMotionAssets {
    std::array<std::array<std::uint32_t,32>,2> commands{};
    float scalar=0;
    std::array<AffinePoint3,4> local_points{};
};
struct DrivingSoundRow { std::array<std::uint8_t,3> bytes{}; };
enum class DrivingAssetError {
    none, invalid_path, open_failed, read_failed, invalid_header, unsupported_version,
    limit_exceeded, wrong_size, checksum_mismatch, invalid_vertex, invalid_indices,
    invalid_segments, invalid_polygon, invalid_normals, invalid_local_points,
    invalid_vehicle_assets, allocation_failed, invalid_course_direction
};
const char* driving_asset_error_message(DrivingAssetError error) noexcept;

// Owns immutable typed storage and all spans below. Views survive calls to game
// operations without allocation. Successful replacement or destruction expires
// prior views; failed decode/load leaves them intact. Move transfers ownership.
// Explicit recovered immutable assets, not a vehicle-state initializer. Both
// power-curve variants are loaded; their selection remains an explicit caller
// decision. No captured motion, original program or mapped pointers are stored.
class DrivingAssets {
public:
    DrivingAssets() noexcept;
    ~DrivingAssets();
    DrivingAssets(const DrivingAssets&)=delete;
    DrivingAssets& operator=(const DrivingAssets&)=delete;
    DrivingAssets(DrivingAssets&&) noexcept;
    DrivingAssets& operator=(DrivingAssets&&) noexcept;
    bool loaded() const noexcept;
    std::uint32_t selector() const noexcept;
    std::uint32_t payload_crc32() const noexcept;
    std::size_t storage_bytes() const noexcept;
    std::span<const ContactPointXZ> selection_vertices() const noexcept;
    std::span<const CourseSegmentCorners> selection_segments() const noexcept;
    std::span<const ContactPointXZ> course_directions() const noexcept;
    std::span<const ContactSegment> course() const noexcept;
    std::span<const ContactNormalPair> normal_table() const noexcept;
    // Null until loaded; the fixed arrays can be passed directly to recovered
    // parameter-selection and contact-point operations.
    const std::array<VehicleParameterRecord,10>* parameters() const noexcept;
    const std::array<AffinePoint3,4>* local_points() const noexcept;
    const MainBalanceAssets* balance_assets(unsigned power_variant) const noexcept;
    const DrivingMotionAssets* motion_assets() const noexcept;
    std::span<const DrivingSoundRow> sound_commands() const noexcept;
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
    friend bool decode_driving_assets(std::span<const std::uint8_t>,DrivingAssets&,DrivingAssetError&) noexcept;
};

// v3 little-endian wire header: "RLYDRV3\0", version3, header96, selector,
// vertices,segments,polygons,8192,10,4,CRC32,payload_bytes,flags0,sound_rows;
// bytes60..95zero. After the v1 contact subset, append260 balance words,
// two82-float power curves, two32-word motion command lists, motion scalar,
// four secondary localXYZ, sound_rows three-byte command records, then one
// bounded XZ guidance pair per segment. No captured per-frame direction input.
// Payload order: vertexXZ(8B),quad4xu16(8B),segment{first,count}(8B),polygon
// {4XYZ,4coefficients,attributes}(68B),normal{slope,intercept}(8B),10 raw13-word
// parameter records,4 localXYZ. Ranges partition the polygon pool,1..255 each.
// CRC32 is reflectedIEEE. Used XZ/local values are finite <=2^20; normal slopes
// [1/8,1],intercepts[1,4]. PolygonY/coefficients and tuning preserve raw bits;
// each arithmetic consumer retains its own validation. New asset scalars must
// be finite; consumer-specific arithmetic domains remain separately validated.
// No arbitrary offsets, no implied completeness of dynamic input/state data.
bool decode_driving_assets(std::span<const std::uint8_t> bytes,DrivingAssets& output,
                           DrivingAssetError& error) noexcept;
bool load_driving_assets_file(const char* path,DrivingAssets& output,
                             DrivingAssetError& error) noexcept;
} // namespace ps2::native
