# Course query and projected contact recovery

This work follows the governing design's **Platform layer & build** native
rewrite exception. Native code consumes typed geometry and contains no guest
processor, mapped memory, firmware, dispatcher, or command transport. The
owner's original program, firmware, data, generated reference and captured
records remain under ignored `build/`.

## Verified boundaries

The uploaded geometry firmware is 6,660 bytes, SHA-256
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
The verified program/data identities are the same as the count probe documented
in [contact neighborhoods](contact_neighborhood.md).

| Operation | Firmware word interval | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Command `0x52`, including candidate search | `[0x58a,0x63c)` | 712 | `6e1b551ddd47a389a02c568408713e845b23cc7d326d22e980b0dc69cac2c60b` |
| Command `0x53`, result retrieval | `[0x63c,0x652)` | 88 | `90df2ca0b5b1116aa9d8ac7cfe6b853f7ab03260c36963e0de7cca882d4ebddc` |
| Projected predicate and its two outcome sites | `[0x5da,0x633)` | 356 | `f952416ee76dd2d423a20bf8f75fdab1aedde1b0416add8cbb14c5ed9dc8df70` |

The original CPU result adapter spans ROM `[0x2acc0,0x2aec4)`, 516 bytes,
SHA-256 `4a4c446316ee2c37644b3142acd090c3c002d8933b8c8bc886e5038ca88d852e`.
The alternate integrator calls it after query preparation at ROM `0x2e8a4`.
This adapter is distinct from the firmware search and from the native predicate.

## Search data and reply contract

Command `0x52` receives the staged point triple, descriptor word 1, the segment
count multiplied by four, previous/current/next indices multiplied by four,
and the fourth query argument multiplied by two. The firmware stages point Y
but its candidate containment arithmetic uses only X and Z. It tries the
**current, next, then previous** segment, in that order, and stops at the first
matching polygon in the first matching segment. This is not a nearest-height
or nearest-distance search.

Descriptor word 1 is the coprocessor-data **word** base of four-word segment
records. Within each segment record:

| Byte offset | Established use |
| --- | --- |
| `+0` | Polygon count, loaded into the firmware's eight-bit loop counter. |
| `+4` | Absolute coprocessor-data **byte** offset of the polygon array. |
| `+8` | Absolute coprocessor-data **byte** offset of the per-polygon attribute words. |
| `+12` | Not read by this query. |

Each polygon occupies 16 words: four XYZ triples followed by four plane/result
coefficients. The search loads X/Z for the four triples. Attribute bit 2 selects
four edges when set and three when clear. Other attribute bits are returned
unchanged; this predicate does not interpret them. A first inspection incorrectly
added the descriptor base to the two subordinate offsets and produced impossible
coordinates. Actual firmware memory-read addresses establish that those offsets
are absolute, correcting that interpretation.

Across the eight inspected descriptors, segment polygon counts range from
6 through 24. All inspected geometry coordinates are finite; the largest
absolute coordinate is `750.0670166015625`. These observations do not establish
safe behavior for malformed assets, zero/overflowing loop counts, or every
possible runtime descriptor. Repeated descriptors may refer to overlapping data.

The fourth query argument selects a result slot; it is not a geometric flag.
The search saves an attribute-pointer/geometry-pointer pair at internal word
`0x68 + 2*slot`, or two zeros when nothing matches. Command `0x53` retrieves
that slot. A hit returns the selected 16 geometry words followed by its attribute
word. A miss returns 17 zero words. The synthetic original-firmware fixture
independently writes slots 0 through 7 before retrieving them in reverse order;
all eight retain their distinct hit/miss payloads. This verifies that bounded
slot range and lifetime, not arbitrary slot encodings. The 34 captured caller
samples use slot zero. The native API returns the logical search value directly
and does not reproduce firmware slot storage or command transport.

The CPU adapter then interprets the raw reply. A zero final attribute word sets
its output status to 15. Otherwise it extracts `(attributes >> 16) & 0x8f`.
One branch computes a separately rounded plane-based vertical gap from the last
four coefficients and query point, then requests a separate geometry operation
for the normal. The special branch uses a projected determinant divided by a
returned coefficient and copies a direction triple. These downstream arithmetic
and normalization contracts are not implemented by `contact_projection`.
No physical axis units, force model, wheel ownership or player-object label is
inferred from the observed caller addresses.

## Native projected predicate

