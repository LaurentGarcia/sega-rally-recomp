#include "alternate_orientation.h"
#include "angle_fold.h"
#include <array>
#include <bit>
#include <cstdint>

namespace ps2::native {
namespace {
constexpr std::uint32_t sign_mask = 0x80000000u;
constexpr std::uint32_t magnitude_mask = 0x7fffffffu;
constexpr std::uint32_t domain = 0x40c90fdbu;

// Exact signed numerators in units of 2^-202. Seven limbs suffice for this
// bounded operation: a binary32 subnormal times binary64(0.9) reaches that
// quantum, and all intermediate magnitudes stay below16 (207 bits).
struct Value {
    std::array<std::uint32_t, 7> words{};
    bool negative = false;
};

int top(const Value &value) noexcept {
    for (int i = 6; i >= 0; --i)
        if (value.words[i]) return i * 32 + int(std::bit_width(value.words[i])) - 1;
    return -1;
}

Value shifted(Value value, unsigned bits) noexcept {
    Value result{{}, value.negative};
    const unsigned whole = bits / 32, part = bits % 32;
    for (unsigned i = 0; i + whole < 7; ++i) {
        result.words[i + whole] |= value.words[i] << part;
        if (part && i + whole + 1 < 7)
            result.words[i + whole + 1] |= value.words[i] >> (32 - part);
    }
    return result;
}

Value numerator(std::uint64_t significand, unsigned shift, bool negative) noexcept {
    Value result{{}, negative};
    result.words[0] = std::uint32_t(significand);
    result.words[1] = std::uint32_t(significand >> 32);
    return shifted(result, shift);
}

Value single(std::uint32_t bits) noexcept {
    const unsigned exponent = (bits >> 23) & 255;
    const auto significand = (bits & 0x7fffff) | (exponent ? 0x800000 : 0);
    return numerator(significand, exponent ? exponent + 52 : 53, bool(bits & sign_mask));
}

int compare_magnitude(const Value &a, const Value &b) noexcept {
    for (int i = 6; i >= 0; --i)
        if (a.words[i] != b.words[i]) return a.words[i] > b.words[i] ? 1 : -1;
    return 0;
}

Value add(Value a, Value b) noexcept {
    Value result;
    if (a.negative == b.negative) {
        result.negative = a.negative;
        std::uint64_t carry = 0;
        for (unsigned i = 0; i < 7; ++i) {
            const auto sum = std::uint64_t(a.words[i]) + b.words[i] + carry;
            result.words[i] = std::uint32_t(sum);
            carry = sum >> 32;
        }
        return result;
    }
    const int order = compare_magnitude(a, b);
    if (!order) return result; // Exact cancellation rounds to positive zero.
    if (order < 0) { const auto saved = a; a = b; b = saved; }
    result.negative = a.negative;
    std::uint64_t borrow = 0;
    for (unsigned i = 0; i < 7; ++i) {
        const auto subtrahend = std::uint64_t(b.words[i]) + borrow;
        result.words[i] = std::uint32_t(std::uint64_t(a.words[i]) - subtrahend);
        borrow = std::uint64_t(a.words[i]) < subtrahend;
    }
    return result;
}

bool bit(const Value &value, unsigned position) noexcept {
    return (value.words[position / 32] >> (position % 32)) & 1;
}

Value rounded(Value value, unsigned precision, unsigned minimum_shift = 0) noexcept {
    const int highest = top(value);
    if (highest < 0) return value;
    const int wanted = highest + 1 - int(precision);
    const unsigned shift = wanted > int(minimum_shift) ? unsigned(wanted) : minimum_shift;
    if (!shift) return value;
    const bool guard = bit(value, shift - 1);
    bool sticky = false;
    for (unsigned i = 0; i + 1 < shift; ++i) sticky |= bit(value, i);
    const bool increment = guard && (sticky || bit(value, shift));
    const unsigned word = shift / 32, part = shift % 32;
    for (unsigned i = 0; i < word; ++i) value.words[i] = 0;
    if (part) value.words[word] &= ~((std::uint32_t(1) << part) - 1);
    if (increment) {
        std::uint64_t carry = std::uint64_t(1) << part;
        for (unsigned i = word; i < 7 && carry; ++i) {
            carry += value.words[i];
            value.words[i] = std::uint32_t(carry);
            carry >>= 32;
        }
    }
    return value;
}

std::uint32_t to_single(Value value) noexcept {
    value = rounded(value, 24, 53); // Binary32's minimum quantum is2^-149.
    const auto sign = value.negative ? sign_mask : 0;
    const int highest = top(value);
    if (highest < 0) return sign;
    const unsigned shift = highest < 76 ? 53 : unsigned(highest - 23);
    const unsigned word = shift / 32, part = shift % 32;
    auto significand = value.words[word] >> part;
    if (part && word + 1 < 7) significand |= value.words[word + 1] << (32 - part);
    if (highest < 76) return sign | significand;
    return sign | (std::uint32_t(highest - 75) << 23) | (significand & 0x7fffff);
}

Value weighted(std::uint32_t delta) noexcept {
    // binary64(0.9) = 0x1ccccccccccccd *2^-53 exactly. The 53x24-bit
    // product is formed before its separate binary64 rounding boundary.
    Value product = numerator(0x1ccccccccccccdull, 0, bool(delta & sign_mask));
    const unsigned exponent = (delta >> 23) & 255;
    const auto factor = (delta & 0x7fffff) | (exponent ? 0x800000 : 0);
    std::uint64_t carry = 0;
    for (auto &word : product.words) {
        const auto partial = std::uint64_t(word) * factor + carry;
        word = std::uint32_t(partial);
        carry = partial >> 32;
    }
    // power32 -53 +202; normal power32=exponent-150.
    return rounded(shifted(product, exponent ? exponent - 1 : 0), 53);
}

std::uint32_t outer(std::uint32_t candidate, std::uint32_t delta) noexcept {
    return to_single(rounded(add(single(candidate), weighted(delta)), 53));
}

std::uint32_t middle(std::uint32_t current, std::uint32_t candidate,
                     std::uint32_t delta) noexcept {
    // Exact binary64 threshold0x3f91e2efa68c6f10, not pi/180.
    auto step = numerator(0x11e2efa68c6f10ull, 144, false);
    if (compare_magnitude(single(delta), step) < 0) return candidate;
    step.negative = !(delta & sign_mask); // current - signed_threshold.
    return to_single(rounded(add(single(current), step), 53));
}

std::array<std::uint32_t, 3> bits(const OrientationAngles &angles) noexcept {
    return {std::bit_cast<std::uint32_t>(angles.first),
            std::bit_cast<std::uint32_t>(angles.middle),
            std::bit_cast<std::uint32_t>(angles.third)};
}
OrientationAngles values(const std::array<std::uint32_t, 3> &words) noexcept {
    return {std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]),
            std::bit_cast<float>(words[2])};
}
} // namespace

