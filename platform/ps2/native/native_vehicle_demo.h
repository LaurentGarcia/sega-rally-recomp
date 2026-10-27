#pragma once
#include "native_display.h"
#include "pad_input.h"
namespace ps2::native {
// Developer driver: one owner-provided initial checkpoint, then live pad
// updates and an explicit diagnostic view. This is not the arcade game flow.
int run_native_vehicle_demo(Display& display,ps2::PadInput& pad,
                            const char* checkpoint_path,const char* course_path,
                            const char* body_path);
}
