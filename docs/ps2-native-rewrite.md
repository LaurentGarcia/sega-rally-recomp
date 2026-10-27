# Native PS2 rewrite

The target is a native reimplementation of the game for the PS2 architecture.
The translated arcade-machine player is retained as an offline behavioral
reference and a separate development target. It is not the new game's runtime.

## Executable boundary

`build_ps2.py build --target native-port` has a fixed source inventory and checks
its transitive project includes before compilation. The native target excludes:

- Generated i960, TGP and 68000 programs or processor interpreters.
- Guest registers, a Model 2 memory bus, device registers and board scheduling.
- SCSP/DSP emulation and guest-clock-driven audio generation.
- The reference `Model2AMachine`, runtime renderer and `Model2Frame` adapters.

The driver also rejects extra core manifests, checks defined ELF symbols for
known arcade-core entry points, and rejects source changes during compilation.
These are reproducible dependency checks, not a general security proof.

Runtime code uses typed game state and PS2-native graphics/audio/input data.
Asset conversion and original-behavior comparisons happen offline. Native
textures, mesh buffers, sound samples and music remain user-supplied/private;
removing machine emulation does not make the original game's assets original
project content.

## Reusable engine boundary

The [PS2 engine architecture](ps2-engine-architecture.md) defines shared services
for additional games. The build checks dependency direction for every project
source/header: engine code cannot import Rally rules, the captured-scene study
or offline recovery tools. The `engine-demo` target uses the same engine inventory
with original content and no game-specific sources. Build reports record both
applications' shared engine source identity.

## Architecture

```text
DualShock -> native input -> race / car / AI state
                                  |
                            camera + geometry
                                  |
                         VU work -> GS packets -> GIF -> GS

race events -> native mixer / sample voices -> IOP -> SPU2
```

The EE owns game logic and asset lifetime. VU work is explicit geometry work,
with tested rounding and bounds. GS receives ordered primitives and native
texture/material descriptions. The IOP streams bounded audio without running
an arcade sound processor. Frame pacing is a native game-loop responsibility,
not execution of guest instructions until an emulated vblank.

The open SDK, indexed GS textures, CLUT management, DMA ownership, controller
adapter and IOP PCM output can be reused. Their hardware decisions are recorded
in [PS2 hardware notes](ps2-hardware-notes.md). References to Model 2 registers or
RAM decoding must stay in offline conversion/reference tools.

## Recovery and validation order

1. Build and execute the independent native platform target with explicitly
   original diagnostic content; prove the forbidden runtime is absent.
2. Complete the main player's entire state update as the next integration unit.
   Write ordinary C++ against persistent typed state, explicit inputs and
   immutable course/tuning assets. Use recovered functions and focused
   comparisons to diagnose differences inside that update; another isolated
   helper is not the exit criterion.
3. Compare consecutive complete updates against the original from one initial
   state and a control sequence, then connect their output to the native
   application. Recover the course/collision inputs rather than substituting
   generic vehicle physics or captured intermediate results.
4. Migrate race progression, AI, cameras, menus, rendering assets and audio
   events; connect the recovered systems into the native loop.
5. Compare complete courses, restarts and input replays, then validate memory,
   timing, controllers and audio on a physical PS2.

The native target now has a developer vehicle loop alongside the platform
bootstrap and unchanged static study. It advances recovered game state from
one validated initial checkpoint, reads the pad, and renders a body against a
stationary collision course. This is not the complete arcade game: the model
bridge, camera and flat materials remain diagnostic; race initialization,
menus, AI, game audio and full-course validation remain open.

### Continuous vehicle milestone

The [complete main-player update](../platform/ps2/native/main_vehicle_update.md)
matches **601 consecutive original updates from one initial seed**: 96,761
canonical state comparisons, 13,222 published pose words and 458 ordered sound
requests. The same checker passes when compiled for EE and executed in Play,
including byte-identical exported checkpoints. ASan/UBSan and six malformed-
fixture checks pass. Expected original
outputs are comparison data only; the next update uses its own native state.
Immutable course directions are selected using the previous native segment.
The original processor/device runtime remains confined to offline observation.

The application now calls this update and consumes the changing pose. Its
bounded version-3 course package and 752-byte developer checkpoint carry no
runtime addresses. The checkpoint is a development start state, not a recovered
race initializer. Each subsequent input comes from the pad; no recorded motion,
forces or contact results are supplied.

