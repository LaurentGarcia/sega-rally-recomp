// Exercise the real PadInput implementation against a bounded fake SDK.
// These checks establish ownership/retry behavior, not physical pad operation.
#include "pad_input.h"
#include <libpad.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {
void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
struct FakeSdk {
    int module_result = 0, init_result = 1, open_result = 1;
    int state = PAD_STATE_STABLE, mode_result = 1;
    int request_state = PAD_RSTAT_COMPLETE, mode_count = 1;
    int opens = 0, closes = 0, requests = 0, reads = 0;
    bool dualshock = true;
    void *dma_buffer = nullptr;
} sdk;
void reset() { sdk = {}; }
}

extern "C" {
unsigned char sio2man_irx[1]{}, padman_irx[1]{};
unsigned int size_sio2man_irx = 1, size_padman_irx = 1;
}
void SifInitRpc(int) {}
int SifIopReset(const char *, int) { return 1; }
int SifIopSync() { return 1; }
int sbv_patch_enable_lmb() { return 0; }
int SifExecModuleBuffer(void *, int, int, const char *, int *result) {
    *result = sdk.module_result;
    return sdk.module_result;
}
int padInit(int) { return sdk.init_result; }
int padPortOpen(int port, int slot, void *buffer) {
    check(port == 0 && slot == 0, "opens intended port");
    check(reinterpret_cast<std::uintptr_t>(buffer) % 64 == 0, "DMA buffer alignment");
    ++sdk.opens;
    if (sdk.open_result) sdk.dma_buffer = buffer;
    return sdk.open_result;
}
int padPortClose(int port, int slot) {
    check(port == 0 && slot == 0 && sdk.dma_buffer, "only closes an opened port");
    // Simulate the final IOP write during synchronous close. ASan must still
    // consider the complete buffer alive while the destructor waits for close.
    std::memset(sdk.dma_buffer, 0x5a, 256);
    sdk.dma_buffer = nullptr;
    ++sdk.closes;
    return 1;
}
int padGetState(int, int) { return sdk.state; }
int padGetReqState(int, int) { return sdk.request_state; }
int padInfoMode(int, int, int mode, int index) {
    check(mode == PAD_MODETABLE, "queries supported modes");
    return index == -1 ? sdk.mode_count : (sdk.dualshock ? PAD_TYPE_DUALSHOCK : PAD_TYPE_DIGITAL);
}
int padSetMainMode(int, int, int mode, int lock) {
    check(mode == PAD_MMODE_DUALSHOCK && lock == PAD_MMODE_LOCK, "requests locked analog mode");
    ++sdk.requests;
    return sdk.mode_result;
}
int padRead(int, int, padButtonStatus *raw) {
    ++sdk.reads;
    raw->mode = (sdk.dualshock ? PAD_TYPE_DUALSHOCK : PAD_TYPE_DIGITAL) << 4;
    raw->ljoy_h = 17;
    raw->btns = static_cast<std::uint16_t>(0xffffu ^ PAD_CROSS);
    return 1;
}

