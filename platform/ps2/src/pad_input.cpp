#include "pad_input.h"
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <cstdio>

extern "C" {
extern unsigned char sio2man_irx[], padman_irx[];
extern unsigned int size_sio2man_irx, size_padman_irx;
}

namespace ps2 {
PadInput::~PadInput() {
    if (opened_) padPortClose(0, 0);
}

bool PadInput::initialize(bool reset_iop) {
    SifInitRpc(0);
    // A fresh open-source IOP module set avoids coupling input to the boot
    // loader's module versions. No firmware or proprietary SDK is bundled.
    if (reset_iop) {
        while (!SifIopReset("", 0)) {}
        while (!SifIopSync()) {}
    }
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    int result = -1;
    const int sio = SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, nullptr, &result);
    if (sio < 0 || result < 0) { status_ = "SIO MODULE FAILED"; return false; }
    const int pad = SifExecModuleBuffer(padman_irx, size_padman_irx, 0, nullptr, &result);
    if (pad < 0 || result < 0) { status_ = "PAD MODULE FAILED"; return false; }
    if (padInit(0) != 1 || !padPortOpen(0, 0, buffer_)) { status_ = "PAD OPEN FAILED"; return false; }
    opened_ = true;
    status_ = "WAITING FOR PAD";
    std::printf("PS2 IOP sio2man=%d padman=%d; controller port 1 open\n", sio, pad);
    return true;
}

PadSnapshot PadInput::poll() {
    PadSnapshot out;
    if (!opened_) return out;
    const int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        if (state == PAD_STATE_DISCONN) requested_analog_ = analog_pending_ = false;
        status_ = analog_pending_ && state == PAD_STATE_EXECCMD ?
            "WAITING FOR ANALOG MODE" : "CONNECT PAD PORT 1";
        return out;
    }
    if (analog_pending_) {
        const int request = padGetReqState(0, 0);
        if (request == PAD_RSTAT_BUSY) {
            status_ = "WAITING FOR ANALOG MODE";
            return out;
        }
        analog_pending_ = false;
        if (request != PAD_RSTAT_COMPLETE) {
            status_ = "ANALOG REQUEST RETRY";
            return out;
        }
        requested_analog_ = true;
    }
    if (!requested_analog_) {
        const int count = padInfoMode(0, 0, PAD_MODETABLE, -1);
        for (int i = 0; i < count; ++i) {
            if (padInfoMode(0, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK) {
                // Accepted means queued, not applied. The IOP can later
                // reject the request; only completion confirms this mode.
                analog_pending_ = padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) == 1;
                status_ = analog_pending_ ? "WAITING FOR ANALOG MODE" : "ANALOG REQUEST RETRY";
                return out;
            }
        }
        if (count > 0) {
            // A known mode table without DualShock needs no mode change.
            requested_analog_ = true;
            return out;
        }
        // Zero can mean the IOP is still negotiating capabilities. Read any
        // valid input now, but retry metadata on subsequent polls; legacy
        // digital pads with no table remain usable too.
    }
    padButtonStatus raw{};
    if (!padRead(0, 0, &raw)) { status_ = "PAD READ FAILED"; return out; }
    out.connected = true;
    const unsigned mode = raw.mode >> 4;
    out.analog = mode == PAD_TYPE_ANALOG || mode == PAD_TYPE_DUALSHOCK;
    out.steer = out.analog ? raw.ljoy_h : 128;
    const unsigned down = unsigned(raw.btns) ^ 0xffffu;
    const struct { unsigned sdk; uint16_t ours; } bindings[] = {
        {PAD_LEFT, Left}, {PAD_RIGHT, Right}, {PAD_CROSS, Cross}, {PAD_SQUARE, Square},
        {PAD_CIRCLE, Circle}, {PAD_TRIANGLE, Triangle}, {PAD_L1, L1}, {PAD_R1, R1},
        {PAD_SELECT, Select}, {PAD_START, Start},
    };
    for (const auto &binding : bindings) if (down & binding.sdk) out.buttons |= binding.ours;
    status_ = out.analog ? "DUALSHOCK ANALOG" : "DIGITAL PAD";
    return out;
}
} // namespace ps2
