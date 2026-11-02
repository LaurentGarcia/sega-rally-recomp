#pragma once
#include "world_scene.h"
#include <memory>
namespace ps2::native {
// Native material asset only: model-local XYZ, per-corner texel UV, immutable
// PSMT4 indices, final ARGB CLUTs and optional authored facing normals/points.
// Version1 remains readable without facing; version2 preserves this live-view data. No source addresses, view matrices, board
// types or executable instructions are accepted by this format.
enum class TexturedModelError {none,io,format,limit,checksum,geometry,material,allocation};
class TexturedModel {
public:
    TexturedModel() noexcept;~TexturedModel();
    TexturedModel(const TexturedModel&)=delete;
    TexturedModel&operator=(const TexturedModel&)=delete;
    bool valid()const noexcept;
    WorldModel model()const noexcept;
    std::size_t persistent_bytes()const noexcept;
private:
    struct Storage;std::unique_ptr<Storage> storage_;
    friend bool decode_textured_model(std::span<const std::uint8_t>,TexturedModel&,TexturedModelError&)noexcept;
};
// Failure preserves the complete previous owner/view. Success replaces its
// storage: callers must retire borrowed frames before replacing it. Prepared
// models own copies. Maximum65536vertices,32768faces,4096materials,1024textures
// and4MiB packed indices. New texture identities are assigned on each load.
bool decode_textured_model(std::span<const std::uint8_t>,TexturedModel&,TexturedModelError&)noexcept;
bool load_textured_model(const char*,TexturedModel&,TexturedModelError&)noexcept;
}
