#include "debug_flow.h"
#include "launch_options.h"
#include "debug_controls.h"
#include <cassert>
#include <cstring>
#include <string>

int main(int argc, char **argv) {
    assert(argc == 2);
    ps2::DebugPageToggle toggle;
    ps2::PadSnapshot pad;
    pad.connected = true;
    pad.buttons = ps2::Start;
    assert(!toggle.update(pad) && pad.buttons == ps2::Start);
    pad.buttons = ps2::Start | ps2::Select;
    assert(toggle.update(pad) && pad.buttons == 0);
    pad.buttons = ps2::Start;
    assert(!toggle.update(pad) && pad.buttons == 0);
    pad.buttons = ps2::Start | ps2::Select;
    assert(!toggle.update(pad) && pad.buttons == 0);
    pad.buttons = 0;
    assert(!toggle.update(pad));
    pad.buttons = ps2::Start | ps2::Select;
    assert(toggle.update(pad) && pad.buttons == 0);
    pad.connected = false;
    assert(!toggle.update(pad));
    pad.connected = true; pad.buttons = ps2::Select;
    assert(!toggle.update(pad) && pad.buttons == ps2::Select);
    ps2::FlowDiagram diagram;
    ps2::FlowSample sample;
    diagram.update(sample);
    assert(std::strstr(diagram.text(), "WAITING FOR DATA"));
    // Recorded boot timings: the large EE stage must remain separate from
    // tile/composite costs already included in machine execution.
    sample.frame = 3; sample.frequency = 147456000;
    sample.total = 380640768; sample.machine = 378808832;
    sample.tiles = 2560 + 24370688; sample.composite = 2573824;
    sample.submit = 1831168; sample.cache_misses = 121; sample.cache_delta = 35;
    diagram.update(sample);
    assert(std::strstr(diagram.text(), "EE CORE + DEVICES  2386 MS"));
    assert(std::strstr(diagram.text(), "TILES + COMPOSE    182 MS"));
    assert(std::strstr(diagram.text(), "TOP EE CORE+DEVICES"));
    assert(std::strstr(diagram.text(), "GS BUSY UNMEASURED"));
    assert(std::strstr(diagram.text(), "VU0 INACTIVE / VU1 INACTIVE"));
    unsigned columns = 0, lines = 1;
    for (const char *p = diagram.text(); *p; ++p) {
        if (*p == '\n') { assert(columns <= 40); columns = 0; ++lines; }
        else ++columns;
    }
    assert(columns <= 40 && lines <= 23);
    sample.renderer = ps2::FlowSample::Renderer::Native;
    sample.vu0 = true;
    sample.submit = sample.machine * 2;
    diagram.update(sample);
    assert(std::strstr(diagram.text(), "TOP SUBMIT+WAIT"));
    assert(std::strstr(diagram.text(), "VU0 XYZ PACK"));
    sample.renderer = ps2::FlowSample::Renderer::Fallback;
    diagram.update(sample);
    assert(std::strstr(diagram.text(), "VU0 INACTIVE"));

    ps2::LaunchOptions options;
    char error[128]{};
    assert(!options.debug && !options.native_gs && !options.audio);
    assert(ps2::read_launch_options(argv[1], options, error, sizeof(error)));
    auto write = [&](const char *text) {
        FILE *file = std::fopen(argv[1], "wb"); assert(file);
        std::fputs(text, file); std::fclose(file);
    };
    write("# Development switches\r\nrenderer=gs-nearest\r\nvu0=1\naudio=1\ndebug=1\n");
    assert(ps2::read_launch_options(argv[1], options, error, sizeof(error)));
    assert(options.native_gs && options.vu0 && options.audio && options.debug);
    write("renderer=reference\nunknown=1\n");
    assert(!ps2::read_launch_options(argv[1], options, error, sizeof(error)));
    assert(options.native_gs && options.vu0 && options.audio && options.debug);
    assert(std::strstr(error, "LINE 2"));
    write(std::string(200, 'x').c_str());
    assert(!ps2::read_launch_options(argv[1], options, error, sizeof(error)));
    std::puts("PS2 diagnostics: measured costs, legible bounds and atomic launch options passed");
}
