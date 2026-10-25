// Offline original-firmware comparison using only original synthetic course data.
#include "contact_search_cases.h"
#include "runtime/tgp.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace ps2::native;
constexpr std::uint32_t records = 0x100, geometry = 0x200, attributes = 0x4000;
std::uint32_t word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return bytes.at(offset) | (std::uint32_t(bytes.at(offset + 1)) << 8) |
           (std::uint32_t(bytes.at(offset + 2)) << 16) | (std::uint32_t(bytes.at(offset + 3)) << 24);
}
std::vector<std::uint8_t> read(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw rt::TgpFatal("cannot open private search input");
    return {std::istreambuf_iterator<char>(input), {}};
}
struct Bus final : rt::TgpBus {
    std::array<std::uint32_t, 0x8000> buffer{};
    std::array<std::uint32_t, 0x5000> copro{};
    std::vector<std::uint32_t> input, output;
    std::size_t consumed = 0;
    bool fifo_pop(std::uint32_t& value) override {
        if (consumed == input.size()) return false;
        value = input[consumed++]; return true;
    }
    void fifo_push(std::uint32_t value) override { output.push_back(value); }
    std::uint32_t mem_r(std::uint32_t address) override {
        if (address & 0x800000) return copro.at(address & 0x1fffff);
        if (address & 0x400000) return buffer.at(address & 0x7fff);
        throw rt::TgpFatal("unexpected synthetic search read");
    }
    void mem_w(std::uint32_t address, std::uint32_t value) override {
        if (!(address & 0x400000) || (address & 0x800000)) throw rt::TgpFatal("unexpected synthetic search write");
        buffer.at(address & 0x7fff) = value;
    }
};
struct Oracle {
    Bus bus;
    rt::Tgp tgp;
    std::uint32_t cursor = 0x6000;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> addresses;
    Oracle(const std::vector<std::uint8_t>& firmware, const std::array<std::uint32_t, 0x10000>& tables,
           std::span<const ContactSegment> course) {
        tgp.bus = &bus; tgp.tables = tables.data();
        for (unsigned i = 0; i < firmware.size() / 4; ++i) tgp.prog[i] = word(firmware, i * 4);
        std::uint32_t next_geometry = geometry, next_attributes = attributes;
        for (std::size_t s = 0; s < course.size(); ++s) {
            const auto polygons = course[s].polygons;
            addresses.emplace_back(next_geometry, next_attributes);
            bus.copro.at(records + s * 4) = polygons.size();
            bus.copro.at(records + s * 4 + 1) = next_geometry * 4;
            bus.copro.at(records + s * 4 + 2) = next_attributes * 4;
            bus.copro.at(records + s * 4 + 3) = 0xdeadbeef; // Unused record word.
            for (const auto& polygon : polygons) {
                const auto payload = contact_search_cases::payload(polygon);
                for (unsigned i = 0; i < 16; ++i) bus.copro.at(next_geometry++) = payload[i];
                bus.copro.at(next_attributes++) = payload[16];
            }
        }
        bus.copro[0x80] = course.size();
        tgp.reset(); command({0x04000000});
        if (bus.output != std::vector<std::uint32_t>{8}) throw rt::TgpFatal("unexpected startup output");
        bus.output.clear();
    }
    void command(std::initializer_list<std::uint32_t> values) {
        bus.input.insert(bus.input.end(), values);
        const auto bound = tgp.count + 100000;
        rt::tgpgen::run(tgp, bound);
        if (tgp.count >= bound || bus.consumed != bus.input.size()) throw rt::TgpFatal("search command exceeded bound");
    }
    void search(std::uint32_t count, std::int16_t current, const ContactVertex& point, unsigned slot) {
        if (slot >= 8) throw rt::TgpFatal("unsupported synthetic result slot");
        bus.buffer.at(cursor++) = 0x80;
        command({0x03800707});
        if (bus.output != std::vector<std::uint32_t>{count}) throw rt::TgpFatal("synthetic count reply mismatch");
        bus.output.clear();
        const auto index = std::uint32_t(current);
        for (auto value : {std::bit_cast<std::uint32_t>(point.x), std::bit_cast<std::uint32_t>(point.y),
                           std::bit_cast<std::uint32_t>(point.z), records, count * 4,
                           (index ? index - 1 : count - 1) * 4, index * 4,
                           (index + 1 == count ? 0 : index + 1) * 4, std::uint32_t(slot * 2)})
            bus.buffer.at(cursor++) = value;
        command({0x29005252});
        if (!bus.output.empty()) throw rt::TgpFatal("search emitted an unexpected reply");
    }
    std::array<std::uint32_t, 17> reply(unsigned slot) {
        command({0x29805353, slot * 2});
        if (bus.output.size() != 17) throw rt::TgpFatal("result length differs from 17 words");
        std::array<std::uint32_t, 17> result;
        std::copy(bus.output.begin(), bus.output.end(), result.begin()); bus.output.clear();
        return result;
    }
    void selected(const ContactSearchResult& result, unsigned slot) {
        const auto attribute_pointer = tgp.data[0x68 + slot * 2], geometry_pointer = tgp.data[0x69 + slot * 2];
        if (result.matched) {
            const auto [g, a] = addresses.at(result.segment);
            if (geometry_pointer != (0xff800000u | (g + result.polygon * 16)) ||
                attribute_pointer != (0xff800000u | (a + result.polygon))) {
                std::fprintf(stderr,"POINTER slot=%u segment=%u polygon=%u geometry=%08x/%08x attribute=%08x/%08x\n", slot, result.segment, result.polygon,
                             geometry_pointer,0xff800000u|(g+result.polygon*16),attribute_pointer,0xff800000u|(a+result.polygon));
                throw rt::TgpFatal("native selected indices differ from original pointers");
            }
        } else if (geometry_pointer || attribute_pointer) throw rt::TgpFatal("miss retained a result pointer");
    }
};
std::uint64_t hash = 14695981039346656037ull, steps = 0;
std::uint32_t cases = 0, retained = 0;
void compare(Oracle& oracle, std::span<const ContactSegment> course, std::int16_t current,
             const ContactVertex& point, unsigned slot, bool known_hit, unsigned known_segment, unsigned known_polygon) {
    ContactSearchResult native;
    if (!search_course_contact(course, current, point, native)) throw rt::TgpFatal("native rejected supported original fixture");
    if (native.matched != known_hit || (known_hit && (native.segment != known_segment || native.polygon != known_polygon)))
        throw rt::TgpFatal("native differs from independently selected fixture result");
    const auto before = oracle.tgp.count;
    oracle.search(course.size(), current, point, slot); oracle.selected(native, slot);
    const auto original = oracle.reply(slot), expected = contact_search_cases::payload(native.data);
    if (original != expected) throw rt::TgpFatal("complete native payload differs from original reply");
    steps += oracle.tgp.count - before;
    hash = (hash ^ native.matched) * 1099511628211ull;
    hash = (hash ^ native.segment) * 1099511628211ull;
    hash = (hash ^ native.polygon) * 1099511628211ull;
    for (auto value : original) hash = (hash ^ value) * 1099511628211ull;
    ++cases;
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto firmware = read(argv[1]), table_bytes = read(argv[2]);
        if (firmware.size() != 6660 || table_bytes.size() != 262144) throw rt::TgpFatal("wrong input sizes");
        std::array<std::uint32_t, 0x10000> tables{};
        for (unsigned i = 0; i < tables.size(); ++i) tables[i] = word(table_bytes, 4 * i);
        contact_search_cases::Case representative{};
        if (!contact_search_cases::each([&](const auto& c) {
            std::array<ContactSegment, 3> course;
            for (unsigned i = 0; i < 3; ++i) course[i].polygons = c.polygons[i];
            Oracle oracle(firmware, tables, course);
            compare(oracle, course, c.current, c.point, cases % 8, c.matched, c.segment, c.polygon);
            representative = c;
            return true;
        }, true)) return 1;
        // All attribute bits other than shape selection are uninterpreted
        // payload, including the top bit. Check each bit and all bits together.
        ContactPolygon flagged = representative.polygons[0][0];
        const ContactSegment flagged_segment{std::span(&flagged, 1)};
        for (unsigned bit = 0; bit <= 32; ++bit) {
            flagged.attributes = bit == 32 ? 0xffffffffu : std::uint32_t(1) << bit;
            Oracle oracle(firmware, tables, std::span(&flagged_segment, 1));
            compare(oracle, std::span(&flagged_segment, 1), 0, representative.point, bit % 8, true, 0, 0);
        }
        // Maximum supported polygon count, with a late match and with a miss.
        std::vector<ContactPolygon> polygons(255, representative.polygons[0][0]);
        for (auto& p : polygons) for (auto& v : p.vertices) v.x += 20;
        polygons.back() = representative.polygons[0][0];
        polygons.back().attributes = 0; // Geometry hit despite a zero result attribute.
        const ContactSegment segment{polygons};
        for (const auto point : {representative.point, ContactVertex{40, 0, 40}}) {
            Oracle oracle(firmware, tables, std::span(&segment, 1));
            compare(oracle, std::span(&segment, 1), 0, point, 7, point.x != 40, 0, 254);
        }
        // Slot writes are independent state: defer all replies, then read them
        // in reverse order. Geometry and miss payloads are both retained.
        std::array<ContactSegment, 2> course{ContactSegment{representative.polygons[0]}, ContactSegment{representative.polygons[1]}};
        Oracle oracle(firmware, tables, course);
        std::array<ContactSearchResult, 8> expected{};
        for (unsigned slot = 0; slot < 8; ++slot) {
            auto point = representative.point; if (slot & 1) point.x = 40;
            if (!search_course_contact(course, slot % 2, point, expected[slot])) throw rt::TgpFatal("native slot fixture rejected");
            const auto before = oracle.tgp.count;
            oracle.search(course.size(), slot % 2, point, slot); oracle.selected(expected[slot], slot);
            steps += oracle.tgp.count - before;
        }
        for (unsigned index = 8; index; --index) {
            const auto slot = index - 1;
            const auto before = oracle.tgp.count;
            const auto reply = oracle.reply(slot);
            if (reply != contact_search_cases::payload(expected[slot].data)) throw rt::TgpFatal("retained result slot mismatch");
            steps += oracle.tgp.count - before;
            for (auto value : reply) hash = (hash ^ value) * 1099511628211ull;
            ++retained;
        }
        std::printf("CONTACT_SEARCH_ORACLE PASS cases=%u retained_slots=%u instructions=%llu hash=%016llx\n",
                    cases, retained, (unsigned long long)steps, (unsigned long long)hash);
    } catch (const std::exception& e) { std::fprintf(stderr, "contact search oracle failed: %s\n", e.what()); return 1; }
}
