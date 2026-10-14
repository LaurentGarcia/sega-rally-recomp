#include "runtime/model2a_bus.h"
#include "runtime/m2_replay_bus.h"
#include <cstdio>
#include <utility>

namespace rt {
namespace {
bool reference_boot_gap(uint32_t address, unsigned width)
{
    // Reset reads/ORs/writes a byte here. This range has no device in pinned
    // MAME's Model 2A map: address_map defaults to unmap=0 and an unmapped
    // write has no effect. Restrict compatibility to this observed gap so
    // unsupported real devices still stop. PCB behavior remains unverified.
    const bool reset_gap = address >= 0x00f80000 && uint64_t(address) + width <= 0x00f80004;
    // ROM 0x1c64 probes a location inside the original Model 2 DPRAM. The
    // Model 2A map has only 0x1c00000..1f plus a no-op at 0x1c00040..43:
    // this observed probe is unmapped, not an emulated DPRAM mailbox.
    const bool legacy_io_probe = address >= 0x01c00200 && uint64_t(address) + width <= 0x01c00204;
    return reset_gap || legacy_io_probe;
}

std::string access_message(uint32_t address, unsigned width, bool write, uint32_t value)
{
    char message[160];
    std::snprintf(message, sizeof(message),
        "unimplemented Model 2A %s%u at 0x%08x (value 0x%08x)",
        write ? "write" : "read", width * 8, address, value);
    return message;
}
}

Model2AAccess::Model2AAccess(uint32_t a, unsigned w, bool wr, uint32_t v)
    : Fatal(access_message(a, w, wr, v)), address(a), value(v), width(w), write(wr) {}

Model2ABus::Model2ABus(std::vector<uint8_t> program, std::vector<uint8_t> data)
    : program_(std::move(program)), data_(std::move(data)), ram_(0x40000),
      work_(0x100000), buffer_(0x20000), cpu_control_(0x38), backup_(0x4000, 0xff)
{
    // The verified srallycb manifest loads 1 MiB/12 MiB into these zero-filled
    // ROM regions. The high data window aliases +16 MiB of the same region.
    if (program_.size() > 0x200000 || data_.size() > 0x2400000)
        throw Fatal("Model 2A ROM image exceeds its mapped region");
    program_.resize(0x200000, 0);
    data_.resize(0x2400000, 0);
    // model2a_state inherits model2_state::machine_reset's display-list end
    // pattern; fresh backup1 NVRAM uses DEFAULT_ALL_1, not cleared RAM.
    for (size_t offset = 0; offset < buffer_.size(); offset += 4) {
        buffer_[offset] = 0x0f;
        buffer_[offset + 1] = 0x0f;
        buffer_[offset + 2] = 0x80;
        buffer_[offset + 3] = 0x07;
    }
}

Model2ABus::Mapping Model2ABus::mapping(uint32_t address)
{
    auto region = [](std::vector<uint8_t> &bytes, size_t offset, bool rom, bool burst) {
        return Mapping{bytes.data() + offset, bytes.size() - offset, rom, burst};
    };
    if (address <= 0x001fffff) return region(program_, address, true, true);
    if (address >= 0x00200000 && address <= 0x0023ffff)
        return region(ram_, address - 0x00200000, false, true);
    if (address >= 0x00500000 && address <= 0x005fffff)
        return region(work_, address - 0x00500000, false, true);
    if (address >= 0x00900000 && address <= 0x0097ffff)
        return region(buffer_, (address - 0x00900000) & 0x1ffff, false, true);
    if (address >= 0x00e00000 && address <= 0x00e00037)
        return region(cpu_control_, address - 0x00e00000, false, false);
    if (address >= 0x01d00000 && address <= 0x01d03fff)
        return region(backup_, address - 0x01d00000, false, true);
    if (address >= 0x02000000 && address <= 0x03ffffff) {
        auto result = region(data_, address - 0x02000000, true, true);
        result.available = size_t(0x04000000) - address;
        return result;
    }
    if (address >= 0x06000000 && address <= 0x06ffffff) {
        auto result = region(data_, address - 0x06000000 + 0x1000000, true, true);
        result.available = size_t(0x07000000) - address;
        return result;
    }
    return {nullptr, 0, false, false};
}

uint32_t Model2ABus::read(uint32_t address, unsigned width)
{
    if (model_ && model_->claims(address)) {
        const unsigned lane = address & 3;
        if (lane + width > 4 || !model_->claims(address + width - 1))
            throw Model2AAccess(address, width, false, 0);
        const uint32_t mask = uint32_t((uint64_t(1) << (width * 8)) - 1);
        uint32_t value = 0;
        (void)model_->read(address & ~3u, mask << (lane * 8), value);
        return (value >> (lane * 8)) & mask;
    }
    // IRQ request/enable reset to zero. Enabling any source remains fenced
    // below until the board clock and asynchronous sources are connected.
    if (address >= 0x00e80000 && uint64_t(address) + width <= 0x00e80008)
        return 0;
    if (reference_boot_gap(address, width)) {
        ++diagnostics_.reference_gap_reads;
        return 0;
    }
    Mapping m = mapping(address);
    if (!m.data || m.available < width) throw Model2AAccess(address, width, false, 0);
    uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i) result |= uint32_t(m.data[i]) << (8 * i);
    return result;
}

