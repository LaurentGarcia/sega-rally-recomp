// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
// Map/reset contracts from MAME dddd73680656e355bb2b5beecab1167c9f07bf81:
// src/mame/sega/model2.cpp, model2_snd/model2snd_ctrl/reset_model2_scsp.
// SHA256 359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6
// This adapter has no SCSP emulation, UART, CPU cycle clock or wait states.
// Unknown access is an explicit bring-up frontier, not an invented value.
#include "runtime/model2a_sound_bus.h"
#include <algorithm>
#include <cstdio>

namespace snd {
namespace {
std::string describe(Model2ASoundBus::Frontier::Region region, bool write,
                     uint32_t address, uint16_t data, uint16_t mask)
{
    char text[128];
    std::snprintf(text, sizeof text, "unsupported sound %s %s address=%06x data=%04x mask=%04x",
                  region == Model2ASoundBus::Frontier::Region::Scsp ? "SCSP" : "unmapped",
                  write ? "write" : "read", unsigned(address), data, mask);
    return text;
}
void check_address(uint32_t address)
{
    if (address > 0xffffff || (address & 1))
        throw rt::Fatal("sound bus requires an aligned 24-bit address");
}
uint16_t word(std::span<const uint8_t> bytes, size_t offset)
{
    return uint16_t(uint16_t(bytes[offset]) << 8 | bytes[offset + 1]);
}
}

Model2ASoundBus::Frontier::Frontier(Region region_, bool write_, uint32_t address_,
                                  uint16_t data_, uint16_t mask_)
    : rt::Fatal(describe(region_, write_, address_, data_, mask_)), region(region_),
      write(write_), address(address_), data(data_), mask(mask_) {}

Model2ASoundBus::Model2ASoundBus(std::span<const uint8_t> program, std::span<const uint8_t> samples,
                               SoundImageSource *source)
    : program_(program), samples_(samples), source_(source)
{
    if ((source_ ? source_->size(SoundImage::Program) : program.size()) != PROGRAM_BYTES ||
        (source_ ? source_->size(SoundImage::Samples) : samples.size()) != SAMPLE_BYTES)
        throw rt::Fatal("Model2A Rally sound requires 512 KiB program and 8 MiB samples");
    reset();
}

void Model2ASoundBus::reset()
{
    if (source_) {
        for (unsigned i = 0; i < 16; i += 2) {
            const uint16_t value = source_->read16_be(SoundImage::Program, i);
            ram_[i] = uint8_t(value >> 8);
            ram_[i + 1] = uint8_t(value);
        }
    } else {
        std::copy_n(program_.begin(), 16, ram_.begin());
    }
}

uint16_t Model2ASoundBus::read16(uint32_t address, uint16_t mask)
{
    check_address(address);
    if (!mask) return 0;
    if (address < RAM_BYTES) return word(ram_, address) & mask;
    if (address >= 0x100000 && address < 0x101000) {
        if (!scsp_) throw Frontier(Frontier::Region::Scsp, false, address, 0, mask);
        return scsp_->read16(uint16_t(address - 0x100000), mask) & mask;
    }
    if (address >= 0x600000 && address < 0x680000)
        return (source_ ? source_->read16_be(SoundImage::Program, address - 0x600000)
                        : word(program_, address - 0x600000)) & mask;
    // Reset bank4 at samples+0x200000 and bank5 at samples+0x600000 makes
    // Rally's three CPU sample windows one contiguous 8 MiB image.
    if (address >= 0x800000)
        return (source_ ? source_->read16_be(SoundImage::Samples, address - 0x800000)
                        : word(samples_, address - 0x800000)) & mask;
    throw Frontier(Frontier::Region::Unmapped, false, address, 0, mask);
}

void Model2ASoundBus::write16(uint32_t address, uint16_t data, uint16_t mask)
{
    check_address(address);
    if (!mask) return;
    if (address < RAM_BYTES) {
        const uint16_t value = uint16_t((word(ram_, address) & ~mask) | (data & mask));
        ram_[address] = uint8_t(value >> 8);
        ram_[address + 1] = uint8_t(value);
        return;
    }
    if (address >= 0x100000 && address < 0x101000) {
        if (!scsp_) throw Frontier(Frontier::Region::Scsp, true, address, data, mask);
        scsp_->write16(uint16_t(address - 0x100000), data, mask);
        return;
    }
    if (address == 0x400000) return; // reference only banks when samples >8 MiB
    // Writes to these ROM-only mappings have no effect.
    if ((address >= 0x600000 && address < 0x680000) || address >= 0x800000) return;
    throw Frontier(Frontier::Region::Unmapped, true, address, data, mask);
}

} // namespace snd
