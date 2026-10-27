# Main vehicle contact-slot update

`update_main_contact_slot` independently implements the complete bounded value
operation called four times by the main vehicle callback. It updates one slot,
its packed type nibble and the accumulated type mask, and returns a response
triple and active flag. It uses typed values and an immutable coefficient span;
no guest registers, mapped addresses, processor execution or device transport
are present in the native component.

The governing design is the native exception in
[Platform layer & build](../../../docs/daytona-usa-recomp-design.md#platform-layer--build).
The helper has independent comparisons and is now connected to the persistent
[main callback](main_vehicle_update.md), including its 601-update host/EE proof.
The developer loop remains short of a playable game. The array names describe
observed value flow; suspension units, physical axes and tuning meanings remain
unproven.

## Original boundary and inputs

The owner-provided program identity is SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The original helper occupies `[0x37200,0x37678)`, 1,144 bytes, with range SHA-256
`0fc800954def0406a250625a02b4ec97b124d6e81a1ea47933c907c35ee92e65`.
Its observed relocated entry is `0x5d6200`; the four callers return to
`0x5d4b88`, `0x5d4bb4`, `0x5d4bf0` and `0x5d4c28`. The offline comparison stops
at either original return instruction after verifying its complete value work;
it does not model caller return behavior in the native API.

The helper has no CPU subroutine calls. It consumes a slot index, a contact
result, three vector components, two reference-direction components, the middle
component of another triple, a factor, an object parameter, selected scalar
parameters, a control value and previous slot state. The other component of the
reference direction, unused parameters, unused contact-record words and unused
state fields retain arbitrary payload bits.

The immutable 8,192 normal-coefficient pairs use the existing
`ContactNormalPair` format. For a private full table image, conversion takes the
magnitude of each stored slope and retains the intercept. That is the same sign
handling as the pinned reference's interpolation port. The verified full table
identity is `3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04`.
No coefficients or extracted records are committed.

## Value behavior

Let `R32` and `R64` mean separate nearest-even rounding to binary32 and binary64.
Binary64 constants below mean their binary64 encodings; decimal labels do not
remove the specified rounding points.

Every successful call publishes `R32(reference_middle + gap)`, replaces only its
four-bit packed type with `status & 15`, and adds that type to the mask.

If status bit 7 is set or the gap is negative, the selected history, gain, metric
and lateral values become positive zero. Both signs of zero are nonnegative.
The response starts at positive zero. A positive paired history at `slot ^ 1`
replaces its middle component with the rounded product chain
`gap × binary64(0.006) × factor × object_parameter × direction.middle`:
each multiplication rounds to binary64, and the final result rounds to binary32.
The returned active flag is false.

Otherwise the helper selects stiffness and damping by the slot pair. The initial
magnitude is `R32(R64(R64(R32(stiffness × gap) × 100) × control))`.
A gap strictly greater than binary64 `0.2` adds
`R64(R64(R64(gap − binary64(0.2)) × factor) × object_parameter)` through a
separate binary64 addition and binary32 store. The retained gap then becomes
binary32 `0.2`. These two representations of `0.2` are deliberately distinct.

The damping change is
`R32(R32(R32(history − retained_gap) × damping) × control)`.
The final magnitude is
`R32(R64(magnitude − R64(damping_change × 3600)))`, clamped to positive zero
only when negative. Each response component is a separate rounded product of
that magnitude and the contact direction. The selected history and gain are
updated, and the active flag is true even when the magnitude clamps to zero.

The original geometry command `0x58` supplies the
[recovered scalar metric](recovered_scalar_metric.md), with input pairs
`{0,0,vector.middle}` and `{0,vector.first,vector.third}`. The result is not
substituted with a generic Euclidean length. Values at or below binary64
`1e-10` publish positive-zero metric and lateral values.

Above that gate, the first/third vector components pass through the original
command `0x2f` interpolation expression. Its ordered rounded squared sum chooses
the coefficient row and exponent scaling. The lateral value is the signed
binary32 square root of `R32(1 − R32(cosine × cosine))`, where cosine is the
absolute rounded two-component dot product, clamped to one. The sign is negative
only for a strictly negative rounded two-component cross product. Zero cross
products, including negative zero, select positive sign.

## Supported domain and atomic behavior

The slot is 0–3. Used gaps, histories, direction components, factor and control
have finite magnitude at most 16. Used scalar parameters have finite magnitude
at most `2^16`; reference and vector coordinates have finite magnitude at most
`2^20`. Unused fields are not validated. The metric's squared sum must satisfy
its existing zero-or-normal contract.

When interpolation is used, its rounded XZ squared sum must be zero or at least
`2^-40`, and at most `2^42`. Zero includes nonzero components whose squares round
to zero. The same interpolation expression runs in that case; it is not replaced
with an invented zero-normal special case. The used table slope must be in
`[1/8,1]` and intercept in `[1,4]`. At squared sum zero, exponent adjustment 63
keeps both scaled coefficients normal and finite. This case is necessary for
four observed vertical-only calls.

Unsupported input returns false without changing any state or output field.
All calculations complete before either output is committed; no input pointer
or table pointer is retained. The component allocates no storage dynamically.
Its local integer dyadics preserve separate binary32/binary64 rounding and
signed zeros without depending on the EE's floating-point mode. Four 32-bit
limbs bound the largest exact binary64 product and aligned sum.

## Validation

Original, independently authored synthetic inputs cover all 256 low status
bytes in all four slots, gap neighbors around binary64 `0.2`, signed zeros,
subnormal exponents, both inactive sibling branches, random bounded inputs and
64 zero-XZ/underflow-XZ controls. The 25,852 cases include 7,248 active updates;
the native and original CPU/geometry outputs have fingerprint
`121b47e419609ea7`. These are case counts, not an exhaustive binary32 domain
claim. The original firmware starts through its verified initialization path;
constant preconditions, bounded instruction spans, transfer counts and forbidden
external accesses are checked. Table reads use explicit private or original
synthetic coefficients.

Capture 012 independently matches all 2,850 enabled/disabled observer checkpoints.
It records 104 distinct slot inputs from 26 sampled main-callback executions:
99 active and five inactive calls. All 26 output/state words per call match both
the original helper and native component, including unchanged slot state.
Observed statuses are 2 and 128; the synthetic coverage is wider. The combined
25,956 comparisons execute 2,393,780 original CPU and 504,364 geometry
instructions and have fingerprint `57a543d2f4626b3f`.

The final private comparison is
`build/ps2-native-main-slot-live-002/result.json`, SHA-256
`c35715459a3bcc96e584f59b6d0d9855653b848fcefacd659d1a23266d15441a`.
Its predecessor preserves the failed initial domain probe; it is not passing
evidence. The final wrapper binds the images, source closure, capture, checkpoint
files and generated comparison executable. Original instructions and capture
bytes remain under ignored `build/`.

Four portable tests cover arithmetic fingerprints, atomic failures and malformed
capture bindings. ASan/UBSan passes the 2,876-case probe, 316 rejection checks,
two independently calculable controls and the 25,852-case host corpus. The
source-bound host report is
`build/ps2-native-main-slot-host-001/validation.json`, SHA-256
`377a34ec4aece023d0f50aa695cd7ea685772668f744a461a8b16428187f0727`.
The standalone EE fixture executed in Play 0.72 with its built-in HLE and passed
2,876 cases, including 947 active updates, fingerprint `32ef92cd4f0d4583`, plus
all 316 rejection checks and both known controls. Its ELF SHA-256 is
`5572be87518eb41e8de5cfaa8b1abc896b5ed937c71cb1fce44786e32a8e0876`,
1,679,840 file bytes. Guest stderr was empty and two window captures succeeded.
The 20.270-second observation is not measured fixture execution time or a
physical-console performance result. Source/build/ELF/log identities are bound
in `build/ps2-native-main-contact-slot-ee-001/runtime-validation.json`.
Physical PS2 execution and a connected native driving loop remain unverified.

```sh
python3 tests/test_ps2_main_contact_slot.py
python3 platform/ps2/tools/check_main_contact_slot.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tgp-program build/ps2/native/tgp/program.bin \
  --tables build/ps2/native/tgp/copro_tgp_tables.bin \
  --capture build/ps2/driving-capture-012/capture \
  --output build/ps2-native-main-slot-live-new
```