void Model2ABus::write(uint32_t address, unsigned width, uint32_t value)
{
    if (model_ && model_->claims(address)) {
        const unsigned lane = address & 3;
        if (lane + width > 4 || !model_->claims(address + width - 1))
            throw Model2AAccess(address, width, true, value);
        const uint32_t mask = uint32_t((uint64_t(1) << (width * 8)) - 1);
        model_->write(address & ~3u, (value & mask) << (lane * 8), mask << (lane * 8));
        return;
    }
    if (address >= 0x00e80000 && uint64_t(address) + width <= 0x00e80008) {
        // Acknowledge only clears already-pending bits. With no enabled
        // sources, request stays zero. A zero mask write is also idempotent
        // under MAME's delayed mask update; nonzero enable needs timing.
        if (uint64_t(address) + width <= 0x00e80004 || value == 0) {
            ++diagnostics_.irq_reset_writes;
            return;
        }
        throw Model2AAccess(address, width, true, value);
    }
    if (reference_boot_gap(address, width)) {
        ++diagnostics_.reference_gap_writes;
        diagnostics_.reference_gap_last_write = value;
        return;
    }
    Mapping m = mapping(address);
    if (!m.data || m.available < width) throw Model2AAccess(address, width, true, value);
    if (m.read_only) return;
    for (unsigned i = 0; i < width; ++i) m.data[i] = uint8_t(value >> (8 * i));
}

uint32_t Model2ABus::fetch(uint32_t address) { return read(address, 4); }
uint8_t Model2ABus::read_byte(uint32_t address) { return uint8_t(read(address, 1)); }
uint16_t Model2ABus::read_word(uint32_t address) { return uint16_t(read(address, 2)); }
uint32_t Model2ABus::read_dword(uint32_t address) { return read(address, 4); }
void Model2ABus::write_byte(uint32_t a, uint8_t v) { write(a, 1, v); }
void Model2ABus::write_word(uint32_t a, uint16_t v) { write(a, 2, v); }
void Model2ABus::write_dword(uint32_t a, uint32_t v) { write(a, 4, v); }
uint16_t Model2ABus::flags(uint32_t address) {
    // Program and function ports advance on multiword transfers. FIFO ports
    // repeat the same address and consume/produce one word per access.
    if ((address >= 0x00804000 && address <= 0x00807fff) ||
        (address >= 0x00880000 && address <= 0x00883fff)) return Cpu::BURST;
    return mapping(address).burst ? Cpu::BURST : 0;
}

} // namespace rt
