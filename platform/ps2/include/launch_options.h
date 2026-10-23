#pragma once
#include <cstdio>
#include <cstring>

namespace ps2 {
struct LaunchOptions {
    bool native_gs = false, vu0 = false, audio = false, debug = false;
};

// Optional rally.cfg beside game.rpk. Explicit switches keep approximate GS
// texture sampling separate from the reference-rendered default. Parse into a
// temporary value so a malformed file cannot partially activate hardware work.
inline bool read_launch_options(const char *path, LaunchOptions &out, char *error, std::size_t capacity) {
    FILE *file = std::fopen(path, "rb");
    if (!file) return true; // Loader filesystems do not consistently set errno.
    LaunchOptions result = out;
    char line[128];
    unsigned number = 0;
    bool ok = true;
    while (std::fgets(line, sizeof(line), file)) {
        ++number;
        auto size = std::strlen(line);
        if (size == sizeof(line) - 1 && line[size - 1] != '\n') { ok = false; break; }
        while (size && (line[size - 1] == '\n' || line[size - 1] == '\r' || line[size - 1] == ' ')) line[--size] = 0;
        if (!size || line[0] == '#') continue;
        if (!std::strcmp(line, "renderer=reference")) result.native_gs = false;
        else if (!std::strcmp(line, "renderer=gs-nearest")) result.native_gs = true;
        else if (!std::strcmp(line, "vu0=0")) result.vu0 = false;
        else if (!std::strcmp(line, "vu0=1")) result.vu0 = true;
        else if (!std::strcmp(line, "audio=0")) result.audio = false;
        else if (!std::strcmp(line, "audio=1")) result.audio = true;
        else if (!std::strcmp(line, "debug=0")) result.debug = false;
        else if (!std::strcmp(line, "debug=1")) result.debug = true;
        else { ok = false; break; }
    }
    ok = ok && !std::ferror(file);
    std::fclose(file);
    if (!ok) {
        std::snprintf(error, capacity, "INVALID RALLY.CFG LINE %u", number);
        return false;
    }
    out = result;
    return true;
}
} // namespace ps2
