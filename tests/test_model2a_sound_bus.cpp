// Synthetic board/CPU contracts; contains no game bytes.
#include "runtime/model2a_sound_bus.h"
#include "m68k/reach.h"
#include <algorithm>
#include <cstdio>
#include <tuple>

namespace {
unsigned checks = 0;
#define CHECK(test) do { ++checks; if (!(test)) throw rt::Fatal("failed: " #test); } while (0)
template<class Function> void fatal(Function operation) {
    bool caught = false;
    try { operation(); } catch (const rt::Fatal &) { caught = true; }
    CHECK(caught);
}
struct Chip : snd::Model2ASoundBus::Scsp {
    unsigned reads = 0, writes = 0;
    uint16_t offset = 0, data = 0, mask = 0;
    uint16_t read16(uint16_t address, uint16_t active) override {
        ++reads; offset = address; mask = active; return 0x95c3;
    }
    void write16(uint16_t address, uint16_t value, uint16_t active) override {
        ++writes; offset = address; data = value; mask = active;
    }
};
struct Legacy : snd::Devices {
    uint32_t address = 0;
    uint16_t data = 0;
    uint8_t read(uint32_t a) override { address = a; return 0x69; }
    void write(uint32_t a, uint16_t d) override { address = a; data = d; }
};
void default_daytona() {
    std::vector<uint8_t> image(0x40000);
    image[2] = 0x12; image[3] = 0x34; image[6] = 1; image[0x20000] = 0xab; image[0x20001] = 0xcd;
    snd::Cpu68k c;
    Legacy legacy;
    c.rom = image.data(); c.dev = &legacy;
    c.reset();
    CHECK(c.bus == nullptr && c.a[7] == 0x1234 && c.pc == 0x100);
    CHECK(c.r16(0x80000) == 0xabcd && c.r8(0x1080001) == 0xcd);
    c.w32(0xf00000, 0x12345678);
    c.w8(0xf00000, 0xab); c.w8(0xf00003, 0xcd);
    CHECK(c.r32(0xf00000) == 0xab3456cd);
    CHECK(c.r8(0xc20000) == 0 && c.r8(0xc20001) == 0x69);
    c.w8(0xc20001, 0xa5); CHECK(legacy.address == 0xc20001 && legacy.data == 0xa5);
    c.w16(0xc50000, 0x1379); CHECK(legacy.address == 0xc50000 && legacy.data == 0x1379);
    CHECK(c.r16(0x40000) == 0);
}
void model2a() {
    std::vector<uint8_t> program(snd::Model2ASoundBus::PROGRAM_BYTES), samples(snd::Model2ASoundBus::SAMPLE_BYTES);
    for (size_t i = 0; i < program.size(); ++i) program[i] = uint8_t(i * 19 + 7);
    for (size_t i = 0; i < samples.size(); ++i) samples[i] = uint8_t((i >> 20) ^ i);
    snd::Model2ASoundBus bus(program, samples);
    snd::Cpu68k c; c.bus = &bus;
    CHECK(std::equal(program.begin(), program.begin() + 16, bus.sound_ram().begin()));
    CHECK(std::all_of(bus.sound_ram().begin() + 16, bus.sound_ram().end(), [](auto b) { return b == 0; }));
    c.w32(0x20, 0x11223344); c.w8(0x20, 0xaa); c.w8(0x23, 0xbb);
    CHECK(c.r32(0x20) == 0xaa2233bb);
    for (uint16_t mask : {0x0000, 0xff00, 0x00ff, 0xffff, 0x8181, 0x0f0f}) {
        bus.write16(0x30, 0xa55a, 0xffff);
        bus.write16(0x30, 0x1234, mask);
        CHECK(c.r16(0x30) == uint16_t((0xa55a & ~mask) | (0x1234 & mask)));
        CHECK(bus.read16(0x30, mask) == (c.r16(0x30) & mask));
    }
    c.w16(0x107fffe, 0xcdef); CHECK(c.r16(0x7fffe) == 0xcdef);
    c.w16(0, 0xffff);
    bus.reset();
    CHECK(c.r16(0) == uint16_t(program[0] << 8 | program[1]));
    CHECK(c.r32(0x20) == 0xaa2233bb && c.r16(0x7fffe) == 0xcdef);
    for (uint32_t address : {0x600000u, 0x63fffeu, 0x640000u, 0x67fffeu}) {
        const auto index = address - 0x600000;
        CHECK(c.r16(address) == uint16_t(program[index] << 8 | program[index + 1]));
        c.w16(address, 0); CHECK(c.r16(address) == uint16_t(program[index] << 8 | program[index + 1]));
    }
    for (uint32_t address : {0x800000u, 0x9ffffeu, 0xa00000u, 0xdffffeu, 0xe00000u, 0xfffffeu}) {
        const auto index = address - 0x800000;
        CHECK(c.r16(address) == uint16_t(samples[index] << 8 | samples[index + 1]));
    }
    const auto sample_end = uint32_t(c.r16(0xfffffe)) << 16;
    CHECK(c.r32(0xfffffe) == (sample_end | c.r16(0)));
    c.w32(0xfffffe, 0x12345678); CHECK(c.r16(0) == 0x5678); // second word wraps
    c.w8(0x400000, 0xff); c.w8(0x400001, 0); // 8 MiB banking is inert
    CHECK(c.r16(0xa00000) == uint16_t(samples[0x200000] << 8 | samples[0x200001]));
    fatal([&] { c.r16(1); }); fatal([&] { c.w16(1, 0); });
    fatal([&] { bus.read16(0x1000000, 0xffff); });
    for (auto [address, write, mask] : {std::tuple{0x100000u, false, uint16_t(0xff00)},
                                      std::tuple{0x100ffeu, true, uint16_t(0x00ff)},
                                      std::tuple{0x700000u, false, uint16_t(0xffff)}}) {
        bool caught = false;
        try {
            if (write) bus.write16(address, 0x5a, mask); else bus.read16(address, mask);
        } catch (const snd::Model2ASoundBus::Frontier &f) {
            caught = true;
            CHECK(f.address == address && f.write == write && f.mask == mask);
            CHECK(f.region == (address == 0x700000 ? snd::Model2ASoundBus::Frontier::Region::Unmapped
                                                : snd::Model2ASoundBus::Frontier::Region::Scsp));
        }
        CHECK(caught);
    }
    Chip chip; bus.set_scsp(&chip);
    CHECK(c.r8(0x100000) == 0x95 && chip.reads == 1 && chip.mask == 0xff00 && chip.offset == 0);
    CHECK(c.r8(0x100001) == 0xc3 && chip.reads == 2 && chip.mask == 0x00ff);
    CHECK(c.r16(0x100ffe) == 0x95c3 && chip.reads == 3 && chip.offset == 0xffe && chip.mask == 0xffff);
    c.w8(0x100400, 3); CHECK(chip.data == 0x0300 && chip.mask == 0xff00 && chip.offset == 0x400);
    c.w8(0x100401, 0x69); CHECK(chip.data == 0x0069 && chip.mask == 0x00ff);
    bus.write16(0x100000, 0, 0); CHECK(chip.writes == 2);
    CHECK(bus.read16(0x100000, 0) == 0 && chip.reads == 3);
    CHECK(c.r32(0x100000) == 0x95c395c3 && chip.reads == 5 && chip.offset == 2);
    fatal([&] { snd::Model2ASoundBus bad(std::span(program).first(16), samples); });
}
void vectors() {
    const std::vector<uint16_t> words = {0, 0x100, 0, 0x20, 0, 0x40, 0, 0x60, 0, 0x80};
    const auto read = [&](uint32_t a) -> std::optional<uint16_t> {
        if (a < words.size() * 2) return words[a / 2];
        if (a == 0x20 || a == 0x40 || a == 0x60 || a == 0x80) return 0x4e75;
        return std::nullopt;
    };
    CHECK(m68k::vector_seeds(read, 0, 4) == std::vector<uint32_t>({0x20, 0x40, 0x60}));
    CHECK(m68k::vector_seeds(read) == std::vector<uint32_t>({0x20, 0x40, 0x60, 0x80}));
    CHECK(m68k::vector_seeds(read, 0xfffffffcu, 4).empty());
}
}

int main() {
    try {
        default_daytona(); model2a(); vectors();
        std::printf("Model2A sound bus: %u checks passed, including unchanged Daytona default\n", checks);
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
