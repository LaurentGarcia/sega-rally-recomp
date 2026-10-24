#include "mesh_capture.h"
#include "mesh_decode.h"
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _EE
#error "Original model capture belongs only in the offline host reference"
#endif

namespace ps2::offline {
namespace {
struct Capture {
    std::filesystem::path path;
    std::ofstream index;
    unsigned frame = 0, objects = 0, direct_commands = 0;
    std::size_t words = 0;
};
std::unique_ptr<Capture> active;
void le32(std::ostream &stream, std::uint32_t word) {
    for (unsigned shift = 0; shift < 32; shift += 8) stream.put(char(word >> shift));
}
template<class Sequence> void integers(std::ostream &stream, const Sequence &values) {
    stream << '[';
    bool first = true;
    for (auto value : values) { if (!first) stream << ','; first = false; stream << value; }
    stream << ']';
}
}
void begin_mesh_capture(const char *directory, unsigned frame) {
    if (active || !directory || !*directory || !frame)
        throw std::runtime_error("invalid mesh capture start");
    namespace fs = std::filesystem;
    const auto root = fs::canonical(__FILE__).parent_path().parent_path().parent_path().parent_path();
    if (fs::is_symlink(root / "build")) throw std::runtime_error("mesh capture build root is a symlink");
    const auto build = fs::canonical(root / "build");
    const auto path = fs::weakly_canonical(fs::absolute(directory));
    auto r = build.begin(), p = path.begin();
    for (; r != build.end() && p != path.end() && *r == *p; ++r,++p) {}
    if (r != build.end() || p == path.end() || fs::exists(path))
        throw std::runtime_error("mesh capture requires a new directory below ignored build/");
    fs::create_directories(path);
    auto capture = std::make_unique<Capture>();
    capture->path = path; capture->frame = frame;
    capture->index.open(path / "objects.jsonl.tmp");
    if (!capture->index) throw std::runtime_error("cannot open private mesh capture index");
    active = std::move(capture);
}
void capture_mesh_object(const rt::Geo::geo_state &state, rt::GeoPtr input,
                         std::uint32_t opcode, std::uint32_t texture_points,
                         std::uint32_t texture_headers, std::uint32_t object_address,
                         std::uint32_t declared_links) {
    if (!active) return;
    auto &capture = *active;
    if (capture.objects == 1024) throw std::runtime_error("mesh capture object limit exceeded");
    const auto source_start = input.i, source_size = input.size;
    const auto mesh = decode_mesh([&] { return *input++; },declared_links,state.mode & 3u);
    if (capture.words + mesh.source_words.size() > 4194304)
        throw std::runtime_error("mesh capture source byte budget exceeded");
    capture.words += mesh.source_words.size();
    const auto name = "object-" + std::to_string(capture.objects) + ".bin";
    std::ofstream bytes(capture.path / name,std::ios::binary);
    for (auto value : mesh.source_words) le32(bytes,value);
    bytes.close();
    if (!bytes) throw std::runtime_error("cannot write private model source words");
    auto &out = capture.index;
    out << "{\"schema\":1,\"frame\":" << capture.frame << ",\"object\":" << capture.objects
        << ",\"opcode\":" << opcode << ",\"mode\":" << state.mode
        << ",\"object_address\":" << object_address << ",\"declared_links\":" << declared_links
        << ",\"texture_points\":" << texture_points << ",\"texture_headers\":" << texture_headers
        << ",\"source_start_word\":" << source_start << ",\"source_size_words\":" << source_size
        << ",\"source_file\":\"" << name << "\",\"source_words\":" << mesh.source_words.size()
        << ",\"links\":" << mesh.links << ",\"hidden_links\":" << mesh.hidden_links
        << ",\"degenerate_faces\":" << mesh.degenerate_faces
        << ",\"terminated\":" << (mesh.terminated ? "true" : "false") << ",\"matrix_bits\":";
    std::array<std::uint32_t,12> matrix{};
    for (unsigned i = 0; i < matrix.size(); ++i) matrix[i] = std::bit_cast<std::uint32_t>(state.matrix[i]);
    integers(out,matrix);
    out << ",\"focus_bits\":";
    integers(out,std::array{std::bit_cast<std::uint32_t>(state.focus.x),std::bit_cast<std::uint32_t>(state.focus.y)});
    out << ",\"points\":[";
    constexpr auto infinity = std::numeric_limits<double>::infinity();
    std::array<double,3> low{infinity,infinity,infinity}, high{-infinity,-infinity,-infinity};
    bool first = true;
    for (const auto &point : mesh.points) {
        if (!first) out << ',';
        first = false;
        integers(out,point.bits);
        for (unsigned axis = 0; axis < 3; ++axis) {
            const double value = std::bit_cast<float>(point.bits[axis]);
            low[axis] = std::min(low[axis],value); high[axis] = std::max(high[axis],value);
        }
    }
    out << "],\"faces\":["; first = true;
    for (const auto &face : mesh.faces) {
        if (!first) out << ',';
        first = false;
        integers(out,std::array{face.indices[0],face.indices[1],face.indices[2],face.attributes});
    }
    out << "],\"bounds_min\":" << std::setprecision(17); integers(out,low);
    out << ",\"bounds_max\":"; integers(out,high); out << "}\n";
    if (!out) throw std::runtime_error("cannot write private mesh capture index");
    ++capture.objects;
}
void capture_direct_geometry() { if (active) ++active->direct_commands; }
void end_mesh_capture() {
    if (!active) throw std::runtime_error("mesh capture is not active");
    auto &capture = *active;
    capture.index.close();
    if (!capture.index) throw std::runtime_error("cannot finish private mesh capture index");
    std::ofstream summary(capture.path / "capture.json.tmp");
    summary << "{\"schema\":1,\"complete\":true,\"frame\":" << capture.frame
            << ",\"objects\":" << capture.objects << ",\"direct_commands_excluded\":" << capture.direct_commands
            << ",\"source_words\":" << capture.words << "}\n";
    summary.close();
    if (!summary) throw std::runtime_error("cannot finish mesh capture metadata");
    std::filesystem::rename(capture.path / "objects.jsonl.tmp",capture.path / "objects.jsonl");
    std::filesystem::rename(capture.path / "capture.json.tmp",capture.path / "capture.json");
    active.reset();
}
} // namespace ps2::offline
