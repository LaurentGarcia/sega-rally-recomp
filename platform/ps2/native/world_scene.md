# Explicit world presentation

The reusable engine's `WorldScene` (`include/world_scene.h`,
`src/world_scene.cpp`) is the presentation bridge for a persistent native vehicle update.
It receives model-local points, indexed opaque flat faces, an explicit
`object_to_world` matrix per instance, and a fixed `world_to_clip` matrix.
The existing `StudyScene` and its captured reference-view transforms are not
used or changed. There is no automatic fitting, recentering, inferred camera,
recorded movement or game-state advancement.

Both matrix types use column vectors and column-major storage. Object matrices
must be affine. The view may be projective and follows `native_geometry`'s
y-up clip coordinates, positive-w guard, `0 <= z <= w` depth convention and
496×384 logical viewport. The adapter uses the existing homogeneous clipping
and projection operation, then sends an ordinary `Frame` with native reverse
depth enabled. It does not reconstruct original lighting, textures, polygon
priority or game camera behavior.

`configure(source_faces, output_triangles, error)` allocates two bounded banks
of materials and triangles. Limits are 32,768 source faces, 16,384 output
triangles, 1,024 instances and 65,536 declared vertices across all instances.
`update()` allocates nothing. The inactive bank is prepared before committing;
failure preserves the last complete frame. No clipping overflow is truncated.
Caller model storage and transforms need only survive `update()`. The returned
frame refers to owned storage and expires at the next successful update or
reconfiguration, or owner destruction. Calls must be serialized with display
submission. `stats().persistent_bytes` reports actual bank/object storage.

Clipping input and its eight-triangle output scratch are initialized once per
update and reused across faces. Every input corner and material pointer is
overwritten before clipping; only the returned output count enters the inactive
frame bank. Unused scratch entries are never copied. This removes repeated
default initialization without changing allocation, capacity, ordering or
transactional publication.

Configuration also allocates an exact transformed-vertex cache by default,
with a power-of-two capacity capped at 4,096 entries / 128 KiB. Its key contains
the model vertex index and a serial that changes for every instance and every
update. Direct-map collisions replace entries; they do not change geometry.
Serial wrap clears validity before reuse. No model pointer, transform or vertex
result survives logically into the next instance, so callers may still edit,
replace or release their model storage between updates. A hit reuses precisely
the scalar transform's four narrowed clip components. Materials, clipping,
errors and output ordering remain unchanged. The optional fourth
`configure()` argument disables this cache for the uncached diagnostic path.

`set_profile_clock(clock, context)` enables bounded stage diagnostics in the
caller's clock units: validation, group bounds, vertex transforms and clipping.
The caller owns the context and supplies a monotonic clock; null disables it.
Successful frame statistics also count transformed corners and cache hits.
Timing callbacks add overhead, so these measurements are explicitly
instrumented. No platform timer or allocator enters the engine dependency set.

A model may provide `WorldFaceGroup` ranges that partition its faces in order.
Each range must be nonempty and contiguous, with every face covered exactly
once. The engine derives a bounding box from validated active vertices each
update; it never trusts an external bounding box. Extrema are selected using
exact comparisons of validated binary32 normal-or-zero values; only the six
selected endpoints are widened to binary64. Sequential binary64 interval
transforms widen every multiply and add outward by one ULP. A group is skipped
only when its complete rounded clip bounds lie outside a common clip plane and
prove that no transformed component could violate the ordinary clip-input
domain. Bit-based endpoint classification preserves rejection of subnormals on
the EE. Uncertain bounds use the existing per-face path. Face/material order,
transformation order, clipping, output limits and frame commit behavior remain
unchanged. Counters report groups, rejected groups and transformed corners;
the latter counts triangle corners, not unique model vertices.

`set_numeric_mode(WorldNumericMode::reference)` is the default. It uses
binary64 CPU scratch arithmetic and the existing reference clipper. The
explicit `native_float` mode instead uses binary32 for both matrix stages,
group bounds, clipping, projection and projected-area checks. No implicit
binary64 fallback runs in that mode. A failed or rejected provider batch uses
the native float scalar transform. Numerical failure rejects the transaction
and retains the previous frame; it does not quietly select another precision.
The selected mode survives reconfiguration and is reported in successful-frame
statistics. Changing it between updates starts fresh logical vertex caches.

