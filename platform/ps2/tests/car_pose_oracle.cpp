// Host-only owned-ROM oracle. Never linked into the native game.
#include "car_pose.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using namespace ps2::native;
constexpr std::uint32_t base = 0x200000, node = base + 64, pose_address = base + 512;
constexpr std::uint32_t entry = 0x5cac08, end = 0x5cace8, sentinel = 0xdead00;
constexpr unsigned attributes[] = {0x74, 0x80, 0x84, 0x64, 0x68, 0x6c, 0x70, 0xb4, 0xb8, 0xbc, 0xc0};
struct Write {
    std::uint32_t address, value;
    unsigned width;
    bool operator==(const Write &) const = default;
};
struct Memory : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::array<std::uint8_t, 1024> ram{};
    std::vector<Write> writes;
    explicit Memory(const std::vector<std::uint8_t> &image) : program(image) {
        ram.fill(0x5a);
        writes.reserve(24);
    }
    std::uint32_t fetch(std::uint32_t address) override {
        if (address < entry || address >= end || (address & 3)) throw rt::Fatal("pose oracle escaped function");
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(program.at(address - 0x59f000 + i)) << (8 * i);
        return value;
    }
    std::uint8_t read_byte(std::uint32_t address) override { return ram.at(address - base); }
    std::uint16_t read_word(std::uint32_t address) override {
        return read_byte(address) | (std::uint16_t(read_byte(address + 1)) << 8);
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        return read_word(address) | (std::uint32_t(read_word(address + 2)) << 16);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) ram.at(address - base + i) = std::uint8_t(value >> (i * 8));
    }
    void write_byte(std::uint32_t address, std::uint8_t value) override {
        writes.push_back({address, value, 1}); put(address, value, 1);
    }
    void write_word(std::uint32_t, std::uint16_t) override { throw rt::Fatal("unexpected pose halfword write"); }
    void write_dword(std::uint32_t address, std::uint32_t value) override {
        writes.push_back({address, value, 4}); put(address, value);
    }
    std::uint16_t flags(std::uint32_t) override { return rt::Cpu::BURST; }
};
std::uint32_t random_state = 0xa519b723;
std::uint32_t random_word() {
    random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5;
    return random_state;
}
std::array<std::uint32_t, 20> flatten(const CarPoseWords &words) {
    std::array<std::uint32_t, 20> result;
    std::copy(words.position.begin(), words.position.end(), result.begin());
    std::copy(words.motion.begin(), words.motion.end(), result.begin() + 3);
    std::copy(words.orientation.begin(), words.orientation.end(), result.begin() + 6);
    std::copy(words.attributes.begin(), words.attributes.end(), result.begin() + 9);
    return result;
}
std::uint64_t fingerprint = 14695981039346656037ull;
unsigned cases = 0, instructions = 0;
bool compare(const std::vector<std::uint8_t> &program, unsigned source_byte, unsigned old_byte) {
    CarPoseState state;
    for (auto &v : state.words.position) v = random_word();
    for (auto &v : state.words.motion) v = random_word();
    for (auto &v : state.words.orientation) v = random_word();
    for (auto &v : state.words.attributes) v = random_word();
    state.flags = (random_word() & 0xffff00ff) | (source_byte << 8);
    state.progress = random_word();
    RenderPose pose;
    pose.flags = (random_word() & 0xffff00ff) | (old_byte << 8);
    Memory actual(program), expected(program);
    const auto input = flatten(state.words);
    for (unsigned i = 0; i < 3; ++i) {
        actual.put(node + 0x14 + i * 4, input[i]);
        actual.put(node + 0x2c + i * 4, input[i + 3]);
        actual.put(node + 0x38 + i * 4, input[i + 6]);
    }
    for (unsigned i = 0; i < 11; ++i) actual.put(node + attributes[i], input[i + 9]);
    actual.put(node + 0x10, state.flags);
    actual.put(node + 0x88, state.progress);
    actual.put(node + 0x8c, pose_address);
    actual.put(pose_address + 0x50, pose.flags);
    expected.ram = actual.ram;
    publish_car_pose(state, pose);
    const auto output = flatten(pose.words);
    for (unsigned i = 0; i < 20; ++i) expected.put(pose_address + i * 4, output[i]);
    expected.put(node + 0x10, state.flags);
    expected.put(pose_address + 0x50, pose.flags);
    expected.put(pose_address + 0x54, pose.progress);
    rt::I960Ref reference(&actual);
    for (auto &v : reference.m_r) v = random_word();
    reference.m_r[16] = node; reference.m_r[30] = sentinel; reference.m_IP = entry;
    unsigned executed = 0;
    while (reference.m_IP != sentinel) {
        if (++executed > 56) return false;
        reference.execute_one();
    }
    std::vector<Write> expected_writes;
    for (unsigned i = 0; i < 9; ++i) expected_writes.push_back({pose_address + i * 4, output[i], 4});
    for (unsigned i : {11u, 10u, 9u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u})
        expected_writes.push_back({pose_address + i * 4, output[i], 4});
    expected_writes.push_back({node + 17, (state.flags >> 8) & 255, 1});
    expected_writes.push_back({pose_address + 0x50, state.flags, 4});
    expected_writes.push_back({pose_address + 0x54, state.progress, 4});
    expected_writes.push_back({pose_address + 0x50, pose.flags & 255, 1});
    if (executed != 56 || actual.ram != expected.ram || actual.writes != expected_writes || reference.m_r[30])
        return false;
    for (const auto &write : actual.writes) {
        fingerprint = (fingerprint ^ write.address) * 1099511628211ull;
        fingerprint = (fingerprint ^ write.value) * 1099511628211ull;
        fingerprint = (fingerprint ^ write.width) * 1099511628211ull;
    }
    ++cases; instructions += executed;
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> program{std::istreambuf_iterator<char>(stream), {}};
    if (program.size() != 2097152) return 2;
    try {
        for (unsigned source = 0; source < 256; ++source)
            for (unsigned old = 0; old < 256; ++old)
                if (!compare(program, source, old)) return 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "pose oracle failed: %s\n", error.what());
        return 1;
    }
    std::printf("NATIVE_POSE_ORACLE PASS cases=%u instructions=%u writes=%u hash=%016llx\n",
                cases, instructions, cases * 24, (unsigned long long)fingerprint);
    return 0;
}
