// Native Model 2A address-space bring-up. The plain memory regions follow
// model2_base_mem/model2_tgp_mem/model2a_crx_mem in the pinned MAME reference.
// Unimplemented devices stop execution with the exact access; no ready-value
// guesses or Daytona ROM aliases are substituted for missing board behavior.
#pragma once

#include "runtime/cpu.h"
#include <vector>

namespace rt {

struct Model2AAccess : Fatal {
    uint32_t address, value;
    unsigned width;
    bool write;
    Model2AAccess(uint32_t address, unsigned width, bool write, uint32_t value);
};

class Model2ABus final : public Bus {
public:
    struct Diagnostics {
        uint64_t reference_gap_reads = 0, reference_gap_writes = 0;
        uint32_t reference_gap_last_write = 0;
    };
    Model2ABus(std::vector<uint8_t> program, std::vector<uint8_t> main_data);
    const Diagnostics &diagnostics() const { return diagnostics_; }
    // Width accesses stay within one mapping. Cpu's unaligned instruction
    // helpers split into byte accesses, preserving mirror/boundary behavior.
    uint32_t fetch(uint32_t address) override;
    uint8_t read_byte(uint32_t address) override;
    uint16_t read_word(uint32_t address) override;
    uint32_t read_dword(uint32_t address) override;
    void write_byte(uint32_t address, uint8_t value) override;
    void write_word(uint32_t address, uint16_t value) override;
    void write_dword(uint32_t address, uint32_t value) override;
    uint16_t flags(uint32_t address) override;

private:
    struct Mapping { uint8_t *data; size_t available; bool read_only; bool burst; };
    Mapping mapping(uint32_t address);
    uint32_t read(uint32_t address, unsigned width);
    void write(uint32_t address, unsigned width, uint32_t value);
    Diagnostics diagnostics_;
    std::vector<uint8_t> program_, data_, ram_, work_, buffer_, cpu_control_, backup_;
};

} // namespace rt
