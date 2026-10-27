#pragma once
#include "main_vehicle_update.h"
#include <array>
#include <span>
namespace ps2::native {
struct MainVehicleCheckpoint {
    std::uint32_t course_selector=0,asset_payload_crc32=0;
    MainVehicleSetup setup{};
    MainVehicleInputs input{};
    MainVehicleState state{};
};
// One initial developer checkpoint. Not a recovered race initializer or replay
// transport: the driver advances the loaded state with live controls afterward.
// Raw unused float payloads are retained; numerical validity is enforced by the
// game operation when consumed, not inferred by the storage decoder.
inline constexpr std::size_t main_vehicle_checkpoint_word_count=180;
inline constexpr std::size_t main_vehicle_checkpoint_header_bytes=32;
inline constexpr std::size_t main_vehicle_checkpoint_bytes=main_vehicle_checkpoint_header_bytes+4*main_vehicle_checkpoint_word_count;
using MainVehicleCheckpointBytes=std::array<std::uint8_t,main_vehicle_checkpoint_bytes>;
enum class MainVehicleCheckpointError {
    none, invalid_size, invalid_header, checksum_mismatch, invalid_encoding,
    open_failed, read_failed
};
const char* main_vehicle_checkpoint_error_message(MainVehicleCheckpointError error) noexcept;
// Wire format: RLYCAR1\0, LEu32 version1/header32/word_count/payloadCRC32/
// course_selector/asset_payloadCRC32, then canonical LE32 typed values in the
// version1 field order. Float bits are verbatim; signed narrow integers are
// sign-extended, unsigned narrow integers zero-extended, booleans exactly0/1.
// CRC32 uses reflectedIEEE polynomial0xedb88320 with initial/final xor allones.
bool encode_main_vehicle_checkpoint(const MainVehicleCheckpoint& source,
                                   std::span<std::uint8_t> bytes,
                                   MainVehicleCheckpointError& error) noexcept;
bool decode_main_vehicle_checkpoint(std::span<const std::uint8_t> bytes,
                                   MainVehicleCheckpoint& output,
                                   MainVehicleCheckpointError& error) noexcept;
bool load_main_vehicle_checkpoint_file(const char* path,MainVehicleCheckpoint& output,
                                      MainVehicleCheckpointError& error) noexcept;
} // namespace ps2::native
