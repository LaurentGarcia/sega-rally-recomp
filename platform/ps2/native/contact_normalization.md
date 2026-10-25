# Contact-direction normalization

`normalize_contact_direction(input, output)` recovers the bounded vector
normalization inside the main contact-response region. It accepts an explicit
three-component value and produces another value. Addresses, registers and the
original processor exist only in the private comparison tool. This follows
the governing design's **Platform layer & build** native-rewrite exception.

Every input must be finite binary32 with magnitude at most `2^20`. Signed
zeros and subnormals are accepted. Rejection preserves all output bits, and
output may alias input. These are native validation rules rather than a claim
that the original program exposed this error interface.

With `R32` meaning nearest/even binary32 rounding with gradual underflow:

```
squared = R32(R32(R32(x*x) + R32(y*y)) + R32(z*z))
length = R32(sqrt(squared))
if length <= binary64(0x380fffffff9fdba8):
    output = {+0, +0, 1}
else:
    output = {R32(x/length), R32(y/length), R32(z/length)}
```

The threshold is approximately `2^-126`. Any positive binary32 squared sum
is at least `2^-149`, so its square root is greater than `2^-75`. Within this
contract only a zero rounded squared sum can select the fallback. That includes
some nonzero inputs: squaring `2^-75` rounds to zero, while the next larger
binary32 value has a nonzero square. Computing a higher-precision length first
would change this behavior. The result need not have an exactly unit norm.

The implementation uses bounded integer significands and explicit rounding.
A restoring integer square root produces the floor root of an exact scaled
47/48-bit integer radicand. The midpoint between consecutive root significands
has square `n*n+n+1/4`, so an integer radicand cannot tie; a remainder greater
than `n` selects rounding up. The length is normal and bounded below `2^21`,
which also bounds the subsequent division shifts. No host or EE floating
arithmetic determines the output. The previously verified arithmetic files
remain unchanged.

## Original-program boundary

The original Revision B program is 2,097,152 bytes with SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The region `[0x348ec,0x34968)` has 124 bytes and SHA-256
`50d22d6c9883588bd118027412792f2fd91ec535756aeac1d6b8fd411af9b847`.
It is a region inside a larger function, not a separately callable original
helper. The normal caller's zero scratch value and nearest rounding are
explicit oracle preconditions.

The host-only oracle permits instruction fetches only inside that region and
data accesses only to the three input/output words. It checks the ordered
three stores. Both paths read XYZ; the normal path then rereads YZ, while the
fallback does not. The first oracle incorrectly expected three reads on both
paths and rejected a matching nonzero result; correcting its access contract
did not change native arithmetic.

The isolated original comparison passes 59,912 evaluations and 59,896 distinct
inputs, executing 1,416,212 original instructions. It exercises 5,419 fallback
branches. Fingerprint `53b71c272e0a45ea` also matches an independent volatile-float
host reference. Coverage includes selected zero/subnormal/underflow/boundary
combinations, exponent sweeps, near-one values and 50,000 deterministic random
triples; it is not exhaustive over the complete input domain. Every case checks
in-place aliasing. There are 48 separate atomic invalid-input checks and an
underflow/fallback discriminator. ASan/UBSan pass.

The standalone EE fixture executed in Play 0.72 HLE and passed 4,064 cases,
fingerprint `2f11e06fc7af1915`, alias checks, all 48 rejections and the underflow
guard. Its ELF SHA-256 is
`153922383664734e9b37e4fd6a653edb655f90f75d2ae8a6f6214b5077223fd8`,
1,227,036 file bytes / 181,088 resident bytes before dynamic allocations. Guest
stderr was empty. The 20.064-second observation is not measured fixture
execution time. Source/build/ELF/log identities are bound in
`build/ps2-native-contact-normalization-ee-001/runtime-validation.json`.
Physical console execution and integration into native driving remain pending.

Capture 010 supplies all 13 sampled normalization pairs. Each native result
matches isolated original execution and the recorded output words.
The combined comparison passes 59,925 evaluations / 59,909 distinct inputs,
1,416,524 original instructions and fingerprint `82716f3cde064327`. The live
samples do not exercise the fallback; its coverage remains synthetic. Final
source-bound evidence is
`build/ps2-native-contact-normalization-live-003/result.json`.

## Reproduction

```sh
python3 tests/test_ps2_contact_normalization.py
python3 platform/ps2/tools/check_contact_normalization.py \
  --program /path/to/owned/program.bin \
  --output build/contact-normalization-oracle
```

Add `--capture build/ps2/driving-capture-010/capture` for the paired live comparison.
The strict reader verifies the complete main-pass envelope, saved caller proof,
index, pointers, rounding mode and raw vector storage. It observes ordinary
RAM/registers after existing instructions and adds no FIFO or device read.
Firmware, generated reference code and recorded values stay under ignored
`build/`; the public fixture uses independently constructed synthetic values.
This component does not recover the following force expression or the complete
contact response.