`contact_projection(vertices, point, output)` accepts exactly three or four
`ContactPointXZ` vertices. Each input binary32 coordinate must lie in
`[-2^20, 2^20]`, including signed zeros and subnormals. Invalid size or coordinate
returns false without modifying output. This is an explicit native domain,
not a claim that the original firmware validates assets.

For each consecutive pair, including the closing edge, the operation computes:

```
dx[i] = R32(vertex[i].x - point.x)
dz[i] = R32(vertex[i].z - point.z)
edge[i] = R32(R32(dx[i] * dz[next]) - R32(dx[next] * dz[i]))
```

`R32` is nearest/even binary32 rounding at each boundary. The result exposes
these rounded edge values and the original acceptance predicate: all edge sign
bits clear, or all edge sign bits set. This is a sign-bit test, including the
sign of zero, rather than a conventional epsilon-based point-in-polygon test.
A boundary zero can reject one winding while accepting its reverse. The API
does not validate convexity, reject degeneracy, or repair inconsistent winding.
Those properties must not be silently added to the recovered behavior.

The implementation uses bounded integer dyadics: at most 48-bit exact products
and 56-bit aligned sums. Input differences have magnitude at most `2^21`,
products at most `2^42`, and edge differences at most `2^43`, so no binary32
overflow occurs. Explicit rounding retains gradual underflow and signed zero
without depending on the EE FPU. This arithmetic shares the already reviewed
original bounded method used for direction blending; no firmware instruction
sequence is compiled into the native component.

## Native ordered course search

`search_course_contact(course, current, point, output)` operates on immutable
`ContactSegment` spans containing typed `ContactPolygon` values. Each polygon
contains four XYZ vertices, four uninterpreted coefficient values and an
attribute word. The result identifies the selected segment and polygon and
copies the entire selected value; it keeps no pointers to caller storage.

The operation reuses the verified cyclic neighborhood and projected predicate.
It checks current, next, then previous, stopping at the first accepted polygon.
The course must contain 1 through 65,535 segments, with a valid signed 16-bit
current index. Each visited segment must contain 1 through 255 polygons, and
each encountered X/Z value used by its triangle/quad must satisfy the predicate
domain. These are explicit native asset policies; the original firmware's
eight-bit loop-counter behavior for zero or overflowing counts is not adopted.
Unvisited segments and unused fourth triangle vertices are not prevalidated.
Unsupported encountered data returns false without changing output.

On success, a separate `matched` value distinguishes a geometric hit from a
miss, including a hit whose attribute word happens to be zero. A valid miss
returns zero indices and an all-zero polygon payload. Y and coefficient words
are copied by object representation without arithmetic, preserving signed zero,
subnormals and arbitrary payload encodings. The query's Y component does not
participate in selection. No allocation, height ordering, normal transformation,
force response, game ownership lookup or firmware execution occurs in the
native function. Course loading and gameplay integration remain separate work.

## Validation and limits

- ROM-free host tests compare every edge bit and acceptance with an independent
  mathematical oracle on **100,774** original cases, including both windings,
  edges/corners, degeneracy, signed zeros, subnormals, exponent extremes and
  random coordinates. Fingerprint: `128f72100657ddf0`. AddressSanitizer and
  UndefinedBehaviorSanitizer pass the same suite. There are 64 atomic rejection
  checks for invalid sizes and coordinates.
- The private original-firmware differential agrees on all **100,774** cases,
  executing **5,791,705** reference instructions and exactly one attribute-word
  read per case. It compares every edge and acceptance, confines execution to
  the recovered predicate/outcome sites, and rejects unexpected FIFO/memory
  traffic. Fingerprint: `128f72100657ddf0`. The first harness stopped at branch
  budgets, which do not stop on an untaken branch's fall-through; explicit
  outcome observation corrected the harness without changing native arithmetic.
- The native EE subset passed **4,870** cases, fingerprint
  `402152f180506e07`, plus all 64 rejection checks in Play 0.72. ELF SHA-256:
  `c499c0944e9a1ac557b865111661f16b0c59bc2b213cb97c71381e7d75879b2f`;
  1,226,272 file bytes and 180,576 load-segment memory bytes. Guest stderr is
  empty. The 15.073-second emulator observation is not measured fixture
  completion time or physical-console performance.
