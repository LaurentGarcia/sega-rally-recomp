# Ordered world geometry through VU1 and GS

`Display::present_world` consumes a `WorldDrawPlan` without first building an
EE `Triangle` array. It is a separate, explicit renderer path. The ordinary
`Display::present(Frame)` and transform-only WorldScene provider remain available.
The development application selects the new path with
`--drive-world-vu CHECKPOINT COURSE VISUAL_DIRECTORY`; it does not change the
native vehicle arithmetic or load a reference runtime.

The optional `WorldSubmissionMode` argument selects `fused`, `classify_first`,
or the explicit `classify_async` scheduler.
The API's default remains `fused` for compatibility and comparison. The
application's explicit `--drive-world-vu` mode selects `classify_first` after
the paired validation below. `--drive-world-async CHECKPOINT COURSE VISUAL_DIRECTORY`
selects `classify_async`; presentation still completes before returning to the
application. Automatic textured startup still selects Frame,
which retains its specialized, inlined primitive writer and texture lookup.

## Ownership and lifetime

`WorldScene::configure_plan` allocates two plan banks separately from Frame
storage. `plan_prepared` validates the instance/view inputs, applies the existing
group and authored-facing decisions, and publishes ordered source-face indices.
Each bank retains every immutable prepared snapshot. The borrowed plan survives
normal Frame updates and source-owner destruction, until the next successful
plan/configuration or scene destruction. Planning does not prove that transformed
geometry fits the numeric domain or expanded triangle capacity.

The caller initializes one `Vu1Transform`, then explicitly calls
`initialize_world` before presenting a plan. This reuses the existing exclusive
VIF1 lease and allocates persistent world staging once. It does not introduce a
second VU owner. Calls, resource changes and destruction are serialized; there
must be no concurrent VIF1, GIF or GS producer.

Display owns GS state and texture/CLUT residency. A batch contains at most
16 consecutive source triangles (48 corners), with independent per-corner UVs
and mixed materials. In fused mode, Display preloads their textures and palettes, then rechecks
every texture identity and palette's complete content. An eviction during
preloading makes the batch use ordered scalar fallback. The next resource
overwrite waits for GS completion of preceding draws. Neither plan consumption nor drawing
allocates memory per frame.

In `classify_first`, Display gathers source corners and UV scales before any
texture/CLUT lookup. VU1 transforms, classifies and prepares the projected packet
without emitting to GS. A crossing or numerically unsupported batch goes directly
to ordered clipping; only emitted fallback fans acquire material resources.
A ready batch checks remaining triangle capacity, preloads resources, rechecks
every resident identity, and supplies state/color to the retained packet.
If a ready batch cannot be emitted because of capacity or residency, Display
retrieves its original retained clips rather than recomputing them. Capacity
failure still follows the existing no-flip drain and recovery contract.

The current sixteen-texture batch can require at most 64 of 96 texture pages and
sixteen of 256 CLUT slots. Residency rechecking remains defensive; a zero count
of ready-but-nonresident fallbacks is not evidence that this branch was exercised.

## Immutable material resolution

The optional `WorldMaterialMode` selects `resolve_each` (the default control)
or `memoized`, independently of fused, classified or async submission.
The application explicitly selects `memoized` for `--drive-world-vu` and
`--drive-world-async`; `--drive-world-no-memo CHECKPOINT COURSE VISUAL_DIRECTORY`
retains synchronous classification with `resolve_each` for comparison.
The memo is local to one `present_world` call: 64 entries occupy 4,096 bytes
with the pinned EE compiler, with no allocation or cache retained across calls.
Only immutable textured material pointers from the plan are keys; temporary flat
materials bypass it. Each call, including a retry after rejection, starts with
empty tags. The control does not initialize the table.

A memo hit still performs the original texture LRU touch, then palette age and
last-hit updates, and increments both existing resource-hit counters. The
known-slot texture helper validates the complete texture identity before any
mutation. All palette prerequisites are checked before that touch, so a failed
lookup cannot double-count or repeat a partial resource operation. Misses use
the unchanged texture/palette functions and their completion/upload fences.

