# Main vehicle candidate preparation

`prepare_main_candidate` independently implements a bounded prefix of the main
vehicle callback. It clamps a pending middle component, evaluates the recovered
scalar gate, conditionally consumes the pending position delta, and forms the
candidate position and orientation used by the next contact pass. The typed
state contains five input triples, a marker and two output triples. It has no
processor, mapped-memory, geometry-command or scheduler dependency.

This follows the native exception in
[Platform layer & build](../../../docs/daytona-usa-recomp-design.md#platform-layer--build).
The function is independently validated and remains unconnected to the native
main callback. It does not establish a complete driving loop, physical units,
axis names, a time step or measured gameplay performance.

## Recovered boundary

The original interval is `[0x34ff0,0x35218)`, 552 bytes, within the main callback
at `0x34f40`. Its SHA-256 is
`b49f8290c4ca32b010985755a2243c4bd4fc70da9e666cfb9228aa2bd9cbaba8`.
The complete owner-provided program identity is
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The observed relocated boundaries are `0x5d3ff0` and `0x5d4218`.

The five triples occupy original object words 5–19. The region may change only
position words 5–7, pending-delta words 8–10 and motion words 11 and 13. Motion's
middle component, orientation, angular motion and unrelated object storage stay
unchanged. The original marker is at `0x213974`; the two candidate triples are
at caller-frame offsets `0x130` and `0x140`. These addresses exist only in the
offline comparison and capture binding, never in the native component.

The original region invokes geometry command `0x58`, represented by the existing
[recovered scalar metric](recovered_scalar_metric.md). Its firmware is verified
by SHA-256
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
That operation uses initialized numeric constants and requires no table asset.
It is not replaced with a generic Euclidean length.

## Explicit value behavior

Let `R32` mean nearest-even binary32 rounding. Comparisons described as binary64
use the original separately represented binary64 threshold.

The pending delta's middle component is clamped to the binary32 representations
of `±0.1` when its magnitude exceeds binary64 `0.1`. The component's sign,
including negative zero below the threshold, is retained. Binary64 `0.1` lies
between adjacent binary32 values, so the native integer comparison preserves the
original threshold without changing floating-point mode.

The metric receives `{0,0,delta.middle}` and
`{0,delta.first,delta.third}` after that clamp. If its result is strictly greater
than binary64 `0.002`, the function:

1. Sets the response marker to 15.
2. Adds `R32(delta × binary32(0.6))` to each of the first and third motion
   components, rounding each addition separately.
3. Scales the first and third pending components by the binary64 constant whose
   value is exactly binary32 encoding `0x3ecccccc`, or
   `0.39999997615814208984375`. It is not binary32 `0.4`. The exact product of
   these two binary32 operands fits binary64 before its binary32 store.
4. Adds each resulting delta component to position with separate rounding and
   clears all three pending components to positive zero.

Otherwise the marker, position and motion remain unchanged; the pending middle
clamp still applies. In either branch, the output triples are the separately
rounded componentwise sums `position + motion` and
`orientation + angular_motion`.

All fifteen input components must be finite with magnitude at most `2^20`.
The metric's ordered squared sum must be zero or normal. Signed input zeros and
subnormals are supported within that domain. Existing candidate triples are
unused payloads and may contain arbitrary bit patterns. Unsupported inputs leave
the entire state unchanged. The implementation uses bounded integer dyadics,
retains each rounding boundary and allocates no dynamic storage.

## Evidence and limits

The synthetic corpus has 17,044 cases, with 16,315 supported updates and 729
atomic metric-domain rejections. It includes random bounded bit patterns,
neighboring clamp/gate values, both signs of zero and subnormal values in each
input component. Independent host arithmetic agrees with the native function.
Accepted cases additionally match the original CPU interval and initialized
geometry firmware. The corpus fingerprint is `966c10f581152bec`.

Capture 012 supplies 26 complete before/after pairs within main-callback groups
whose enabled/disabled observer runs match all 2,850 checkpoints. All 22 output
state words per prefix match the native function and original program. The
parser also checks unchanged object storage, candidate-buffer addresses,
parent proofs, order, completion counts and exact capture/checkpoint hashes.

**Live coverage is limited to the zero-metric branch.** All 26 sampled prefixes
enter with three positive-zero pending components and execute 60 original CPU
instructions. Their markers remain unchanged; 22 already contain 15 at entry.
The log field called `consumed` counts accepted outputs whose marker equals 15;
it is not a reliable branch counter for captured states. The consuming and
clamping branches are supported by the synthetic original-program comparison,
not by these 26 live samples.

Combined evidence has 17,070 cases, 16,341 accepted updates and 26 captured pairs;
it executes 1,294,632 original CPU instructions and 690,733 geometry instructions,
with fingerprint `426c711e238b610d`. The private source-bound report is
`build/ps2-native-main-candidate-live-001/result.json`, SHA-256
`1d83bc95ea527765f43b0713516b7a710ed09965526d168054b6107ee891f203`.
Original instructions, generated comparison code and captured records remain
under ignored `build/`.

The host fixture passes ASan/UBSan, including 75 explicit out-of-domain atomic
rejection checks. The standalone EE fixture passed in Play 0.72 with its built-in
HLE: 1,684 probe cases, 1,641 accepted updates, fingerprint
`d782474a4f893dc8`, and all 75 rejection checks. Guest stderr was empty.
Its ELF SHA-256 is
`a3f286ac7eb8ba94ded4edad9185c9b57ca65524846a4f5c97ffa2cbf85c3a5c`;
the source-bound runtime report is
`build/ps2-native-main-candidate-ee-001/runtime-validation.json`.
The 20.105-second observation window is not execution timing. No physical
console or connected gameplay validation is claimed.

```sh
python3 tests/test_ps2_main_candidate.py
python3 platform/ps2/tools/check_main_candidate.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tgp-program build/ps2/native/tgp/program.bin \
  --capture build/ps2/driving-capture-012/capture \
  --output build/ps2-native-main-candidate-live-new
```
