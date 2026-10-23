#include "presentation.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ps2 {
bool convert_frame(std::span<const uint32_t> source, int width, int height, int source_stride,
                   std::span<uint32_t> destination, int destination_stride) {
    if (width <= 0 || height <= 0 || source_stride < width || destination_stride < width) return false;
    const auto rows = size_t(height), src_pitch = size_t(source_stride), dst_pitch = size_t(destination_stride);
    if (rows > std::numeric_limits<size_t>::max() / src_pitch ||
        rows > std::numeric_limits<size_t>::max() / dst_pitch) return false;
    if (source.size() < (rows - 1) * src_pitch + size_t(width) || destination.size() < rows * dst_pitch) return false;
    for (size_t y = 0; y < rows; ++y) {
        uint32_t *out = destination.data() + y * dst_pitch;
        for (int x = 0; x < width; ++x) out[x] = gs_pixel(source[y * src_pitch + size_t(x)]);
        std::fill(out + width, out + dst_pitch, gs_pixel(0));
    }
    return true;
}

bool flat_frame_supported(std::span<const app::Model2Frame::PolyVertex> vertices,
                          std::span<const app::Model2Frame::Batch> batches) {
    size_t next = 0;
    for (const auto &batch : batches) {
        if (batch.first != next || batch.count % 3 || batch.count > vertices.size() - next) return false;
        if (batch.clip.x < 0 || batch.clip.y < 0 || batch.clip.w <= 0 || batch.clip.h <= 0 ||
            batch.clip.x > kFrameWidth - batch.clip.w || batch.clip.y > kFrameHeight - batch.clip.h) return false;
        for (size_t i = next; i < next + batch.count; ++i) {
            const auto &v = vertices[i];
            if ((v.info[0] & 7u) || v.edges[1] || !std::isfinite(v.x) || !std::isfinite(v.y) ||
                !std::isfinite(v.depth) || v.depth <= 0.f || v.depth >= 1.f ||
                v.x < -kFrameWidth || v.x > 2 * kFrameWidth || v.y < -kFrameHeight || v.y > 2 * kFrameHeight ||
                !(v.r >= 0.f && v.r <= 1.f) || !(v.g >= 0.f && v.g <= 1.f) ||
                !(v.b >= 0.f && v.b <= 1.f) || v.a != 1.f) return false;
            const auto &first = vertices[i - (i - next) % 3];
            if (v.r != first.r || v.g != first.g || v.b != first.b || v.depth != first.depth) return false;
            if (i > 0 && i % 3 == 0 && v.depth < vertices[i - 1].depth) return false;
        }
        next += batch.count;
    }
    return next == vertices.size();
}
} // namespace ps2
