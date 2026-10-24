#include "mesh_decode.h"
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

using ps2::offline::decode_mesh;
int main() {
    std::vector<std::uint32_t> source;
    const auto point = [&](float x) {
        source.push_back(std::bit_cast<std::uint32_t>(x));
        source.push_back(0);
        source.push_back(0);
    };
    point(0); point(1);
    const auto link = [&](unsigned attr, float first, float second) {
        source.push_back(attr);
        // Arbitrary normal/unused words must not become model coordinates.
        source.insert(source.end(), {0x7fc00000, 0xffffffff, 0x12345678});
        point(first); point(second);
    };
    link(0x101, 2, 3); // Keep first strip endpoint.
    link(0x202, 4, 99); // Triangle replaces both endpoints with the same point.
    link(0x301, 5, 6); // Keep second endpoint; first triangle is index-degenerate.
    link(0x001, 7, 8); // Hidden connector still replaces strip endpoints.
    link(0x201, 9, 10);
    source.insert(source.end(), {0, 0x12345678, 0x12345678, 0x12345678});
    constexpr std::array<std::array<unsigned,3>,6> expected{{
        {1,0,2}, {1,2,3}, {2,0,4}, {4,5,6}, {8,7,9}, {8,9,10}
    }};
    for (unsigned mode = 0; mode != 4; ++mode) {
        std::size_t cursor = 0;
        auto result = decode_mesh([&] { return source.at(cursor++); },100,mode);
        assert(result.points.size() == 11 && result.faces.size() == expected.size());
        assert(result.links == 5 && result.hidden_links == 1 && result.degenerate_faces == 1);
        assert(result.terminated && cursor == (mode < 2 ? source.size() : source.size() - 3));
        assert(result.source_words.size() == cursor);
        for (unsigned i = 0; i < expected.size(); ++i) assert(result.faces[i].indices == expected[i]);
        for (unsigned i = 0; i < 11; ++i)
            assert(std::bit_cast<float>(result.points[i].bits[0]) == float(i));
    }
    std::size_t cursor = 0;
    auto bounded = decode_mesh([&] { return source.at(cursor++); },1,0);
    assert(!bounded.terminated && bounded.links == 1 && cursor == 16);
    // A short read is propagated, never converted to a fabricated terminator.
    bool rejected = false;
    cursor = 0;
    try { (void)decode_mesh([&] { if (cursor == 12) throw std::runtime_error("short read"); return source.at(cursor++); },1,0); }
    catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
    for (unsigned invalid : {0x7f800000u,0xff800000u,0x7fc12345u}) {
        source[0] = invalid; cursor = 0; rejected = false;
        try { (void)decode_mesh([&] { return source.at(cursor++); },1,0); }
        catch (const std::runtime_error &) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { (void)decode_mesh({},1,0); } catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
    std::puts("OFFLINE_MESH_DECODE PASS modes=4 links=5 original_vectors=1");
}
