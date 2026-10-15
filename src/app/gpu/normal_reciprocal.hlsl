// Certified normal-range binary32 reciprocal for optional native spans.
// Own integer derivation; the exact domain and legacy forwarding are below.

// Exact product of two unsigned 24-bit integers, low limb first.
// low_product <= 0xfffe0001; middle <= 33488384 (< 2^25).
// high <= 65535. No integer multiplication/addition overflows uint32.
uint2 reciprocal_mul24(uint a, uint b) {
    const uint al = a & 0xffffu, ah = a >> 16u;
    const uint bl = b & 0xffffu, bh = b >> 16u;
    const uint low_product = al * bl;
    const uint middle = al * bh + ah * bl + (low_product >> 16u);
    return uint2(((middle & 0xffffu) << 16u) | (low_product & 0xffffu),
                 ah * bh + (middle >> 16u));
}

// m is strictly between 2^23 and 2^24. Return RN-even(2^47/m).
// Each partial remainder stays below m; its double stays below 2^25.
uint reciprocal_exact_significand(uint m) {
    uint remainder = 0x800000u, quotient = 0u;
    [loop] for (uint bit = 0u; bit < 24u; ++bit) {
        remainder <<= 1u;
        quotient <<= 1u;
        if (remainder >= m) { remainder -= m; quotient |= 1u; }
    }
    const uint twice = remainder << 1u;
    if (twice > m || (twice == m && (quotient & 1u) != 0u)) ++quotient;
    return quotient;
}

// Certify an approximate reciprocal of m/2^23, without depending on its
// accuracy. Correct at most two neighboring candidates; otherwise use the
// exact reference above. The common successful path has four uint multiplies,
// one two-limb subtraction and a comparison, not 24 integer divide steps.
uint reciprocal_certified_significand(uint m, uint seed_bits) {
    if (seed_bits >= 0x3f000000u && seed_bits <= 0x3f800000u) {
        // In [0.5,1), seed * 2^24 is exactly its 24-bit significand.
        // Exactly 1.0 has a different exponent: clamp its initial candidate
        // separately. This changes only the seed, never the certified answer.
        uint q = seed_bits == 0x3f800000u ? 0xffffffu :
                 0x800000u | (seed_bits & 0x7fffffu);
        const uint2 product = reciprocal_mul24(m, q);
        // Target is 2^47 == uint2(0,0x8000). Compute exact absolute distance.
        bool below = product.y < 0x8000u;
        uint distance, distance_high;
        if (below) {
            distance = product.x == 0u ? 0u : (~product.x) + 1u;
            distance_high = 0x8000u - product.y - uint(product.x != 0u);
        } else {
            distance = product.x;
            distance_high = product.y - 0x8000u;
        }
        // A distance >=2^32 cannot be repaired by two <=24-bit steps.
        if (distance_high == 0u) {
            [unroll] for (uint attempt = 0u; attempt < 3u; ++attempt) {
                const uint half_m = m >> 1u;
                // |m*q - 2^47| < m/2; an exact tie requires even q.
                // For odd m, distance==floor(m/2) is strictly inside.
                if (distance < half_m || (distance == half_m &&
                    (((m & 1u) != 0u) || ((q & 1u) == 0u)))) return q;
                if (attempt == 2u) break;
                q = below ? q + 1u : q - 1u;
                if (q < 0x800000u || q > 0xffffffu) break;
                // Updating q changes the product by exactly m. Keep distance
                // in uint32; no second wide product or overflowing add needed.
                if (distance >= m) distance -= m;
                else { distance = m - distance; below = !below; }
            }
        }
    }
    return reciprocal_exact_significand(m);
}

// Correctly rounded reciprocal for exactly:
//   0x00800000 <= absbits(x) <= 0x7e800000
//   (finite normal 2^-126 <= |x| <= 2^126).
// Both input and result are normal, so no denormal preservation is assumed.
// All other inputs retain the original source operation. This is intentionally
// NOT a new IEEE exception/NaN/subnormal policy or a full-range reciprocal.
float normal_reciprocal_or_legacy(float x) {
    const uint bits = asuint(x), magnitude = bits & 0x7fffffffu;
    if (magnitude < 0x00800000u || magnitude > 0x7e800000u) return 1.0 / x;
    const uint sign = bits & 0x80000000u;
    const uint exponent = magnitude >> 23u;
    const uint fraction = magnitude & 0x7fffffu;
    if (fraction == 0u) return asfloat(sign | ((254u - exponent) << 23u));
    const uint m = 0x800000u | fraction;
    const float normalized = asfloat(0x3f800000u | fraction);
    const uint q = reciprocal_certified_significand(m, asuint(1.0 / normalized));
    return asfloat(sign | ((253u - exponent) << 23u) | (q & 0x7fffffu));
}
