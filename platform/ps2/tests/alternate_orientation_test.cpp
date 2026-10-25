#include "alternate_orientation.h"
#include "alternate_orientation_cases.h"
#include <cstdio>

namespace {
using namespace ps2::native;
using alternate_cases::Input;
using alternate_cases::Triple;
OrientationAngles values(const Triple &v) {
    return {std::bit_cast<float>(v[0]),std::bit_cast<float>(v[1]),std::bit_cast<float>(v[2])};
}
Triple bits(const OrientationAngles &v) {
    return {std::bit_cast<std::uint32_t>(v.first),std::bit_cast<std::uint32_t>(v.middle),
            std::bit_cast<std::uint32_t>(v.third)};
}
std::uint64_t count = 0, accepted = 0, hash = 14695981039346656037ull;
bool check(Input input) {
    const auto current = values(input.current), candidate = values(input.candidate);
    const AlternateOrientationUpdate sentinel{values({0x7fc00123,0xff800000,0xdeadbeef}),
                                               values({0xffc00123,0x7f800000,0xcafebabe})};
    auto output = sentinel;
    const bool ok = smooth_alternate_orientation(current,candidate,output);
    for (auto triple : {input.current,input.candidate})
        for (auto word : triple)
            if ((word & 0x7fffffffu) > alternate_cases::limit && ok) return false;
#if !defined(_EE)
    Triple expected_folded{}, expected_result{};
    const bool expected = alternate_cases::mathematical(input,expected_folded,expected_result);
    if (ok != expected || (ok && (bits(output.folded_candidate) != expected_folded ||
                                  bits(output.orientation) != expected_result))) {
        std::fprintf(stderr,"alternate mismatch case=%llu current=%08x,%08x,%08x candidate=%08x,%08x,%08x\n",
            (unsigned long long)count,input.current[0],input.current[1],input.current[2],
            input.candidate[0],input.candidate[1],input.candidate[2]);
        return false;
    }
#endif
    if (!ok && (bits(output.folded_candidate) != bits(sentinel.folded_candidate) ||
                bits(output.orientation) != bits(sentinel.orientation))) return false;
    auto aliased = AlternateOrientationUpdate{candidate,current};
    if (smooth_alternate_orientation(aliased.orientation,aliased.folded_candidate,aliased) != ok) return false;
    if (ok && (bits(aliased.folded_candidate) != bits(output.folded_candidate) ||
               bits(aliased.orientation) != bits(output.orientation))) return false;
    if (!ok && (bits(aliased.folded_candidate) != input.candidate || bits(aliased.orientation) != input.current))
        return false;
    auto reversed = AlternateOrientationUpdate{current,candidate};
    if (smooth_alternate_orientation(reversed.folded_candidate,reversed.orientation,reversed) != ok) return false;
    if (ok && (bits(reversed.folded_candidate) != bits(output.folded_candidate) ||
               bits(reversed.orientation) != bits(output.orientation))) return false;
    if (!ok && (bits(reversed.folded_candidate) != input.current || bits(reversed.orientation) != input.candidate))
        return false;
    for (auto triple : {input.current,input.candidate,bits(output.folded_candidate),bits(output.orientation)})
        for (auto word : triple) hash = (hash ^ word) * 1099511628211ull;
    hash = (hash ^ std::uint32_t(ok)) * 1099511628211ull;
    ++count; accepted += ok;
    return true;
}
}
int main() {
    if (!alternate_cases::probe(check)) return 1;
    if (count != 3728 || accepted != 3638 || hash != 0xf9e5c92050970893ull) return 1;
    std::printf("NATIVE_ALTERNATE_PROBE PASS cases=%llu accepted=%llu hash=%016llx aliases=2\n",
                (unsigned long long)count,(unsigned long long)accepted,(unsigned long long)hash);
#if !defined(_EE)
    if (!alternate_cases::extended(check)) return 1;
    std::printf("NATIVE_ALTERNATE_HOST PASS cases=%llu accepted=%llu hash=%016llx aliases=2\n",
                (unsigned long long)count,(unsigned long long)accepted,(unsigned long long)hash);
#endif
    unsigned invalid = 0;
    for (auto word : {0x40c90fdcu,0xc0c90fdcu,0x7f800000u,0xff800000u,0x7fc01234u,0xffc01234u,
                      0x7f800001u,0xff800001u,0x7fffffffu,0xffffffffu})
        for (unsigned i = 0; i < 6; ++i) {
            Input input{};
            (i < 3 ? input.current : input.candidate)[i % 3] = word;
            if (!check(input)) return 1;
            ++invalid;
        }
    std::printf("NATIVE_ALTERNATE_REJECTION PASS cases=%u aliases=2\n",invalid);
    return 0;
}
