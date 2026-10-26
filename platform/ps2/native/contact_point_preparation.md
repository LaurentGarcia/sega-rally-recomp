# Native contact-point preparation

`prepare_contact_points` constructs the main contact pass's orientation matrix,
transforms its four local points, and adds the candidate position. It is an
independently written native value operation. It contains no processor state,
addresses, command decoder, device access, external trigonometric table or
original generated instructions. It reuses the separately validated
`rounded_affine_transform` value operation.

The implementation follows the governing design's **Platform layer & build**
native PS2 exception. The original machine remains an offline behavioral
reference. This component does not recover motion integration, a complete
frame loop, physical units or console performance.

## Contract and arithmetic

The input consists of three orientation components, a position, and four local
XYZ triples. `ContactPointPreparation` returns `matrix`, `offsets` and `points`.
Orientation components must be finite and within `[-8,8]`; position and local
coordinates must be finite and within `[-2^20,2^20]`. Signed zero and subnormal
inputs are supported. Rejection leaves the entire output unchanged. Inputs
are copied before calculation, so they may reference output members.

The original float-angle interface uses an internal polynomial. Its raw-angle
interface can use external table ports, but that is a different command path.
Replacing this polynomial with `sin`, `cos`, or an ordinary rotation library
would change the recovered arithmetic.

Write `R` for binary32 rounding to nearest, ties to even. For one input `a`:

- `n = floor(R(abs(a) * reciprocal_pi))`.
- `x = R(R(abs(a) - R(float(n) * pi)) - half_pi)`.
- `t = R(x*x)`; successive even powers use `power[k] = R(t*power[k-1])`.
- Multiply each even power by its coefficient separately. Accumulate from the
  degree-12 term downwards with alternating subtraction/addition, then add one;
  every multiply and addition/subtraction has its own `R` boundary.
- Negate the resulting polynomial when `n + (a <= 0)` is odd. Both zero signs
  satisfy that comparison.

The numerical constants are fixed binary32 values:

| Quantity | Encoding |
| --- | --- |
| `pi`, `half_pi`, `reciprocal_pi` | `40490fdb`, `3fc90fdb`, `3ea2f983` |
| Coefficients of powers 2, 4, 6 | `3f000000`, `3d2aaaab`, `3ab60b61` |
| Coefficients of powers 8, 10, 12 | `37d00d01`, `3493f27e`, `310f76c7` |

The sine-like coefficient is the polynomial result at `a`; the cosine-like
coefficient is the result at `R(a + half_pi)`. No generic trigonometric
accuracy claim is attached to those names. The rounded shift is retained.
The bounded angle range keeps `n` in `[0,3]`, making its integer-to-float
conversion exact. Range reduction keeps `abs(x) < 2`; conservative bounds on
powers, polynomial terms and three matrix updates remain far below binary32
overflow. Integer significand arithmetic preserves every rounding boundary,
including signed cancellation and underflow, without depending on EE floating
point rounding.

Starting from identity, the component applies the **middle**, **first**, then
**third** orientation values. These names do not assume yaw, pitch or roll.
For each row, each product below rounds separately before the final sum:

| Stage | Column updates, using the previous columns |
| --- | --- |
| Middle | `C0 = R(R(c*C0) + R(s*C2))`; `C2 = R(R(c*C2) - R(s*C0))` |
| First | `C1 = R(R(c*C1) - R(s*C2))`; `C2 = R(R(s*C1) + R(c*C2))` |
| Third | `C0 = R(R(c*C0) - R(s*C1))`; `C1 = R(R(s*C0) + R(c*C1))` |

Translation remains positive zero. Identity products are evaluated rather than
algebraically removed. Each local point then uses the existing translation-first,
column-ordered rounded affine transform. Each final query coordinate is one
separately rounded addition of its offset and candidate position.

## Original evidence and private comparison

The original CPU preparation interval is `[0x34380,0x346d8)`, 856 bytes,
SHA-256 `4298396bc5e5ca77d1b9c95485dabaaec2534c5adca65ac2f11a8a6fb117e776`.
It stores four indexed local points, pushes the previous matrix, resets the
new matrix to identity, applies float-angle commands `2a`, `29`, `2b`, transforms
and reads each point, then pops the matrix. The native API has no stack: it
constructs its local value directly. The oracle establishes equivalence when
the original stack has capacity, and verifies restoration of a varied previous
matrix for every case. Full-stack behavior is outside this native contract.

