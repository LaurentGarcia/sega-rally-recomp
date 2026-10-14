// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels
// Address/mask/reset and SCSP UART clock wiring from MAME model2.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81,
// SHA256359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6.
// Framework replaced by DeviceModel; serial peer/clock supplied externally.
// No SCSP or 68000 execution and no fabricated sound acknowledgement.
#include "runtime/model2a_uart.h"

namespace rt {
namespace { constexpr uint32_t kUART = 0x01c80000; }

bool Model2AUART::claims(uint32_t address) const { return address >= kUART && address <= kUART + 3; }
DeviceModel::Check Model2AUART::read(uint32_t address, uint32_t mask, uint32_t &value) {
    if (address != kUART) throw Fatal("Model2A UART access outside its aligned mapped word");
    value = 0;
    if (mask & 0xff) value |= chip_.data_read();
    if (mask & 0xff0000) value |= uint32_t(chip_.status_read()) << 16;
    value &= mask;
    return Strict;
}
void Model2AUART::write(uint32_t address, uint32_t data, uint32_t mask) {
    if (address != kUART) throw Fatal("Model2A UART access outside its aligned mapped word");
    if (mask & 0xff) chip_.data_write(uint8_t(data));
    if (mask & 0xff0000) chip_.control_write(uint8_t(data >> 16));
}
void Model2AUART::measured_mismatch(uint32_t, uint32_t, uint32_t) {
    throw Divergence("Strict Model2A UART unexpectedly received a measured mismatch");
}
} // namespace rt
