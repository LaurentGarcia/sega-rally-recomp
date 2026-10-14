// Native Model 2A address-space bring-up. The plain memory regions follow
// model2_base_mem/model2_tgp_mem/model2a_crx_mem in the pinned MAME reference.
// Unimplemented devices stop execution with the exact access; no ready-value
// guesses or Daytona ROM aliases are substituted for missing board behavior.
#pragma once

#include "runtime/cpu.h"
#include <functional>
#include <vector>

namespace rt {

class DeviceModel;

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
        uint64_t irq_reset_writes = 0;
    };
    Model2ABus(std::vector<uint8_t> program, std::vector<uint8_t> main_data);
    const Diagnostics &diagnostics() const { return diagnostics_; }
    std::vector<uint8_t> save_backup() const { return backup_; }
    void load_backup(const std::vector<uint8_t> &image) {
        if (image.size() != backup_.size()) throw Fatal("Model2A backup image must contain 16384 bytes");
        backup_ = image;
    }
    // Attach before CPU reset. The caller owns the shared native device model.
    void set_model(DeviceModel *model) { models_.clear(); add_model(model); }
    void add_model(DeviceModel *model) { if (model) models_.push_back(model); }
    // Host time is synchronized before each memory access and burst query.
    // The host must guard recursive accesses from device event callbacks.
    void set_before_access(std::function<void()> callback) { before_access_ = std::move(callback); }
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
    std::function<void()> before_access_;
    std::vector<DeviceModel *> models_;
    std::vector<uint8_t> program_, data_, ram_, work_, buffer_, cpu_control_, backup_;
};

} // namespace rt
