#include "main_car_pose_cases.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
bool run(bool extended) {
    unsigned cases = 0;
    std::uint64_t hash = 14695981039346656037ull;
    const auto test = [&](const main_car_pose_cases::Case &c) {
        auto state = c.state;
        auto external = c.external;
        auto destination = c.previous;
        destination = ps2::native::make_main_car_pose(state, external);
        if (state != c.state || external != c.external ||
            main_car_pose_cases::words(destination) != c.expected) return false;
        const auto again = ps2::native::make_main_car_pose(state, external);
        if (again != destination) return false;
        // The returned value owns its payload; future inputs cannot change it.
        state = {}; external.fill(0);
        if (main_car_pose_cases::words(destination) != c.expected) return false;
        main_car_pose_cases::hash_words(hash, c.expected);
        ++cases;
        return true;
    };
    if (!main_car_pose_cases::each(test, extended)) return false;
    if (cases != (extended ? 77208u : 2184u) ||
        hash != (extended ? 0xb74eb51b13296a1cull : 0xd2fae2297e475353ull)) return false;
    std::printf("MAIN_CAR_POSE_%s PASS cases=%u words=%u hash=%016llx\n",
                extended ? "HOST" : "PROBE", cases, cases * 22,
                static_cast<unsigned long long>(hash));
    return true;
}
} // namespace

int main() {
    if (!run(false)) return 1;
#ifndef _EE
    if (!run(true)) return 1;
#endif
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
