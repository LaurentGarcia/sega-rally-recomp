# Separately rounded cross product

`rounded_cross_product(left, right, output)` independently implements the
three product-difference expressions used by geometry command `0x5b`. This is
a bounded numerical operation, not a complete contact response, torque model
or driving update. Production code contains only typed vectors and integer
arithmetic; it has no firmware, addresses, processor state, transport or tables.
The design's **Platform layer & build** native-rewrite exception governs the
separate native implementation and offline original-firmware comparison.

## Result contract

Each input component must be a binary32 value in `[-2^20,2^20]`. Both signed
zeros and all subnormals are included. Values outside that domain, infinities
and NaNs return `false` without changing output. This is an explicit native
policy; the original command does not have the same rejection interface.
Output may alias either input, or both when they are the same object. All six
input encodings are captured and validated before any output is committed.

With `R32` denoting nearest/even binary32 rounding, the result is:

```
x = R32(R32(left.y * right.z) - R32(left.z * right.y))
y = R32(R32(left.z * right.x) - R32(left.x * right.z))
z = R32(R32(left.x * right.y) - R32(left.y * right.x))
```

Every product and subtraction has its own rounding boundary. A fused product
and subtraction, a higher-precision determinant rounded only at the end, or an
epsilon test would change results. Product signs, signed underflow zeros,
nearest/even ties and gradual underflow are preserved. Exact nonzero
cancellation produces positive zero. Zero subtraction follows the operand
signs, including negative zero for `-0 - +0`. Exception status, other rounding
modes and rejected non-finite behavior are outside the contract.

The mathematical `x,y,z` component names do not establish the game's units,
axis handedness or physical meaning. There is no normalization or magnitude
calculation here. Output magnitudes may exceed the input bound; this is not a
closed-domain operation for arbitrarily repeated applications.

## Bounded integer arithmetic

The component locally reuses the independently written dyadic product/sum
method from `contact_projection.cpp`, leaving that validated source unchanged.
A binary32 operand is an integer significand with at most 24 bits and a power
of two. Exact products require at most 48 bits. Individually rounded products
have magnitude at most `2^40`; their rounded difference has magnitude at most
`2^41`, safely finite.

For the subtraction, the two rounded operands are aligned only when their
power difference is at most 32. The aligned integer numerator and any sum fit
within 56 bits. Beyond that difference, the smaller operand cannot reach half
of even the narrower adjacent spacing of the already rounded larger value.
Guard, sticky and tie-to-even handling round the resulting integer dyadic;
subnormal rounding uses the minimum binary32 quantum `2^-149`. No host or EE
floating arithmetic computes the native result.

## Original identity and comparison

The verified geometry firmware is 6,660 bytes, SHA-256
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
The command occupies word interval `[0x55f,0x577)`: 24 instructions, 96 bytes,
SHA-256 `fa2fb8f0f29928fda96ab2c57bed7bf65048cc86215ead05afc63d6b5680004f`.
These are the original boundaries identified in the
[main-contact dependency study](main_contact_pass_recovery.md).

Only the private oracle knows the original FIFO order:
`left.y, right.z, left.z, right.y, right.x, left.x`. Three results emerge in
ordinary `x,y,z` order. The oracle executes the actual verified instructions
and checks all 24 completed instruction sites, the exact point of each of six
input reads and three output writes, return-stack destination, and all internal
scratch words. External memory, table and bank accesses are rejected. Only the
four established scratch words may change. The native function does not expose
those device effects.

The initial private comparison passed **110,048 evaluations**, representing
**110,032 unique six-word inputs**. It executed 2,641,152 original instructions,
660,288 FIFO reads and 330,144 output writes, with fingerprint
`428bbf5526cd241b`. The portable mathematical reference gives the same result.
Coverage contains 64 basis/zero vector pairs, all 64 zero-sign combinations,
1,728 rotated encoding-boundary pairs, 8,192 near-one product/cancellation cases
and 100,000 deterministic random vectors. Random cases deliberately include
identical vectors and zero middle components. This is not an exhaustive sweep
of the supported binary32 domain.

Portable tests check three successful alias modes, 180 invalid-input/alias
combinations with unchanged output, and a case that distinguishes separate
rounding from fused arithmetic. AddressSanitizer and UndefinedBehaviorSanitizer
pass the same cases. The standalone EE fixture checks 6,208 inputs and
fingerprint `408f01a4e8d06ea5` before reporting success. It passed in Play 0.72
HLE, including rejection and fused-operation checks, with empty guest stderr.
Its executed ELF SHA-256 is
`96251ba4dd8fa8a00a8dc46746ece7fe665d2400e9d5ee3a47bd8f843f50361d`;
file and resident sizes are 1,226,020 and 180,192 bytes. The 20.18-second
observation window is not measured completion time or console performance.

## Live adapter and reproduction

The separate live adapter accepts completed, observer-parity-validated main
contact captures. Its offline pre/post boundaries are ROM `0x34a88` and
`0x34b04`, after the two middle inputs are zeroed and after all three results
are stored. It binds each pair to the actual saved main-call frame, requires
all four ordered pairs when that path runs, checks unchanged inputs and object
state, and checks capture completion/sample counts. Inputs outside the native
domain fail the comparison instead of being omitted. Merely capturing the
larger contact pass does not establish a matching cross-product result.

Capture 008 passed all 2,850 observer-enabled/disabled frame checkpoints, but
its initial sample schedule missed every active cross operation. The strict
consumer correctly rejected that empty arithmetic sample. Its cadence evidence
identified nine active frames, 2,571–2,579; a fresh capture 009 added precisely
that window and again matched every one of the 2,850 checkpoints.

Capture 009 supplies **all 36 observed cross operations**, with 36 distinct
six-word inputs from main object `0x500000`. All 26 sampled parent contact
passes have complete caller/return proofs; nine contain the four cross pairs.
Native output, isolated original-firmware output and all three captured result
words agree exactly. Thirteen calls produce a nonzero vector. Both middle
inputs are positive zero, as established by the caller's stores. Input
magnitudes stay below 52.04 and output magnitudes below 59.20 in this replay;
these numbers carry no inferred physical units. No captured input is
subnormal, so subnormal coverage remains synthetic.

The combined comparison has 110,084 evaluations, 110,068 distinct inputs and
2,642,016 original instructions, fingerprint `61cedd9373797928`. Its source-bound
report is `build/ps2-native-rounded-cross-live-002/result.json`, refreshed against
the final capture helper. Measured caller domains remain recorded in
`build/ps2-native-rounded-cross-live-001/domain.json`. The validated main-contact
stream SHA-256 is
`e6845aae16d47a88592e37adad91a02c9d723d565b511c735e09ff13379023dd`.
This covers the observed replay window, not every mode or complete response
path. The full native contact pass remains disconnected from gameplay.

```sh
python3 tests/test_ps2_rounded_cross_product.py
python3 platform/ps2/tools/check_rounded_cross_product.py \
  --tgp-program /path/to/owned/geometry-program.bin \
  --output build/rounded-cross-oracle
```

An optional `--capture build/.../capture` adds strict live comparisons. The
initial synthetic report is
`build/ps2-native-rounded-cross-oracle-001/result.json`. Original firmware,
generated code, captured values and reports remain under ignored `build/`.
The reference processor and capture adapter are excluded from native builds.
Neither isolated firmware comparison nor an emulated EE fixture certifies
original PCB arithmetic, a complete contact pass or physical PS2 execution.
