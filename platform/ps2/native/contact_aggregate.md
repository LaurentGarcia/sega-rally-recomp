# Native contact response aggregation

`contact_aggregate.h/.cpp` recovers two bounded operations from the main
four-point contact pass: signed scalar accumulation and the final state update
on an already-active response path. The design's **Platform layer & build**
native exception governs this typed implementation. It contains no original
instructions, mapped addresses, device state, allocation or generic replacement
physics.

The earlier response gate, construction of each response vector and its
state-dependent gain are caller responsibilities. These functions do not make
the complete contact pass or native driving operational.

## Original identities

Intervals are program byte offsets with excluded ends, from the verified
Revision B program identified in [contact-result recovery](contact_result.md).

| Region | Interval | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Sign comparison consumed by accumulation | `0x34b48..0x34b4c` | 4 | `1576ac16c32d8c1e026cc53a61728e2724931abb39d34cda9feec68f71aa5cbc` |
| Signed scalar accumulation | `0x34b5c..0x34b84` | 40 | `98250fdd5ece906e4e9df98566239aae6e77aad7a0f2b00f08ecb79c3a1ab227` |
| Final aggregate update | `0x34b90..0x34cc0` | 304 | `71e1c406f6c710ac7349859e7f738f1ff2a1929800653b712c49a1c5cb4ae017` |

The first region supplies the condition from the cross product's middle
component. The accumulation region consumes the explicit scalar returned by
the [recovered metric](recovered_scalar_metric.md). Native code accepts that
scalar directly; it does not duplicate or execute the geometry command.

## Typed contract

`accumulate_contact_scalar(accumulator, metric, sign_source, output)` computes
`R32(R64(accumulator - signed_metric))`, flipping the metric's sign only when
`sign_source < 0`. Both signs of zero take the nonnegative branch. The scalar
must be nonnegative, with either zero sign accepted; all three inputs must be
finite and have magnitude at most `2^20`. Output may alias an input float.

Here `R32` and `R64` denote nearest/even binary32 and binary64 rounding. For
these binary32 operands, the intermediate binary64 rounding cannot change the
subsequent binary32 result: when the smaller operand can affect the binary32
rounding decision, the required significant tail fits binary64. The native
operation therefore uses exact bounded integer addition/subtraction and one
binary32 rounding. Original-instruction comparison checks this equivalence.

`apply_contact_aggregate(input, state)` consumes the accumulated scalar, four
response triples, three bases, two divisors and an explicit feedback flag.
Its state contains three motion fields, candidate position/orientation triples
and two feedback integers. The storage names do not assign physical units.

| Typed member | Original field or source |
| --- | --- |
| `bases[0..2]` | Object `+0x14`, `+0x1c`, `+0x3c` |
| `motion[0..2]` | Object `+0x2c`, `+0x34`, `+0x48` |
| `response_divisor` | Object `+0x50` |
| `scalar_divisor` | Object `+0x58` |
| `responses` | Four triples in the caller's local response array |
| `feedback_enabled` | True exactly when the original enable word is zero |
| `feedback[0..1]` | Original feedback words at `0x213964` and `0x213968` |

The active path performs these separately rounded operations:

```text
scaled = R32(signed_scalar / scalar_divisor)
motion[2] = R32(old_motion[2] + scaled)

first = R32(R32(R32(response[0].x + response[1].x) + response[2].x) + response[3].x)
third = R32(R32(R32(response[0].z + response[1].z) + response[2].z) + response[3].z)
motion[0] = R32(R32(first / response_divisor) + old_motion[0])
motion[1] = R32(R32(third / response_divisor) + old_motion[1])

candidate_position[0] = R32(bases[0] + motion[0])
candidate_position[2] = R32(bases[1] + motion[1])
candidate_orientation[1] = R32(bases[2] + motion[2])
```

