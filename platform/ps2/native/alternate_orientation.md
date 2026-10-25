# Alternate orientation update

`smooth_alternate_orientation(current, candidate, output)` independently
implements the bounded arithmetic update in the alternate integration route.
It follows the governing design's **Platform layer & build** native-rewrite
exception. The production component uses typed angles and integer arithmetic;
it contains no original addresses, registers, processor, bus, geometry queries
or scheduling. The surrounding integrator and a native driving loop remain
unimplemented. The observed objects' player/AI ownership is not established.

This update differs from [orientation finalization](orientation.md). It does
not bound the first and third channels to pi/8. Channel names preserve order;
they do not establish physical axes, units or a timestep.

## Typed contract

The output contains both the folded candidate triple and the resulting current
triple. Each input component must be finite and within
`[-round32(2*pi), +round32(2*pi)]`. After folding the candidate, every
binary32-rounded `current-candidate` difference must fit that same interval.
Any failure preserves every output bit. Both inputs may alias either output
member. These restrictions belong to the native interface; the original region
does not reject inputs through a boolean result.

Let `F` be the separately recovered [one-step angle fold](angle_fold.md), and
let `R32` and `R64` denote nearest/even rounding to binary32 and binary64:

```
folded[i] = F(candidate[i])
delta[i]  = F(R32(current[i] - folded[i]))

outer[i]  = R32(R64(folded[i] + R64(scale * delta[i])))
```

The outer formula applies to the first and third channels. `scale` is exactly
binary64 `0x3feccccccccccccd`, the nearest binary64 value to 0.9. Its product
and the following sum have separate binary64 rounding boundaries. Fusing them,
performing them directly in binary32 or replacing the wrapped difference with
ordinary linear interpolation changes the contract.

For the middle channel, `step` is exactly binary64 `0x3f91e2efa68c6f10`
(`0.01746725515395925`), which is not exactly pi/180:

```
if abs(delta.middle) < step:
    middle = folded.middle
else:
    middle = R32(R64(current.middle - signed_step))
```

`signed_step` takes the sign of the difference. The comparison is strict;
there is no final fold or clamp after the step or outer interpolation. Signed
zero and gradual binary32 underflow are included in the result contract.
Floating exception status and other rounding modes are excluded.

## Exact arithmetic and bounds

The implementation represents signed values as seven 32-bit limbs in units
of `2^-202`. A minimum binary32 subnormal is `2^-149`, while the exact scale
is `0x1ccccccccccccd * 2^-53`; their product reaches precisely that grid.
The scale product needs at most 77 significant bits before alignment. Every
intermediate magnitude is below 16, including the pre-rejection subtraction,
so fewer than 207 bits suffice within the 224-bit storage.

Unsigned limb addition/subtraction and a 53-by-24-bit product preserve the
exact numerators. Explicit guard, sticky and retained low bits implement
nearest/even rounding. Binary64 operations round to 53 significant bits;
binary32 operations round to 24 with a minimum quantum of `2^-149`. Each
binary64 boundary is retained before the final binary32 conversion, including
potential double-rounding cases. Production arithmetic does not depend on
host or EE floating-point operations.

The existing angle fold is reused only after checking its declared domain.
Candidate folding yields the same one-step result; the independently rounded
difference receives its own domain check before the second fold. An earlier
candidate or channel may already have been calculated locally when a later
channel fails, but output is committed only after all checks succeed.

## Original boundary and oracle

The verified owner's Revision B image has SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
Only offline tools contain the address/state mapping:

| Region | ROM interval, end excluded | SHA-256 |
| --- | --- | --- |
| Alternate update | `0x2ecb4..0x2ee00` | `849cc9ef5df8b5544b81463ad002ae6ed4c6529e69d743aff8c1a286e16be5f8` |
| Candidate fold | `0x27b28..0x27bc4` | `bd67bceb1abed34efad3c73b3b3ac663d5bc5119dee5e58ab870c7a68cc80ed2` |
| Difference then fold | `0x27bd8..0x27c60` | `2123ca58d261efacb63000cb8cb454a67b6394be4e84fb36f06b243bd73460c7` |

