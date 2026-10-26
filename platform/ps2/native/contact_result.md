# Native contact-result adapter

`contact_result.h/.cpp` implements the bounded result interpretation shared by
the alternate integrator and the main four-point contact pass. It consumes an
ordinary `ContactPolygon`, a point and an immutable coefficient span. It has no
guest processor, emulated bus, command FIFO, mapped address or allocation.

This closes the result adapter, including its table-dependent normal operation.
It does **not** connect the main contact pass, collision response or driving to
the native application. The standalone normal coefficients would still need an
owner-supplied asset import/loading path. No original table is embedded here.

## Verified original boundaries

| Operation | Original interval, end excluded | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| CPU adapter | program `0x2acc0..0x2aec4` | 516 | `4a4c446316ee2c37644b3142acd090c3c002d8933b8c8bc886e5038ca88d852e` |
| Normal calculation | geometry words `0x30e..0x327` | 100 | `4ac9c46e5516e4563babb665dbadcc80eadd1384781996c394dedf46b9daef65` |

The adapter takes a three-word point, an eight-word output and a result-slot
index. Command `0x53` supplies 17 words: four XYZ vertices, four coefficients
and attributes. The adapter preserves the first three output words and writes
direction XYZ, gap and status into the remaining five. The native API exposes
only these five fields; it does not own the preserved prefix or command slots.

The original adapter uses two FIFO writes and 17 reads on the miss/edge paths.
The plane path additionally invokes command `0x2f`, giving six writes and 20
reads overall. Its normal routine executes 25 instructions with three inputs,
three outputs and no external-memory accesses. The offline oracle checks these
counts, exact instruction boundaries, return branch and guarded output memory.

## Typed behavior and arithmetic

Let `R32` mean round to nearest, ties to even, to binary32 after the indicated
operation. The native implementation uses bounded integer arithmetic to preserve
those rounding points on EE as well as the host; it does not substitute a host
square root or generic vector normalization.

* Zero attributes set status to 15 and preserve all four previous direction/gap
  words, including their arbitrary encodings. Coordinates and table are unused.
* Nonzero attributes set status to `(attributes >> 16) & 0x8f`.
* Status bit 7 selects the edge branch. Direction is copied verbatim from the
  first three coefficients, deliberately including nonfinite encodings. With
  the first two vertices `a,b`, the gap is
  `R32(abs(R32(R32(R32(ax-x)*R32(bz-z)) - R32(R32(az-z)*R32(bx-x)))) / d)`.
  Absolute value here clears the determinant's sign bit. Point Y, other
  vertices and the normal table are unused.
* Otherwise the plane gap is
  `R32(R32(R32(R32(R32(-nx*x)-R32(nz*z))-d)/ny)-y)`.
  Negating `nx` flips its sign bit. Direction uses the original interpolation
  described below.

Inputs actually used in arithmetic must be finite with magnitude at most
`2^20`. A divisor must have magnitude from `2^-20` through `2^20`; **negative
divisors are accepted**. The squared normal length must lie from `2^-40` through
`2^42`. The result is staged locally and copied only after all checks succeed;
rejection preserves every output field. Unused payloads are not rejected just
because they contain unusual bit patterns.

## Immutable normal coefficients

The native span has exactly 8,192 `ContactNormalPair { slope, intercept }`
records, totaling 65,536 bytes. Only the selected pair is read and validated:
positive slope in `[1/8,1]`, positive intercept in `[1,4]`. The accepted domain
permits a zero or negative interpolation factor; the implementation preserves
its sign and separately rounded products.

For squared length
`s = R32(R32(R32(nx*nx)+R32(ny*ny))+R32(nz*nz))`, let `u` be its binary32 bits:

```text
row = ((u >> 11) & 8191) XOR 4096
power = 63 - ((u >> 24) & 127)
mantissa = binary32 bits (u & 0x7fffff) | 0x3f800000
factor = R32(scale(intercept,power) - R32(mantissa*scale(slope,power)))
direction[i] = R32(coefficient[i]*factor)
```

