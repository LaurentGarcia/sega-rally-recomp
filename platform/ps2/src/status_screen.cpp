#include "status_screen.h"
#include "gs_display.h"
#include <cstdio>

namespace ps2 {
bool show_status(GsDisplay &display, const char *message) {
    std::printf("PS2 status begin: %s\n", message ? message : "NO STATUS");
    std::fflush(stdout);
    const bool result = display.present_status(message);
    if (!result) {
        std::printf("PS2 status presentation failed: %s\n", display.error());
        std::fflush(stdout);
    } else {
        std::puts("PS2 status presented");
        std::fflush(stdout);
    }
    return result;
}
} // namespace ps2
