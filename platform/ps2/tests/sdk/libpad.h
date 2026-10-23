#pragma once
#include <cstdint>
// Minimal host-only surface for the fields and calls used by pad_input.cpp.
// This does not emulate libpad, its binary ABI, or controller hardware.
enum {
    PAD_STATE_DISCONN = 0, PAD_STATE_FINDCTP1 = 2, PAD_STATE_EXECCMD = 5, PAD_STATE_STABLE = 6,
    PAD_MODETABLE = 4, PAD_TYPE_DIGITAL = 4, PAD_TYPE_ANALOG = 5,
    PAD_TYPE_DUALSHOCK = 7, PAD_MMODE_DUALSHOCK = 1, PAD_MMODE_LOCK = 3,
    PAD_RSTAT_COMPLETE = 0, PAD_RSTAT_FAILED = 1, PAD_RSTAT_BUSY = 2,
    PAD_LEFT = 0x0080, PAD_RIGHT = 0x0020, PAD_CROSS = 0x4000,
    PAD_SQUARE = 0x8000, PAD_CIRCLE = 0x2000, PAD_TRIANGLE = 0x1000,
    PAD_L1 = 0x0400, PAD_R1 = 0x0800, PAD_SELECT = 0x0001, PAD_START = 0x0008,
};
struct padButtonStatus {
    std::uint8_t mode = 0, ljoy_h = 128;
    std::uint16_t btns = 0xffff;
};
int padInit(int);
int padPortOpen(int, int, void *);
int padPortClose(int, int);
int padGetState(int, int);
int padGetReqState(int, int);
int padInfoMode(int, int, int, int);
int padSetMainMode(int, int, int, int);
int padRead(int, int, padButtonStatus *);
