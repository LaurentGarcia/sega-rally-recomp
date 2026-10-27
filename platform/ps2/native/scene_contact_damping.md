# Scene-relative contact damping

`correct_scene_contact_damping` supplies the previously unsupported scene-relative
branch of `update_main_motion`. Its inputs are ordinary point, direction and
scalar values. It retains no original addresses, processor state or geometry
command queue. The native caller owns the surrounding scene and resolves its
published point, reference point and direction before calling this operation.

The original region is ROM `[0x36ee0, 0x37024)`, 324 bytes, SHA-256
`21a7eed194b587389e78b2a960ddc877ece2fd4e50cdb0d4e7fd62819d24081e`.
It reads the published point and following direction triple through the pointer
already in `g7`, the reference point through global `0x213980`, and base damping
from `g13`. It writes three temporary displacement words at frame offsets
`0x160..0x168`, then replaces `g13` with the resulting damping. The containing
motion stage uses this result for its separately rounded X/Z motion products.
The surrounding gate requires nonpositive selected scalar, nonzero active
contacts, signed global `0x20a560 > 0`, and nonzero published-point pointer
`0x2139f8`. The native runtime receives these relationships as explicit values.

Each displacement component is rounded to binary32. Command `0x59` computes
three rounded direction/displacement products, then the rounded X-plus-Y sum
and final Z addition. Its instruction span is firmware `[0x530, 0x53a)`, SHA-256
`5a95f531c51bca486f6af7274d8762c7bfe2c74881df259d9c218297d86e5178`.
A nonpositive dot result preserves the base damping bits, including negative
zero and values above one.

For a positive dot, the original command `0x58` receives adjacent pairs from
`{0, 0, 0, dx, dy, dz}`. The native operation therefore calls the existing
recovered metric with `{0, 0, dy}` and `{0, dx, dz}`. This pairing and the metric's
established correction are significant: it is not a Euclidean norm, and even
unit X displacement does not give exactly one. The scalar result is squared
with binary32 rounding; the dot is divided by that square with binary32
rounding. The quotient is multiplied by binary64 `0.005`, added to the base in
binary64, then upper-clamped to one or rounded to binary32. There is no lower
clamp. Native integer value arithmetic preserves these distinct boundaries.

The supported inputs are finite and within ±2²⁰. On the positive-dot branch,
the recovered metric's own input and squared-sum domain must hold; its rounded
square must be finite and nonzero, and the quotient must be finite. Unsupported
cases reject atomically. In particular, equal nonzero Y/Z displacement can
produce a zero metric through the original pairing. The implementation does
not claim to reproduce the original's nonfinite intermediate behavior. The
nonpositive-dot path does not evaluate or validate an unused metric.

The final source-bound original comparison is
`build/ps2-scene-contact-damping-original-002/result.json`. It executes the
verified original CPU region and complete pinned geometry firmware with no
table or external geometry-memory access. All 4,116 accepted synthetic cases
match, including 2,007 positive-dot corrections; three deliberately unsupported
cases leave the native output untouched. The comparison executes 182,147 CPU
and 240,066 geometry instructions, with fingerprint `714dc71e65aaf84c`. It checks
the resulting scalar, command/output counts, guarded temporary memory and
bounded completion. Program, firmware, generated oracle and source identities
are recorded privately. No original bytes are included in the source tree.

The portable test has 110 checks, including signed zero, upper-clamp behavior,
ordered cancellation, explicit domain rejection and legal scalar output reuse.
It also executes the containing native motion update: active correction changes
motion after position publication, a rejected correction preserves the complete
state and command list, and inactive invalid scene data remains unused.
Address/undefined-behavior sanitizer evidence is recorded in
`build/ps2-scene-contact-damping-sanitized-001/result.json`.

The first portable control incorrectly assumed the recovered metric gave one
for unit X displacement. That assumption failed immediately; the control now
independently evaluates the established rounded metric formula. Production and
the original-program comparison did not use that assumption.

The existing 601 captured motion stages have this scene branch inactive. They
can establish that integrating this operation preserves their behavior, but
they cannot validate active scene ownership or this branch on a real course.
Active behavior currently has synthetic original-program proof and host
integration tests. There is no physical PS2 validation, continuous native driving
claim or performance claim from this fixture.

```sh
python3 tests/test_ps2_scene_contact_damping.py
python3 platform/ps2/tools/check_scene_contact_damping.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tgp-program build/ps2/native/tgp/program.bin \
  --output build/ps2-scene-contact-damping-check
```
