# Bounded planar direction blend

`blend_planar_direction(base, target, output)` is an independently implemented
arithmetic component recovered from the velocity-step lead. It follows the
governing design's native PS2 exception: production code operates on typed
pairs, with no guest registers, addresses, processor dispatcher, geometry
processor, memory bus or scheduler. Its arithmetic is original integer code.

The names `base`, `target`, `first` and `third` describe operand relationships.
Physical axes, units and the normalization of live inputs are not established
by this isolated function. It does not implement the complete velocity helper,
car integration, collision, or a driving model.

## Contract and rounding

Every input component must have a binary32 encoding in `[-1,1]`, including
signed zeros and subnormal encodings. Any other input returns `false` and
preserves every output bit. This is an explicit native restriction; the
original instruction region does not reject out-of-range inputs. Output may
alias either input. The result is neither normalized nor clamped to `[-1,1]`.

For `R32` and `R64` denoting nearest/even rounding, the mathematical contract is:

```
dot = R32(R32(base.first * target.first)
        + R32(base.third * target.third))
weight = R32(min(1, R64(1 - dot)))
output.component = R32(base.component
                    + R32(R32(target.component - base.component) * weight))
```

Only the upper bound on `weight` exists. A positive dot product greater than
one produces a negative weight. Replacing it with a conventional `[0,1]` clamp,
fused multiply/add, or a rearranged weighted sum changes the operation. Even
when the weight is one, separately rounded subtraction and addition can differ
from directly returning `target`. Signed zero and subnormal results are part
of the tested output contract; floating exception flags and other rounding
modes are not.

The implementation decodes bounded binary32 values as signed dyadic numbers.
Products contain at most 48 significant bits. Addition aligns exact integer
numerators when their exponent gap is at most 32; larger gaps cannot reach half
the narrower neighboring binary32 spacing of the larger operand. The aligned
numerators occupy at most 56 bits. Each operation rounds explicitly, including
ties/even, gradual underflow, carry and signed zero. No floating arithmetic or
host floating environment determines the production result.

The binary64 subtraction in the weight has a separate proof. The rounded dot
product lies in `[-2,2]`. Nonpositive values select the upper cap one. For a
positive dot at least `2^-25`, the exact difference `1-dot` fits within 49
significant bits, so binary64 rounding is exact. Smaller positive dots leave
the final binary32 weight at one, including after binary64 rounding. The
implementation therefore obtains the same weight using its exact binary32
subtraction, without relying on EE floating behavior.

## Original-program boundary

The owner's verified Revision B program has 2,097,152 bytes and SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
Fresh decoding of the address-only velocity-step lead at ROM `0x30e00`
confirms 984 bytes, 177 instruction sites, and SHA-256
`494f0202ffbccc7c5f1faca495e7144f0a36e5fe3c75637767b95815473da756`.
It has 36 geometry-FIFO write sites and 13 read sites, with five result groups.
Those are static sites, not a measured update frequency.

The isolated blend spans ROM `[0x3110c,0x31158)`, SHA-256
`71d232c3d3f16c5230a3b3da158d3a1ad571fbfcad3de4133f0ff06a19f53670`.
The offline oracle uses live-copy addresses `[0x5d010c,0x5d0158)`.
Only this adapter contains the recovered register/memory mapping:

| Boundary value | Offline source |
| --- | --- |
| Base first/third | Entry `r4` and `g7` |
| Target first | Object word at `+0x9c` |
| Target third | Entry `g2`, previously loaded from object `+0xa4` |
| Result first/third | Final four-word store at entry `g1`, offsets 0 and 8 |
| Preserved middle/padding | Entry `r5` and `r7`, stored unchanged at offsets 4 and 12 |

The region performs exactly one data read and four word writes, taking 18 or 19
instructions depending on its weight-cap branch. It contains no geometry-FIFO
access. The surrounding helper obtains both input pairs from earlier geometry
queries and submits the blended pair to another query before scaling two
motion-like object fields. Matching this leaf does not justify replacing any
of those queries with a modern vector library.

The offline memory adapter requires ordinary RAM burst behavior for the final
four-word store. An initial harness omitted that property and overwrote one
address repeatedly; correcting the adapter, rather than changing the native
arithmetic, resolved the harness failure.

## Validation and remaining evidence

```sh
python3 tests/test_ps2_direction_blend.py
python3 platform/ps2/tools/check_direction_blend.py \
  --program /path/to/verified/program.bin \
  --output build/native-direction-oracle-001
```

The original-instruction differential passed **305,824 cases, comprising
303,888 unique input quadruples and 5,607,871 original instructions**. Its
result fingerprint is `07c477eae4944c20`. It checks both result components,
preservation of the other two stored words, the exact data-access count and
instruction/data confinement. A separate host mathematical oracle uses
binary64 arithmetic with explicit binary32 boundaries; it agrees on the same
original boundary/random cases. Host AddressSanitizer and
UndefinedBehaviorSanitizer also pass.

The compiled EE probe passed **6,032 original cases**, fingerprint
`dd9701872b4776e2`, plus 36 invalid-input cases with two alias configurations
each in Play 0.72. Its fingerprint matches the host probe. This is emulator
execution of native EE arithmetic, not a physical-console test. It does not
repeat the full host differential or establish hardware speed.

The private checker also accepts `--capture build/.../capture`. It requires a
completed capture whose observer checkpoints exactly match a run without the
observer. It pairs entry, pre-store and post-store events, verifies object and
output identity, preserved words and nearest rounding control, and compares
captured outputs with both native arithmetic and isolated original execution.
Incomplete or inconsistent event groups are rejected. Captured bytes and
comparison records remain under ignored `build/`.

The validated driving capture `driving-capture-004` produced **32 detailed
blend calls, with 29 distinct input quadruples**, from two objects during
frames 2,220–2,760. Every captured result agrees bit-for-bit with both the
native component and isolated original-program execution. All inputs fit the
explicit native domain, all observed arithmetic-control values select nearest
rounding, and the other two stored words remain unchanged. The combined
synthetic/live comparison has 305,856 cases, 303,917 distinct quadruples and
5,608,447 original instructions, fingerprint `75876bd959ec4992`.

These are the observed component ranges, rounded here for readability:

| Component | Minimum | Maximum |
| --- | ---: | ---: |
| Base first | -0.042551931 | 0.769088387 |
| Base third | 0 | 0.999936759 |
| Target first | -0.044758990 | 0.845188022 |
| Target third | 0.534469068 | 0.999996424 |

The capture-enabled and disabled runs match at every checkpoint for all 2,850
frames. The captured event stream has SHA-256
`fe9f2e47a5b5391f442994e6bcca29e4511abf6e2fff041496d4b13a08731747`;
the private comparison report binds that stream, its completed-capture marker,
the observer-validation report and the native/oracle sources. These are sparse
samples from the translated host reference, not independent PCB validation or
a proof of the input domain for every vehicle, mode and course.

The first attempts watched the other integrator and produced no matching
calls. The successful capture reaches the alternate integrator described in
[the caller investigation](velocity_step_recovery.md). Its live orientation
tail is a different operation from the previously recovered orientation leaf.
Dependent geometry-query semantics and full-course behavior remain unproven.
The native component remains disconnected from gameplay.
