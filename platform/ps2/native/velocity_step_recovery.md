# Velocity-step callers and remaining dependencies

This is an address/hash/contract investigation, not another native simulation.
The verified program identity and recovered blend are documented in
[direction blending](direction_blend.md). Game-derived disassembly and records
stay in ignored `build/`.

## Caller selection

Fresh decoding at ROM `0x2bfa4` identifies an object word at `+0xe8` selecting
three distinct paths. The object is in local register `r5` at that boundary.

| Selector | Established path |
| --- | --- |
| 0 | Calls integrator `0x2cce0` at `0x2bfe8` |
| 1 | Calls alternate integrator `0x2e650` at `0x2c018` |
| 2 | Enters a different inline body at `0x2c020` |
| Other | Branches to `0x2c440` |

The two explicit integrators both call the shared velocity-step helper
`0x30e00`, at `0x2cdd4` and `0x2e72c` respectively. The alternate entry still
receives the object in `g0`; it preserves it in `r12` before this shared call.
These facts do not establish player/AI labels, vehicle identities or the
meaning of the selector. A capture armed only on `0x2cce0` can miss the shared
velocity operation when another path is active.

The completed `driving-capture-004` run observes exactly one alternate entry
per object per frame for two objects (`0x500100` and `0x500200`) across frames
2,200–2,800 inclusive: 1,202 entries and matching returns. Every such entry
records selector 1; no primary entry occurs in that window. This establishes
per-frame cadence for those objects in this replay, not a general update rate
or a player/AI classification. The cadence stream has SHA-256
`176f7174da31e39269554e76b73acf922fc8b4791c2d0a7650c6bc26ccd172c0`.
The capture observer leaves all 2,850 host frame checkpoints unchanged.
The shared direction blend agrees for all 32 detailed calls retained from
this run; see its separate arithmetic validation.

The alternate function spans `[0x2e650,0x2eee8)`: 2,200 bytes, ending with the
return at `0x2eee4`, SHA-256
`2bfb53d04064e187ef6c9a6a60520197fd23a5b88d177c9957b7a5271ac05d86`.
An initial analysis mistakenly continued past that first return into the next
function at `0x2eef0`, attributing its later orientation block to this one.
The corrected boundaries above replace that attribution. The earlier private
research record is preserved alongside a clearly corrected record.

## Geometry queries remain dependencies

The shared velocity helper performs state/setup commands followed by five
geometry-result groups: a three-component point result, a three-component
result from the planar offset, a scalar result from two supplied triples,
a three-component result from existing planar motion, and a three-component
result from the blended pair. There are 13 read sites and 36 write sites.

The setup operands include position and orientation fields; later arguments
include planar motion and the recovered blend. Matrix transform, distance and
normalization are useful hypotheses for the operation names, but the exact
firmware arithmetic and result contracts must be recovered before replacement.
Ordinary vector-library functions are not yet justified substitutes.

Other inputs include object fields `+0x64/+0x68` for an early-out condition,
`+0xac/+0xb0` for a scalar target/history relation, and two tuning values at
offline addresses `0x20cae4/0x20cae8`. The helper changes flag bit 4 in the byte
at object `+0x10`, updates the planar intermediate fields `+0x9c/+0xa0/+0xa4`,
updates `+0xb0`, and ultimately writes motion-like fields `+0x2c/+0x34`.
Units and the remaining query contracts are not inferred from those address
relationships. The observed caller cadence above does not prove how those
queries should behave or which timestep units the arithmetic uses.

## Alternate orientation is a different contract

The alternate orientation region is `[0x2ecb4,0x2ee00)`, 332 bytes, SHA-256
`849cc9ef5df8b5544b81463ad002ae6ed4c6529e69d743aff8c1a286e16be5f8`.
It does **not** match `finalize_orientation`, which folds three channels and
bounds the first/third to signed pi/8. Extending that component's live-PC
whitelist would falsely assert equivalence.

The alternate first folds a candidate triple in stack-local storage at
`fp+0x90/+0x94/+0x98`. This fold-only span `[0x2ecb4,0x2ece0)` has SHA-256
`69ea98964dd216928e5657b7f2cb267c676fe73da0b6073098326fb01310915b`.
Its corresponding live-copy boundaries are `0x5cdcb4` and `0x5cdce0`.
The current orientation comes from object `r12+0x38/+0x3c/+0x40`.

The next helper at `[0x27bd8,0x27c60)` computes a binary32-rounded difference
`current-candidate`, then applies one pi-based fold if its magnitude exceeds
the binary64 pi constant. Its 136-byte span has SHA-256
`2123ca58d261efacb63000cb8cb454a67b6394be4e84fb36f06b243bd73460c7`.
It is related to the recovered one-angle fold, but has a preceding subtraction
and potentially different input bounds. Those bounds require their own proof.

Let `candidate` denote the already folded candidate and `delta` the output of
that difference helper. With each observed binary64 store rounded separately:

* First and third become
  `R32(R64(candidate + R64(binary64(0.9) * delta)))`.
* The middle becomes `candidate` when `abs(delta)` is strictly less than the
  binary64 threshold `0.01746725515395925`.
* Otherwise the middle becomes the binary32-rounded binary64 subtraction of
  that signed threshold from its current value, with sign chosen from `delta`.

The threshold is **not** exactly pi/180. An initial approximate analogy must
not be used as an exact constant, unit interpretation or timestep claim.
The middle equality boundary follows the step path rather than the snap path.
There is no matching pi/8 bound here.

The independent [alternate orientation component](alternate_orientation.md)
now implements this bounded arithmetic. It preserves every binary32/binary64
rounding boundary with integer arithmetic and rejects unsupported inputs
without mutation. Capture 007 records the candidate stack triple, current object
triple, arithmetic control and pre/folded/post boundaries. All 34 sampled calls
agree with native and isolated original execution; boundary and wrap cases have
separate synthetic coverage. This completes that orientation region, not the
surrounding integrator or the separate [main vehicle callback](main_vehicle_recovery.md).
