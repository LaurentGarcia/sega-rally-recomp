#include "tools/srally_boot/input_replay.h"
#include <cstdio>
#include <functional>

namespace {
unsigned checks = 0;
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()> &operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error &) { rejected = true; }
    require(rejected, "invalid input replay accepted");
}
srally::InputReplay parse(const std::string &text) {
    std::istringstream stream(text);
    return srally::InputReplay(stream);
}
const std::string neutral = " ff 8f 00 ff 80 00 00 ff ff ff ff ff\n";
}

int main() {
    try {
        auto replay = parse("# cabinet pins, complete snapshots\nsegarally-input-v1\r\n"
            "2 fe 8f 00 ff 80 00 00 ff ff ff ff ff # coin on\n"
            "4" + neutral + "8 bf af ff 7f 00 ff 80 01 02 03 04 05\n");
        rt::Model2AIO::Inputs pins;
        require(replay.size() == 3 && replay.applied() == 0, "event count");
        require(!replay.apply(0, pins) && pins.in0 == 0xff, "input changed before first event");
        require(!replay.apply(1, pins), "input applied early");
        require(replay.apply(2, pins) && pins.in0 == 0xfe, "coin edge missed");
        require(!replay.apply(2, pins) && replay.applied() == 1, "duplicate application");
        require(!replay.apply(3, pins) && pins.in0 == 0xfe, "coin pulse did not persist");
        require(replay.apply(4, pins) && pins.in0 == 0xff, "coin release missed");
        require(replay.apply(8, pins) && pins.in0 == 0xbf && pins.in1 == 0xaf &&
            pins.in2 == 0xff && pins.switches == 0x7f &&
            pins.analog == std::array<uint8_t, 8>{0, 255, 128, 1, 2, 3, 4, 5},
            "complete cabinet snapshot not preserved");
        require(replay.applied() == 3 && !replay.apply(9, pins), "completed replay");
        rejects([&] { replay.apply(8, pins); });
        auto missed = parse("segarally-input-v1\n2" + neutral);
        pins.in0 = 0xfe;
        rejects([&] { missed.apply(3, pins); });
        require(pins.in0 == 0xfe && missed.applied() == 0, "missed event mutated cabinet pins");
        auto reset = parse("segarally-input-v1\n0 00 00 00 00 00 00 00 00 00 00 00 00\n");
        require(reset.apply(0, pins) && pins.in0 == 0 && pins.analog[7] == 0, "reset-time input");
        for (const auto &bad : {std::string{}, std::string("# only comments\n"),
                std::string("segarally-input-v2\n0") + neutral,
                std::string("segarally-input-v1 extra\n0") + neutral,
                std::string("segarally-input-v1\n"),
                std::string("segarally-input-v1\n0 ff\n"),
                std::string("segarally-input-v1\n0") + neutral + "0" + neutral,
                std::string("segarally-input-v1\n2") + neutral + "1" + neutral}) {
            rejects([&] { (void)parse(bad); });
        }
        for (const char *frame : {"-1", "+1", "1x", "0x10", "1000001", "18446744073709551616"})
            rejects([&] { (void)parse(std::string("segarally-input-v1\n") + frame + neutral); });
        for (const char *port : {"f", "fff", "gg", "+1", "-1", "0xff"})
            rejects([&] { (void)parse(std::string("segarally-input-v1\n0 ") + port + " 8f 00 ff 80 00 00 ff ff ff ff ff\n"); });
        rejects([&] { (void)parse("segarally-input-v1\n0 ff 8f 00 ff 80 00 00 ff ff ff ff ff 00\n"); });
        rejects([&] { (void)parse("segarally-input-v1\n" + std::string(513, ' ')); });
        std::string many = "segarally-input-v1\n";
        for (unsigned n = 0; n < 4096; ++n) many += std::to_string(n) + neutral;
        require(parse(many).size() == 4096, "event limit rejected valid input");
        rejects([&] { (void)parse(many + "4096" + neutral); });
        require(parse("segarally-input-v1\n1000000" + neutral).size() == 1, "frame limit rejected");
        std::printf("Sega Rally input replay: %u checks passed\n", checks);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
