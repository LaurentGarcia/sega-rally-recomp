#pragma once
// Host-only model asset decoding. No original bytes are embedded here.
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace ps2::offline {
struct MeshPoint { std::array<std::uint32_t, 3> bits; };
struct MeshFace { std::array<std::uint32_t, 3> indices; std::uint32_t attributes; };
struct DecodedMesh {
    std::vector<MeshPoint> points;
    std::vector<MeshFace> faces;
    std::vector<std::uint32_t> source_words;
    unsigned links = 0, hidden_links = 0, degenerate_faces = 0;
    bool terminated = false;
};
// Decode raw XYZ/link records before any instance matrix, focus, visibility or
// perspective operation. The reader must enforce its own source-memory bounds.
// At most32768links are accepted; source capture is limited to327690words.
DecodedMesh decode_mesh(const std::function<std::uint32_t()> &reader,
                        std::uint32_t declared_links, unsigned mode);
} // namespace ps2::offline
