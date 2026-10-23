// Synthetic resident-versus-paged ROM checks; no game code or ROM inputs.
#include "runtime/model2a_bus.h"
#include "runtime/m2_replay_bus.h"
#include "runtime/m2_tgp_board.h"
#include "runtime/geo.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <random>

// Memory-only fixture. Executing a generated TGP program would be a test bug.
namespace rt::tgpgen {
const uint32_t program_crc32 = 0, program_words = 0;
void run(Tgp &, uint64_t) { throw std::runtime_error("unexpected TGP execution in memory fixture"); }
}

class SyntheticRom final : public rt::RomSource {
public:
    std::array<std::vector<uint8_t>, 5> images;
    uint32_t calls = 0;
    uint32_t size(rt::RomRegion r) const override { return uint32_t(images[unsigned(r)].size()); }
    const uint8_t *page(rt::RomRegion r, uint32_t index) override {
        ++calls;
        forget(); // Required whenever another page may overwrite cached bytes.
        auto &slot = cache_[next_++ % cache_.size()];
        slot.fill(0);
        const auto &image = images[unsigned(r)];
        const size_t offset = size_t(index) * kPageSize;
        if (offset >= image.size()) throw std::runtime_error("test page out of bounds");
        std::copy_n(image.data() + offset, std::min(size_t(kPageSize), image.size() - offset), slot.data());
        return slot.data();
    }
private:
    std::array<std::array<uint8_t, kPageSize>, 16> cache_{};
    unsigned next_ = 0;
};

static std::vector<uint8_t> pattern(size_t bytes, unsigned seed) {
    std::vector<uint8_t> output(bytes);
    for (size_t i = 0; i < bytes; ++i) output[i] = uint8_t((i * 17) ^ (i >> 12) ^ seed);
    return output;
}

struct ReadResult {
    uint32_t value = 0, address = 0;
    unsigned width = 0;
    bool error = false;
    bool operator==(const ReadResult &) const = default;
};

static ReadResult read(rt::Model2ABus &bus, uint32_t address, unsigned width) {
    try {
        const uint32_t value = width == 1 ? bus.read_byte(address) :
            width == 2 ? bus.read_word(address) : bus.read_dword(address);
        return {value, 0, 0, false};
    } catch (const rt::Model2AAccess &e) {
        assert(!e.write);
        return {0, e.address, e.width, true};
    }
}

struct Overlay final : rt::DeviceModel {
    uint32_t reads = 0;
    bool claims(uint32_t a) const override { return a >= 0x100 && a <= 0x103; }
    Check read(uint32_t, uint32_t, uint32_t &value) override { ++reads; value = 0x87654321; return Strict; }
    void write(uint32_t, uint32_t, uint32_t) override {}
    void measured_mismatch(uint32_t, uint32_t, uint32_t) override {}
};