int main() {
    static_assert(!std::is_copy_constructible_v<ps2::PadInput>);
    static_assert(!std::is_copy_assignable_v<ps2::PadInput>);
    static_assert(!std::is_move_constructible_v<ps2::PadInput>);
    static_assert(!std::is_move_assignable_v<ps2::PadInput>);
    try {
        reset();
        { ps2::PadInput pad; check(!pad.poll().connected, "uninitialized pad is neutral");
          check(pad.sdk_state() == -1 && pad.sdk_request_state() == -1, "unobserved SDK state is explicit"); }
        check(sdk.closes == 0, "uninitialized pad is not closed");
        reset(); sdk.module_result = -1;
        { ps2::PadInput pad; check(!pad.initialize(), "module failure reported"); }
        check(sdk.opens == 0 && sdk.closes == 0, "module failure opens or closes no port");
        reset(); sdk.init_result = 0;
        { ps2::PadInput pad; check(!pad.initialize(), "padInit failure reported"); }
        check(sdk.opens == 0 && sdk.closes == 0, "init failure opens or closes no port");
        reset(); sdk.open_result = 0;
        { ps2::PadInput pad; check(!pad.initialize(), "open failure reported"); }
        check(sdk.opens == 1 && sdk.closes == 0, "failed open is not closed");

        reset();
        {
            ps2::PadInput pad;
            check(pad.initialize(), "diagnostic controller initialization");
            const int states[]{PAD_STATE_DISCONN, PAD_STATE_EXECCMD, PAD_STATE_ERROR};
            for (int state : states) {
                sdk.state = state; sdk.request_state = PAD_RSTAT_BUSY;
                check(!pad.poll().connected && pad.sdk_state() == state &&
                      pad.sdk_request_state() == PAD_RSTAT_BUSY && sdk.reads == 0 && sdk.requests == 0,
                      "numeric diagnostics distinguish non-ready states without input or mode requests");
            }
        }
        reset();
        {
            ps2::PadInput pad;
            check(pad.initialize(false), "no-reset initialization succeeds");
            sdk.mode_result = 0;
            check(!pad.poll().connected && sdk.requests == 1, "rejected request remains neutral");
            check(!pad.poll().connected && sdk.requests == 2, "rejected request retried");
            sdk.mode_result = 1;
            check(!pad.poll().connected && sdk.requests == 3, "accepted request awaits stable reading");
            const auto snapshot = pad.poll();
            check(snapshot.connected && snapshot.analog && snapshot.left_x == 17 &&
                  snapshot.buttons == ps2::Cross && sdk.requests == 3, "accepted mode read without resubmission");
            sdk.state = PAD_STATE_DISCONN;
            check(!pad.poll().connected, "disconnect neutralizes input");
            sdk.state = PAD_STATE_STABLE;
            pad.poll();
            check(sdk.requests == 4, "reconnected controller renegotiates mode");
        }
        check(sdk.closes == 1 && !sdk.dma_buffer, "owned DMA port closed before scope exits");

        reset();
        {
            ps2::PadInput pad;
            check(pad.initialize(), "asynchronous controller initialization");
            sdk.request_state = PAD_RSTAT_BUSY;
            pad.poll();
            check(!pad.poll().connected && sdk.requests == 1 && sdk.reads == 0,
                  "accepted busy request is neither resubmitted nor treated as complete");
            sdk.state = PAD_STATE_EXECCMD; // libpad maps stable+busy to this state.
            check(!pad.poll().connected && sdk.requests == 1 && sdk.reads == 0 &&
                  std::strcmp(pad.status(), "WAITING FOR ANALOG MODE") == 0,
                  "SDK executing state remains neutral and reports negotiation");
            sdk.state = PAD_STATE_STABLE;
            sdk.request_state = PAD_RSTAT_FAILED;
            check(!pad.poll().connected && sdk.requests == 1, "later request failure remains neutral");
            pad.poll();
            check(sdk.requests == 2, "failed asynchronous request retried");
            sdk.request_state = PAD_RSTAT_COMPLETE;
            check(pad.poll().analog && sdk.requests == 2, "completed retry enables analog input");
            sdk.state = PAD_STATE_DISCONN;
            pad.poll();
            sdk.state = PAD_STATE_STABLE;
            sdk.request_state = PAD_RSTAT_BUSY;
            pad.poll();
            sdk.state = PAD_STATE_DISCONN;
            pad.poll();
            sdk.state = PAD_STATE_STABLE;
            pad.poll();
            check(sdk.requests == 4, "disconnect discards pending mode request");
        }

        reset(); sdk.mode_count = 0; sdk.dualshock = false;
        {
            ps2::PadInput pad;
            check(pad.initialize(), "unknown-capabilities initialization");
            const auto digital = pad.poll();
            check(digital.connected && !digital.analog && sdk.requests == 0,
                  "digital input works without a mode table");
            sdk.mode_count = 1; sdk.dualshock = true;
            pad.poll();
            check(sdk.requests == 1, "late mode table triggers analog negotiation");
            check(pad.poll().analog, "late capability negotiation completes");
        }

        reset(); sdk.dualshock = false;
        {
            ps2::PadInput pad;
            check(pad.initialize(), "digital controller initialization");
            pad.poll();
            const auto snapshot = pad.poll();
            check(snapshot.connected && !snapshot.analog && snapshot.left_x == 128 &&
                  sdk.requests == 0, "digital-only device remains usable without mode request");
        }
        check(sdk.closes == 1, "digital controller closes once");
        reset();
        try {
            ps2::PadInput pad;
            check(pad.initialize(), "exception-path initialization");
            throw 1;
        } catch (int) {}
        check(sdk.closes == 1 && !sdk.dma_buffer, "exception unwind closes DMA port");
        std::puts("PS2 mocked pad lifecycle and analog retry checks passed; hardware untested.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "PS2 mocked pad checks failed: %s\n", error.what());
        return 1;
    }
}