Native group bounds widen each elementary operation by eight binary32 encoding
steps, enough for four ULPs across exponent boundaries. This includes interval
evaluation rounding and the provider's allowed two-ULP arithmetic variation.
Zero and ±1 exact operations are handled separately; possible underflow is
enclosed by normal endpoints around zero. An interval that could hide invalid
clip values disables group rejection and reaches the ordinary vertex path.
Plane rejection compares endpoints directly without rounded plane sums. This
is a conservative native presentation bound, not an original-game arithmetic
or universal pixel-error guarantee.

The optional `set_transform_provider()` hook batches up to
96 distinct cache misses from at most 32 faces, without crossing a culling-group
boundary. Its input/output storage is owned by the scene and reused without
allocation. The provider supplies individually guarded clip coordinates; normal
guard rejection uses the exact scalar result. A failed/unavailable backend
returns false, discards every batch output, increments `failed_batches`, and
uses the scalar path. The application owns transport error reporting and may
disable a retired backend. The callback must not retain spans or reenter the
scene. No callback is installed by default.

Provider results are explicitly rendering approximations unless the provider
proves otherwise. Cache provenance distinguishes them from exact scalar values.
In reference mode, if any corner requires clipping, all approximate corners of that
triangle are recomputed exactly, including cached endpoints. This is necessary:
a small projected endpoint error can be amplified by the intersection of a
nearly parallel edge with a clip plane. A per-vertex projection budget alone
does not bound that new intersection. Only wholly inside triangles retain
approximate results. Counters distinguish prefetched accepted vertices,
triangles restored for clipping, and approximate corners actually consumed by
nondegenerate output triangles. The serial's high bit stores provenance while
the remaining 31 bits identify an instance, keeping cache entries at 32 bytes.
Changing providers between updates never reuses old approximate coordinates.
Native-float mode accepts bounded outside provider results and clips them in
binary32 without this exact-reference restoration. The provider must implement
the supplied two-matrix transform within its stated elementary arithmetic
allowance; arbitrary vertex deformation would invalidate group culling.

The VU1 provider has its own arithmetic, transfer and projection-budget
validation. Continuous projected-coordinate budgets do not promise identical
GS quantization, depth occlusion or pixels. Neither the generic hook nor host
mock-provider tests establish actual VU execution or a hardware speedup.

The original synthetic fixture passes 50 checks. With an unchanged camera,
translating one instance by +0.25 on world X moves it exactly 62 logical pixels;
a second fixed instance does not move. An asymmetric affine/view case has an
independently calculated first vertex at (240.25,198), reverse depth 0.625.
The fixture also checks no update allocations, five initialization allocation
failures, malformed models/transforms, output overflow, clipped-away geometry
and preserved frames. The portable runner is `tests/test_ps2_world_scene.py`.
The same original fixture passes ASan/UBSan. These are host geometry/ownership
tests, not a new console execution or recovered driving demonstration.

The separate culling fixture compares the grouped path with the unchanged
ungrouped path across 2,052 original synthetic frames. All 60,534 checks pass,
including exact ordered output vertex/material bits, errors, malformed ranges,
subnormal and transform-limit cases, and no update allocations. It rejects
146,843 groups and transforms 299,742 corners versus 1,180,800 without grouping.
ASan/UBSan and source identities are recorded in
`build/ps2-world-groups-002/group-result.json`.

An additional private comparison uses the owner's actual course at 60 fixed
diagnostic views. Both paths produce the same 2,512 output triangles, hash
`95c6d30c2f94d9c1`; 16,105 of 18,000 groups are rejected and transformed corners
fall from 1,037,520 to 113,772. Evidence is bound in
`build/ps2-world-groups-002/course-result.json`. These counts demonstrate work
removed and image agreement; they are not PS2 frame-rate measurements.

The subsequent exact-cache comparison is recorded separately under
`build/ps2-world-cache-001/`. Its original fixture passes 519 frames / 111,546
checks for mutable geometry, colliding indices, multiple instances, rollback,
stage-clock accounting and no update allocations. The 2,052-frame group corpus
remains exact; its transformed-corner count falls further to 199,828. The same
60 real-course views preserve all vertex/material bits and the image-data hash
above while transforming 77,714 corners. All three source-bound reports passed
ASan/UBSan. These results establish exact cache reuse, not a hardware speedup.