The integrated EE driver advances the car under neutral controls in Play.
The production renderer uses binary32 geometry and both vector units. The
equivalent initial samples measured approximately **12.5 ms for the native
update**, **27.6 ms for geometry preparation** and **52.1 ms combined
update/presentation**, about 19 updates/sec. Geometry is 13.6 times faster than
the preceding native 026 build's 375.7 ms. The intervening exact-cache path
measured 224.3 ms; its arithmetic remains available for comparison.
These are emulator timer observations, not physical-console performance or a
full-race benchmark. Display refresh shown by the emulator is not game speed.

Conservative course-section rejection, exact clipping classification, shared
vertex reuse and fewer bounds conversions reduce that geometry cost. The
[VU1 path](../platform/ps2/vu1_transform.md) now processes bounded model/view
transform batches, while VU0 performs final GS coordinate packing. The normal
path uses float bounds, clipping and projection on EE, with no binary64 guard
or clipped-triangle restoration. A separate guarded comparison mode restores
exact coordinates before clipping. The initial guarded integration was slower
than the exact reference because per-vertex numerical guards cost about 49 ms;
preparing those guards helped, but removing reference arithmetic from normal
rendering produced the larger gain. The recovered game calculations are
unchanged. Transfers and vector-unit work are still serialized.

Complete-frame replay of actual VU output covers all 601 recorded poses and
1,364,846 transformed vertices. Against the binary64 reference, the independent
host raster differs at 305 of 114,468,864 color pixels, at most four per frame,
with 14 coverage differences. Triangle topology differs in 173 frames; native
presentation is not bit-identical. This comparison uses host clipping and
rasterization, not GS readback or physical-console validation. Detailed evidence
and numerical limits are in the [world-scene contract](../platform/ps2/native/world_scene.md).

A separate GS correction sends each solid triangle corner's own depth rather
than using a 2D helper's single depth. All 1,536 readback samples pass in the
extended constant/sloped depth fixture, and the diagnostic body is visible
without the previous false road occlusion.

Remaining integration work includes further measured geometry optimization,
working interactive pad input, verified update cadence and the model-local/world
bridge before calling the view faithful. The measured runs had no connected pad;
the car advanced from its moving checkpoint with neutral inputs. Complete races,
AI, recovered cameras, textured presentation, game audio and physical-console
validation remain later work.

### Developer vehicle test

Build `native-port` as below. Supply these private files on the loader's
`host:` filesystem:

- `native-driving.bin`: a validated developer checkpoint exported by the
  whole-update comparison after every comparison passes.
- `native-course.bin`: its matching version-3 course/tuning package.
- `native-body.bin`: the bounded model-local body exported by
  `platform/ps2/tools/export_rally_body.py`.

The application checks the checkpoint's course selector and asset CRC. An
explicit loader command also accepts `--drive CHECKPOINT COURSE BODY`; each
path must use `host:` or `mass:`. Without a developer checkpoint it retains
the existing static-study/bootstrap selection.

The driver requests native float/VU1 rendering by default. Use `--drive-ee` with
the same three paths for the exact scalar comparison, or `--drive-guarded` for
guarded VU transforms with reference clipping. VU startup failure keeps the
selected scalar mode, and transport retirement is logged before disabling offload.
The application establishes DMA master enable before initializing the display.

Cross accelerates, Square brakes, and the left stick or directional buttons
steer. L1/R1 select gears only when the checkpoint enables manual selection.
Start restores the initial checkpoint; Select+Start toggles the debug page.
Triangle cycles native float/VU1, exact EE and guarded VU1 presentation. The page and serial log
separate update, geometry and presentation timing; counters distinguish
submitted/accepted VU batches, corners actually drawn, clipping fallbacks and
VU0-packed vertices. No simultaneous VU0/VU1 execution is claimed.
Game sound requests are generated but playback is not yet connected in this
mode. These controls are implementation mappings; physical pad validation is
still required.

## Recovered components

