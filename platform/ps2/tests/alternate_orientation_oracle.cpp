// Private original-program differential. Never linked into a native target.
#include "alternate_orientation.h"
#include "alternate_orientation_cases.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using alternate_cases::Input;
using alternate_cases::Triple;
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::uint32_t entry = relocation + 0x2ecb4, end = relocation + 0x2ee00;
constexpr std::uint32_t object = 0x100000, frame = 0x200000, candidate_address = frame + 0x90;
struct Memory final : rt::Bus {
    const std::vector<std::uint8_t> &program;
    Triple current{};
    std::array<std::uint32_t, 4> candidate{};
    unsigned reads = 0, writes = 0;
    std::array<unsigned, 6> written{};
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address & 3) || !((address >= entry && address < end) ||
            (address >= relocation + 0x27b28 && address < relocation + 0x27bc4) ||
            (address >= relocation + 0x27bd8 && address < relocation + 0x27c60)))
            throw rt::Fatal("alternate oracle escaped verified instruction ranges");
        const auto offset = address - relocation;
        return program.at(offset) | (std::uint32_t(program.at(offset + 1)) << 8) |
               (std::uint32_t(program.at(offset + 2)) << 16) | (std::uint32_t(program.at(offset + 3)) << 24);
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        ++reads;
        if (!(address & 3) && address >= candidate_address && address < candidate_address + 16)
            return candidate[(address - candidate_address) / 4];
        if (!(address & 3) && address >= object + 0x38 && address <= object + 0x40)
            return current[(address - object - 0x38) / 4];
        throw rt::Fatal("alternate oracle unexpected data read");
    }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        ++writes;
        if (!(address & 3) && address >= candidate_address && address < candidate_address + 12) {
            const auto index = (address - candidate_address) / 4;
            candidate[index] = value; ++written[index]; return;
        }
        if (!(address & 3) && address >= object + 0x38 && address <= object + 0x40) {
            const auto index = (address - object - 0x38) / 4;
            current[index] = value; ++written[3 + index]; return;
        }
        throw rt::Fatal("alternate oracle unexpected data write");
    }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected halfword read"); }
    void write_byte(std::uint32_t, std::uint8_t) override { throw rt::Fatal("unexpected byte write"); }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return rt::Cpu::BURST; }
};
ps2::native::OrientationAngles values(const Triple &v) {
    return {std::bit_cast<float>(v[0]),std::bit_cast<float>(v[1]),std::bit_cast<float>(v[2])};
}
Triple bits(const ps2::native::OrientationAngles &v) {
    return {std::bit_cast<std::uint32_t>(v.first),std::bit_cast<std::uint32_t>(v.middle),
            std::bit_cast<std::uint32_t>(v.third)};
}
std::uint32_t read_word(std::ifstream &input) {
    std::array<unsigned char, 4> bytes{};
    if (!input.read(reinterpret_cast<char *>(bytes.data()), 4)) throw rt::Fatal("truncated live alternate record");
    return bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) |
           (std::uint32_t(bytes[3]) << 24);
}
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream),{}};
    if (program.size() != 2097152) return 2;
    std::uint64_t cases = 0, rejected = 0, instructions = 0, reads = 0, writes = 0;
    std::uint64_t fingerprint = 14695981039346656037ull;
    unsigned minimum = 1000, maximum = 0, live = 0;
    std::vector<std::array<std::uint32_t, 6>> visited;
    try {
        Memory memory(program);
        rt::I960Ref original(&memory);
        auto compare = [&](Input input, const Triple *live_folded = nullptr, const Triple *live_result = nullptr) {
            Triple expected_folded{}, expected_result{};
            const bool expected = alternate_cases::mathematical(input,expected_folded,expected_result);
            ps2::native::AlternateOrientationUpdate result{};
            const bool accepted = ps2::native::smooth_alternate_orientation(values(input.current),values(input.candidate),result);
            if (accepted != expected) throw rt::Fatal("native and independent domain disagree");
            if (!accepted) {
                if (live_result) throw rt::Fatal("live alternate input outside declared native domain");
                ++rejected; return true;
            }
            if (bits(result.folded_candidate) != expected_folded || bits(result.orientation) != expected_result)
                throw rt::Fatal("native and mathematical outputs disagree");
            memory.current = input.current;
            std::copy(input.candidate.begin(),input.candidate.end(),memory.candidate.begin());
            const auto padding = std::uint32_t(cases) ^ 0xabc01234u;
            memory.candidate[3] = padding;
            memory.reads = memory.writes = 0; memory.written.fill(0);
            std::fill(std::begin(original.m_r),std::end(original.m_r),std::uint32_t(cases) ^ 0x518a7f03u);
            std::fill(std::begin(original.m_fp),std::end(original.m_fp),-19.25);
            original.m_r[12] = object; original.m_r[5] = candidate_address; original.m_r[31] = frame;
            original.m_AC = 0x3f001000; original.m_IP = entry;
            unsigned steps = 0;
            while (original.m_IP != end) {
                if (++steps > 256) throw rt::Fatal("alternate oracle instruction limit");
                original.execute_one();
            }
            const Triple folded{memory.candidate[0],memory.candidate[1],memory.candidate[2]};
            if (folded != expected_folded || memory.current != expected_result || memory.candidate[3] != padding ||
                memory.writes != 6 || memory.written != std::array<unsigned,6>{1,1,1,1,1,1}) {
                std::fprintf(stderr,"alternate original mismatch case=%llu current=%08x,%08x,%08x candidate=%08x,%08x,%08x\n",
                    (unsigned long long)cases,input.current[0],input.current[1],input.current[2],
                    input.candidate[0],input.candidate[1],input.candidate[2]);
                throw rt::Fatal("original memory result mismatch");
            }
            if (live_result && (folded != *live_folded || memory.current != *live_result))
                throw rt::Fatal("captured alternate result differs from native/original replay");
            for (const auto &triple : {input.current,input.candidate,expected_folded,expected_result})
                for (auto word : triple) fingerprint = (fingerprint ^ word) * 1099511628211ull;
            fingerprint = (fingerprint ^ steps) * 1099511628211ull;
            minimum = std::min(minimum,steps); maximum = std::max(maximum,steps);
            ++cases; instructions += steps; reads += memory.reads; writes += memory.writes;
            visited.push_back({input.current[0],input.current[1],input.current[2],
                               input.candidate[0],input.candidate[1],input.candidate[2]});
            return true;
        };
        if (!alternate_cases::probe(compare) || !alternate_cases::extended(compare)) return 1;
        if (argc == 3) {
            std::ifstream records(argv[2],std::ios::binary);
            std::array<char,8> magic{};
            if (!records.read(magic.data(),8) || magic != std::array<char,8>{'A','L','T','O','R','I','1','\0'})
                throw rt::Fatal("invalid alternate record header");
            live = read_word(records);
            if (!live || live > 16384) throw rt::Fatal("invalid alternate record count");
            for (unsigned i = 0; i < live; ++i) {
                Input input{}; Triple folded{}, result{};
                for (auto *triple : {&input.current,&input.candidate,&folded,&result})
                    for (auto &word : *triple) word = read_word(records);
                compare(input,&folded,&result);
            }
            if (records.peek() != std::char_traits<char>::eof()) throw rt::Fatal("trailing alternate records");
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr,"alternate oracle failed: %s\n",error.what()); return 1;
    }
    std::sort(visited.begin(),visited.end());
    const auto unique = std::unique(visited.begin(),visited.end()) - visited.begin();
    std::printf("NATIVE_ALTERNATE_ORACLE PASS cases=%llu unique_inputs=%llu rejected_native_domain=%llu live_records=%u instructions=%llu reads=%llu writes=%llu steps=%u..%u hash=%016llx\n",
        (unsigned long long)cases,(unsigned long long)unique,(unsigned long long)rejected,live,
        (unsigned long long)instructions,(unsigned long long)reads,(unsigned long long)writes,
        minimum,maximum,(unsigned long long)fingerprint);
    return 0;
}
