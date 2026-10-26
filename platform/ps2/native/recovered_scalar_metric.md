# Recovered scalar metric

`recovered_scalar_metric(first, second, output)` independently implements the
bounded scalar operation used by geometry command `0x58`. It preserves the
current pinned reference's arithmetic, including its unusual first correction.
It is not a correctly rounded Euclidean distance or a substitute for recovered
contact physics. The native function contains typed points and integer
arithmetic; firmware execution, addresses and transport exist only in the
offline checker. The design's **Platform layer & build** native-rewrite
exception governs this implementation.

## Domain and result

Each coordinate must be finite binary32 with magnitude at most `2^20`. Both
signed zeros and subnormal coordinates are allowed. The separately rounded
squared sum defined below must be zero or normal; a positive subnormal sum
returns `false`. These explicit native rejection rules do not describe an
original-game error interface. Any rejection leaves output unchanged.

Output may alias any coordinate of either input, including when both points
are the same object. All six input encodings are copied before validation and
the scalar is committed once. Output is nonnegative, with positive zero for a
zero squared sum. The input bound is not a guaranteed bound on the result;
this operation is not closed under arbitrary reuse of its output.

Let `R32` denote nearest/even binary32 rounding with gradual underflow. For
points `A` and `B`, the ordered computation is:

```
d0 = R32(B.x - A.x)
d1 = R32(B.y - A.y)
d2 = R32(B.z - A.z)
S = R32(R32(R32(d2*d2) + R32(d1*d1)) + R32(d0*d0))
```

For positive normal `S`, replace its exponent with 126 to obtain `m` in
`[0.5,1)`. Let `e` be the original biased exponent minus 126. The semantic
constants are binary32 encodings `C0=0x3f1714ba`, `C1=0x3ed5a9a8` and
`K=0x3f3504f4`:

```
seed = R32(C1 + R32(m*C0))
h = R32(R32(1 + seed) * 0.5)
q = R32(R32(R32(m/h) + h) * 0.5)
if e is odd:
    q = R32(q*K)
    e = e+1
result = q scaled by 2^(e/2)
```

Every displayed rounding boundary matters. Scaling replaces the exponent
without a further rounding step in the accepted domain. The division omitted
from the first correction is `seed/seed`, exactly one here; replacing it with
`m/seed` changes the pinned reference. A unit difference returns encoding
`0x3f825985`, approximately `1.0183569`, rather than `1.0`.

This behavior is established against the verified instructions executed by
the existing reference model. It does not establish the original PCB's
pipeline timing, the intended mathematical operation, or a hardware defect.
Using `sqrtf`, changing the input order or silently repairing the first
correction would break the stated contract.

## Bounded integer implementation

The implementation locally reuses the independently written dyadic methods
from the contact and cross-product components; their validated files remain
unchanged. Differences have magnitude at most `2^21`, individual squares at
most `2^42`, and their ordered nonnegative sum is below `2^44`. All remain
finite. Exact significand products use at most 48 bits; the bounded sum
alignment uses at most 56 bits. Guard, sticky and tie-to-even handling preserve
each separate rounding and signed underflow zero.

The correction's division has positive normal operands within `[0.5,2]`.
Scaling its integer numerator requires at most 48 bits. Integer quotient and
remainder determine nearest/even rounding without host or EE floating-point
instructions. The final exponent adjustment stays normal and finite. A
positive subnormal `S` is rejected before exponent handling, so raw exponent
rebiasing is never mistaken for normalizing a subnormal number.

## Original-program comparison

The verified 6,660-byte geometry firmware has SHA-256
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
The private checker additionally verifies these instruction-region hashes:

| Region, in words | Bytes | SHA-256 |
| --- | ---: | --- |
| Command `[0x51c,0x530)` | 80 | `3ae2c280643488b6e1a161908b726c7e1bf4ec72d05d5bdfe2317a95b25a1bc2` |
| Primitive `[0x1ab,0x1cb)` | 128 | `a11bf92c9aa55906b4113ff89a9cefe94555b8a63c9095c13b35e3180e350e57` |
| Initialization `[0x13,0x5e)` | 300 | `ddc9fb4c5c2eeedd460ecc61f7507b6b84c73a09e3e24862a8ec9a30ab214e64` |

The oracle executes verified firmware initialization and checks its reply,
then checks every required constant before and after each command. External
memory accesses are rejected, the external table pointer remains null, and
table-port state must remain unchanged. Command and primitive instruction
paths, both return destinations, each of six FIFO reads and the single result
write are checked. Only four established scratch words may change. The command
takes 24, 49 or 52 reference instructions according to its zero/parity path.

