// Original bounded consumer of private captured results. No guest runtime.
#include "contact_neighborhood.h"
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>

namespace {
bool read_word(std::ifstream &input, std::uint32_t &value) {
    std::array<unsigned char, 4> bytes{};
    if (!input.read(reinterpret_cast<char *>(bytes.data()), 4)) return false;
    value = bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) |
            (std::uint32_t(bytes[3]) << 24);
    return true;
}
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::array<char, 8> magic{};
    std::uint32_t count = 0;
    if (!input.read(magic.data(), 8) || magic != std::array<char, 8>{'C','O','N','T','A','C','T','1'} ||
        !read_word(input, count) || !count || count > 16384) return 2;
    std::uint64_t hash = 14695981039346656037ull;
    unsigned accepted_count = 0;
    for (unsigned i = 0; i < count; ++i) {
        std::array<std::uint32_t, 6> row{};
        for (auto &value : row) if (!read_word(input, value)) return 2;
        if (row[1] > 65535 || row[2] > 1 || row[3] > 65535 || row[4] > 65535 || row[5] > 65535)
            return 2;
        const auto current = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(row[1]));
        constexpr ps2::native::ContactNeighborhood sentinel{591, 592, 593};
        auto result = sentinel;
        const bool accepted = ps2::native::contact_neighborhood(row[0], current, result);
        if (accepted != bool(row[2]) || (!accepted && result != sentinel) ||
            (accepted && (result.previous != row[3] || result.current != row[4] || result.next != row[5]))) {
            std::fprintf(stderr, "captured contact mismatch row=%u count=%u current=%d\n", i, row[0], current);
            return 1;
        }
        accepted_count += accepted;
        for (auto value : row) hash = (hash ^ value) * 1099511628211ull;
    }
    if (input.peek() != std::char_traits<char>::eof()) return 2;
    std::printf("NATIVE_CONTACT_CAPTURE PASS queries=%u accepted=%u rejected=%u hash=%016llx\n",
                count, accepted_count, count - accepted_count, static_cast<unsigned long long>(hash));
}
