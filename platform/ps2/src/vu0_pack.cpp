#include "vu0_pack.h"
#include <bit>

namespace ps2 {
void quantize_gs_xy_scalar(const GsCoordinates *input, GsFixedCoordinates *output,
                           std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i)
        for (unsigned lane = 0; lane < 4; ++lane)
            // Legal results fit in 16 bits. A signed intermediate avoids the
            // EE compiler's unnecessary float-to-unsigned >=2^31 slow path.
            output[i].xy[lane] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(input[i].xy[lane] * 16.f));
}

bool vu0_pack_available() noexcept {
#ifdef _EE
    return true;
#else
    return false;
#endif
}

void quantize_gs_xy_vu0(const GsCoordinates *input, GsFixedCoordinates *output,
                        std::size_t count) noexcept {
#ifdef _EE
    // Macro-mode interlocks synchronize loads, conversion and stores. GCC does
    // not allocate VU vector registers; these blocks own vf1..vf4, with memory
    // barriers for the source and destination arrays. No VU state is persistent.
    // Independent registers give the conversion pipeline four batches of work.
    while (count >= 4) {
        asm volatile(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 16(%0)\n\t"
            "lqc2 $vf3, 32(%0)\n\t"
            "lqc2 $vf4, 48(%0)\n\t"
            "vftoi4.xyzw $vf1, $vf1\n\t"
            "vftoi4.xyzw $vf2, $vf2\n\t"
            "vftoi4.xyzw $vf3, $vf3\n\t"
            "vftoi4.xyzw $vf4, $vf4\n\t"
            "sqc2 $vf1, 0(%1)\n\t"
            "sqc2 $vf2, 16(%1)\n\t"
            "sqc2 $vf3, 32(%1)\n\t"
            "sqc2 $vf4, 48(%1)\n\t"
            : : "r"(input), "r"(output) : "memory");
        input += 4;
        output += 4;
        count -= 4;
    }
    while (count--) {
        asm volatile(
            "lqc2 $vf1, 0(%0)\n\t"
            "vftoi4.xyzw $vf1, $vf1\n\t"
            "sqc2 $vf1, 0(%1)\n\t"
            : : "r"(input), "r"(output) : "memory");
        ++input;
        ++output;
    }
#else
    quantize_gs_xy_scalar(input, output, count);
#endif
}

bool quantize_gs_xy_checked(std::span<const GsCoordinates> input,
                            std::span<GsFixedCoordinates> output,
                            bool use_vu0) noexcept {
    if (output.size() < input.size()) return false;
    if (input.empty()) return true;
    const auto source = reinterpret_cast<std::uintptr_t>(input.data());
    const auto destination = reinterpret_cast<std::uintptr_t>(output.data());
    const auto bytes = input.size_bytes();
    // Subtraction avoids end-address overflow for the overlap check.
    if (source <= destination ? destination - source < bytes : source - destination < bytes) return false;
    for (const auto &pair : input) for (const auto coordinate : pair.xy) {
        // Classify bits rather than assuming IEEE comparisons on the EE FPU.
        // Negative zero is valid; negative subnormals, infinities and NaNs are not.
        const auto bits = std::bit_cast<std::uint32_t>(coordinate);
        if ((bits & 0x7fffffffu) >= 0x45800000u || bits > 0x80000000u) return false;
    }
    if (use_vu0) quantize_gs_xy_vu0(input.data(), output.data(), input.size());
    else quantize_gs_xy_scalar(input.data(), output.data(), input.size());
    return true;
}
} // namespace ps2
