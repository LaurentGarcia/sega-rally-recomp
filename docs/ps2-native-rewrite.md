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
2. Recover small game functions as explicit input/output contracts. Write
   ordinary C++ against typed state and compare it offline with the original
   behavior across edge cases and recorded game states.
3. Recover course/collision data and native car integration before substituting
   a playable driving model. A generic vehicle simulation is not an authentic
   implementation of Sega Rally.
4. Migrate race progression, AI, cameras, menus, rendering assets and audio
   events; connect the recovered systems into the native loop.
5. Compare complete courses, restarts and input replays, then validate memory,
   timing, controllers and audio on a physical PS2.

The initial native target is a **rewrite bootstrap**, not a playable game.
The existing captured car scene validates a renderer, not migrated gameplay.
No frame-rate or fidelity claim for the completed native game follows from
that scene or a ROM-free hardware test.

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

The separate [driving observer](../platform/ps2/tools/driving_capture.md) now
records those routes using the existing cabinet replay. Capture 011 passes all
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
This is bounded host-reference evidence, not independent arcade-board fidelity
or a full-course proof. The native PS2 application still does not connect these
components into driving gameplay.

The [hardware study](ps2-architecture-study.md) records the complete local
manual collection, specific constraints and the executed VU1 experiments.
The [native clipping reference](ps2-native-geometry.md) provides bounded
homogeneous clipping and ordered triangle output. Its selected EE checks
pass, but it uses software-double arithmetic with unmeasured console cost.
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

The latest source-bound `native-016` build reproduces the executed `native-011`
ELF byte for byte (SHA-256
`2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158`).
The 10.212-second Play 0.72 HLE/Vulkan run visibly rendered the private six-object
car mesh, with flat study shading and a diagnostic caption. All 1,040 triangles
were submitted and guest stderr was empty. The 50-file dependency audit and
forbidden-runtime symbol check pass; load segments occupy 593,080 bytes before
dynamic allocation. Earlier native-008 runs also checked the no-asset bootstrap.
The recovered game components have separate EE fixture evidence;
they remain unused and are discarded from this diagnostic ELF by the linker.

A separate original depth fixture passed 206,609 quantization checks and read
512 exact RGB samples from GS memory with zero mismatches, covering solid and
textured draw ordering, the diagnostic page and depth clearing on return.
This validates those sampled regions in Play, not original-game image fidelity
or physical PS2 behavior. The visible car is a static study: the emulator's
frame-rate overlay is not a native gameplay or console performance measurement.
Private records remain under `build/ps2-native-016/` and
`build/ps2-native-depth-wrapper-004/`. Controller interaction, speaker output
and physical hardware still need separate validation.
