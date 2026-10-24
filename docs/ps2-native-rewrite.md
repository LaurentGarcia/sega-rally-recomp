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

All four components have portable boundary tests, sanitizer checks and standalone
EE fixtures executed successfully in Play: 121 progress boundary/rejection
checks plus repeated updates, and all 65,536 pose flag pairs with raw-value
and repeated-publication checks. The angle fixture passed 36,262 EE checks;
the exhaustive folded interval is covered by its host original-program oracle.
Orientation finalization passed 27,031 EE checks; its axis meanings and live
caller domain remain unproven. It recovers a tail region of the integrator,
not an additional standalone original function.
The original-program comparisons run on the host against the
owner's verified Revision B image; the reference interpreter and memory
adapters are not part of the native executable. The components are compiled
by the native target but remain unconnected to its diagnostic loop and may
be removed by linker garbage collection. None establishes car physics,
coordinate conventions, collision, caller cadence or a complete race system.
The [car integration dependency map](../platform/ps2/native/car_integration_recovery.md)
records the remaining data and arithmetic contracts. The larger integrator
is not recovered merely because its angle helper matches.

The [hardware study](ps2-architecture-study.md) records the complete local
manual collection, specific constraints and the executed VU1 experiments.
The [native clipping reference](ps2-native-geometry.md) provides bounded
homogeneous clipping and ordered triangle output. Its selected EE checks
pass, but it uses unmeasured software-double arithmetic and remains outside
the active draw loop. It does not establish recovered game projection.

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
The startup page explicitly identifies original diagnostic content and
unconnected game systems. Select + Start toggles that page. The left stick
moves the original checkerboard test geometry; holding Cross submits a quiet
original 440 Hz tone. PCM refill follows the IOP queue independently of
PAL/NTSC display cadence. No game files or reference-probe `rally.cfg` are read.

The frozen `native-006` ELF (SHA-256
`52ab97be39d4897c48e637f25a4deb780c183b613628514c56516cf497deeb18`)
ran for 17.249 seconds in Play 0.72 with its HLE BIOS and Vulkan renderer.
Guest logs recorded two submitted triangles, initialized PCM transport and
zero ring-empty/late-service counters at all 19 checkpoints through frame 960.
All 30 project input hashes and the forbidden-runtime symbol audit pass;
load segments occupy 450,088 bytes before dynamic allocation.

Its visible content is still the original diagnostic geometry and status
page. The recovered game components and clipping reference remain unconnected
to the active loop. Window capture failed on this run; the last successful
visual capture is native-004. These are bootstrap checks in an emulator,
not gameplay or physical-console performance. Controller interaction,
measured speaker output and actual hardware require separate validation.
