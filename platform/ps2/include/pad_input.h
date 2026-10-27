#pragma once
#include "pad_state.h"

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
    // Last observed SDK states, not an independent connection verdict.
    int sdk_state() const { return sdk_state_; }
    int sdk_request_state() const { return sdk_request_state_; }
private:
    bool opened_ = false, requested_analog_ = false, analog_pending_ = false;
    const char *status_ = "NOT INITIALIZED";
    int sdk_state_ = -1, sdk_request_state_ = -1;
    alignas(64) unsigned char buffer_[256]{};
};
} // namespace ps2
