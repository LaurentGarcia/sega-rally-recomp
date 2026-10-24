#include "mesh_decode.h"
#include <stdexcept>

#ifdef _EE
#error "Original model asset decoding belongs only in the offline host tools"
#endif

namespace ps2::offline {
DecodedMesh decode_mesh(const std::function<std::uint32_t()> &reader,
                        std::uint32_t declared_links, unsigned mode) {
    if (!reader || mode > 3 || !declared_links)
        throw std::runtime_error("invalid model decode parameters");
    DecodedMesh mesh;
    const auto word = [&]() {
        if (mesh.source_words.size() == 327690)
            throw std::runtime_error("model source word limit exceeded");
        const auto result = reader();
        mesh.source_words.push_back(result);
        return result;
    };
    const auto point = [&]() {
        MeshPoint vertex{{word(),word(),word()}};
        for (auto value : vertex.bits)
            if ((value & 0x7f800000u) == 0x7f800000u)
                throw std::runtime_error("nonfinite raw model coordinate");
        mesh.points.push_back(vertex);
        return std::uint32_t(mesh.points.size() - 1);
    };
    const auto face = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t attr) {
        if (a == b || b == c || c == a) { ++mesh.degenerate_faces; return; }
        mesh.faces.push_back({{a,b,c},attr});
    };
    auto previous_first = point(), previous_second = point();
    for (std::uint32_t link = 0; link < declared_links; ++link) {
        if (link == 32768) throw std::runtime_error("model link limit exceeded");
        const auto attributes = word();
        if ((attributes & 3u) == 0) {
            // Normals-present modes consume the terminator's normal words.
            if (mode < 2) for (unsigned i = 0; i < 3; ++i) (void)word();
            mesh.terminated = true;
            break;
        }
        for (unsigned i = 0; i < 3; ++i) (void)word(); // Normal slot exists in every mode.
        const auto next_first = point();
        auto next_second = next_first;
        if (attributes & 1u) next_second = point();
        else for (unsigned i = 0; i < 3; ++i) (void)word(); // Unused second-point slot.
        const auto connection = (attributes >> 8) & 3u;
        ++mesh.links;
        if (connection) {
            face(previous_second,previous_first,next_first,attributes);
            if (attributes & 1u) face(previous_second,next_first,next_second,attributes);
        } else ++mesh.hidden_links;
        // Strip topology is independent of normals, shading and visibility.
        if (connection == 0 || connection == 2) {
            previous_first = next_first;
            previous_second = next_second;
        } else if (connection == 1) previous_second = next_first;
        else previous_first = next_second;
    }
    return mesh;
}
} // namespace ps2::offline
