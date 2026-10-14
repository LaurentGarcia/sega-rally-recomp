#include "runtime/model2a_bus.h"
#include <cstdio>
#include <utility>

namespace rt {
namespace {
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
      work_(0x100000), buffer_(0x20000), cpu_control_(0x38), backup_(0x4000)
{
    // The verified srallycb manifest loads 1 MiB/12 MiB into these zero-filled
    // ROM regions. The high data window aliases +16 MiB of the same region.
    if (program_.size() > 0x200000 || data_.size() > 0x2400000)
        throw Fatal("Model 2A ROM image exceeds its mapped region");
    program_.resize(0x200000, 0);
    data_.resize(0x2400000, 0);
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
    Mapping m = mapping(address);
    if (!m.data || m.available < width) throw Model2AAccess(address, width, false, 0);
    uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i) result |= uint32_t(m.data[i]) << (8 * i);
    return result;
}

void Model2ABus::write(uint32_t address, unsigned width, uint32_t value)
{
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
uint16_t Model2ABus::flags(uint32_t address) { return mapping(address).burst ? Cpu::BURST : 0; }

} // namespace rt