Successful texture and palette upload counts serve as per-call residency
epochs. With at most 32,768 source faces, 16,384 emitted fans and one checker
setup, these counters cannot wrap during a presentation. Unchanged epochs prove
that a saved slot has not been overwritten; changed epochs retain the full
texture-identity and sixteen-word palette comparison after all preloads.
Clipped fallback uses the original writer, whose uploads invalidate the same
proof. Source order and every existing touch, upload and eviction remain intact.

After residency is established, the memo can copy the complete state previously
built by the SDK for the same material, texture slot, CLUT slot and epochs.
Depth/checker setup is fixed for the call. State resolution re-finds the key
after preloading, because another material may have replaced that direct-mapped
bucket; it never retains an unverified pointer into an evictable memo entry.
GS packets still contain their original per-triangle state writes.

The additional `world_material_memo_hits`, `world_material_memo_misses` and
`world_material_memo_invalidations` count resolved preload requests and stale
epoch observations. `world_material_state_hits` counts reused SDK state records;
`world_material_residency_fast` counts post-preload face checks justified by
unchanged epochs. These diagnostics do not replace existing cache counters or
establish a speedup without a paired target measurement. The ordinary Frame
texture, palette and primitive-writing functions are unchanged.

## Validated fallback rejection

The trailing `WorldFallbackMode` selects `clip_each` (the default control) or
`reject_outside`. The application selects `reject_outside` for its explicit
world modes; `--drive-world-no-reject CHECKPOINT COURSE VISUAL_DIRECTORY`
retains synchronous classification and memoized materials with `clip_each`.
Automatic Frame startup is unchanged. The rejection gate applies only after the VU owner successfully
publishes a fully validated fallback clip prefix, including ready cancellation.
Before constructing a `ClipTriangle` and copying its UVs, it checks whether all
three vertices share a strict outside plane. The seven comparisons are exactly
the float clipper's signed-W comparisons; vertices on a plane remain inside.
The VU CLIP instruction's absolute-W behavior is not substituted for this test.

Every remaining face uses the unchanged clipper, fan order, capacity checks and
primitive writer. Rejected VU submissions and scalar retries always take that
complete path, preserving their numerical errors. This adds no transform,
projection, VU program or resource-order change. `world_trivial_rejects` counts
source fallback faces that skip construction and clipping, not removed output
triangles or guaranteed avoided material requests: fused mode may already have
preloaded them. A late failure retains the same GS drain and no-flip recovery.

The internal predicate requires already validated normal-or-zero bounded clip
coordinates. Its separate host proof checks 100,233 cases, including signed W
and all seven boundaries; all 82,901 true results match the public clipper's
successful empty output without changing caller storage. Optimized and
sanitizer runs pass. The report is
`build/ps2-common-outside-001/validation.json`, SHA-256
`c9c56654ed5f1cd4c3bda6ee121f34fc6e4ed05e856c42cbe579856fdad00676`.

## Prepared geometry handles

`Vu1Transform::prepare_world` accepts `Vu1WorldGeometry`, with the two matrices,
corners, output height and sequence but no resident GS state. Its result is
`ready`, `fallback`, `rejected` or `failed`. Ready publishes only an opaque
`Vu1WorldToken`; fallback publishes the complete clip prefix atomically.
Rejected or failed preparation preserves both caller outputs. Empty preparation
is rejected, unlike the existing empty fused draw.

`emit_world` supplies resolved per-triangle state and an explicit completion
mode. `cancel_world` publishes the retained clips without emission. Successful
emit or cancel consumes all copies of a token. A copied token is another handle
to the same preparation, not another lease. A process-wide nonwrapping ticket
also prevents a recycled owner address from reviving an old handle.

Invalid token, state or output arguments reject without consuming a current
ticket. Synchronous transform, draw or preparation attempts invalidate older
preparations, including empty or rejected attempts; actual program initialization
does too. Idempotent initialization preserves them. While an async submission is
live, synchronous mutation rejects BUSY before changing either bank or pending
DMA storage. Display returns no token to the caller.

## Nonblocking submission and EE lookahead