bool smooth_alternate_orientation(const OrientationAngles &current,
                                  const OrientationAngles &candidate,
                                  AlternateOrientationUpdate &output) noexcept {
    const auto current_bits = bits(current);
    auto candidate_bits = bits(candidate);
    for (unsigned i = 0; i < 3; ++i)
        if ((current_bits[i] & magnitude_mask) > domain ||
            (candidate_bits[i] & magnitude_mask) > domain) return false;
    std::array<std::uint32_t, 3> differences{};
    for (unsigned i = 0; i < 3; ++i) {
        auto folded = std::bit_cast<float>(candidate_bits[i]);
        if (!fold_angle_once(folded)) return false;
        candidate_bits[i] = std::bit_cast<std::uint32_t>(folded);
        auto negative_candidate = single(candidate_bits[i]);
        negative_candidate.negative = !negative_candidate.negative;
        auto delta = std::bit_cast<float>(to_single(add(single(current_bits[i]), negative_candidate)));
        if (!fold_angle_once(delta)) return false;
        differences[i] = std::bit_cast<std::uint32_t>(delta);
    }
    const std::array<std::uint32_t, 3> result{
        outer(candidate_bits[0], differences[0]),
        middle(current_bits[1], candidate_bits[1], differences[1]),
        outer(candidate_bits[2], differences[2])};
    output = {values(candidate_bits), values(result)};
    return true;
}

} // namespace ps2::native