The final hook/provenance source is bound by four reports under
`build/ps2-world-batch-002/`. The original provider fixture covers 106 frames
with 3,299,994 assertions, including the 96-vertex limit, group boundaries,
deduplication, mutable/shared models, invalid output encodings, failed transport,
output overflow and allocation-free updates. Authored right-plane and
positive-w-plane cases first cache approximate endpoints in an inside triangle,
then prove that the following clipped triangle is exactly restored. Switching
the provider off also restores the complete exact frame. The cache/group
corpora and 60 real-course views remain unchanged with an empty provider.
All four current reports passed ASan/UBSan.

The native-float fixture passes 775 original frames / 70,417 checks under
ASan/UBSan. Grouped, ungrouped and host float-provider paths have identical
ordered frames and failure results in that corpus, including outside vertices,
transport rejection, invalid provider output, mutable source data, mode changes,
rollback and allocation-free updates. It conservatively rejects 46,759 groups
and consumes 20,124 provider corners. Evidence is bound in
`build/ps2-world-float-001/test-result.json`.

A private whole-frame comparison processes all 601 owner-derived native poses
with the actual body/course and the diagnostic camera. Native float produces
546,158 triangles versus the reference's 546,073; 171 frames have different fan
or degenerate counts. At corresponding vertices, the largest observed screen
coordinate difference is 0.018586 logical pixels and reverse-depth difference
is 6.855e-7. An independent host pixel-center raster changes 68 of 114,468,864
color samples (at most two per frame), including four coverage changes. Raster
depth differences may jump where a different surface wins, so they are not
bounded by the corresponding-vertex error. Host float-provider frames match the
native scalar frames exactly in this corpus. This is measured presentation
agreement, not topology identity, GS pixel parity, actual VU execution or a
universal tolerance. Sources, private inputs and per-frame measurements are
bound in `build/ps2-world-float-001/corpus-result.json`.

The subsequent actual-VU replay uses all 35,276 native-mode batches / 1,364,846
vertices executed in Play 0.72 HLE. Its reader requires the current matrices,
input vertices, frame identifiers and batch sizes to match the sealed corpus
byte-for-byte, then consumes every output record and the completion footer.
Those VU outputs run through the host native-float clipper and the same private
pixel-center raster. Across all 601 scenes, native output contains 546,152
triangles; 173 frames differ in fan/degenerate counts from the 546,073-triangle
reference. Compared same-count/same-color output positions differ by at most
0.043061 logical pixels and 2.057e-6 reverse depth. The raster changes 305 of
114,468,864 color samples, at most four per frame, with 14 coverage changes.
All 601 VU-derived frames differ in some raw geometry bits from host float;
no bit-identity assertion is made. The path consumes 1,614,990 provider corners
with no reference clipping restorations.

The largest raster depth difference is 0.0276916425, much larger than the
compared vertex-depth difference. Its recorded diagnostic is frame 2569,
pixel (266,156): a nearer triangle moves across the pixel center, changing its
edge weight from −0.00009076 in the reference to +0.00019485 natively. The
farther triangle remains covered in both images. Interpolated depths of the
matched individual surfaces differ by about 1.06e-7 and 2.28e-7; the larger
raster difference is therefore a confirmed change in which surface wins at
that edge sample. `depth-worst.json` retains both triangles and their matched
counterparts. This observation does not imply a bound on all raster depths.

`build/ps2-world-vu-float-002/corpus-result.json` binds the complete-frame
comparison, private inputs, VU result stream and
`build/ps2-vu1-corpus-ee-004/runtime-validation.json`. The latter proves actual
VU instruction execution in Play, including VU0 packing after each VU1 pair;
the replay proves the resulting complete host-clipped images remain close in
this corpus. Neither result establishes physical-console behavior, GS pixel
identity, a full game camera/material reproduction or a universal error bound.
Replay revision 001 is superseded: its reader rejected an extra callback but
did not latch that error, allowing scalar fallback to conceal an excess request.
Revision 002 latches the error and checks an explicit excess-callback rejection
after completing the stream. All 601 image measurements remain unchanged.

The subsequent scratch-reuse change passes all five portable fixtures and the
775-frame native-float fixture under ASan/UBSan. A direct old/new stream
comparison covers all ordered material colors and six binary32 vertex fields
in 1,803 complete frames: reference, native scalar and actual-VU replay for
each of the 601 poses. All 124,539,359 compared bytes, including deterministic
summaries, match. The source-bound evidence is
`build/ps2-world-cost-002/comparison.json`; it does not retain another copy of
the private geometry stream. The refreshed strict VU replay at
`build/ps2-world-vu-scratch-003/corpus-result.json` preserves every numerical
and raster measurement above and the excess-callback negative check. This
binds the subsequent transport fixture's
`build/ps2-vu1-corpus-ee-005/runtime-validation.json`: its complete 22,119,772-byte
VU output matches the previous run exactly. The replay uses those sealed
outputs with byte-matched batch inputs; the replay itself is not another VU
execution or a physical-console test.

