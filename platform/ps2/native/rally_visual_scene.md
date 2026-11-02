# Native textured vehicle presentation

`RallyVisualScene` connects explicit native render poses to the recovered chase
view and full-car geometric transforms. It owns native model/material assets and
uses the shared `WorldScene` clip/project path and Display's indexed GS renderer.
It does not execute a guest processor, replay projected triangles, read later
motion samples, advance gameplay, or reinterpret a `StudyScene` matrix as world
coordinates. Gameplay remains the caller's native vehicle update.

The seven ordered roles are fixed course geometry, shadow, body, and four
attachments in their recovered descriptor order. A role selects one of at most
seven owned model assets. Multiple roles can explicitly select the same asset;
four identical wheel models therefore require one prepared owner. Identical
CRCs alone never establish sharing. The two body submissions may be merged
offline only when their proven model-to-view transforms agree.

Each update computes the recovered camera `C` from the supplied pose and retained
chase history. The course instance receives `C`; the body, shadow and attachments
receive the exact matrices from `build_car_render_bridge(pose, C)`. Conversion
to column-major engine matrices copies columns and translation directly, without
another rounded composition. One common perspective matrix follows them all.
The existing render path uses screen coordinates
`x = 248 + focus_x * view_x / view_z` and
`y = 192 - focus_y * view_y / view_z`: source center `[248,320]` plus CRTC Y offset
128 gives that logical center. Thus positive focus Y is retained in clip space.
The recovered camera currently supplies focus 300 on both axes.

Near and far Z are explicit native presentation configuration, not a claim to
reproduce the original clipping planes or polygon priority. They must satisfy
`1/1024 <= near < far <= 65536`, with bounded projection coefficients. The
renderer uses its native reverse-depth test. The fixed exported course is a
visual neighborhood; it is not full-course streaming. CLUTs contain the exported
lighting result, so dynamic relighting is not recovered here. No frozen HUD is
accepted by this asset format. A uniform source background can be represented
by the config's opaque clear color.

`load(directory, expected_initial_game_frame, error)` reads `view.bin` and the
fixed names `model0.bin` through `model6.bin`. It verifies exact file length and
CRC on the same byte buffer passed to the bounded native model decoder. Paths
come only from the caller's bounded directory plus fixed leaf names. No source
addresses, captured matrices or executable instructions enter these models.
`configure(config, models, error)` provides the corresponding typed owner-data
entry for authored tests and conversion tools.

The first pose after load or reset must match all 22 configured raw publication
words exactly. It is the output of the first native vehicle update at the given
game frame, not a manually reconstructed pose from checkpoint state. This binds
camera history and visual assets to the chosen starting update. Later updates
consume only new native poses. The camera history, frame and successful timing
statistics publish together; any pose/domain/capacity failure preserves the
last successful state and frame. `reset()` restores the initial history and
pose guard, hides the old frame and clears timing statistics without allocation.
All calls are serialized with rendering, and returned frame views expire on
successful replacement/update, reset or destruction.

Model assignment deep-copies prepared textures and materials. Temporary decoder
storage is released after each individual asset is prepared. Existing group
partitions are retained; otherwise consecutive groups of at most 32 faces are
created without changing face order. Bounds are then owned and validated by
`PreparedWorldModel`. Update uses seven stack instances, no dynamic allocation,
and forwards the selected numeric mode, profiling clock and transform provider.
The default is native float. Actual VU1 execution still requires the caller's
installed provider; selecting the numeric mode alone does not offload work.

`enable_draw_plan(error)` allocates optional ordered plan banks after loading.
It retains the same recovered camera/body equations and history; subsequent
`update()` calls publish `draw_plan()` instead of an assembled `Frame`. The
course/body/attachment instance order and authored facing remain unchanged.
This path always uses the planner's native-float bounds and does not invoke an
installed transform/projection provider. The downstream renderer owns world
transformation, projection and clipping, including final numeric and capacity
rejection. Calling `frame()` in this mode returns an empty frame.

Enabling is atomic on allocation failure and idempotent after success. It hides
the previously published frame without resetting or advancing camera history.
`reset()` retains the opt-in and allocations while restoring the first-pose
check; successful `load()` or `configure()` discards the opt-in. Camera history
commits only after successful planning, as it previously did after successful
frame preparation. Plan views share the adapter's serialized publication
lifetime. `persistent_bytes()` includes both ordinary frame storage and any
optional plan banks; enabling does not silently free the ordinary storage.
The extended fixture passes 210 checks under ASan/UBSan, including allocation
rollback, eight independently checked camera-history updates, no planning
allocation/provider calls, failed-update history preservation, reset and
reconfiguration. Its current report is
`build/ps2-world-plan-validation-001/sanitizer-result.json`.

The cumulative scene is limited to 65,536 declared vertices and 32,768 source
faces, counting repeated role instances. Output is capped at 16,384 triangles;
clipping expansion beyond that cap rejects the frame. Assets are each subject
to the native model and prepared texture limits. `persistent_bytes()` includes
the adapter, prepared snapshots and engine storage. Initial loading/replacement
also needs one bounded file buffer, one decoded model and the old owned scene
until commit. No promise is made that all independent maximum asset limits fit
simultaneously in 32 MiB; the actual package footprint must be measured.

The fixed config is 320 bytes, little endian. All unspecified bytes are zero.

| Offset | Value |
| --- | --- |
| 0 | Eight bytes `RLYVIEW1` |
| 8, 12, 16 | u32 version 1, total bytes 320, payload bytes 288 |
| 20 | IEEE CRC32 of bytes 32–319 |
| 24, 28 | Initial game frame, reserved zero |
| 32 | 22 raw u32 pose words: position 3, motion 3, orientation 3, attributes 11, flags, progress |
| 120, 124 | Binary32 initial yaw history and distance adjustment |
| 128 | Four binary32 XYZ attachment offsets, 48 bytes |
| 176, 180 | Binary32 native near and far Z |
| 184, 188 | u32 opaque ARGB clear color and asset count 1–7 |
| 192 | Seven u32 asset indices in role order |
| 220 | Seven pairs of u32 complete-file byte count and CRC32 |
| 276 | 44 reserved zero bytes |

Every declared asset must be referenced by a role; unused identity entries must
be zero. No count or path is inferred from the directory. The expected frame is
checked independently of the payload CRC. The CRC detects corruption/mismatched
packages; it is not an authenticity or adversarial-security mechanism.

The original host fixture passes 94 checks under ASan/UBSan, bound in
`build/ps2-rally-visual-001/validation.json`. It checks malformed/short configs,
CRC and frame binding, malformed model identities, exact initial-pose rejection,
native-pose-driven movement, independent screen-center/focus projection, provider
forwarding, copied source lifetime, reset, allocation failure and transactional
camera history across both pose and engine-domain rejection. It verifies no
allocations during updates and reset. The portable runner is
`tests/test_ps2_rally_visual_scene.py`. These are original synthetic geometry
tests, not a new GS or physical-console execution. Original CPU/firmware camera
and car-transform comparisons belong to their separate recovered components;
runtime textured scene validation is recorded by the integration lane.