int main() {
    auto source = std::make_unique<SyntheticRom>();
    // Partial last pages exercise both within-page and whole-image zero fill.
    source->images[0] = pattern(0x12003, 9);
    source->images[1] = pattern(0x1000003, 17);
    source->images[2] = pattern(8192, 31);
    source->images[3] = pattern(8192, 41);
    source->images[4] = pattern(0x800000, 23);
    rt::Model2ABus resident(source->images[0], source->images[1]);
    rt::Model2ABus paged({}, {}, source.get());
    uint64_t resident_sync = 0, paged_sync = 0;
    resident.set_before_access([&] { ++resident_sync; });
    paged.set_before_access([&] { ++paged_sync; });
    constexpr uint32_t edges[] = {0, 0xfff, 0x1000, 0x11fff, 0x12002, 0x12003,
        0x1ffffc, 0x1ffffd, 0x1fffff, 0x200000, 0x23ffff, 0x240000,
        0x2000000, 0x2000fff, 0x2ffffff, 0x3000000, 0x3000002, 0x3000003,
        0x3fffffc, 0x3fffffd, 0x3ffffff, 0x4000000,
        0x6000000, 0x6000002, 0x6000003, 0x6000fff, 0x6fffffc, 0x6fffffd, 0x6ffffff, 0x7000000};
    for (uint32_t address : edges) {
        for (unsigned width : {1u, 2u, 4u}) assert(read(resident, address, width) == read(paged, address, width));
        const auto before = source->calls;
        assert(resident.flags(address) == paged.flags(address));
        assert(source->calls == before);
    }
    std::mt19937 random(0x3900);
    for (unsigned trial = 0; trial < 10000; ++trial) {
        const unsigned region = random() % 3;
        const uint32_t address = region == 0 ? random() % 0x200000 :
            region == 1 ? 0x2000000 + random() % 0x2000000 : 0x6000000 + random() % 0x1000000;
        const unsigned width = 1u << (random() % 3);
        assert(read(resident, address, width) == read(paged, address, width));
    }
    assert(resident_sync == paged_sync);
    for (uint32_t address : {0x1000u, 0x1ffffcu, 0x2000fffu, 0x3000000u, 0x6000000u, 0x6fffffcu}) {
        const auto before = source->calls;
        paged.write_dword(address, 0xdeadbeef);
        resident.write_dword(address, 0xdeadbeef);
        assert(source->calls == before);
        assert(read(resident, address, 4) == read(paged, address, 4));
    }
    for (uint32_t address : {0x1fffffu, 0x3ffffffu, 0x6ffffffu}) {
        const auto before = source->calls;
        bool caught = false;
        try { paged.write_word(address, 0xbeef); }
        catch (const rt::Model2AAccess &e) { caught = e.write && e.address == address && e.width == 2 && e.value == 0xbeef; }
        assert(caught && source->calls == before);
    }
    for (auto *bus : {&resident, &paged}) {
        bus->write_dword(0x0091fffc, 0x2468ace0);
        assert(bus->read_dword(0x0097fffc) == 0x2468ace0);
        bus->write_word(0x00200001, 0x9876);
        assert(bus->read_word(0x00200001) == 0x9876);
        assert(bus->read_dword(0x01d00000) == 0xffffffff);
    }
    // Invalidating per-region remembered pointers must survive cache reuse.
    const auto expected = paged.fetch(0x1000);
    for (unsigned i = 0; i < 32; ++i) paged.read_byte(0x02000000 + i * 4096);
    assert(paged.fetch(0x1000) == expected);
    Overlay device;
    paged.add_model(&device);
    const auto before = source->calls;
    assert(paged.read_word(0x102) == 0x8765 && device.reads == 1 && source->calls == before);
    bool rejected = false;
    try { rt::Model2ABus ambiguous({1}, {}, source.get()); }
    catch (const rt::Fatal &) { rejected = true; }
    assert(rejected);
    // TGP resident constructors must remain usable in a paged-enabled build.
    std::vector<uint8_t> tables(0x40000);
    auto tgp_resident = std::make_unique<rt::TgpBoard>(tables, source->images[4]);
    auto tgp_paged = std::make_unique<rt::TgpBoard>(tables, *source);
    for (uint32_t word : {0u, 1023u, 1024u, 0x1fffffu, 0x200000u, 0x7fffffu})
        assert(tgp_resident->mem_r(0x800000 | word) == tgp_paged->mem_r(0x800000 | word));
    // Exercise the geometry checksum ROM reads and end command through both
    // constructors. No generated TGP code runs and no polygon is fabricated.
    std::array<uint32_t, 0x8000> buffer{};
    unsigned cursor = 0;
    buffer[cursor++] = 0x0e << 23;
    for (unsigned bit = 0; bit < 32; ++bit) buffer[cursor++] = 1u << bit;
    buffer[cursor++] = 1; // one checksum block
    buffer[cursor++] = 1022; // cross page boundary
    buffer[cursor++] = 8;
    buffer[cursor++] = 0;
    buffer[cursor++] = 0x0f << 23;
    auto geo_resident = std::make_unique<rt::Geo>(source->images[2], source->images[3], buffer.data());
    auto geo_paged = std::make_unique<rt::Geo>(*source, buffer.data());
    geo_resident->record_pushes = geo_paged->record_pushes = true;
    geo_resident->parse(0); geo_paged->parse(0);
    assert(geo_resident->pushed == geo_paged->pushed && geo_resident->polys.empty() && geo_paged->polys.empty());
    std::vector<uint32_t> polygon_words(2048);
    std::memcpy(polygon_words.data(), source->images[2].data(), 8192);
    rt::GeoPtr a{polygon_words.data(), 2048, 2046}, b{nullptr, 2048, 2046, source.get()};
    for (unsigned i = 0; i < 4096; ++i) assert(*a++ == *b++);
    std::puts("Paged Model2A: 10,000 ROM reads, width/mirror boundaries, cache reuse, resident TGP/Geo passed");
}
