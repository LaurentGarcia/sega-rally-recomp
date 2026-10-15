// Backend-independent Model 2 projection, ordering and shader input packet.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rt { class Video; }
namespace app {
class Model2Frame {
public:
    struct Rect { int x, y, w, h; };
    struct PolyVertex {
        float x, y, depth;
        float ooz, uoz, voz;
        float r, g, b, a;
        uint32_t info[4], more[2];
        uint32_t edges[2];
    };
    struct PolyEdge { float origin[4], step[4], tail[4]; };
    struct Batch { uint32_t first, count; Rect clip; };
    static_assert(sizeof(PolyVertex) == 72);
    static_assert(sizeof(PolyEdge) == 48);
    Model2Frame();
    // Caller supplies positive dimensions and internal scale 1..4, as before.
    // Data remains owned by this object until the next build. Backends must
    // upload/copy it before calling build again; it contains no GPU handles.
    void build(const rt::Video &, int width, int height, int scale);
    void set_polygon_scanlines(bool enabled) { polygon_scanlines_ = enabled; }
    const std::vector<PolyVertex> &vertices() const { return verts_; }
    const std::vector<PolyEdge> &edges() const { return poly_edges_; }
    const std::vector<Batch> &batches() const { return batches_; }
protected:
    bool polygon_scanlines_ = false;
    std::vector<PolyVertex> verts_;
    std::vector<PolyEdge> poly_edges_;
    std::vector<Batch> batches_;
    std::vector<size_t> order_;
    uint8_t gamma_[256];
};
} // namespace app