The four owner-supplied local triples occupy `[0x33e30,0x33e60)`, 48 bytes,
SHA-256 `1f2b37b346c38c1a535c6aa8dd85af25d5897483480148ce5ce424b423aeda7a`.
They are input data, not values embedded in tracked source or tests. The
verified full program SHA-256 is
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.

The verified 6,660-byte geometry firmware SHA-256 is
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
Its polynomial interval `[0x1e6,0x21d)` has SHA-256
`852c3c81157047d7b219d662648aec78eace62a3ef552630108302b850ebafcc`.
The private checker verifies all rotation, indexed point, transform, identity,
stack and initialization regions individually as well as both complete images.

The oracle executes the verified 108-instruction startup, checks every numerical
constant, and runs actual command dispatch, matrix construction, point writes,
transforms, readback and stack restoration. An instruction allowlist rejects
other firmware paths; external memory reads/writes fail, external tables remain
null, and table-port state must stay unchanged. Each bounded command group must
return to the input stall with all expected words consumed. The CPU's final
`addr` ordering is independently evaluated with explicit binary32 operations;
this synthetic oracle does not execute the complete CPU preparation interval.
The sealed live comparison below checks its actual CPU outputs.

The synthetic comparison passes **49,295 evaluations**, **49,287 distinct
18-word inputs** and **46,675,549 original firmware instructions**, fingerprint
`f05d0d03253121d8`. Coverage includes zero signs, every permitted angle exponent,
encoding neighbors around quarter/half/full rotations, endpoint angles,
coordinate subnormals and bounds, 8,193 stratified mixed-angle samples and
40,000 deterministic random cases. It samples the domain; it does not enumerate
all float triples. The independent volatile-float host reference matches.

Successful aliasing of all three input references into the output is checked
for each fixture case. An additional 114 invalid-input cases verify atomic
rejection. AddressSanitizer and UndefinedBehaviorSanitizer pass, recorded in
`build/ps2-native-contact-point-host-001/validation.json`. The standalone EE
fixture passed 3,279 cases and fingerprint `ef519d24a3f602fa` in Play 0.72 HLE,
including all three alias modes and 114 rejection cases, with empty guest
stderr. Executed ELF SHA-256 is
`af5a0dcf4f1f14edeab66e9b36c6ed75f5ba1ea6f4ee320d747475c8c5518168`
(1,232,160 file bytes). The 20.203-second observation window is not execution
time or physical-console performance. Window capture attempts failed; the
source-bound numerical result is recorded in
`build/ps2-native-contact-point-ee-001/runtime-validation.json`.

## Captured driving comparison

Capture 011 matched all 2,850 observer-enabled/disabled frame checkpoints. Its
26 sampled preparation calls provide raw candidate position/orientation, the
four unchanged local triples and all 24 output words. The reader requires the
verified saved parent frame, exact entry/prequery boundaries, unchanged inputs
and object state, nearest/even CPU rounding, complete enclosing return and
matching sample/cadence counts. The local triples must also match the verified
program image. No extra device read is performed.

All **26 distinct captured preparations** match the native implementation and
isolated original firmware exactly. Including the synthetic suite, the result
is **49,321 evaluations**, **49,313 distinct inputs**, **46,700,153 instructions**,
fingerprint `a0a0b262f4a83792`. The source-bound report is
`build/ps2-native-contact-point-live-001/result.json`. This proves the measured
replay sample; it does not establish every course, car, mode or PCB arithmetic.

```sh
python3 tests/test_ps2_contact_point_preparation.py
python3 platform/ps2/tools/check_contact_point_preparation.py \
  --program /path/to/owned/program.bin \
  --tgp-program /path/to/owned/geometry-program.bin \
  --capture build/ps2/driving-capture-011/capture \
  --output build/contact-point-proof
```

Omit `--capture` for synthetic-only comparison. Original instructions, generated
source, local point values and capture records remain under ignored `build/`.
The offline oracle and capture reader are excluded from native application
source inventories.