The initial comparison covers **156,644 evaluations** and **156,622 distinct
six-coordinate inputs**. Native success agrees with the original result in
**146,736 accepted cases**. The original executes **148,411 commands**, totaling
**6,996,272 instructions**: an additional 1,675 executions establish the
positive-subnormal sum rejected by the native policy. The 8,233 out-of-bound
coordinate cases test rejection and do not execute firmware. These categories
are not presented as 156,644 successful original-command comparisons.

Coverage includes all 64 input zero-sign patterns, every coordinate exponent
within the bound, fractions around squared-exponent transitions, subnormal and
underflow thresholds, six scales of uniformly sampled mantissas, and 100,000
deterministic random inputs. It samples the domain rather than exhaustively
enumerating all mantissas or point pairs. The independent volatile-float host
reference and original-command oracle share fingerprint `807252722fa67a0c`.

Six coordinate alias positions are checked for every host/fixture case,
including rejected inputs. Separate tests cover 96 invalid-input/alias checks,
identical-input aliasing, and the unit-value discriminator against ordinary
square root. AddressSanitizer and UndefinedBehaviorSanitizer pass. The portable
EE fixture enforces 11,972 cases, 11,234 accepted results and fingerprint
`46407e33dcf00f1b`. It passed in Play 0.72 HLE, including the rejection and
alias checks, with empty guest stderr. Executed ELF SHA-256 is
`b989dc8897c59852908e0aa26f67d88fc1fedff258b3bd7766c82c47af7812b1`;
file/resident sizes are 1,228,276/182,368 bytes. The 20.175-second observation
window is not fixture completion time or physical-console performance.

The initial source-bound oracle report is
`build/ps2-native-scalar-metric-oracle-001/result.json`; host sanitizer evidence
is `build/ps2-native-scalar-metric-host-001/validation.json`.

## Live boundary and reproduction

The original transport interleaves the two points as
`A.x,B.x,A.y,B.y,A.z,B.z`. The main contact caller supplies
`0,0,0,cross.x,cross.y,cross.z`, so its actual native mapping is
`A={0,0,cross.y}`, `B={0,cross.x,cross.z}`. This ordering must not be replaced
by contiguous XYZ triples. The observed earlier cross results have only their
middle component nonzero; that limited observation does not establish a
general distance interpretation or physical units.

The strict live adapter uses the existing `cross_post` boundary at ROM
`0x34b04` as its input, then reads the recorded register result at `0x34b5c`,
after the game's own FIFO read. It adds no device read. Complete parent saved
caller proofs, four ordered indices, unchanged input/object state, transport
registers and capture completion counts are required. Empty samples, rejected
native domains or a mismatch fail the comparison. The subsequent signed
accumulation is a separate component. i960 AC does not control geometry
firmware arithmetic.

Capture 010 matched all 2,850 observer-enabled/disabled frame checkpoints.
All **36 observed scalar calls** in its nine active frames, 2,571–2,579, match
the native function and isolated original command exactly. They represent
16 distinct raw input tuples, including signed-zero differences. Twenty-three
results are positive zero; the remaining 13 are nonzero. All captured inputs
are within the declared domain, with no subnormal input words. This is a
measured replay window, not every driving mode or physical-hardware fidelity.

The combined source-bound report is
`build/ps2-native-scalar-metric-live-002/result.json`: 156,680 evaluations,
156,635 distinct inputs, 146,772 accepted results, 148,447 original commands
and 6,997,470 instructions, fingerprint `0b7c806697edb751`. Measured ranges are
recorded in `build/ps2-native-scalar-metric-live-001/domain.json`. The final
report refreshes source binding against capture 011, which again matched all
2,850 checkpoints and retained the same 36 scalar operations. Its validated
main-contact stream hash is
`a6dc646caf1569f4bb71a5bddfb89d9f6a92cd069a271abba6610de8fbb71600`.
EE execution and its source binding are recorded separately in
`build/ps2-native-scalar-metric-ee-001/runtime-validation.json`.

```sh
python3 tests/test_ps2_recovered_scalar_metric.py
python3 platform/ps2/tools/check_recovered_scalar_metric.py \
  --tgp-program /path/to/owned/geometry-program.bin \
  --output build/scalar-metric-oracle
```

Add `--capture build/.../capture` to require a validated live comparison.
Original firmware, generated code and captured values stay under ignored
`build/`. The reference processor and live adapter are excluded from the
native application. This leaf remains separate from a playable driving loop.
