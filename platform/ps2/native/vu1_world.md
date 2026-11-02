# Ordered world geometry through VU1 and GS

`Display::present_world` consumes a `WorldDrawPlan` without first building an
EE `Triangle` array. It is a separate, explicit renderer path. The ordinary
`Display::present(Frame)` and transform-only WorldScene provider remain available.
The development application selects the new path with
`--drive-world-vu CHECKPOINT COURSE VISUAL_DIRECTORY`; it does not change the
native vehicle arithmetic or load a reference runtime.

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
and mixed materials. Display preloads their textures and palettes, then rechecks
every texture identity and palette's complete content. An eviction during
preloading makes the batch use ordered scalar fallback. The next resource
overwrite waits for GS completion of preceding draws. Neither plan consumption nor drawing
allocates memory per frame.

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

Submission remains synchronous at each GIF path handoff. Display terminates
queued PATH3 state and fallback primitives with an explicit zero-loop EOP tag,
then waits for bounded D2 completion and the full GIF-idle guard before PATH1.
The delimiter is necessary because SDK clear/state packets can end with EOP
unset. This retires the packet stream without waiting for GS rendering to finish.
The VIF owner waits for bounded DMA/FLUSH/MARK and full GIF-idle completion before
reading the kernel's count/sequence status or reusing a VU bank. Two disjoint VU
data banks do not imply transfer overlap.

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

## Evidence and limits

The original authored fixture is
[`native_world_display_fixture.cpp`](../tests/native_world_display_fixture.cpp).
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
establish a material speed improvement. The world path reduces scene preparation
but increases draw cost, so it remains explicit opt-in. These timings share warm
renderer resources and are not full-game cadence or independently cold-cache
measurements.