- ROM-free complete-search tests cover all 4,096 hit/miss masks over three
  four-polygon segments for each current index: **12,288** configurations,
  fingerprint `4513b3de87618b42`. Independent expected selection checks current,
  next and previous priority and polygon priority. Twelve host boundary checks
  cover one/two-segment repetition, maximum segment/polygon policies, zero
  attributes, unsupported sizes and atomic rejection. AddressSanitizer and
  UndefinedBehaviorSanitizer pass the same suite.
- The native EE search subset passed **192** configurations, fingerprint
  `37f1a2cc4dd9eb9b`, plus **10** boundary checks in Play 0.72. ELF SHA-256:
  `cb0ba641fe5e52fa1f0ae4387663f21bc93dcedc9f80e625130d40bb55172dfe`;
  1,228,244 file bytes and 182,112 load-segment memory bytes. Guest stderr is
  empty. The 15.090-second observation is not measured completion time or
  physical-console performance. Source/runtime binding:
  `build/ps2-native-contact-search-ee-001/runtime-validation.json`.
- The full original-firmware differential compares selected pointer identity
  and every one of the 17 result words, including opaque Y/coefficient values.
  It covers the complete 12,288 selection configurations, a maximum 255-polygon
  late hit and complete miss, all 32 individual attribute bits plus all bits
  together, and eight independently retained result slots. The original
  instructions and typed native implementation agree throughout: **12,323**
  searches plus **8** retained-slot comparisons, **3,550,622** reference
  instructions, fingerprint `ccd788d0a091b5f8`. Source-bound report:
  `build/ps2-native-contact-search-005/result.json`. The first
  pointer assertion mistakenly treated stored addresses as zero-extended and
  swapped the slot pair; inspecting exact stored pointers corrected this
  offline harness without changing the native implementation.
- Observer capture 007 preserves all **2,850** enabled/disabled machine
  checkpoints. Its **34** complete query/result groups match the independent
  original firmware on all **578** reply words, and the native search selects
  the same segment/polygon and copies the same complete payload for each query.
  All **227** visited projected predicates, including **34** acceptances, also
  match every rounded edge word. The original search executes **20,697**
  instructions and result retrieval **1,700** instructions in this replay.
  Result fingerprint: `efc0379c98c1055f`; edge fingerprint: `f8630eb1a6dc71b0`.
  These are sampled slot-zero queries, not proof of every course or game state.

The capture parser requires complete query/result boundaries, their strict
global instruction order, matching object/call identities, unchanged query
points and output prefix, and a captured caller-frame return proof. An interrupt
can replace the child's raw return register; the saved caller frame's return PC
and frame pointer are therefore validated instead. Earlier capture 004 retains
its separately supported legacy query proof. Incomplete captures 005/006 are
not counted as passing evidence. A synthetic regression exposed cross-stream
chronology that two separately ordered streams had allowed; the final live
report binds the corrected global-order parser and its six focused tests.

The source-bound original-firmware report is
`build/ps2-native-contact-projection-001/result.json`. The final native/live
query report is `build/ps2-native-contact-query-live-004/result.json`, bound to
`build/ps2/driving-capture-007/validation.json`. Initial full-query study artifacts
are under `build/ps2-contact-query-study-001`; these contain private query data
and are not distributable assets. The recovered bounded search remains
disconnected from gameplay. Its downstream CPU reply interpretation, normal
transformation, contact response and vehicle integration are not a completed
native collision system.

Reproduce the source-only and private differential checks with:

```sh
python3 tests/test_ps2_contact_projection.py
python3 tests/test_ps2_contact_search.py
python3 tests/test_ps2_contact_query.py
python3 platform/ps2/tools/check_contact_projection.py \
  --tgp-program /path/to/owned/tgp/program.bin \
  --output build/contact-projection-check
python3 platform/ps2/tools/check_contact_search.py \
  --tgp-program /path/to/owned/tgp/program.bin \
  --tgp-tables /path/to/owned/tgp/copro_tgp_tables.bin \
  --output build/contact-search-check
python3 platform/ps2/tools/check_contact_query.py \
  --program /path/to/owned/program.bin \
  --tgp-program /path/to/owned/tgp/program.bin \
  --tgp-tables /path/to/owned/tgp/copro_tgp_tables.bin \
  --copro-data /path/to/owned/tgp/copro_data.bin \
  --capture build/verified-driving-capture/capture \
  --output build/contact-query-check
```
