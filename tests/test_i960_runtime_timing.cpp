// Synthetic instruction/bus fixtures. Interpreter is a test oracle only.
#include "runtime/gen_support.h"
#include "refcore/i960_ref.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

// No external events in these one-instruction fixtures; the emitted boundary
// still enforces the exact instruction budget.
namespace rt {
Lockstep::Lockstep(Cpu &c) : core_(c) {}
bool Lockstep::apply() { return finished(); }
}

struct Memory : rt::Bus {
    struct Access {
        uint32_t address, value;
        unsigned width;
        bool write;
        uint64_t cycles;
        bool operator==(const Access &) const = default;
    };
    std::vector<uint8_t> bytes;
    std::vector<Access> events;
    const rt::Cpu *cpu = nullptr;
    bool burst = false;
    explicit Memory(const std::vector<uint8_t> &image) : bytes(image) {}
    uint32_t raw(uint32_t a, unsigned width) const {
        uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i) value |= uint32_t(bytes.at(a + i)) << (i * 8);
        return value;
    }
    void put(uint32_t a, uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) bytes.at(a + i) = uint8_t(value >> (i * 8));
    }
    uint32_t read(uint32_t a, unsigned width) {
        const uint32_t value = raw(a, width);
        events.push_back({a, value, width, false, cpu->reference_cycles});
        return value;
    }
    void write(uint32_t a, uint32_t value, unsigned width) {
        events.push_back({a, value, width, true, cpu->reference_cycles});
        put(a, value, width);
    }
    uint32_t fetch(uint32_t a) override { return raw(a, 4); }
    uint8_t read_byte(uint32_t a) override { return uint8_t(read(a, 1)); }
    uint16_t read_word(uint32_t a) override { return uint16_t(read(a, 2)); }
    uint32_t read_dword(uint32_t a) override { return read(a, 4); }
    void write_byte(uint32_t a, uint8_t v) override { write(a, v, 1); }
    void write_word(uint32_t a, uint16_t v) override { write(a, v, 2); }
    void write_dword(uint32_t a, uint32_t v) override { write(a, v, 4); }
    uint16_t flags(uint32_t) override { return burst ? rt::Cpu::BURST : 0; }
};

static void seed(rt::Cpu &cpu, uint32_t ip) {
    cpu.m_IP = ip;
    cpu.reference_cycles = 123;
    cpu.m_r[16] = 0xabcdef78;
    cpu.m_r[17] = 0x12345678;
    cpu.m_r[18] = 0x99999999;
    cpu.m_r[19] = 0x55555555;
    cpu.m_r[rt::I960_FP] = 0x8000;
    cpu.m_r[rt::I960_SP] = 0x8040;
}

static void one(rt::Cpu &native) {
    rt::Lockstep ls(native);
    ls.end_count = 1;
    gen::Env env{native, ls};
    while (!ls.finished()) gen::run(env);
}

static void compare(const std::vector<uint8_t> &image, uint32_t pc, unsigned cost, bool burst) {
    Memory reference_memory(image), native_memory(image);
    rt::I960Ref reference(&reference_memory);
    rt::Cpu native(&native_memory);
    reference_memory.cpu = &reference;
    native_memory.cpu = &native;
    reference_memory.burst = native_memory.burst = burst;
    seed(reference, pc);
    seed(native, pc);
    if (pc == 0x200) {
        reference.m_rcache_pos = native.m_rcache_pos = 1;
        reference.m_rcache[0][rt::I960_RIP] = native.m_rcache[0][rt::I960_RIP] = 0x104;
        reference.m_r[rt::I960_PFP] = native.m_r[rt::I960_PFP] = 0x8000;
    }
    if (pc == 0x300) {
        reference.m_r[16] = native.m_r[16] = 0xff000010;
        reference.m_r[18] = native.m_r[18] = 0x4000;
        for (auto *mem : {&reference_memory, &native_memory}) {
            mem->put(0x4000, 0x93000000);
            mem->put(0x4004, 0x100);
            mem->put(0x4008, 0x200);
            mem->put(0x400c, 0x40);
        }
    }
    reference.execute_one();
    one(native);
    assert(reference.reference_cycles == 123 + cost && native.reference_cycles == reference.reference_cycles);
    assert(reference.m_IP == native.m_IP && reference.m_AC == native.m_AC);
    assert(std::memcmp(reference.m_r, native.m_r, sizeof(native.m_r)) == 0);
    assert(reference_memory.events == native_memory.events);
    assert(reference_memory.bytes == native_memory.bytes);
    // All memory side effects see the full cost, including multiword accesses.
    for (const auto &event : native_memory.events) assert(event.cycles == 123 + cost);
    if (pc == 0x300) assert(native.m_IP == 0x40); // IAC reinit does not rewind elapsed time
}

