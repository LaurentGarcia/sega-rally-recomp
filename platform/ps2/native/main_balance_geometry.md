# Main-helper geometry and publication tail

`publish_main_balance_geometry` independently implements the complete final
geometry/publication region of the main vehicle's larger balance helper. It
combines two groups of scalar values, applies two recovered polynomial rotations,
updates two caller-owned triples and publishes a selected index. It contains no
original processor, mapped address, command transport, global matrix stack or
embedded game asset. The two accumulator roles and scalar names describe value
flow; physical axes, units and a vehicle time step remain unproven.

This follows the native exception in
[Platform layer & build](../../../docs/daytona-usa-recomp-design.md#platform-layer--build).
It does not complete the larger helper or connect a native driving loop.

## Boundary and remaining dependencies

The recovered region is `[0x382f8,0x38658)`, 864 bytes, SHA-256
`f06980f8870d7e5471caa73f1d7f5be9a305ec45d8876bb8fd3f871fddcb246c`.
Its observed relocated boundaries are `0x5d72f8` and `0x5d7658`, after the last
CPU subroutine call and before register-restoring epilogue. This region has no
CPU callees. The owner-provided complete program identity is
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.

| Larger-helper region | Current recovery |
| --- | --- |
| `0x37870..0x37900` | Parameter-record selection recovered; called `0x3a320` input/state calculation remains unrecovered |
| `0x37900..0x379d0` | [Paired sums and balances](paired_balance.md) recovered |
| `0x379d0..0x382f8` | Scalar filtering, limiting and event-related dependencies remain unrecovered |
| `0x382f8..0x38658` | Complete geometry/publication tail recovered here |
| `0x38658..0x38674` | Original register restoration and return, unnecessary in a typed native function |

The complete helper still has nine direct/BAL dependencies: `0x38680`, `0x388d0`,
`0x38958`, `0x389b8`, `0x38a18`, `0x39750`, `0x39c40`, `0x3a320` and `0x3aa38`.
The `0x3a320` dependency itself includes indexed data, additional calls, geometry
operations and state updates; it is not just a scalar input map. The `0x38680`
and `0x388d0` paths include sound-command calls. Future recovery can represent
verified event outputs as typed commands, but their semantics are not supplied
by this component. Full disassembly and derived byte streams remain private.

## Typed inputs and operation

The input contains two longitudinal-role values, two lateral-role values, two
paired balances, steering and middle-orientation values, and an uninterpreted
32-bit selected index. State contains two triples and the previously published
index. These names identify storage/calculation roles, not recovered physical
coordinates. The original observed input/output triple buffers are distinct;
the native state owns both by value.

Let `R32` and `R64` denote separate nearest-even binary32/binary64 rounding. All
constants below use their stated binary64 representations. The native integer
implementation retains each boundary, including signed-zero results.

If the first longitudinal value is strictly negative, it is added to the second
with `R32`, then replaced by positive zero. Negative zero does not take this
branch.

The initial middle contribution is computed as:

```
second_weighted = R32(R64(R64(second_balance × binary64(0.66)) × second_longitudinal))
first_weighted  =     R64(R64(first_balance  × binary64(0.635)) × first_longitudinal)
middle         = R32(R64(second_weighted − first_weighted))
```

The binary32 store of the first line occurs before the subtraction; combining
these expressions algebraically would change rounding. The component then:

1. Builds the recovered middle-axis rotation from the steering value and
   transforms `{first_lateral, +0, first_longitudinal}` into a temporary triple.
2. Adds `R64(R64(second_lateral − temporary.first) × binary64(1.25))` to the
   middle contribution through a binary64 sum and binary32 store.
3. Forms a combined triple from `R32(second_lateral + temporary.first)`,
   `temporary.middle`, and `R32(second_longitudinal + temporary.third)`.
4. Rotates that triple by the middle-orientation value, then adds the old first
   state triple componentwise with `R32`.
5. Adds `{R32(R64(combined.third × binary64(0.2))), middle,
   R32(R64(combined.first × binary64(-0.2)))}` to the old second state triple
   componentwise with `R32`.
6. Publishes the input index without interpreting its bits.

The rotations use the recovered periodic polynomial and ordered matrix-column
updates, not a library sine/cosine approximation. They reuse
[`rounded_affine_transform`](rounded_affine_transform.md) for each rounded
transform. Only the single middle rotation is applied at each stage: adding
extra zero-angle stages could change zero signs or rounding. The original
sequence's matrix push/pop saves and restores outside state; the native value
operation simply has no external matrix state to alter.

Longitudinal/lateral magnitudes are bounded by `2^16`, balances by 16, both
angles by 8, and initial state components by `2^20`. All used values must be
finite; signed zeros and subnormals are supported. The coefficient products and
aligned binary64 sums fit the private four-limb integer arithmetic. Intermediate
rotation inputs remain within the existing affine primitive's domain. The
function performs no allocation and commits the entire state only after every
validation and calculation succeeds. Unsupported input preserves both triples
and the index.

## Original and captured evidence

The private oracle runs the complete original CPU interval and actual initialized
geometry firmware. Its firmware identity is
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
It rejects external-memory and table access, checks allowed instruction regions,
31 input words and six output words, and checks restored matrix base/depth and
all 12 saved matrix words. Non-output ordinary RAM must remain unchanged.
The native runtime uses no firmware or coefficient asset.

The original comparison passes 20,480 synthetic cases, including both fold
branches, all 256 zero-sign combinations across the eight scalar/angle inputs,
subnormal and boundary values in each numeric input, every declared input limit,
and 20,000 deterministic bounded bit-pattern cases. Its 3,173,890 CPU and
13,783,767 geometry instructions yield fingerprint `56b32fd92694a6a0`.
This is broad sampled coverage, not exhaustive binary32 certification.

Capture 013 matches all 2,850 enabled/disabled reference checkpoints and preserves
the earlier capture streams byte-for-byte. It observes 601 completed tail calls
and samples 26 paired calls. Every sampled native result matches all seven
original output words: both triples and the published index. Three sampled calls
take the strictly negative first-longitudinal branch; the other 23 do not.
All sampled indices are 2, while synthetic inputs test arbitrary index bits.
The reader binds helper/parent return proofs, exact boundaries and saved buffer
identities; the numerical wrapper additionally verifies unchanged object state,
zero scratch, preserved pointers/index and publication value.

The combined 20,506-case comparison executes 3,177,900 CPU and 13,801,262 geometry
instructions, with fingerprint `12f85315dea7bb1c`. The final source-bound private
report is `build/ps2-native-main-balance-geometry-live-002/result.json`, SHA-256
`735c0e42bb757bee7c32b799034f4a69ad293ff327d693f1107f02fccf910857`.
All original images, generated comparison source and captured values remain
under ignored `build/`.

Four portable tests cover the native fingerprint and strict malformed-capture
rejection. ASan/UBSan passes the 1,504-case probe, 84 atomic rejection checks,
two independently calculable controls and complete 20,480-case corpus. The
source-bound host report is
`build/ps2-native-main-balance-geometry-host-001/validation.json`, SHA-256
`1517a41735385c2d6c52a4c40b582d2ea6cd2695a4fe97b263621b17aa2191f1`.

The standalone EE fixture also passes the same 1,504-case probe, fingerprint
`3e2a507aab74e9d4`, 84 rejection checks and two controls in Play 0.72 with its
built-in HLE. The ELF is 1,234,556 file bytes and 188,000 resident bytes, SHA-256
`3a8d5a720857face4899568e0ddf0ed93f826b526a46bc7727c84b569ae41a05`.
Its source-bound execution record is
`build/ps2-native-main-balance-geometry-ee-001/runtime-validation.json`;
guest stderr is empty. The 20.274-second observation window is not a measured
fixture execution time or a hardware performance result.

Physical-console validation, a complete larger helper and a connected driving
loop remain outstanding.

```sh
python3 tests/test_ps2_main_balance_geometry.py
python3 platform/ps2/tools/check_main_balance_geometry.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tgp-program build/ps2/native/tgp/program.bin \
  --capture build/ps2/driving-capture-013/capture \
  --output build/ps2-native-main-balance-geometry-live-new
```
