#pragma once
#include <array>

namespace ps2::native {
struct AffinePoint3 { float x = 0, y = 0, z = 0; };
struct AffineTransform3 {
    std::array<AffinePoint3, 3> columns{};
    AffinePoint3 translation{};
};

// For each component: translation, then the three separately rounded products
// added in column order, rounding every sum to binary32 nearest/even.
// All 15 input scalars must be finite and within [-2^20, 2^20]. Signed zero and
// subnormals are supported. Rejection leaves output untouched; output may alias
// point or any column/translation. No matrix stack or indexed store is modeled.
bool rounded_affine_transform(const AffineTransform3 &transform,
                              const AffinePoint3 &point,
                              AffinePoint3 &output) noexcept;
} // namespace ps2::native