static void helper_and_reset(const std::vector<uint8_t> &image) {
    Memory memory(image);
    rt::Cpu cpu(&memory);
    memory.cpu = &cpu;
    seed(cpu, 0x104);
    cpu.m_rcache_pos = 4; // force real frame spill/reload accesses
    cpu.do_call(0x200, 0, 0x8040);
    assert(cpu.reference_cycles == 132 && memory.events.size() == 16);
    for (const auto &event : memory.events) assert(event.write && event.cycles == 132);
    memory.events.clear();
    cpu.do_ret();
    assert(cpu.reference_cycles == 139 && cpu.m_IP == 0x104 && memory.events.size() == 16);
    for (const auto &event : memory.events) assert(!event.write && event.cycles == 139);
    memory.events.clear();
    memory.put(0, 0x100);
    memory.put(4, 0x200);
    memory.put(12, 0x40);
    memory.put(0x200 + 24, 0x8000);
    cpu.reset();
    assert(cpu.reference_cycles == 139 && cpu.m_IP == 0x40);
    for (const auto &event : memory.events) assert(event.cycles == 139);
    cpu.consume_reference_cycles(0);
    assert(cpu.reference_cycles == 139);
    cpu.reference_cycles = UINT64_MAX;
    bool overflow = false;
    try { cpu.consume_reference_cycles(1); }
    catch (const rt::Fatal &) { overflow = true; }
    assert(overflow && cpu.reference_cycles == UINT64_MAX);
}

static void interrupt_helpers(const std::vector<uint8_t> &image) {
    Memory memory(image);
    rt::Cpu cpu(&memory);
    memory.cpu = &cpu;
    seed(cpu, 0x104);
    cpu.m_PRCB = 0x4000;
    memory.put(0x4014, 0x4100); // interrupt table
    memory.put(0x4018, 0x8000); // interrupt stack
    memory.put(0x4124, 0x200); // vector8 handler
    cpu.take_interrupt(8, 1);
    assert(cpu.reference_cycles == 132 && cpu.m_IP == 0x200);
    unsigned before_call = 0, after_call = 0;
    for (const auto &event : memory.events) {
        if (event.write) { assert(event.cycles == 132); ++after_call; }
        else { assert(event.cycles == 123); ++before_call; }
    }
    assert(before_call == 3 && after_call == 3);
    memory.events.clear();
    cpu.do_ret(); // interrupt return charges7 before state/pending-table reads
    assert(cpu.reference_cycles == 139 && cpu.m_IP == 0x104);
    for (const auto &event : memory.events) assert(event.cycles == 139);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<uint8_t> image{std::istreambuf_iterator<char>(input), {}};
    assert(image.size() == 0x10000);
    for (bool burst : {false, true}) {
        compare(image, 0x40, 4, burst);
        compare(image, 0x50, 2, burst);
        compare(image, 0x60, 1, burst);
        compare(image, 0x70, 7, burst);
        compare(image, 0x80, 5, burst);
        compare(image, 0x100, 9, burst);
        compare(image, 0x200, 7, burst);
        compare(image, 0x300, 12, burst);
    }
    helper_and_reset(image);
    interrupt_helpers(image);
    std::puts("Native/reference timing: MMIO timestamps, bursts, helper costs, reset/IAC continuity and overflow passed");
}