`submit_prepare_world` and `submit_emit_world` copy their inputs into the owner's
persistent DMA staging and return an opaque `Vu1WorldSubmission`. A successful
submission starts transport without waiting for VU/GIF completion. `poll_world`
checks transport and the bounded whole-operation deadline without reading mapped
VU memory; either pending or already complete is valid immediately after submit.
`finish_prepare_world` and `finish_emit_world` wait as needed, inspect status and
consume the submission. Wrong finish methods or invalid output spans reject
without consuming it, allowing a valid retry. `drain_world` retires and consumes
it without publishing geometry. Fatal transport faults quarantine the owner and
retain source storage if owned DMA cannot be stopped.

Only one DMA/VU operation is in flight. Two independent bank tickets may retain
completed ready geometry; async preparation preserves the other bank and rejects
when both are occupied. Emitting or cancelling one ticket leaves the other
intact. This provides bank ownership for later scheduling; it does not mean two
VU programs execute simultaneously.

Display's initial `classify_async` schedule uses two fixed CPU batch buffers.
While prepare N is submitted, it gathers the first half of N+1's original
corners/material references. While emit N is submitted, it gathers the remainder.
If N falls back, it completes that remaining CPU work after ordered fallback.
Each face is gathered once. Gathering performs no resource lookup, PATH3 transfer
or mapped VU access. The driver finishes before any such operation. Display marks
the GS completion obligation as soon as a deferred emit is submitted, and drains
any live submission before its GS rejection/destruction cleanup. There is no
per-frame allocation and no pending operation survives a successful return.

`world_async_prepare_submissions` and `world_async_emit_submissions` count accepted
operations. `world_async_gathered_triangles` counts source faces gathered between
submit and finish, not measured concurrent VU execution. In particular, an
emulator may complete work while processing the DMA-start write. The separate
synchronous mode remains the comparison control; asynchronous API shape alone
does not establish a frame-time improvement.

## Submission and fallback

Display supplies two matrices, original XYZ/UV corners, and resolved per-triangle
TEST/TEX0/CLAMP/PRIM/color state. The kernel transforms all corners, retains their
clip coordinates, and classifies the whole batch before emitting anything.
Wholly inside, admitted geometry is projected and packed into ordered GS packets
on VU1 and sent through XGKICK. The packet preserves cutout, checker and reverse
depth behavior. An uncertain/zero-area or crossing batch emits no prefix.

The typed result distinguishes four cases:

- `drawn`: the packet has retired from PATH1 and clip-output memory is unchanged.
  The default `gs_finished` completion mode also proves GS completion; Display
  explicitly selects `path1_retired` and owns the later GS completion fence.
- `fallback`: no packet was emitted; the complete valid clip prefix is published
  atomically for the existing binary32 clipper and Triangle writer.
- `rejected`: no packet was emitted and clip output is unchanged; Display
  recomputes that entire batch using the existing scalar transform and clipper.
- `failed`: transport/ownership is no longer usable; drawing stops without a
  buffer flip and the affected owner is quarantined.

Fallback keeps source and fan order, including UV seams and materials. Display
checks the expanded output capacity before appending each fan. Numeric/capacity
rejection may leave partial work in the backbuffer, but never publishes it.
Display discards unsent CPU geometry and drains submitted work before returning
failure; the next successful presentation clears that buffer. Transport failure is different:
the renderer must restart, and DMA source storage is retained if its owned
channel cannot be stopped safely.

## Transfer boundaries

Completion remains mandatory at each GIF path handoff. Display terminates
queued PATH3 state and fallback primitives with an explicit zero-loop EOP tag,
then waits for bounded D2 completion and the full GIF-idle guard before PATH1.
The delimiter is necessary because SDK clear/state packets can end with EOP
unset. This retires the packet stream without waiting for GS rendering to finish.
The VIF owner waits for bounded DMA/FLUSH/MARK and full GIF-idle completion before
reading the kernel's count/sequence status or reusing a VU bank. Two disjoint VU
data banks do not imply transfer overlap.

