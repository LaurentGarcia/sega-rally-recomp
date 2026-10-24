// Original ROM-free boundary tests with an independent modulo definition.
#include "contact_neighborhood.h"
#include <array>
#include <cstdio>

namespace {
using namespace ps2::native;
std::uint64_t checks = 0, hash = 14695981039346656037ull;
bool check(std::uint32_t count, std::int16_t current) {
    constexpr ContactNeighborhood sentinel{501, 502, 503};
    auto result = sentinel;
    const bool accepted = contact_neighborhood(count, current, result);
    const bool expected = count >= 1 && count <= max_contact_segments &&
                          current >= 0 && static_cast<std::uint32_t>(current) < count;
    if (accepted != expected) return false;
    if (accepted) {
        const auto index = static_cast<std::uint32_t>(current);
        const ContactNeighborhood independent{
            static_cast<std::uint16_t>((index + count - 1) % count),
            static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>((index + 1) % count)};
        if (result != independent) return false;
    } else if (result != sentinel) return false;
    for (auto value : {count, static_cast<std::uint32_t>(static_cast<std::int32_t>(current)),
                       static_cast<std::uint32_t>(result.previous),
                       static_cast<std::uint32_t>(result.current),
                       static_cast<std::uint32_t>(result.next), static_cast<std::uint32_t>(accepted)})
        hash = (hash ^ value) * 1099511628211ull;
    ++checks;
    return true;
}
} // namespace

int main() {
    for (const auto count : std::array<std::uint32_t, 12>{
             0, 1, 2, 10, 300, 32767, 32768, 65535, 65536, 0x7fffffff, 0x80000000, 0xffffffff})
        for (std::int32_t index = -32768; index <= 32767; ++index)
            if (!check(count, static_cast<std::int16_t>(index))) return 1;
    // Every accepted count, including ones absent from the owned data image.
    // Boundary/current samples exercise both wrap sites and the signed limit.
    for (std::uint32_t count = 1; count <= max_contact_segments; ++count)
        for (const auto index : std::array<std::int32_t, 6>{
                 -1, 0, 1, static_cast<std::int32_t>(count > 32768 ? 32767 : count - 1),
                 static_cast<std::int32_t>(count > 32767 ? 32767 : count), 32767})
            if (!check(count, static_cast<std::int16_t>(index))) return 1;
    std::printf("NATIVE_CONTACT_NEIGHBORHOOD PASS checks=%llu hash=%016llx\n",
                static_cast<unsigned long long>(checks), static_cast<unsigned long long>(hash));
}
