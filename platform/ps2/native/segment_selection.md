# Course segment selection

`select_course_segment` recovers the complete bounded search used by the main
vehicle callback before candidate preparation. It accepts ordinary immutable
vertices/corner indices and a signed progression value. It contains no guest
processor, mapped memory, device transport, original instructions or extracted
assets. This follows the governing design's **Platform layer & build** native
rewrite exception. The original CPU and geometry firmware remain offline
comparison oracles.

The main callback and driving loop are still unconnected. This component does
not establish a playable native car, physical-console fidelity or a frame rate.

## Verified boundaries and assets

The program and uploaded firmware identities are those recorded by
[contact-query recovery](collision_query_recovery.md). These are complete code
spans, with end addresses exclusive:

| Operation | Original range | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Segment search | CPU ROM `0x2b0b0..0x2b174` | 196 | `8dd76c9554c0ce6cb4610087b7da6aaff5c1769c836d034e7db273d3553e2f79` |
| Quadrilateral predicate callee | CPU ROM `0x2aed0..0x2b0b0` | 480 | `b8a4804004110f1395e93943864db50eb137b3466a164a2fb11ed5898f7235f6` |
| Edge operation, command `0x5c` | Firmware word PC `0x577..0x58a` | 76 | `0482633af4096122bb8db69e0c5f6be4cf5d8b36da74a5e7da6084e14ad2da04` |

The main callback calls the search at ROM `0x34fc4`, passing the three position
words at object `+0x14` and two signed16 indices at object `+0x88/+0x8a`.
The helper returns through ROM `0x2b154` or `0x2b170`. Capture observes its result
at the caller's `0x34fc8`, **before** the caller clamps a negative first index to
zero. That subsequent clamp and global publication are outside this component.

The descriptor selector is the scalar at `0x214354`. In the previously verified
eight-record descriptor window at live `0x5dcac0`, the third word points to a
main-data header. The header contains a count, a vertex base and a record base.
Each vertex is three binary32 words. Each 12-byte record contains four unsigned
16-bit corner indices at byte offsets 4, 6, 8 and 10; the first four bytes are
unused by this helper. The predicate transports only X/Z to the geometry unit.
Y is read into staging storage but is not used in the edge arithmetic.

All eight private descriptors have been checked for bounded extents and used
coordinates: selectors 0..4 have 300 records referencing indices at most 599;
selectors 5..7 have ten records referencing indices at most 21. Used X/Z
magnitudes are below 763. These asset facts do not establish every possible
selector writer or every game revision. The offline wrapper verifies the
owner's entire main-data image, size 37,748,736 bytes, SHA-256
`2c55fca2ed44c87a8886cfcfb0638ad8e25f5fac0e9fbe5d4cf990ba950cc8e1`,
then records per-descriptor extent hashes. Original bytes remain private.

## Typed contract

The native asset consists of `span<const ContactPointXZ>` and
`span<const CourseSegmentCorners>`. Corners retain the original order. The
native policy permits 1..65,535 records and 1..65,536 vertices. Each visited
record's indices must be within that vertex span, and every used coordinate
and the point must be finite binary32 in `[-2^20,2^20]`. Unvisited records and
unreferenced vertices are not read. This permits early completion while still
rejecting malformed data before it is used. Empty/oversized assets and invalid
used data return false without changing the output.

The previous first index is not an input: the original reads only the signed
second index at entry. It reduces that value modulo the positive record count,
then visits offsets `0,+1,-1,+2,-2,...`, wrapping the candidate index at the
count. At most `count` candidates are visited, and the first accepted one wins.
This is distinct from the three-record neighborhood of the subsequent contact
query. The explicit modulo behavior does not establish physical course closure,
distance units or lap semantics for the progression value.

Each candidate uses the already validated `contact_projection` arithmetic:
four differences, separately rounded products and subtraction for each edge,
followed by the original sign-bit unanimity test. The new complete oracle also
executes all four original command `0x5c` operations per candidate, so reuse is
checked against this helper's own operand ordering. Boundary signed zeros are
preserved; no epsilon, convexity check or replacement collision predicate is
introduced.