Preparation also requires retired VIF/DMA/GIF transport before mapped status
access, although GS raster work may remain pending. Display retires an EOP0
PATH3 stream before preparation, and again after any resource upload before
emit or cancel. The separate emit entry preserves retained ST/XYZ while uploading
fresh packet headers and colors. Both the full `gs_finished` tail and deferred
`path1_retired` tail retain their existing completion obligations. These boundaries
follow EE User Manual v6 sections6.2.3–6.2.4 (mapped VU memory ownership),
sections6.3.4–6.3.5 (UNPACK/STCYCL), and the FLUSHE/FLUSH descriptions on
pages111–112; packet retirement is distinct from GS raster completion.

Display tracks CPU-queued words, unretired PATH3 streams and pending GS work
separately. Its `path1_retired` batch emits EOP on the last triangle and omits
FINISH. A forced GS FINISH remains before texture/CLUT overwrite, after late
numeric/capacity rejection, and before the final buffer flip. The final and
rejection drains also retain the full GIF-idle guard; a timeout quarantines the
renderer. Destruction has a pending-work backstop.

`world_path3_stream_retirements` counts EOP-only handoffs;
`world_path3_handoffs` counts forced completion handoffs;
`world_path3_empty_skips` counts handoffs requiring neither submission; and
`world_gs_finishes` counts completion fences that retire pending direct draws.
The latter is not a count of every GS FINISH used for resource uploads. The VIF
packet sends fresh state tags and compact colors every batch; it does not assume
output memory survived an intervening use of the optional projection kernel.

Geometry-first diagnostics count preparation calls (`world_prepared_batches`),
ready results (`world_ready_batches`), attempted emissions (`world_emit_calls`)
and ready residency refusals (`world_residency_fallback_batches`).
`world_skipped_preload_materials` counts textured source-face preload requests
actually omitted before clipping. It does not count avoided uploads or net cache
calls: emitted fans still use normal lookup. Existing hit/upload/byte counters
report the work that actually occurred. Fused mode leaves the new counters zero.

## Evidence and limits

The early-rejection authored generation passed **24,371,200 literal pixel-word
comparisons** and **32 no-flip rejections**. Six new pairs compare `clip_each`
and `reject_outside` in all three submission modes, preserving all 34 previous
Display counters and VU batch, corner and DMA counts. Each outside case rejects
fifteen source faces; the mixed inside, crossing and degenerate case rejects
none. Late capacity and rejected/scalar numerical failures follow an already
drawn batch, drain without flipping, and recover in every submission mode.
Invalid mode admission also rejects without a flip. The prior material, resource,
token and async lifecycle tests remain in this generation. Its report is
`build/ps2-world-trivial-display-ee-001/runtime-validation.json`, SHA-256
`5d956e3a37dfde5dc203df80a6b86d6274c669536bf1a0804d1581df17b0ff8c`.
The report binds the executed ELF, archived compiled sources, configuration and
closed runtime. It is correctness evidence, not a fixture-speed measurement.

The paired actual-scene run uses synchronous classification and memoized
materials in both paths over **601 native updates**. The fallback argument is
the only rendering-mode difference. Both emit 885,445 triangles, and all
**4,874,240 literal pixel words** from seventeen sampled frame pairs match.
The new path skips setup and clipping for **422,477** validated outside source
faces. All 34 previous counters match on 593 stable warm pairs; eight
cold/transition pairs remain separately reported because the modes share a cache
and alternate execution order. VU batch, corner and DMA totals are unchanged.
Mean draw work falls from **15.452 ms to 14.392 ms**, a **1.060 ms** reduction on
pinned Play. This is draw work, excluding measured vsync and readback, rather
than complete application cadence, physical-console timing or universal pixel
equality. Report: `build/ps2-world-reject-scene-ee-001/runtime-validation.json`,
SHA-256 `8dd64e0e0519bae0ea43319310be8889df12824f4194534f0021c70c1265a40d`.

The following material-memo results retain their earlier identities, before
the early-rejection gate.
The material-memo authored generation passed **20,930,560 literal pixel-word
comparisons** and **25 no-flip rejections**. It compares memoized and ordinary
resolution in all three world submission modes, including repeated pointers,
different pointers with equal content, verified within-batch hash collisions,
cold cache invalidation, and late rejection followed by recovery. Two changed
caller descriptor/palette inputs produce new immutable prepared snapshots and
visibly different reference pixels; the test never mutates a live plan. Each
submission mode also reproduces 264 texture evictions and 288 CLUT uploads.
Warm pairs preserve all seventeen resource/path counters checked by the fixture.
The retained async/token and completion cases still execute in this generation.
Its report is `build/ps2-world-material-display-ee-001/runtime-validation.json`,
SHA-256 `a4c802a123e1a4e4bb8ab00fc2ee7b0da4a6893566ceae9a2e31e76ab5aaadb8`,
with executed ELF, source archive, configuration and closed runtime identities.

