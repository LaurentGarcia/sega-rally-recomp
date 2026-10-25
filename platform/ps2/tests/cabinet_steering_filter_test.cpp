#include "cabinet_steering_filter.h"
#include "cabinet_steering_filter_cases.h"
#include <cstdio>

namespace {
using namespace ps2::native;
std::uint64_t count = 0, hash = 14695981039346656037ull;
bool check(steering_filter_cases::Input input, std::uint32_t *next = nullptr) {
    CabinetSteeringUpdate output{std::bit_cast<float>(0x7fc00123u),std::bit_cast<float>(0xff800000u)};
    const auto old = std::bit_cast<float>(input.history);
    if (!filter_cabinet_steering(old,input.sample,output)) return false;
    const auto history = std::bit_cast<std::uint32_t>(output.history);
    const auto steering = std::bit_cast<std::uint32_t>(output.steering);
    if ((history & 0x7fffffffu) > 0x3f800000u || steering != (history ^ 0x80000000u)) return false;
#ifndef _EE
    if (history != steering_filter_cases::mathematical(input)) {
        std::fprintf(stderr,"steering filter mismatch history=%08x sample=%u\n",input.history,unsigned(input.sample));
        return false;
    }
#endif
    CabinetSteeringUpdate aliased{old,old};
    if (!filter_cabinet_steering(aliased.history,input.sample,aliased) ||
        std::bit_cast<std::uint32_t>(aliased.history) != history ||
        std::bit_cast<std::uint32_t>(aliased.steering) != steering) return false;
    aliased = {old,old};
    if (!filter_cabinet_steering(aliased.steering,input.sample,aliased) ||
        std::bit_cast<std::uint32_t>(aliased.history) != history ||
        std::bit_cast<std::uint32_t>(aliased.steering) != steering) return false;
    for (auto value : {input.history,std::uint32_t(input.sample),history,steering})
        hash = (hash ^ value) * 1099511628211ull;
    ++count;
    if (next) *next = history;
    return true;
}
}
int main() {
    if (!steering_filter_cases::probe([](auto input){ return check(input); }) ||
        !steering_filter_cases::sequences([](auto input, auto &next){ return check(input,&next); },512)) return 1;
    if (count != 7168 || hash != 0x335d3a428df4150dull) return 1;
    std::printf("NATIVE_STEERING_FILTER_PROBE PASS cases=%llu sequence_updates=2048 hash=%016llx aliases=2\n",
                (unsigned long long)count,(unsigned long long)hash);
#ifndef _EE
    if (!steering_filter_cases::extended([](auto input){ return check(input); }) ||
        !steering_filter_cases::sequences([](auto input, auto &next){ return check(input,&next); },4096)) return 1;
    std::printf("NATIVE_STEERING_FILTER_HOST PASS cases=%llu sequence_updates=18432 hash=%016llx aliases=2\n",
                (unsigned long long)count,(unsigned long long)hash);
#endif
    unsigned rejected = 0;
    for (auto bad : {0x3f800001u,0xbf800001u,0x7f800000u,0xff800000u,0x7fc01234u,0xffc01234u,
                    0x7f800001u,0xff800001u,0x7fffffffu,0xffffffffu})
        for (unsigned sample = 0; sample < 256; ++sample) {
            CabinetSteeringUpdate out{std::bit_cast<float>(0x7fc01234u),std::bit_cast<float>(0xff800001u)};
            if (filter_cabinet_steering(std::bit_cast<float>(bad),std::uint8_t(sample),out) ||
                std::bit_cast<std::uint32_t>(out.history) != 0x7fc01234u ||
                std::bit_cast<std::uint32_t>(out.steering) != 0xff800001u) return 1;
            ++rejected;
        }
    std::printf("NATIVE_STEERING_FILTER_REJECTION PASS cases=%u\n",rejected);
    return 0;
}