On a hit, `SegmentSelection.selected` holds the full nonnegative index;
`current` holds that index's low16 bits interpreted as signed16; `progression`
adds the signed search offset to the original progression, modulo 65,536. Thus
a selected index above 32,767 can have a negative stored `current`. This is
intentional original behavior. Every signed16 input progression is supported.
On a valid search with no hit, the function returns true and all three result
fields are -1. The boolean distinguishes supported input from invalid input,
not hit from miss. Inputs must not overlap output storage.

## Validation and limits

- Portable tests pass 3,634 probe cases, fingerprint `1c1c549c187f8285`, and
  69,170 extended cases, fingerprint `b4ab429abeb08285`. The extended set adds
  every signed16 progression encoding at count one. Tests cover alternating
  order, odd/even counts, misses, both index wrap boundaries, high selected
  indices, signed zeros, subnormals, corners and edge degeneracy. Fifty-five
  atomic rejection checks and two first-match/read-domain controls pass.
  AddressSanitizer and UndefinedBehaviorSanitizer pass the same corpus.
- The complete original search and predicate, including normal CPU call/return
  frame behavior and actual edge firmware, match all 3,634 synthetic cases:
  1,523 hits, 2,440,020 CPU instructions and 782,648 geometry instructions.
  Fingerprint `1c1c549c187f8285`. Every edge command executes 19 firmware
  instructions with six inputs, one output and no external table accesses.
  The comparison checks ordered candidate visits, ordered index writes,
  caller-frame restoration and preservation outside the callee's scratch and
  two output halfwords. It does not compare unsupported malformed assets or
  zero/negative original counts.
- Capture013 contributes 26 complete main-callback pairs, 24 unique inputs.
  All use selector zero, and selected/progression values are in 0..27.
  Twenty-five calls accept the current candidate; one accepts the next search
  candidate. All 26 match the native result and a fresh original execution
  using the verified owner's main-data assets. Live miss, large-count and
  signed-wrap paths are not claimed. The observer-enabled and disabled runs
  match all 2,850 frame checkpoints; the prior capture streams are unchanged.
  Combined evidence is 3,660 cases, 1,549 hits, 2,443,240 CPU instructions and
  784,700 geometry instructions, fingerprint `af7d5f3820082612`.

The standalone EE fixture passes the same 3,634-case probe and fingerprint,
55 rejection checks and two controls in Play 0.72 HLE. Its ELF SHA-256 is
`39eaf32b3cb72684eeab06ed01e0022a598f1f503e27b70ed03eeba8a82d1375`,
with 1,681,704 file bytes and 316,720 load-segment bytes. The source-bound record
is `build/ps2-native-segment-selection-ee-001/runtime-validation.json`.
Guest stderr is empty and two window captures were obtained. The 20.252-second
observation window is not fixture execution timing. These instruction counts
are reference work, not hardware cycles or PS2 throughput; physical-console
validation and connected native gameplay remain outstanding.

The current combined report is
`build/ps2-native-segment-selection-live-002/result.json`, SHA-256
`2915d831de1be60f6e71f69fa27f75bfde6194f4c9af707e34db54b3d14ed10b`.
The earlier `live-001` report is retained as historical evidence; a subsequent
header clarification removed an unsupported overlapping-storage promise, with
no numerical implementation change.

Reproduce the source-bound comparison with owner-supplied inputs:

```sh
python3 platform/ps2/tools/check_segment_selection.py \
  --program /path/to/owned/program.bin \
  --tgp-program /path/to/owned/tgp/program.bin \
  --main-data /path/to/owned/main_data.bin \
  --capture build/ps2/driving-capture-013/capture \
  --output build/segment-selection-check
```

Omit both `--main-data` and `--capture` for the synthetic original comparison.
Reports bind the original identities, compiled and reader source hashes,
generated reference executable, private row file and validated capture hashes.
The parser binds each pair to its saved helper/caller frames and unchanged
parent stream, requires the exact position/index addresses and verifies
preservation of all other captured object words.
The four portable regression tests in `tests/test_ps2_segment_selection.py`
also check the native fingerprints and malformed paired-capture rejection,
including an explicitly unclamped miss result.

Two investigation corrections are retained: the leading `0x2e` byte in the
transport word is not the operation number; the actual edge command is `0x5c`.
Also, the original main-data pointers here map from `0x02000000`, not the
unrelated `0x01000000` base initially tried during private inspection. Both were
corrected before any claimed native/original comparison.
