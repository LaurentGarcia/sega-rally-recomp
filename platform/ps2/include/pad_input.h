#pragma once
#include "controls.h"

namespace ps2 {
class PadInput {
public:
    PadInput() = default;
    ~PadInput();
    // The IOP writes into buffer_ until padPortClose completes. Its address
    // must stay fixed for the entire open-port lifetime.
    PadInput(const PadInput &) = delete;
    PadInput &operator=(const PadInput &) = delete;
    PadInput(PadInput &&) = delete;
    PadInput &operator=(PadInput &&) = delete;
    // Reset only when this host owns the IOP module set. A caller retaining a
    // loader's filesystem modules can opt out and must handle module conflicts.
    bool initialize(bool reset_iop = true);
    PadSnapshot poll();
    const char *status() const { return status_; }
private:
    bool opened_ = false, requested_analog_ = false;
    const char *status_ = "NOT INITIALIZED";
    alignas(64) unsigned char buffer_[256]{};
};
} // namespace ps2