The timing investigation first suspected exact body-pose preparation outside
the scene stage counters. Native build 031 disproved that explanation in Play:
pose preparation took about 0.08 ms, while `WorldScene` took 27.33–27.55 ms.
Roughly 11.2 ms lay outside its four stage counters. Repeated clipping-scratch
initialization occurred before the clipping counter, making it a measured
candidate for that residual. A 6,010-frame host diagnostic decreased from
0.731 s to 0.617 s after scratch reuse; this single host comparison is not a
PS2 speed claim. Native build 032 subsequently measured about 24.53–24.72 ms
for the first three geometry preparations in Play, approximately 2.9 ms less
than build 031. That build also includes a separate VU cache-publication
change, so the whole difference is not attributed to scratch reuse alone.

The unassigned scene time still measured approximately 11.2 ms in build 032:
frame one's scene total was 3,605,248 timer ticks, while the four stage totals
sum to 1,951,744 ticks, at 147,456,000 reported ticks per second. Initialization
can move across clock calls during compilation; stage attribution alone does
not establish where the saving occurred. Detailed profiling calls the supplied
clock four times per processed face, twice per group and twice for validation.
Only part of each clock call lies inside the accumulated intervals. Native
build 033 disabled those detailed calls while retaining the outer geometry
timer: the first-three mean fell to 23.036 ms, about 1.6 ms less. Thus timer
overhead did not explain the whole previously unassigned interval.

A separate private O3 fixture then used CP0 Count to bracket the complete
scene work, including material setup and output emission, on one fixed real
checkpoint pose. Eight updates alternated the existing scratch-copy path and
a diagnostic direct-output path; all produced the same ordered geometry
fingerprint and 906 triangles. Baseline mean counts were 6.879 million total,
2.789 million clipping, 2.069 million transform, 1.227 million bounds,
0.610 million validation and 0.143 million emission, with about 0.040 million
unassigned. These finer counters attribute much more work to clipping than
the coarse timer did. They remain instrumented emulator observations: compiler
scheduling and emulator counter behavior limit tiny interval attribution
(material setup read exactly one count per processed face).

Direct output reduced the outer timer mean only from 3,443,392 to 3,431,168
ticks, approximately 0.35% / 0.083 ms in this fixed-pose experiment. It was not
adopted into production. The result rules out removing that copy as the large
remaining improvement sought here; it does not establish console timing or
cost across every scene. Source, ELF, logs, input identities and all eight
measurements are bound in
`build/ps2-world-profile-ee-001/runtime-validation.json`. The ordinary renderer
retains the tested scratch-reuse implementation.

The next bounded experiment instead specialized wholly inside triangles in the
native float clipper. After the unchanged input and plane checks, that path
performs the same three projections, area predicate and capacity check, then
publishes one triangle atomically. It avoids initializing the general clipping
polygons, maximum-size projected array and eight-triangle staging array.
Triangles requiring clipping continue through the previous general path.

The same fixed-pose, same-executable O3 comparison alternated the original and
inside-specialized clippers while retaining outer scratch copying in both.
All eight updates again produced identical ordered geometry. Mean outer time
fell from 3,441,280 to 2,740,928 reported ticks: 20.35%, or about 4.750 ms in
this instrumented Play experiment. The source/ELF-bound result is
`build/ps2-world-inside-ee-001/runtime-validation.json`. A separate comparison
of all 601 poses in reference, native-scalar and actual-VU replay modes matches
all 124,539,359 output-stream bytes exactly; its evidence is
`build/ps2-world-inside-frames-002/comparison.json`. This measured fast path was
selected for production; it adds no numerical approximation or game-state
change. Emulator timing on one pose does not establish a console frame rate.

The current `RallyBodyScene` integrates a separate raw model-local body asset
with persistent native position/orientation and an explicit model-to-car bridge.
Its identity bridge remains diagnostic until the original body draw's axis and
matrix provenance is independently verified. `StudyScene` reference-view
matrices are never reinterpreted as world transforms. Separate wheel assembly,
original materials and original camera behavior remain outside this adapter.
