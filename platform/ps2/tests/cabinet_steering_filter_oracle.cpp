// Offline comparison with the owner's original program; never a native source.
#include "cabinet_steering_filter.h"
#include "cabinet_steering_filter_cases.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
constexpr std::uint32_t entry = 0x39a90, stop = 0x39ae8;
struct Memory final : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::uint32_t history = 0;
    std::uint8_t sample = 0;
    unsigned byte_reads = 0, word_reads = 0, writes = 0;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address & 3) || !((address >= entry && address < 0x39aec) ||
                              (address >= 0x395d8 && address < 0x39668)))
            throw rt::Fatal("filter oracle escaped verified instruction ranges");
        return program.at(address) | (std::uint32_t(program.at(address + 1)) << 8) |
               (std::uint32_t(program.at(address + 2)) << 16) | (std::uint32_t(program.at(address + 3)) << 24);
    }
    std::uint8_t read_byte(std::uint32_t address) override {
        if (address != 0x202050) throw rt::Fatal("unexpected filter byte read");
        ++byte_reads; return sample;
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        if (address != 0x2142c0) throw rt::Fatal("unexpected filter history read");
        ++word_reads; return history;
    }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        if (address != 0x2142c0) throw rt::Fatal("unexpected filter write");
        ++writes; history = value;
    }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected halfword read"); }
    void write_byte(std::uint32_t,std::uint8_t) override { throw rt::Fatal("unexpected byte write"); }
    void write_word(std::uint32_t,std::uint16_t) override { throw rt::Fatal("unexpected halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return 0; }
};
std::uint32_t read_word(std::ifstream &input) {
    std::array<unsigned char,4> bytes{};
    if (!input.read(reinterpret_cast<char *>(bytes.data()),4)) throw rt::Fatal("truncated filter record");
    return bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) |
           (std::uint32_t(bytes[3]) << 24);
}
}
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) return 2;
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(file),{}};
    if (program.size() != 2097152) return 2;
    std::uint64_t count = 0, instructions = 0, hash = 14695981039346656037ull;
    unsigned minimum = 1000, maximum = 0, live = 0;
    std::vector<std::array<std::uint32_t,2>> visited;
    try {
        Memory memory(program);
        rt::I960Ref original(&memory);
        const auto test = [&](steering_filter_cases::Input input, std::uint32_t *next = nullptr,
                              const std::array<std::uint32_t,2> *captured = nullptr) {
            ps2::native::CabinetSteeringUpdate native{};
            if (!ps2::native::filter_cabinet_steering(std::bit_cast<float>(input.history),input.sample,native))
                throw rt::Fatal("filter input outside declared native domain");
            memory.history = input.history; memory.sample = input.sample;
            memory.byte_reads = memory.word_reads = memory.writes = 0;
            std::fill(std::begin(original.m_r),std::end(original.m_r),std::uint32_t(count) ^ 0x4913a0deu);
            std::fill(std::begin(original.m_fp),std::end(original.m_fp),-13.5);
            original.m_AC = 0x3f001000; original.m_IP = entry;
            unsigned steps = 0;
            while (original.m_IP != stop) {
                if (++steps > 128) throw rt::Fatal("filter instruction bound exceeded");
                original.execute_one();
            }
            const auto history = std::bit_cast<std::uint32_t>(native.history);
            const auto steering = std::bit_cast<std::uint32_t>(native.steering);
            if (history != memory.history || steering != original.m_r[16] ||
                history != steering_filter_cases::mathematical(input) ||
                memory.byte_reads != 1 || memory.word_reads != 1 || memory.writes != 1 ||
                (history & 0x7fffffffu) > 0x3f800000u) {
                std::fprintf(stderr,"steering filter original mismatch history=%08x sample=%u\n",input.history,unsigned(input.sample));
                return false;
            }
            if (captured && *captured != std::array<std::uint32_t,2>{history,steering})
                throw rt::Fatal("captured filter result differs from native/original replay");
            for (auto value : {input.history,std::uint32_t(input.sample),history,steering})
                hash = (hash ^ value) * 1099511628211ull;
            instructions += steps; minimum = std::min(minimum,steps); maximum = std::max(maximum,steps);
            ++count; visited.push_back({input.history,input.sample});
            if (next) *next = memory.history;
            return true;
        };
        if (!steering_filter_cases::probe(test) ||
            !steering_filter_cases::sequences([&](auto input,auto &next){return test(input,&next);},512) ||
            !steering_filter_cases::extended(test) ||
            !steering_filter_cases::sequences([&](auto input,auto &next){return test(input,&next);},4096)) return 1;
        if (argc == 3) {
            std::ifstream records(argv[2],std::ios::binary);
            std::array<char,8> magic{};
            if (!records.read(magic.data(),8) || magic != std::array<char,8>{'S','T','F','I','L','T','1','\0'})
                throw rt::Fatal("invalid filter record header");
            live = read_word(records);
            if (!live || live > 16384) throw rt::Fatal("invalid filter record count");
            for (unsigned i = 0; i < live; ++i) {
                const auto old = read_word(records), sample = read_word(records);
                const std::array<std::uint32_t,2> result{read_word(records),read_word(records)};
                if (sample > 255 || !test({old,std::uint8_t(sample)},nullptr,&result)) return 1;
            }
            if (records.peek() != std::char_traits<char>::eof()) throw rt::Fatal("trailing filter data");
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr,"steering filter oracle failed: %s\n",error.what()); return 1;
    }
    std::sort(visited.begin(),visited.end());
    const auto unique = std::unique(visited.begin(),visited.end()) - visited.begin();
    std::printf("NATIVE_STEERING_FILTER_ORACLE PASS cases=%llu unique_inputs=%llu sequence_updates=18432 live_records=%u instructions=%llu reads=%llu writes=%llu steps=%u..%u hash=%016llx\n",
        (unsigned long long)count,(unsigned long long)unique,live,(unsigned long long)instructions,
        (unsigned long long)(count*2),(unsigned long long)count,minimum,maximum,(unsigned long long)hash);
    return 0;
}