At entry, `r12` identifies the object and `r5` must equal `fp+0x90`. The
candidate resides at `fp+0x90/+0x94/+0x98`; current values are object words
`+0x38/+0x3c/+0x40`. Two four-word loads also read candidate padding at
`fp+0x9c`, which must remain unchanged. The pre/folded/post live-copy boundaries
are `0x5cdcb4`, `0x5cdce0` and `0x5cde00`.

The isolated original-program oracle confines instruction fetches to those
three regions and data accesses to the two triples plus candidate padding.
It compares both output triples, preserves padding and requires exactly one
write to each of the six output words. Ordinary RAM burst behavior is enabled
for the four-word loads. The oracle does not execute the subsequent geometry
query at `0x2ee00` or any other part of car integration.

The original differential passed **93,943 accepted evaluations**, containing
**93,903 distinct six-input states**, and executed **13,506,635 original
instructions**: 130–195 per evaluation, with 16 reads and six writes each.
Its fingerprint is `e1aaef2718a4fd38`. A separate host arithmetic definition
agrees across 102,892 cases, including 8,949 native-domain rejections; these
counts are not an exhaustive sweep of the complete six-component domain.
Host AddressSanitizer and UndefinedBehaviorSanitizer pass the same cases.
Tests additionally cover 60 invalid encodings placed in every input channel,
with unchanged-output and two input/output alias configurations.

The standalone EE fixture passed 3,728 original input cases (3,638 accepted),
fingerprint `f9e5c92050970893`, plus the 60 invalid-input cases and both alias
configurations in Play 0.72. The native ELF has SHA-256
`ca3b1a51d0fa86f80b03db850ae21624a614382a735267024dd416ae2241562b`;
guest stderr was empty. Its 15.066-second observation window is not a measured
fixture completion time or physical-console performance result.

Validated driving capture `driving-capture-007` supplies 34 complete updates,
all with distinct input states, from two objects during frames 2,220–2,760.
Every candidate/current component and rounded difference fits the native
domain. Both captured output triples agree bit-for-bit with native and isolated
original execution. The middle channel takes the snap path in 26 samples and
the step path in eight; no candidate wrap occurs in these samples, so wrap-edge
coverage comes from the synthetic tests. Input components range approximately
from -0.877380 to 0.042574. This is a bounded observed domain, not every mode or
course.

The combined differential has 93,977 accepted evaluations, 93,937 distinct
input states and 13,511,171 original instructions, fingerprint
`f1d52dd19971b0f0`. Its report is
`build/ps2-native-alternate-live-001/result.json`. The captured event stream has
SHA-256 `5c26bf880c1004c06c43574065d51fb64726c8885ee0f58f80a2adc26b98e54a`.
All 2,850 observer-enabled and disabled frame checkpoints agree. Failed earlier
capture attempts are preserved separately and are not accepted as live proof.

The original reference uses the project's host instruction core. Matching it
does not independently certify PCB arithmetic, floating exception flags, a
complete course or physical PS2 behavior. Game-derived records and original
program bytes remain private beneath ignored `build/`.

## Reproduce

```sh
python3 tests/test_ps2_alternate_orientation.py
python3 platform/ps2/tools/check_alternate_orientation.py \
  --program /path/to/owned/program.bin \
  --output build/alternate-orientation-oracle
```

The checker also accepts `--capture build/.../capture`. It requires complete
pre/folded/post triples from a successful observer-parity run, verifies exact
capture hashes, candidate/object identity, nearest rounding and unchanged
padding/unrelated object words, then compares the native and original-program
outputs with both captured triples. Missing or inconsistent groups are refused.
An actual input outside the declared native domain is a failed comparison,
not a sample silently removed from the report.