The known-slot texture-touch helper separately matches complete cache state
against normal acquisition for **8,323 requests** and age-wrap cases, including
non-mutating rejection. Optimized and sanitizer runs pass. Its report is
`build/ps2-world-material-touch-001/validation.json`, SHA-256
`59af1df7ebbfa701ac20749afefae680108729c6f73ce4bf1cbea1a219e85bec`.
Palette age/touch equivalence is source-reviewed; the GS fixture checks counters
and pixels rather than reading internal palette ages directly.

The actual-scene comparison submits the same plan through synchronous
classification with each material mode over **601 native updates**. Both emit
885,445 triangles, and all **4,874,240 literal pixel words** from seventeen
sampled frame pairs match. Mean draw work falls from **18.478 ms to 15.512 ms**,
a **2.966 ms** reduction. All 29 original counters match on 593 stable warm
pairs; eight cold/transition pairs are reported separately because the modes
share a cache and alternate execution order. The memo records 537,496 preload
hits, 537,032 state reuses and nineteen epoch invalidations. This is measured
draw work on pinned Play, not complete application cadence, avoided uploads,
physical-console performance or universal framebuffer equality. The report is
`build/ps2-world-material-scene-ee-001/runtime-validation.json`, SHA-256
`a1f3050a806f0d5c67ce7188da07ccd23da6f9b736e03c9e153785cbb8fd23e9`.

The following async results retain their earlier, non-memoized identities.
The async authored generation passed **12,328,960 literal pixel-word
comparisons**, 27 async lifecycle checks in addition to the 24 retained token
checks, and fifteen no-flip rejections. Frame, fused, synchronous classified and
async classified output matched for the authored mixed-material, clipping,
checker and depth cases. The tests also cover pending caller-scratch reuse,
wrong/short finish retry, synchronous BUSY rejection, two occupied banks and
third-preparation rejection, one-bank release preserving the other, explicit
prepare/emit drain, cross-instance lookahead, cold offscreen materials, and
resource eviction. Both deferred Display emission and explicit full-FINISH
emission compare against completed framebuffer pixels.

The report is `build/ps2-world-async-display-ee-001/runtime-validation-final.json`,
SHA-256 `d9e7cbce9c48f66975043d8d0876cb17058cbd108ce6366bc761b06501b3f2ee`.
It binds the executed ELF and its archived 23-file compiled source closure.
The driver separately passed strict EE compilation, two portable tests and
146,704 ASan/UBSan checks; its source-bound report is
`build/ps2-world-async-driver-001/driver-validation.json`, SHA-256
`d7def7c0c3b8d03e610504f3ccd4921ecaf52351bbf351db32c68d05805d6a92`.
The 90.040-second observation closed with empty stderr. All ten immediate polls
returned already complete, and none returned pending: this Play execution
establishes ordering, ownership and pixel behavior, not measured simultaneous
EE/VU execution. Scheduled lookahead counts alone are not overlap evidence.
Separate actual-scene and full-application measurements follow.

The paired actual-scene run advances 601 native updates with synchronous and
async submissions of the same immutable plan. All **4,874,240 literal color
words** from seventeen sampled frame pairs match. Mean draw work is **18.411 ms**
synchronous and **18.428 ms** async, a measured increase of 0.017 ms. This is
ownership and output evidence, not a speed gain. The report is
`build/ps2-world-async-scene-ee-001/runtime-validation.json`, SHA-256
`a4c84efa581d6bf22001b59d2b961cb0835d8f182d367dda4318a479afb59733`.

