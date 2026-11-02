#pragma once
#include "native_display.h"
#include "pad_input.h"
namespace ps2::native {
enum class NativeVehicleRenderMode { native_vu, reference, guarded_vu };
// Developer driver: one owner-provided initial checkpoint, then live pad
// updates and an explicit diagnostic view. This is not the arcade game flow.
// world_presentation requires textured/native_vu and keeps that renderer fixed;
// it is an explicit opt-in to untransformed plans and direct VU1 world batches.
// classify_async schedules EE gathering between submit and finish calls; it
// does not imply measured hardware overlap. Presentation still completes here.
int run_native_vehicle_demo(Display& display,ps2::PadInput& pad,
                            const char* checkpoint_path,const char* course_path,
                            const char* body_path,NativeVehicleRenderMode mode=NativeVehicleRenderMode::native_vu,
                            bool detailed_profiling=false,bool textured_presentation=false,bool world_presentation=false,
                            WorldSubmissionMode world_submission=WorldSubmissionMode::classify_first);
}