| Component | Native contract | Private original-program comparison |
| --- | --- | --- |
| [Race progress counters](../platform/ps2/native/race_progress.md) | Positive-only timer decrement, two wrapping counters and two bounded counter banks | 32,768 cases; 735,250 original instructions |
| [Car pose publication](../platform/ps2/native/car_pose.md) | Copy raw pose values and merge observed persistent flags into a distinct render state | 65,536 cases; 3,670,016 original instructions |
| [Bounded angle fold](../platform/ps2/native/angle_fold.md) | One correction around ±pi within a specified finite domain, using exact integer arithmetic and explicit final rounding | 16,844,326 cases; 328,030,729 original instructions |
| [Orientation finalization](../platform/ps2/native/orientation.md) | Fold all three channels, then bound first/third to signed pi/8; atomic rejection outside the recovered domain | 294,300 evaluations, 294,260 unique triples; 25,111,815 original instructions |
| [Planar direction blend](../platform/ps2/native/direction_blend.md) | Explicit rounded dot/weight/interpolation for bounded component pairs; no EE floating arithmetic dependency | 305,824 cases; 5,607,871 original instructions |
| [Contact neighborhood](../platform/ps2/native/contact_neighborhood.md) | Ordered cyclic indices from an explicit count and signed segment; no intersection or forces | 524,288 query-preparation cases; eight separate firmware count queries |
| [Alternate orientation](../platform/ps2/native/alternate_orientation.md) | Candidate folding, wrapped differences and separately rounded smoothing/step rules within an explicit domain | 93,977 evaluations including all 34 sampled live states |
| [Projected contact predicate](../platform/ps2/native/collision_query_recovery.md) | Exact rounded X/Z edge signs for three or four vertices, including signed-zero behavior | 100,774 cases; 5,791,705 original firmware instructions |
| [Course-contact search](../platform/ps2/native/collision_query_recovery.md) | Current/next/previous segment order, first matching polygon and exact copied payload | 12,323 synthetic searches, eight retained result slots and all 34 sampled live replies |
| [Cabinet steering byte](../platform/ps2/native/cabinet_steering.md) | Center, saturate and convert all 256 byte values with exact result bits | Exhaustive 256-byte comparison and 32 sampled live calls |
| [Cabinet steering filter](../platform/ps2/native/cabinet_steering_filter.md) | Stateful rounded input filter and sign-flipped result, including gradual underflow | 68,160 evaluations including 18,432 sequence updates and 32 live calls |
| [Main-car pose publication](../platform/ps2/native/main_car_pose.md) | Return an independent render value containing 22 unchanged words; explicit external state and no flag merge | 77,208 distinct inputs; 3,242,736 original instructions |
| [Rounded cross product](../platform/ps2/native/rounded_cross_product.md) | Three bounded, separately rounded product differences with alias-safe atomic output | 110,084 evaluations, including all 36 captured calls; 2,642,016 original firmware instructions |
| [Affine point transform](../platform/ps2/native/rounded_affine_transform.md) | Explicit matrix columns and translation-first point transformation; separately rounded products and sums | 84,273 evaluations; 2,696,736 original firmware instructions |
| [Contact-result interpretation](../platform/ps2/native/contact_result.md) | Miss/plane/edge status, bounded gap arithmetic and an explicit private normal lookup asset | 40,530 evaluations, including 138 captured replies; original CPU and normalization firmware |
| [Contact-direction normalization](../platform/ps2/native/contact_normalization.md) | Separately rounded squared length, exact root/division and tiny-value fallback | 59,925 evaluations, including all 13 captured pairs; 1,416,524 original instructions |
| [Recovered scalar metric](../platform/ps2/native/recovered_scalar_metric.md) | Explicit interleaved point mapping and the pinned firmware's rounded scalar approximation | 156,680 evaluations, including 146,772 accepted results and all 36 captured pairs |
| [Contact aggregation](../platform/ps2/native/contact_aggregate.md) | Signed accumulation, active response updates and conditional integer feedback | 25,764 scalar steps and 18,409 state updates, including 36 live steps and nine live updates |
| [Contact-point preparation](../platform/ps2/native/contact_point_preparation.md) | Recovered orientation polynomial, ordered matrix construction and four transformed/query points; no external trig table | 49,321 evaluations, including all 26 captured preparations; 46,700,153 original firmware instructions |
| [Per-contact response](../platform/ps2/native/contact_response.md) | Two cross products, normalization, rounded scalar gain, response and selected scalar | 18,536 evaluations, 18,464 accepted results, including all 104 captured responses |
| [Post-query response pass](../platform/ps2/native/contact_pass_response.md) | Four responses, mask and gate, cross/metric accumulation and atomic car-state update | 4,250 passes including all 26 captured passes; 2,128,380 CPU and 615,149 geometry instructions |
| [Candidate contact pass](../platform/ps2/native/candidate_contact_pass.md) | Candidate pose and private course/normal/local-point assets through search and complete response | 26 captured passes, 104 contacts and nine active aggregates; 858 final state/contact words and 312 offset words match |
| [Main-candidate preparation](../platform/ps2/native/main_candidate.md) | Pending-middle clamp, recovered metric gate, conditional residual/state consumption and separately rounded candidate triples | 17,070 evaluations, 16,341 accepted original comparisons including all 26 live states; 729 explicit native-domain rejections |
| [Main contact-slot update](../platform/ps2/native/main_contact_slot.md) | One of four slots: packed type/mask, history, response, metric and table-based lateral value | 25,852 synthetic cases plus all 104 captured calls, including 99 active calls; 2,393,780 CPU and 504,364 geometry instructions |
| [Paired balance](../platform/ps2/native/paired_balance.md) | Two pair sums and signed balances with separate binary64 product/division rounding; zero/subnormal sums supported | 101,130 synthetic cases plus all 26 captured pairs; 4,345,340 original instructions |
| [Vehicle parameter selection](../platform/ps2/native/paired_balance.md#parameter-selection) | Highest enabled bit among 9–1, otherwise index 0; raw copy of an explicit 13-word record | 6,144 synthetic selections plus all 26 captured selections; 99,140 original instructions |
| [Segment selection](../platform/ps2/native/segment_selection.md) | Ordered search of explicit immutable corner/vertex records; signed progression and original hit/miss encoding | 3,634 synthetic cases plus all 26 captured calls; complete original search and edge firmware |
| [Main-helper geometry tail](../platform/ps2/native/main_balance_geometry.md) | Separately rounded scalar combination, recovered polynomial rotations, two triple updates and selected-index publication | 20,480 synthetic cases plus all 26 captured calls; 182 live output words match |
| [Joined main candidate/contact step](../platform/ps2/native/main_contact_step.md) | Candidate preparation followed by the complete private-asset contact pass, with atomic state commit | All 26 captured calls, including nine active responses; 16 independently calculated authored controls and four atomic rejection cases |

The first four components have portable boundary tests, sanitizer checks and standalone
EE fixtures executed successfully in Play: 121 progress boundary/rejection
checks plus repeated updates, and all 65,536 pose flag pairs with raw-value
and repeated-publication checks. The angle fixture passed 36,262 EE checks;
the exhaustive folded interval is covered by its host original-program oracle.
Orientation finalization passed 27,031 EE checks; its axis meanings and live
caller domain remain unproven. It recovers a tail region of the integrator,
not an additional standalone original function.
The direction blend also passed a 6,032-case EE probe and atomic rejection
tests; the neighborhood passed 1,179,642 EE boundary checks. These execute
standalone native arithmetic in Play, not a connected driving loop.
Alternate orientation passed 3,728 EE cases, including 3,638 accepted inputs,
with separate alias and atomic rejection checks. Projected contact passed
4,870 EE cases and 64 rejection checks; byte steering passed all 256 values.
The complete filter passed 7,168 EE evaluations, including 2,048 stateful
updates, plus 2,560 rejection checks.
Course-contact search passed 192 EE selection cases and ten boundary checks.
Main-car pose publication passed 2,184 EE cases and 48,048 raw-word checks;
its original-program proof is isolated, with no live paired helper capture yet.
Rounded cross product passed 6,208 EE cases plus 180 atomic-rejection checks.
Affine point transformation passed 4,065 EE cases plus 720 atomic-rejection
checks. Both include alias checks and a fused-arithmetic discriminator; the
affine fixture also distinguishes translation-first ordering.
Contact-result interpretation passed 2,440 EE cases, 34 atomic-rejection checks
and three interpolation guards using an original synthetic normal table.
Contact-direction normalization passed 4,064 EE cases and 48 rejection checks.
The scalar metric passed 11,972 EE evaluations, including 11,234 accepted
results, six aliases and 96 rejection checks. Contact aggregation passed 2,240
scalar steps, 912 updates and 33 rejection checks. Contact-point preparation
passed 3,279 EE cases with three input aliases and 114 rejection checks.
Per-contact response passed 1,200 cases, including 1,128 accepted inputs, plus
33 rejection checks. The post-query pass passed 640 cases, inactive-state
preservation and late atomic rejection. The complete candidate pass passed
768 EE authored-course cases, including 512 active and 256 missed-contact
passes, a known-geometry result and six atomic rejections. Each fixture enforces its
expected result fingerprint; execution uses Play 0.72 HLE, not physical hardware.
Main-candidate preparation also passed 1,684 EE evaluations, including 1,641
accepted inputs, and 75 separate atomic rejection checks. Its fingerprint is
`d782474a4f893dc8`; the 43 unsupported probe evaluations are not successful
original-program comparisons. The contact-slot EE fixture passed 2,876 cases,
including 947 active updates, fingerprint `32ef92cd4f0d4583`, plus 316 atomic
rejections and two known controls. Paired balance passed 2,154 EE cases,
fingerprint `1fd698b609c8f960`, plus 24 atomic rejections; parameter selection
passed 3,072 cases, fingerprint `cf0278eb2ab1a5a1`. These are standalone
numerical fixtures in Play 0.72 HLE, not physical-console or gameplay tests.
The balance geometry tail passed 1,504 EE cases, 84 rejection checks and two
controls, fingerprint `3e2a507aab74e9d4`. Its earlier helper dependencies remain
unrecovered; executing that fixture does not establish a complete vehicle update.
Segment selection passed 3,634 EE cases, fingerprint `1c1c549c187f8285`,
55 rejection checks and two controls. The joined candidate/contact step passed
all 16 independently calculated authored controls, fingerprint
`76ae45f11adbe279`, and four atomic rejection cases. These fixtures also run in
Play's HLE environment; physical-console timing remains unmeasured.
The original-program comparisons run on the host against the
owner's verified Revision B image; the reference interpreter and memory
adapters are not part of the native executable. The components are compiled
by the native target but remain unconnected to its diagnostic loop and may
be removed by linker garbage collection. Their bounded contracts do not
establish complete car physics, coordinate conventions, every collision path
or a complete race system.
The [car integration dependency map](../platform/ps2/native/car_integration_recovery.md)
records the remaining data and arithmetic contracts and the alternate route
actually reached by the driving replay. That route has different orientation
smoothing, now recovered separately; the existing three-channel finalizer
cannot stand in for it. Neither complete integrator is recovered merely because
its smaller helpers match. The [main vehicle callback](../platform/ps2/native/main_vehicle_recovery.md)
is a third, separate path that consumes cabinet steering. That callback and its
remaining geometry/force dependencies must be recovered to drive the main car.
Its [four-point candidate-contact pass](../platform/ps2/native/main_contact_pass_recovery.md)
now has a bounded native composition: exact orientation/point preparation,
course search and reply interpretation, per-contact response, and final state
aggregation. The preparation and response operations have separate original
and captured proofs. The complete private-asset composition independently
matches all 26 captured passes, including 104 interpreted contacts, 312 offset
words and 858 final state/contact words. That comparison links only typed
native components and private data, with no original processor or machine
runtime. The full main callback and driving loop remain unconnected, and no
native frame-rate claim follows from these numerical fixtures.

The joined [main contact step](../platform/ps2/native/main_contact_step.md) now
derives the contact pass's candidate and motion inputs directly from the newly
prepared vehicle. Its private comparison uses the exact saved caller and
one-instruction transition to join the two recorded streams, then matches all
26 composed results, including nine active responses. It supplies no captured
intermediate offsets, replies or bases to the native calculation. The final
marker is retained from the prefix under the inspected callee's preservation
contract, and the initial selected scalar is an unused/reset zero; those fields
are explicitly distinguished from directly captured state.

The separate [driving observer](../platform/ps2/tools/driving_capture.md) now
records those routes using the existing cabinet replay. Capture 013 passes all
2,850 enabled/disabled frame checkpoints. It records 1,202 complete alternate
calls and 34 detailed groups, plus 601 complete main-object cabinet-filter calls
with 32 detailed pairs. Callback records and matching published steering values
associate the cabinet input with object `0x500000`; the alternate calls belong
to two different objects, each updating once per frame in the observed window.
Its separate main-contact stream records 601 complete passes and 2,404 complete
query/result pairs, with 26 detailed passes, 1,098 main events and all 104 paired per-contact
responses. It includes all 36 cross operations in the observed window,
13 normalization pairs, 36 scalar metric/accumulation pairs and nine active
aggregate updates. Native result interpretation matches all 104 sampled main
replies and 34 alternate replies, including 13 edge and 125 plane results.
No miss occurred in those samples; miss behavior is covered synthetically.
Capture 012 additionally records 468 main-vehicle events across 26 complete
sampled callbacks, with candidate-preparation boundaries, 104 contact-slot pairs,
26 paired-balance regions and 26 parameter selections. The previous streams are
byte-identical to Capture 011. All 26 candidate-preparation states match the
native and original CPU/geometry operations; the combined comparison executes
1,294,632 CPU and 690,733 geometry instructions. All 26 live prefixes have a
zero pending delta and preserve the existing marker; clamp and active-consumption
branches are covered by synthetic original comparisons. All sampled pair
reductions and record copies
match the native functions and isolated original execution; sampled selection
uses only index 2. All 104 slot calls also match, including four vertical-only
inputs that require the original interpolation expression at zero rounded XZ
squared length. Tiny nonzero XZ inputs whose squares underflow to zero are
covered by additional synthetic controls.
Capture 013 retains every earlier stream and adds 104 records: 26 complete
segment-helper pairs and 26 complete geometry-tail pairs, sampled from 601
calls of each region. Segment selection matches all 26 original results; every
sample hits, so missed searches and signed wrap remain synthetic coverage.
The geometry tail matches both output triples and the published index; three
samples take its negative first-longitudinal branch. These additions preserve
the earlier 26-callback population and do not expand it into a full race proof.
This is bounded host-reference evidence, not independent arcade-board fidelity
or a full-course proof. The complete main-player update and its application
integration are described above; full race flow remains unfinished.

The [hardware study](ps2-architecture-study.md) records the complete local
manual collection, specific constraints and the executed VU1 experiments.
The [native clipping reference](ps2-native-geometry.md) provides bounded
homogeneous clipping and ordered triangle output. Its selected EE checks
pass; it uses software-double arithmetic and remains an explicit diagnostic.
The production float clipper has separate coverage, depth and clipping
comparisons, including complete scenes from all 601 recorded vehicle poses.
The optional [static geometry study](ps2-native-scene.md) connects it to native
mesh transforms and GS submission. That diagnostic camera does not establish
recovered game projection or connect the recovered race components.

## Build and check

The same pinned open PS2dev toolchain as the reference probe is used. Docker
must be available; no game data or translated game source is needed:

```sh
python3 -m unittest discover -s tests -p 'test_ps2*.py' -v
python3 platform/ps2/build_ps2.py smoke --target native-port \
  --output build/ps2-native
python3 platform/ps2/build_ps2.py build --target native-port \
  --output build/ps2-native --jobs 4
```

The output is `build/ps2-native/segarally-ps2-native-port.elf`. The smoke command
checks its source boundary; it does not execute the ELF. The build report
records source hashes, toolchain identity, static memory and symbol checks.

Load the ELF through a PS2 homebrew loader or a PS2 development emulator.
Without an explicit mesh path or `host:native-scene.bin`, it shows the original
platform diagnostic. Select + Start toggles that page; the left stick moves
the checkerboard test geometry and holding Cross submits a quiet original
440 Hz tone. PCM refill follows the IOP queue independently of PAL/NTSC cadence.
The reference-probe `rally.cfg` and arcade asset pack are not used.

An explicitly supplied native mesh selects the [static geometry study](ps2-native-scene.md).
That mode loads model-local vertices and captured affine transforms, applies a
diagnostic fit/orbit camera, clips triangles on the EE and submits flat native
GS geometry with depth testing. Left/Right step the orbit; L1/R1 change distance.
Both vector units are inactive for its solid triangles. Recovered game systems,
original textures, collision, driving and game audio remain unconnected.

The latest source-bound `native-020` build reproduces the executed `native-011`
ELF byte for byte (SHA-256
`2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158`).
The 10.212-second Play 0.72 HLE/Vulkan run visibly rendered the private six-object
car mesh, with flat study shading and a diagnostic caption. All 1,040 triangles
were submitted and guest stderr was empty. The current 78-file dependency audit,
dependency-direction checks and forbidden-runtime symbol check pass. The ELF
has 2,274,240 file bytes; load segments occupy 593,080 bytes before dynamic
allocation. Its shared 21-file engine source identity is unchanged. The current
integration proof is `build/ps2-native-020/integration-validation.json`; it does
not imply a new gameplay execution. Earlier native-008 runs also checked the
no-asset bootstrap.
The recovered game components have separate EE fixture evidence;
they remain unused and are discarded from this diagnostic ELF by the linker.

A separate original depth fixture passed 206,609 quantization checks and read
512 exact RGB samples from GS memory with zero mismatches, covering solid and
textured draw ordering, the diagnostic page and depth clearing on return.
This validates those sampled regions in Play, not original-game image fidelity
or physical PS2 behavior. The visible car is a static study: the emulator's
frame-rate overlay is not a native gameplay or console performance measurement.
Private integration records remain under `build/ps2-native-020/`; earlier
execution records remain under their original directories and
`build/ps2-native-depth-wrapper-004/`. Controller interaction, speaker output
and physical hardware still need separate validation.
