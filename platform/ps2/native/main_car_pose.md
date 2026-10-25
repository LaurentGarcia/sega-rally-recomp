# Main-car pose publication

`make_main_car_pose(state, external)` independently implements the main
vehicle's raw-value publication helper. It returns an existing `RenderPose`
value containing 22 words. Every bit is copied unchanged, including the entire
flags word. It neither updates its inputs nor reads an earlier destination.

This differs from [the earlier car-pose helper](car_pose.md): there is no
persistent flag merge and no forced publication bit. Four explicit external
words replace that helper's final four object attributes. The governing
design's **Platform layer & build** native-rewrite exception permits this
typed implementation and its separate offline original-program comparison.
Production code has no guest addresses, registers, bus, processor or scheduler.

## Value contract

`MainCarPoseState` has three raw three-word groups, seven raw attributes, one
flags word and one index word. The separate `MainCarExternalState` supplies
four more words. The returned `RenderPose` holds the three groups unchanged,
then the seven local attributes followed by the four external words. Its
`flags` equals the input flags; its existing opaque `progress` member holds the
input index. Reusing that member does not establish route or lap semantics.

All `uint32_t` values are valid. Signed zeros, subnormals, infinities, NaN
payloads and non-floating encodings are copied without floating arithmetic.
Group names do not establish units, axes, orientation order, interpolation,
camera behavior or physics. The meaning of the external group is not recovered
by this copy operation.

Arguments must denote live typed objects. Return-by-value provides a complete,
independent snapshot before the caller assigns a destination. No pointers into
either input survive the call; modifying either input afterwards cannot change
the result. Repeated calls with unchanged inputs produce identical values.
Guest null pointers, partially overlapping mapped buffers, device side effects
and the original bus-write order are outside the native interface.

## Original boundary

The owner's verified Revision B program is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The helper occupies ROM `[0x34078,0x34130)`, 184 bytes, SHA-256
`16a9bdcd4eddd6a783f2fc0e08d9162ea7a0fe124fe602f348456f46f89ac7a0`.
Fresh decoding and comparison to the image confirms 42 instructions, ending
in a branch to the supplied return address. The wrapper setup before this
region and its following return instruction are not part of the leaf contract.
The [main-vehicle dependency map](main_vehicle_recovery.md) identifies its
call from ROM `0x371dc`; this is not a newly connected native driving loop.

Only the host oracle maps typed values to original storage:

| Native value | Original source | Destination offsets |
| --- | --- | --- |
| `position` | Object `0x14,0x18,0x1c` | `0x00..0x08` |
| `motion` | Object `0x2c,0x30,0x34` | `0x0c..0x14` |
| `orientation` | Object `0x38,0x3c,0x40` | `0x18..0x20` |
| Seven `attributes` | Object `0x74,0x80,0x84,0x64,0x68,0x6c,0x70` | `0x24..0x3c` |
| Four `external` words | Globals `0x2138a0..0x2138ac` | `0x40..0x4c` |
| `flags` | Object `0x10` | `0x50` |
| `index` | Object `0x88` | `0x54` |

The original destination pointer resides at object `0x8c`. The native factory
does not represent it. Its result matches the final values for distinct
word-aligned source, destination and external storage. The oracle also checks
the original 23 ordered reads, 22 ordered word stores, unchanged source and
external bytes, unchanged arithmetic condition register and cleared temporary
return register. Those checks constrain the reference adapter; they do not
promise mapped-memory effects in the native API.

## Validation

The portable host test and private original-program comparison pass **77,208
distinct 22-word inputs** with fingerprint `b74eb51b13296a1c`:

- All 65,536 source flag low-halfword patterns, with varied upper bits and
  previous destination values.
- 1,408 set/clear walking-bit cases: every bit in each of 22 words.
- 264 selected raw-encoding boundaries across every input word.
- 10,000 deterministic random complete inputs.

This exhausts only the source flag low halfword, not every possible full
input or source/destination pair. Every case checks an independent snapshot,
unchanged typed inputs and repeated-publication stability. The original oracle
varies source and destination word-alignment residues, verifies all surrounding
memory and prohibits unexpected instruction or data access. It executes
3,242,736 original instructions, 1,775,784 reads and 1,698,576 writes. These
are isolated host-reference counts, not processor timings.

AddressSanitizer and UndefinedBehaviorSanitizer pass the same portable cases.
The bounded standalone EE fixture contains 2,184 cases and 48,048 result words,
fingerprint `d2fae2297e475353`; it checks those values before reporting success.
That fixture passed in Play 0.72 HLE with empty guest stderr. Its executed ELF
has SHA-256 `1511a352ad80d6d98d86429565f5a46c8513867705de497c44ec6304f4f0f705`,
1,224,904 file bytes and 179,040 resident bytes. The 15.115-second observation
window is not measured fixture completion time or console performance. The
source-bound record is
`build/ps2-native-main-car-pose-ee-001/runtime-validation.json`.
No live paired helper capture or physical-console validation is claimed here.

```sh
python3 tests/test_ps2_main_car_pose.py
python3 platform/ps2/tools/check_main_car_pose.py \
  --program /path/to/owned/program.bin \
  --output build/main-car-pose-oracle
```

The checker rejects other program identities, confines all output beneath
ignored `build/`, and rejects source or input changes during comparison.
Its adapter and reference interpreter are excluded from native game builds.
The private initial report is
`build/ps2-native-main-car-pose-oracle-001/result.json`. No original bytes,
generated implementation or captured game records belong in tracked tests.
