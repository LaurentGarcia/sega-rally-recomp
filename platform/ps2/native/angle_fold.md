# Bounded orientation-angle folding

`fold_angle_once(float &angle)` independently implements one observed angle
correction used by the car integrator. It is a mathematical operation over a
native value, with no guest registers, memory addresses, processor operations,
TGP commands or scheduler in its production implementation.

## Exact contract

The accepted input is an IEEE binary32 value in the inclusive interval
`[-round32(2*pi), +round32(2*pi)]`. Values outside this range, including all
infinities and NaNs, return `false` without changing any input bit. This is an
explicit native API restriction; the original function does not reject them.
Live callers have not yet been proven to stay inside this domain.

Accepted values between the binary64 constants `-pi` and `+pi` remain
bit-identical, including signed zero and subnormal values. Larger positive
values receive one subtraction of the binary64 `2*pi`; smaller negative values
receive one addition. The result has binary32 round-to-nearest, ties-to-even
semantics. It is **one correction**, not a modulo operation or repeated loop.

The input just above positive `pi` folds to the negative side. The largest
binary32 below `pi` stays unchanged. The nearest binary32 to `2*pi` lies slightly
above the binary64 constant, so it folds to a small positive value rather than
zero; the negative endpoint is symmetric. These distinctions are tested.

## Arithmetic proof and PS2 implementation

This follows the exact-arithmetic requirement in the governing design's
[i960 floating-point section](../../../docs/daytona-usa-recomp-design.md#i960-static-recompiler)
and the independent-native implementation boundary in its platform section.

After the unchanged interval is removed, the positive input exponent is 1 or 2.
Every remaining input and the binary64 `2*pi` are integers multiplied by
`2^-50`. The latter's mathematical numerator is `0x1921fb54442d18`. The exact
difference has a nonzero absolute numerator with bit length 28 through 52.
It therefore fits the 53-bit precision of binary64 **without any rounding**.
Extra intermediate extended precision cannot change that exact difference.

The implementation subtracts those integer numerators and rounds the remaining
bits to binary32 explicitly, including halfway/even and significand carry.
No floating arithmetic, division, libm or floating-point control state is used
at runtime. There is only one potentially inexact rounding inside this bounded
contract. This proof does not extend to arbitrary large angles, other rounding
modes, exception flags or general i960 extended-real arithmetic.

## Original-program evidence

The owner's Revision B image is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The address-only lead `geo_view_float_clamp` is ROM `0x27b28`, extent 156 bytes,
span SHA-256
`bd67bceb1abed34efad3c73b3b3ac663d5bc5119dee5e58ab870c7a68cc80ed2`.
The old label is imprecise: this leaf folds once instead of clamping.

Fresh decoding confirms a single binary32 input/output, strict comparisons
against the binary64 half-turn, and the binary64 then binary32 result boundary.
The live-copy entry is `0x005c6b28`; its return branch is `0x005c6bbc`. Depending
on the path, it executes 13 through 20 instructions. It performs no data-memory
reads or writes. The host-only oracle rejects either as an unexpected dependency.

The car integrator calls this leaf at ROM `0x2e568`, `0x2e578` and `0x2e588` on
the three previously observed orientation words. This establishes a concrete
caller relationship. It does not establish axis order, handedness, update
frequency or the larger integrator's arithmetic contract. Radian interpretation
is supported by the pi constants, but no physical coordinate scale follows.

## Validation

```sh
python3 tests/test_ps2_angle_fold.py
python3 platform/ps2/tools/check_angle_fold.py \
  --program /path/to/verified/program.bin \
  --output build/native-angle-oracle-001 --exhaustive-folded
```

The portable test compares every **16,777,218 input bit patterns in the
nontrivial folded interval**, both signs, against the explicit binary64
operation followed by binary32 conversion. Additional cases cover all exponent
classes, pi/turn neighbors, signed zero, subnormals, non-finites and atomic
rejection. These are original mathematical test vectors.

The separate private differential executes the original function for every
one of those folded inputs, plus unchanged-domain boundary and random inputs:
**16,844,326 cases and 328,030,729 original instructions**, all matching, in
1.581 seconds on the validation host. Its result fingerprint is
`256956588cf6bc50`. Not every unchanged-domain bit pattern is enumerated; the
unchanged native branch returns without writing the value.

The oracle verifies image/span hashes, restricts instruction fetches to the
function, checks the return sentinel and forbids all data-memory access.
It uses the existing host-only reference interpreter at nearest/even; that
interpreter is not linked into the native game. Exception status and physical
PS2 execution are not established by this host comparison. Evidence remains
under ignored `build/`. The component remains disconnected from gameplay.

The standalone EE fixture also passed 36,262 boundary, rejection and random
cases in Play 0.72 with the HLE BIOS. The executed ELF SHA-256 is
`9b507289586ffb1f6d8e253c1b904a9ad5b249b548f81ca0bc9b14bdf72f3485`.
This is execution of compiled native EE code in an emulator, not a physical
console test; the EE run does not repeat the host's exhaustive interval sweep.
The compiled production object uses integer instructions only. The EE toolchain
reports non-IEEE FPU behavior and an extra finite exponent, so the implementation
checks its required bounded bit layout instead of assuming full IEEE arithmetic.
Host AddressSanitizer/UndefinedBehaviorSanitizer also pass the exhaustive sweep.
