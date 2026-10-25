# Stateful cabinet steering filter

`filter_cabinet_steering(history, sample, output)` independently implements the
complete bounded filter around the recovered [cabinet byte mapping](cabinet_steering.md).
It returns a new history value and a separate, sign-flipped steering value.
This is input conditioning, not a vehicle simulation, wheel calibration or
force-feedback controller. The component remains disconnected from the native
diagnostic loop.

The governing design's **Platform layer & build** native-rewrite exception
permits this typed component and its separate offline original-program oracle.
The native function has no addresses, guest registers, processor, memory bus,
device access or scheduler. Its arithmetic uses integer operations only.

## Contract and rounding

Every byte sample is supported. History must have a binary32 encoding in
`[-1,1]`, including both zeros and subnormals. Other encodings return `false`
without changing either output field. Input history is passed by value and
may come from either output member. This restriction belongs to the native
interface; the original routine has no equivalent boolean rejection.

Let `mapped` be the exact binary32 result of the separate byte mapping, and let
`R32` and `R64` denote nearest/even rounding:

```
difference = R32(mapped - history)
product    = R64(binary64(0.9) * difference)
new_history = R32(R64(history + product))
steering_bits = new_history_bits XOR 0x80000000
```

The binary64 scale is exactly `0x3feccccccccccccd`. The difference, product,
sum and final conversion have distinct rounding boundaries. Fused multiply/add,
an algebraically rearranged weighted sum or direct EE floating arithmetic is
not an equivalent replacement. The final operation flips the sign bit even
when the stored history is zero. Floating exception status and other rounding
modes are outside this result contract.

The private integer implementation reuses the independently written fixed-grid
arithmetic from [alternate orientation](alternate_orientation.md), locally so
that the separately validated component stays unchanged. Seven 32-bit limbs
represent exact signed values in units of `2^-202`, including a minimum
binary32 subnormal multiplied by the exact 53-bit scale. All intermediates
stay below four in magnitude. Explicit guard/sticky/tie handling preserves
the two binary64 boundaries and final binary32 rounding.

The supported history domain is closed under successful updates. The mapped
magnitude is at most binary32(0.84), below 0.84. Before rounding errors, the
convex update has magnitude at most 0.856000000000001. The binary32 difference
error is at most `2^-24`; product/sum binary64 errors and the final binary32
error keep the total result below 0.856001, well inside one. The implementation
does not clamp to enforce this property. Stateful tests check it after every
step while comparing exact original-program results.

## Original boundary and validation

The owner's verified Revision B program has SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The filter spans ROM `[0x39a90,0x39aec)`, 92 bytes, SHA-256
`0db1b90ee70640a562c233567f7a5aa20ff30856af6edc4941500094b7d1a645`.
It calls the separately verified byte mapping at `0x395d8`.

Only the offline oracle knows the input byte at `0x202050`, history word at
`0x2142c0` and return register `g0`. It executes the actual original instructions
through `0x39ae8`, just before the return, and confines fetches to the filter
and byte-mapping regions. Each evaluation must read one byte and one history
word, write exactly one history word and return that word with its sign flipped.

The private original-program comparison passed **68,128 evaluations** with
**49,995 unique history/sample pairs**, including **18,432 stateful updates**.
It executed 3,223,169 original instructions, 46–49 per evaluation, with
fingerprint `95145e1d72391f91`. Coverage includes every sample byte at 20 edge
histories and 96 deterministic random histories, another 20,000 random pairs,
and ramp, alternating-extreme and centered-decay sequences. Each sequence
feeds the actual previous original result into the next update. These are
original synthetic test sequences, not captured race-input replays.

An independent host arithmetic definition agrees on every result. Portable
tests additionally verify domain closure and two output-alias configurations,
and reject 2,560 invalid-history/sample combinations without changing output.
AddressSanitizer and UndefinedBehaviorSanitizer pass the same checks. This is
not an exhaustive sweep of every binary32 history encoding.

The standalone EE fixture contains 7,168 evaluations, including 2,048 stateful
updates, and checks fingerprint `335d3a428df4150d` before reporting success.
It separately checks the same 2,560 invalid combinations. The host oracle and
capture adapters are excluded from native game builds.

That fixture passed in Play 0.72 HLE, with empty guest stderr. The executed
ELF SHA-256 is `a8fc42139c59289663608713e70fc982a9120b4725dcf6531e80aa44045cce10`;
its 20.243-second observation window is not measured completion time or console
performance. The first cross-build exposed a fixture-only type assumption:
EE `uint32_t` is `unsigned long`, while a literal-inferred sequence variable was
`unsigned int`. Declaring the sequence state explicitly as `uint32_t` fixed
that build; native arithmetic and test fingerprints did not change. The final
host/original evidence was regenerated against the corrected test header.

Validated driving capture `driving-capture-007` supplies 32 complete filter
pairs, seven distinct history/sample inputs, during frames 2,200–2,800. Samples
96, 128 and 160 occur; observed histories stay within approximately ±0.336 and
include both positive and negative subnormal decay values. Both the stored
history and returned sign-flipped value match native and original execution
bit-for-bit. All returns identify `0x5d3f90`, within the observed main-object
callback. These sampled inputs do not cover every transient, mode or course.

The combined comparison has 68,160 evaluations, 50,001 distinct pairs and
3,224,645 original instructions, fingerprint `359512f016c2d54d`. Final reports
are `build/ps2-native-steering-filter-oracle-final-001/result.json` and
`build/ps2-native-steering-filter-live-final-001/result.json`. The captured
filter stream has SHA-256
`fc2195d0876f20d747b0cdbbd4df06b39fad4744a79fab215fee005d20afc2ab`.
All 2,850 observer-enabled and disabled frame checkpoints agree.

## Reproduce and capture scope

```sh
python3 tests/test_ps2_cabinet_steering_filter.py
python3 platform/ps2/tools/check_cabinet_steering_filter.py \
  --program /path/to/owned/program.bin \
  --output build/cabinet-filter-oracle
```

The checker optionally accepts `--capture build/.../capture`. It requires
completed, observer-parity-validated `steering-filter.jsonl` pairs at live-copy
PCs `0x5d8a90` and `0x5d8ae8`, verifies the actual caller frame identity,
unchanged sample byte and nearest rounding, and compares both captured outputs
with native and original-program execution. A snapshot of the history global
at an unrelated point, or a capture of only the byte-mapping helper, is not a
paired filter validation. Inputs outside the native history domain fail the
comparison rather than being dropped.

The caller proof must include sixteen saved words, a matching return-PC word,
the entry previous-frame pointer and a valid register-cache or ordinary-RAM
source; post-event proof must match the entry proof. Completion counts and the
number of paired samples must match the completed capture marker. The initial
consumer checked only the claimed PC/frame fields; independent review found
that insufficient, and the final consumer rejects malformed proof and count
records before comparison. Earlier reports remain preserved separately.

All original bytes, generated code and captured records remain private under
ignored `build/`. Original-instruction host comparisons do not independently
certify PCB arithmetic, a complete driving system or physical PS2 behavior.