Scaling changes the binary32 exponent exactly within the validated range;
it cannot wrap or overflow. The original table-port word index is
`0xc000 | (0x2000 XOR (((u >> 10) & 0x3ffe) | lane))` for lanes 0 and 1.
Offline extraction maps pair `i` from table words `0xc000+2*i` and
`0xc001+2*i`, clearing the first word's sign bit to obtain the positive slope.
That is an asset decoding contract, not a runtime recreation of a device port.

The verified private full-table identity is
`3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04`.
All original images, decoded coefficients, generated reference code and live
records remain in ignored `build/`; they must not be included in source or
release packages.

## Validation and limits

The portable fixture passes 40,392 mathematical cases, fingerprint
`385ddd84352f0d5e`, plus 34 atomic-rejection checks. Its smaller common EE/host
probe passes 2,440 cases with fingerprint `82d33d3b52e3a7d4`. Three independent
interpolation guards cover exact cancellation, signed zero and a negative
factor using original synthetic coefficient pairs. ASan/UBSan passed the same
host checks. Those three synthetic guards are not assertions about coefficients
occurring in the original asset.

The verified original CPU and normal instructions, with the private original
table, pass 40,392 synthetic cases: 13,529 plane, 13,529 edge and 13,334 miss
paths. They execute 2,494,675 CPU instructions and 338,225 normal instructions,
fingerprint `511421beac689f89`. This check also compares an independent
host-arithmetic formula and protects the point, output prefix and surrounding
memory.

The source-bound live comparison is recorded privately at
`build/ps2-native-contact-result-live-004/result.json`. Capture 011 has exact
observer-enabled/disabled parity at all 2,850 frame checkpoints. Its 34
alternate and 104 main adapters produce 138 observed results: 125 plane and
13 edge paths. All five result words agree with both native and original
instruction execution. Combined with synthetic cases, the comparison contains
40,530 cases, 2,505,270 CPU instructions and 341,350 normal instructions,
fingerprint `c898451d51b72992`. No live miss path was observed.

The live reader binds file hashes to the passing capture report, validates
query/reply/return chronology and saved caller-frame evidence, preserves the
raw old/new output words and rejects unsupported rounding modes. Main samples
must contain all four indexed adapters inside a complete parent pass. Their
point addresses, output slots and restored caller frames must match that
parent. The next query is staged before the previous response completes; the
validator preserves this observed ordering rather than assuming a serial loop.

The actual EE fixture passed under Play 0.72 with its built-in HLE:
2,440 cases, 34 rejection checks and three interpolation guards; guest stderr
was empty. The executable SHA-256 is
`0ba20976d1f98c579f3acce88cd49163efd8297e05536bf9f18ea1a24c2f2028`,
with 1,238,196 file bytes and 186,720 load-segment bytes. Its source/log binding
is `build/ps2-native-contact-result-ee-001/runtime-validation.json`.
The 20.122-second observation window is not a measured execution time. Window
capture failed, so this is numerical guest-output evidence, not a screenshot
claim or physical-console test.

Reproduce the private comparison with owner-provided verified images:

```sh
python3 platform/ps2/tools/check_contact_result.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tgp-program build/ps2/native/tgp/program.bin \
  --tgp-tables build/ps2/native/tgp/copro_tgp_tables.bin \
  --capture build/ps2/driving-capture-011/capture \
  --output build/ps2-native-contact-result-new
```

Omit `--capture` for synthetic original-instruction comparison only. Every
output directory must be new and private. The public host test needs no game
images: `python3 tests/test_ps2_contact_result.py`.

The remaining main contact work includes preparation of the transforms,
state-dependent response/feedback and integration into authentic native driving.
This adapter's passing isolated and sampled comparisons do not establish those
systems, a complete collision certification or gameplay on physical PS2.