Three separate native054 application runs compare the first 660 complete SDK
periods with identical controls and assets. Ordinary Frame averages **29.361
updates/s** and **31.065 ms** of work; synchronous world submission averages
**29.985 updates/s / 25.202 ms**, and async world submission **29.985 updates/s /
25.226 ms**. Work subtracts only the measured vsync/flip interval. The current
sync-to-async comparison checks all 98 sampled records, their complete field
sets and 602 equal non-timing values; the 42 scheduling-counter values are
validated separately. Historical native052 comparisons remain separate.
Every measured period exceeds the 60 Hz budget. These pinned Play measurements
establish neither physical-console timing nor a measured async speedup. The
report binds all three ELFs, configurations, logs, assets and the final source
archive: `build/ps2-world-async-final-001/cadence-validation.json`, SHA-256
`28976d6eabf7d40e3ed9b258811cd59544ce75f8ebb050ab2111bd120784699a`.

Owner destruction with pending work is source-reviewed; this fixture executes
explicit drains instead. It does not inject transport timeouts, force ticket
exhaustion or prove physical-console behavior. The following results retain the
prior synchronous generation's separate identity.

The original authored fixture is
[`native_world_display_fixture.cpp`](../tests/native_world_display_fixture.cpp).
The sealed geometry-first generation described below compares Frame, fused and
classified pixels, and checks cold offscreen textures, prepared-token rejection/invalidation, both
completion modes, late capacity/numeric failure and resource eviction between
prepare and emit. The source-bound Play run passed **9,175,040 literal pixel-word
comparisons**, 24 token checks and thirteen no-flip rejections. Each of four
Frame/fused/classified triplets has one direct and two fallback batches with
exact framebuffer equality. An authored batch with one visible flat face and
fifteen offscreen textured faces uploads fifteen textures in fused mode and none
in classify-first mode, with the same pixels.

The ready-then-capacity-rejected case retrieves retained clips, drains the prior
direct batch and recovers; numerical rejection follows the same no-flip contract.
The classified eviction case preserves prepared geometry across 264 texture
evictions and 288 CLUT uploads. The separate `gs_finished` emit compares literally
against Frame after verifying that the cleared target differs from the baseline;
it consumes copied handles and leaves the framebuffer selection unchanged.
The report is `build/ps2-world-classified-display-ee-001/runtime-validation.json`,
SHA-256 `795311872bd26eff122989dbd4aaec1a107ce21c96368163618dd53f4c251c7c`.

No natural ready-but-nonresident failure occurred in this run; that defensive
branch is source-reviewed, while ready cancellation is exercised through capacity
refusal. The fixture establishes authored pixels and lifecycle behavior, not
actual-scene speed, universal pixel equality, transport-fault injection or
physical PS2 execution. Owner-address reuse and ticket exhaustion are reviewed
in the implementation; the fixture does not exercise those cases.

A separate actual-scene comparison advances 601 native updates and submits one
immutable plan per update to both world modes, alternating their order. Each
mode emits 885,445 triangles through 49,358 direct and 33,706 fallback batches.
All per-frame geometry counts and **4,874,240 literal color words** across
seventeen sampled frame pairs agree. This compares the two world modes; it does
not resolve the earlier 66 differing words between world rendering and Frame.

Mean draw work, excluding measured vsync and readback, falls from **27.072 ms**
for fused submission to **18.926 ms** for geometry-first submission, a 30.09%
reduction. Shared scene planning costs 4.119 ms per update and is measured
separately. The classified path omits 525,887 textured source-face preload
requests. Both modes share a warm resource cache, so their differing upload
counts do not establish independently cold-cache savings for this scene.
Whole-application cadence is a separate measurement. Report:
`build/ps2-world-classify-scene-ee-001/runtime-validation.json`, SHA-256
`36705f11ee6cd1f121917c53844cfa8e7cc2d641e0d3c6ec733ae156e4c3d733`.

The historical resource-first proofs follow.
Revision004 compared four complete Frame/PATH3 and world/PATH1 framebuffer pairs
and four recovery images: **2,293,760 literal pixel words**, four direct batches,
eight clipping/degenerate fallback batches, and eight no-flip rejections passed.
The source-bound private report is
`build/ps2-world-display-ee-004/runtime-validation.json`, SHA-256
`09fc966e6719cd8c4db5e5fc5decfc0971fdef066b5510397340ca3a434135d4`.
Its readback-only Display copy changes the ignored destination base of each
local-to-host transfer to obtain fresh snapshots from pinned Play; source VRAM
and readback extents are unchanged.

