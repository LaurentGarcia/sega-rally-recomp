// license:BSD-3-Clause
// copyright-holders:Ben Templeman and contributors
#pragma once

#include "runtime/snd_cpu.h"
#include <span>
#include <vector>

namespace snd {

// Model 2A SCSP board, fixed to Rally's 512 KiB program / 8 MiB sample map.
// Image spans and the optional chip must outlive this bus. CPU timing and
// SCSP behavior belong to the host/chip; accesses do not invent either.
class Model2ASoundBus final : public AddressSpace {
public:
    static constexpr size_t RAM_BYTES = 0x80000, PROGRAM_BYTES = 0x80000,
                            SAMPLE_BYTES = 0x800000;
    class Scsp {
    public:
        virtual ~Scsp() = default;
        // Aligned byte offsets in the SCSP aperture, active-high word masks.
        virtual uint16_t read16(uint16_t offset, uint16_t mask) = 0;
        virtual void write16(uint16_t offset, uint16_t data, uint16_t mask) = 0;
    };
    struct Frontier : rt::Fatal {
        enum class Region { Scsp, Unmapped };
        Region region;
        bool write;
        uint32_t address;
        uint16_t data, mask;
        Frontier(Region region, bool write, uint32_t address, uint16_t data, uint16_t mask);
    };

    Model2ASoundBus(std::span<const uint8_t> program, std::span<const uint8_t> samples);
    // Soft reset preserves RAM except for the reference's 16-byte vector copy.
    void reset();
    void set_scsp(Scsp *chip) { scsp_ = chip; }
    std::span<uint8_t> sound_ram() { return ram_; }
    std::span<const uint8_t> sound_ram() const { return ram_; }
    uint16_t read16(uint32_t address, uint16_t mask) override;
    void write16(uint32_t address, uint16_t data, uint16_t mask) override;

private:
    std::span<const uint8_t> program_, samples_;
    std::vector<uint8_t> ram_ = std::vector<uint8_t>(RAM_BYTES, 0);
    Scsp *scsp_ = nullptr;
};

} // namespace snd
