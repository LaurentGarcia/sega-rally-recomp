#include "bringup_scene.h"
#include "gs_display.h"
#include "pad_input.h"
#include <kernel.h>
#include <cstdio>

int main() {
    std::printf("Sega Rally PS2 hardware bring-up; synthetic geometry only; game core not linked\n");
    ps2::GsDisplay display;
    if (!display.initialize()) {
        std::printf("PS2 display failed: %s\n", display.error());
        SleepThread();
        return 1;
    }
    ps2::PadInput input;
    input.initialize();
    ps2::CabinetControls controls;
    ps2::BringupScene scene;
    bool direct_gs = true, previous_triangle = false;
    for (uint32_t frame = 0;; ++frame) {
        const auto pad = input.poll();
        const auto cabinet = controls.sample(pad);
        const bool triangle = pad.connected && (pad.buttons & ps2::Triangle);
        if (triangle && !previous_triangle) direct_gs = !direct_gs;
        previous_triangle = triangle;
        scene.render(frame, pad, cabinet, controls.gear(), input.status(), direct_gs);
        const bool shown = direct_gs
            ? display.present_flat(scene.pixels(), ps2::kFrameWidth, scene.vertices(), scene.batches())
            : display.present_rgba(scene.pixels(), ps2::kFrameWidth);
        if (!shown) {
            std::printf("PS2 presentation failed: %s\n", display.error());
            SleepThread();
            return 1;
        }
        if (frame % 300 == 0)
            std::printf("PS2 frame=%u path=%s pad=%s reference=%08lx%08lx game_core_running=false\n",
                        unsigned(frame), direct_gs ? "gs-triangles" : "cpu-reference", input.status(),
                        static_cast<unsigned long>(scene.reference_hash() >> 32),
                        static_cast<unsigned long>(scene.reference_hash() & 0xffffffffu));
    }
}