When enabled, feedback becomes `abs(trunc(R64(scaled * 1000)))` and
`trunc(R64(scaled * 8000))`. These products have at most 37 significant bits,
so they are exact in binary64. Native integer arithmetic forms and truncates
the exact dyadic products. Both signed integer results must fit `int32_t`,
and the absolute value must fit positive `int32_t`; otherwise the whole update
is rejected. Disabled feedback retains both previous integers exactly.

Every arithmetic input is finite with magnitude at most `2^20`. Either sign
of divisor is accepted, with magnitude in `[2^-20,2^20]`. Response middle
components, candidate-position middle and candidate-orientation outer fields
are unused and preserve arbitrary encodings. The full result is staged and
committed only after every check succeeds. Rejection changes no state field.
Some valid results exceed the input bound, so this is not a closure guarantee
for arbitrary repeated calls or for every possible metric output.

The original routine has an earlier gate that skips this entire path when its
selected response scalar is zero. Calling the typed update with zero input
does not reproduce that gate: an active path still reconstructs its candidate
fields and may write feedback. The caller must keep that distinction.

## Validation

The ROM-free portable fixture passes 25,728 scalar steps and 18,400 updates,
fingerprint `390cbdf6408065cd`, against independent host binary32/binary64
expressions. ASan/UBSan passed the same cases. The shared host/EE probe has
2,240 steps and 912 updates, fingerprint `63248df7338a0797`, plus 33 rejection
checks. Tests cover signed zero, subnormals, cancellation, signed divisors,
preserved payloads, disabled feedback and adjacent binary32 inputs around the
feedback integer limit.

The original CPU comparison passes all 44,128 synthetic operations with the
same fingerprint, executing 1,271,676 original instructions. It verifies the
complete bounded memory image and ordered stores, including the temporary
local store and conditional feedback overwrite. An early oracle expectation
incorrectly expected the redundant absolute-value store whenever the second
feedback was negative. When the first product truncates to zero, that store is
correctly absent; fixing the expected store sequence required no native change.

Private capture010 passed all 2,850 observer-enabled/disabled checkpoints.
All 36 signed scalar steps and nine aggregate updates match the original CPU
and native implementation. Six steps use the negative sign branch. One update
has feedback enabled; eight preserve disabled feedback. The observed divisors
are 1,000 and 1,146, without an inferred physical unit. Including synthetic
cases, the final comparison has 25,764 steps, 18,409 updates and 1,272,452
original instructions, fingerprint `57b055a27cdfbbf8`. Its source/capture
binding is `build/ps2-native-contact-aggregate-live-002/result.json`.

The live parser requires complete sampled main passes, indexed scalar
pre/post pairs, unchanged parent and ordinary-RAM snapshots, the captured
comparison flags, four chained accumulations and the paired final update.
It binds all observed file hashes to the passing observer report. Preserved
object fields, candidate components, response vectors and disabled feedback
must remain unchanged. Aggregate totals cannot exceed the number of main
passes.

Actual EE execution under Play 0.72 HLE passed the common probe and all 33
checks. The ELF SHA-256 is
`cf4761829bbbfbc11ff553720ba5e2d3b801b43b29114dd62e0e4a93c88eb2c6`,
with 1,232,752 file bytes and 186,336 load-segment bytes. The binding is
`build/ps2-native-contact-aggregate-ee-001/runtime-validation.json`. Guest
stderr was empty. The 20.109-second observation window is not an execution
benchmark; window capture failed, and no physical-console result is claimed.

Reproduce the private comparison with the owner's verified program:

```sh
python3 platform/ps2/tools/check_contact_aggregate.py \
  --program build/ps2/native/rom_cache/program.bin \
  --capture build/ps2/driving-capture-010/capture \
  --output build/ps2-native-contact-aggregate-new
```

Omit `--capture` for synthetic comparison only. The output must be a new
directory under ignored `build/`; original code, images and captures remain
private. `python3 tests/test_ps2_contact_aggregate.py` requires no game data.

The remaining main-pass work includes transform construction, per-contact
response-vector construction and scalar gain, the earlier gate, return-value
composition and end-to-end native driving integration. Passing these two
bounded operations does not establish a complete contact or collision system.