The earlier authored001/002 fixtures used a repeated index, which model
admission correctly rejected. They are preserved as invalid fixture attempts,
not renderer evidence. Revision004 uses distinct indices with coincident
positions to test zero-area geometry. Its historical report is not reused as
evidence for later handoff or compact-upload changes.

Revision005 includes explicit PATH3 dirty tracking and the compact VIF state/color
upload. It passed **2,580,480 literal pixel-word comparisons**, six direct batches,
eight fallback batches and the same eight no-flip rejections. The added pair of
all-inside instances recorded two direct batches, one necessary PATH3 handoff and
two skipped empty handoffs, with exact framebuffer equality. Its report is
`build/ps2-world-display-ee-005/runtime-validation.json`, SHA-256
`9a285ae44d6e385ccb6051c8c46e2d331cb33e43d3129e4d65d0b46570c5c645`.

Revision006 validates deferred GS completion before the EOP-only PATH3 change:
**4,014,080 literal pixel-word comparisons**, 24 direct batches, eight fallback
batches and ten no-flip rejections. Late capacity and numerical failures drain
an already submitted direct batch and recover. A resident two-batch pair requires
one final GS completion; a separate case exercises 264 texture evictions and
288 CLUT uploads after pending draws. Its report is
`build/ps2-world-display-ee-006/runtime-validation.json`, SHA-256
`aed1d4291878853be62833138b22bd9e8c7433e97641899a3b0a1b75b4c79253`.

Revision008 validates the EOP-only PATH3 handoff with **4,300,800 literal
pixel-word comparisons**, 26 direct batches, ten fallback batches and ten
no-flip rejections. A resident pair performs one stream retirement and one final
GS completion. The direct → two clipped batches → direct sequence performs
three stream retirements (clear and two queued fan streams) and one final GS
completion, with exact framebuffer equality. Late-failure recovery and resource
eviction checks remain. Its report is
`build/ps2-world-display-ee-008/runtime-validation.json`, SHA-256
`832422d59e5a48165c61b2b5edc6ddc518f4f0f0ae746418fa4ebe71e1cafa6c`.
Revision007's preserved failure was a fixture assertion expecting two rather
than three stream retirements; revision008 changes that assertion and adds a
counter print, with unchanged production Display code.

Revision009 revalidates the promoted primitive-writer specialization and hot
texture-lookup inlining. It again passes **4,300,800 literal pixel comparisons**,
26 direct batches, ten fallback batches and ten no-flip rejections, including
deferred resource eviction and recovery. Its complete counters and fingerprints
match008; this cross-run agreement is distinct from the literal comparisons
performed within each run. Report:
`build/ps2-world-display-ee-009/runtime-validation.json`, SHA-256
`24137084b2a0280110164417cb6bc44e09e0d4524ce509beeaa7bc6a0258a788`.

These checks do not establish universal pixel equality, physical PS2 execution,
transfer overlap or 60 Hz gameplay. Actual-scene clipping/admission differences
and total application cadence must be measured separately.

The final revision005 scene comparison covers 601 native updates and 885,445
emitted triangles per path. Of 83,064 batches, 49,358 draw directly and 33,706 use
ordered clipping fallback. Seventeen sampled frames compare 4,874,240 GS words;
66 differ in RGB, with maximum eight-bit channel difference 134. Per-frame
triangle counts match; this is not a complete ordered-triangle identity proof.
The source-bound report is
`build/ps2-world-pipeline-scene-ee-005/runtime-validation.json`, SHA-256
`d4c21be3d1efb60c8d3223c84205643c5dcabe52bd10129c54afe36a7cc509d2`.

Mean render work is **34.452 ms** for the world path versus **32.317 ms** for
Frame in that alternating comparison, excluding vsync and readback. The final
EOP-only handoff changes the preceding candidate by only 0.004 ms; it does not
establish a material speed improvement. That candidate reduced scene preparation
but increased draw cost and stayed explicit opt-in. These timings share warm
renderer resources and are not full-game cadence or independently cold-cache
measurements.
