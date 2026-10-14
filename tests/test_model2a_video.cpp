// Synthetic video RAM/register fixtures; no game ROM data.
#include "runtime/model2a_bus.h"
#include "runtime/model2a_video.h"

#include <array>
#include <cassert>
#include <cstdio>

namespace {
void boundaries_and_memory() {
    rt::Model2AVideo video({[] { return uint64_t(0); }, {}, {}});
    rt::Model2ABus bus({}, {});
    bus.add_model(&video);
    assert(video.tile_ram().size() == 0x10000 && video.character_ram().size() == 0x80000);
    assert(video.palette_ram().size() == 0x4000 && video.color_translation_ram().size() == 0xc000);
    assert(video.luma_ram().size() == 0x20000 && video.texture_words(0).size_bytes() == 0x200000);
    assert(video.texture_words(1).size_bytes() == 0x200000 && video.framebuffer_ram(0).size() == 0x80000);
    assert(video.framebuffer_ram(1).size() == 0x80000);
    const auto shared = video.memory();
    assert(shared.palram == video.palette_ram().data() && shared.colorxlat == video.color_translation_ram().data());
    assert(shared.lumaram == video.luma_ram().data() && shared.tex0 == video.texture_words(0).data());
    assert(shared.tex1 == video.texture_words(1).data());
    for (uint32_t address : {0x01000000u, 0x0101ffffu, 0x01100000u, 0x0111ffffu,
                            0x01080000u, 0x010fffffu, 0x01180000u, 0x011fffffu,
                            0x01800000u, 0x01803fffu, 0x01810000u, 0x0181bfffu,
                            0x11600000u, 0x116fffffu, 0x12000000u, 0x123fffffu,
                            0x12400000u, 0x127fffffu, 0x12800000u, 0x1281ffffu,
                            0x01020000u, 0x01120003u}) {
        assert(video.claims(address) && video.flags(address) == rt::Cpu::BURST);
        assert(bus.flags(address) == rt::Cpu::BURST);
    }
    for (uint32_t address : {0x01020004u, 0x01030000u, 0x01040004u, 0x01160004u,
                            0x01804000u, 0x0180ffffu, 0x0181c004u, 0x116fffffu + 1,
                            0x12820000u, 0x00980010u, 0x00980040u, 0x10200000u,
                            0x10600000u, 0x10800004u}) {
        assert(!video.claims(address));
        bool stopped = false;
        try { (void)bus.read_byte(address); }
        catch (const rt::Model2AAccess &) { stopped = true; }
        assert(stopped);
    }
    for (uint32_t address : {0x01040000u, 0x01140003u, 0x01060000u, 0x01160003u,
                            0x01070000u, 0x01170003u, 0x0098000cu, 0x0098003fu,
                            0x0181c000u, 0x101fffffu, 0x105fffffu, 0x10800003u})
        assert(video.claims(address) && !video.flags(address));
    for (uint32_t address : {0x01803ffcu, 0x0181bffcu, 0x1167fffcu, 0x116ffffcu}) {
        bus.write_dword(address, 0x87654321);
        assert(bus.read_dword(address) == 0x87654321);
        bus.write_byte(address + 1, 0xab);
        assert(bus.read_dword(address) == 0x8765ab21);
    }
    // RAM banks retain independent contents; no alias between A and B.
    bus.write_word(0x11600000, 0x1234);
    bus.write_word(0x11680000, 0x5678);
    assert(bus.read_word(0x11600000) == 0x1234 && bus.read_word(0x11680000) == 0x5678);
    for (uint32_t nop : {0x01020000u, 0x01120000u, 0x01070000u, 0x01170000u, 0x10800000u}) {
        bus.write_dword(nop, 0xffffffff);
        assert(bus.read_dword(nop) == 0);
    }
}

void palette_and_tile_notifications() {
    rt::Model2AVideo adapter({[] { return uint64_t(0); }, {}, {}});
    rt::Model2ABus bus({}, {});
    bus.add_model(&adapter);
    auto &video = adapter.video();
    assert(video.crtc_x() == 0 && video.crtc_y() == 0);
    assert(video.render_x() == 90 && video.render_y() == -8);
    bus.write_word(0x01810280, 255); // red index1
    bus.write_word(0x01814480, 192); // green index2
    bus.write_word(0x01818680, 64); // blue index3
    bus.write_word(0x01800000, 0x0c00);
    bus.write_byte(0x01800000, 0x41); // full latched palette value now0x0c41
    assert(video.system24_pen(0) == 0xffffaa00);
    bus.write_dword(0x01800000, 0x0c410c41);
    assert(video.system24_pen(0) == 0xffffaa00 && video.system24_pen(1) == 0xffffaa00);
    bus.write_word(0x01810280, 64); // translated palette colors refresh at screen update
    adapter.screen_update({}, 0);
    assert(video.system24_pen(0) == 0xff00aa00 && video.system24_pen(1) == 0xff00aa00);
    // Four tile aliases all refer to the same source and update decoded tiles.
    const std::array<uint32_t, 4> aliases = {0x01000000, 0x01010000, 0x01100000, 0x01110000};
    for (unsigned glyph = 1; glyph <= 4; ++glyph)
        bus.write_byte(0x01080000 + glyph * 32 + 1, uint8_t(glyph << 4));
    for (unsigned n = 0; n < aliases.size(); ++n) {
        bus.write_word(aliases[n], uint16_t(n + 1));
        for (uint32_t alias : aliases) assert(bus.read_word(alias) == n + 1);
        const uint64_t before = video.system24_tile_generation(0, 0);
        adapter.screen_update({}, 0);
        assert(video.system24_pixels(0)[0] == n + 1);
        assert(video.system24_tile_generation(0, 0) > before);
    }
    // Character alias also notifies the existing decoder after its first frame.
    bus.write_byte(0x01180000 + 4 * 32 + 1, 0xe0);
    adapter.screen_update({}, 0);
    assert(video.system24_pixels(0)[0] == 14);
    assert(bus.read_byte(0x01080000 + 4 * 32 + 1) == 0xe0);
    bus.write_word(0x01040000, 0xffac); // real boot horizontal sync -84
    bus.write_word(0x01160000, 0xff7e); // mirrored vertical sync -130
    assert(video.crtc_x() == 0 && video.crtc_y() == 0 && video.render_x() == 0 && video.render_y() == 0);
    bus.write_word(0x01040002, 0xffff); // unmapped upper half has no callback
    assert(video.crtc_x() == 0 && bus.read_dword(0x01040000) == 0);
    bus.write_byte(0x01140000, 2); // maskless16bit callback receives zero in unwritten lanes
    assert(video.crtc_x() == 86);
    bus.write_dword(0x01060000, 0xffff0001);
    assert(video.crtc_y() == 131);
}

void textures_luma_and_registers() {
    uint64_t frame = 0;
    rt::Model2AVideo adapter({[&] { return frame; }, {}, {}});
    rt::Model2ABus bus({}, {});
    bus.add_model(&adapter);
    bus.write_dword(0x12000000, 0xfeed1234);
    bus.write_dword(0x12000004, 0xbeefabcd);
    assert(bus.read_dword(0x12000000) == 0xabcd1234);
    assert(bus.read_dword(0x12200000) == 0xabcd1234);
    assert(adapter.memory().tex0[0] == 0xabcd1234 && adapter.memory().tex_generation == 2);
    bus.write_byte(0x12200000, 0x56); // narrow write replaces both bytes of packed half
    assert(bus.read_dword(0x12000000) == 0xabcd0056);
    bus.write_byte(0x12000002, 0xff); // upper CPU lane makes low16 zero
    assert(bus.read_dword(0x12000000) == 0xabcd0000);
    bus.write_word(0x127ffffc, 0x789a); // sheet1 mirror/end maps last packed half
    assert(adapter.memory().tex1[0x3ffff] == 0x789a0000);
    assert(bus.read_dword(0x124ffffc) == 0x789a0000);
    assert(bus.read_dword(0x125ffffc) == 0); // upper unfilled backing RAM remains real storage
    bus.write_dword(0x1281fffc, 0xabcdef78);
    assert(bus.read_dword(0x1281fffc) == 0x78 && adapter.memory().lumaram[0x1fffc] == 0x78);
    for (unsigned lane = 1; lane < 4; ++lane) {
        bus.write_byte(0x1281fffc + lane, 0xff);
        assert(bus.read_byte(0x1281fffc + lane) == 0 && adapter.memory().lumaram[0x1fffc + lane] == 0);
    }
    assert(bus.read_dword(0x00980030) == 0x48415400);
    assert(bus.read_dword(0x00980034) == 0x4f4b4100);
    assert(bus.read_dword(0x00980038) == 0x4b415a00);
    assert(bus.read_dword(0x0098003c) == 0x4b544d00);
    bus.write_dword(0x00980030, 0xffffffff);
    assert(bus.read_dword(0x00980030) == 0x48415400);
    bus.write_dword(0x0098000c, 0x12340003);
    for (frame = 0; frame < 8; ++frame) assert(bus.read_dword(0x0098000c) == ((frame & 2) << 1 | 3));
    bus.write_dword(0x10000000, 4);
    for (frame = 0; frame < 8; ++frame) assert(bus.read_dword(0x0098000c) == ((frame & 1) << 2 | 3));
    bus.write_byte(0x0098000d, 0xab);
    assert(adapter.video_control() == 0x1234ab03);
    bus.write_byte(0x10000001, 0x40); // maskless write clears mode/test bits, retains bit14 from data
    assert(bus.read_dword(0x101ffffc) == 0x4000 && !adapter.render_mode());
    bool missing_geometry = false;
    try { (void)bus.read_dword(0x10400000); }
    catch (const rt::Fatal &) { missing_geometry = true; }
    assert(missing_geometry);
    bus.write_dword(0x0181c000, 0x123456);
    assert(adapter.zclip() == 0x56 && bus.read_dword(0x0181c000) == 0);
    unsigned counts = 0, clips = 0;
    uint32_t last_clip = 0;
    adapter.bind_geometry([&] { ++counts; return uint32_t(73); }, [&](uint32_t value) { ++clips; last_clip = value; });
    assert(clips == 1 && last_clip == 0x56);
    assert(bus.read_dword(0x105ffffc) == 73 && counts == 1);
    bus.write_dword(0x10400000, 0xffffffff);
    assert(counts == 1);
    bus.write_dword(0x0181c000, 0xff);
    assert(clips == 2 && last_clip == 0xff);
    bus.write_dword(0x10000000, 1);
    bool unsupported_display = false;
    try { adapter.screen_update({}, 0); }
    catch (const rt::Fatal &) { unsupported_display = true; }
    assert(unsupported_display); // no false blank framebuffer test-mode picture
}
} // namespace

int main() {
    bool rejected = false;
    try { rt::Model2AVideo invalid({}); }
    catch (const rt::Fatal &) { rejected = true; }
    assert(rejected);
    boundaries_and_memory();
    palette_and_tile_notifications();
    textures_luma_and_registers();
    std::puts("Model2A video: exact mirrors/masks, palette/tile notifications, packed textures/luma, CRTC/control and display frontiers passed");
}
