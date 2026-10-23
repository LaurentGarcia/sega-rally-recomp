// Portable checks for the same code compiled into the PS2 ELF. Passing these
// does not establish console rendering, controller operation or game boot.
#include "bringup_scene.h"
#include "controls.h"
#include "presentation.h"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char *reason) {
    if (!value) throw std::runtime_error(reason);
}
}

int main(int argc, char **argv) {
    try {
        check(ps2::gs_pixel(0xffff0000u) == 0x800000ffu, "red channel conversion");
        check(ps2::gs_pixel(0xff00ff00u) == 0x8000ff00u, "green channel conversion");
        check(ps2::gs_pixel(0xff0000ffu) == 0x80ff0000u, "blue channel conversion");
        const std::array<uint32_t, 5> input{0xffff0000u, 0xff00ff00u, 0xdeadbeefu, 0xff0000ffu, 0xffffffffu};
        std::array<uint32_t, 6> output{};
        check(ps2::convert_frame(input, 2, 2, 3, output, 3), "strided frame conversion");
        check(output[0] == 0x800000ffu && output[1] == 0x8000ff00u && output[2] == 0x80000000u &&
              output[3] == 0x80ff0000u && output[4] == 0x80ffffffu && output[5] == 0x80000000u, "strides and cleared padding");
        const auto untouched = output;
        check(!ps2::convert_frame(input, 3, 2, 3, output, 3) && output == untouched, "short source rejected before writes");
        check(!ps2::convert_frame(input, 2, 2, 1, output, 3), "invalid stride rejected");

        ps2::CabinetControls controls;
        ps2::PadSnapshot pad;
        auto cabinet = controls.sample(pad);
        check(cabinet.in0 == 0xff && cabinet.analog[0] == 128 && cabinet.analog[1] == 0, "disconnected neutral");
        pad.connected = pad.analog = true;
        pad.steer = 0;
        pad.buttons = ps2::Cross | ps2::Select | ps2::Start | ps2::R1 | ps2::Triangle;
        cabinet = controls.sample(pad);
        check(cabinet.analog[0] == 0 && cabinet.analog[1] == 255 && cabinet.in0 == 0x9e && cabinet.in1 == 0xaf, "cabinet wiring and gear one");
        controls.sample(pad);
        check(controls.gear() == 1, "held gear does not repeat");
        pad.buttons = 0; pad.steer = 255;
        check(controls.sample(pad).analog[0] == 255, "full right steering");
        pad.buttons = ps2::R1;
        check(controls.sample(pad).in1 == 0x9f, "second gear wiring");
        pad.steer = 140; pad.buttons = 0;
        check(controls.sample(pad).analog[0] == 128, "analog deadzone");
        pad.connected = false;
        cabinet = controls.sample(pad);
        check(cabinet.in0 == 0xff && cabinet.analog[0] == 128 && cabinet.analog[1] == 0 && cabinet.in1 == 0x9f, "disconnect releases inputs and retains gear");

        ps2::BringupScene scene;
        scene.render(0, {}, {}, 0, "HOST TEST", false);
        const uint64_t initial = scene.reference_hash();
        check(initial != 0 && initial != 0xcbf29ce484222325ULL, "shared raster produced a frame hash");
        check(scene.pixels()[170 * ps2::kFrameWidth + 100] == 0xffff6221u, "reference raster foreground color");
        check(ps2::flat_frame_supported(scene.vertices(), scene.batches()), "synthetic direct GS packet accepted");
        std::array<app::Model2Frame::PolyVertex, 6> vertices;
        std::copy(scene.vertices().begin(), scene.vertices().end(), vertices.begin());
        vertices[0].info[0] = 2;
        check(!ps2::flat_frame_supported(vertices, scene.batches()), "textured packet rejected");
        vertices[0] = scene.vertices()[0]; vertices[0].edges[1] = 1;
        check(!ps2::flat_frame_supported(vertices, scene.batches()), "shader edge packet rejected");
        vertices[0] = scene.vertices()[0]; vertices[0].x = std::numeric_limits<float>::quiet_NaN();
        check(!ps2::flat_frame_supported(vertices, scene.batches()), "nonfinite coordinates rejected");
        std::array<app::Model2Frame::Batch, 1> invalid_batch{{{1, 6, {0, 0, 496, 384}}}};
        check(!ps2::flat_frame_supported(scene.vertices(), invalid_batch), "out of range batch rejected");
        scene.render(1, {}, {}, 0, "HOST TEST", false);
        check(scene.reference_hash() == initial, "fixture is deterministic independent of frame label");
        rt::Model2AIO::Inputs shifted; shifted.analog[0] = 255;
        scene.render(2, {}, shifted, 0, "HOST TEST", false);
        check(scene.reference_hash() != initial, "cabinet steering changes actual reference geometry");
        scene.render(0, {}, {}, 0, "HOST TEST", false);
        if (argc == 2) {
            FILE *file = std::fopen(argv[1], "wb");
            check(file != nullptr, "preview output opens");
            std::fprintf(file, "P6\n%d %d\n255\n", ps2::kFrameWidth, ps2::kFrameHeight);
            for (uint32_t pixel : scene.pixels()) {
                const unsigned char rgb[] = {uint8_t(pixel >> 16), uint8_t(pixel >> 8), uint8_t(pixel)};
                check(std::fwrite(rgb, 1, 3, file) == 3, "preview output writes");
            }
            check(std::fclose(file) == 0, "preview output closes");
        }
        std::printf("{\"status\":\"passed\",\"target\":\"portable_host\",\"reference_hash\":\"%016llx\",\"game_core_running\":false,\"hardware_tested\":false}\n",
                    static_cast<unsigned long long>(initial));
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "PS2 host checks failed: %s\n", error.what());
        return 1;
    }
}
