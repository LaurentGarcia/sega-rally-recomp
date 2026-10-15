# Sega Rally Championship native recompilation

A dedicated reverse-engineering project for Sega Rally Championship (1995,
Model 2A), based on Daytona USA's static recompilation tools and shared native
hardware code. It reconstructs behavior from verified personal ROMs; it is not
Sega's original source code.

**macOS is the active target.** The game runs through SDL3 and Metal. iPhone
work is deferred while native game coverage, speed, sound and fidelity improve.

## Documentation

Start with the [project wiki](docs/README.md):

- [Mac player manual](docs/mac-player-manual.md): launch, controls, coins and saves.
- [Build and validation guide](docs/segarally-build-validation.md): reproducible
  import, generation, Mac builds and bounded checks.
- [Technical architecture](docs/segarally-architecture.md): CPU, devices, timing,
  graphics and audio.
- [Evidence and status](docs/segarally-status.md): measured progress and remaining
  validation.

The [design](docs/sega-rally-design.md), [project rules](rules.md) and
[handoff](HANDOFF.md) retain the detailed contracts and engineering record.

## Current checkpoint

The frozen v14 graph contains 75,207 native i960 instructions in 52 compiled
objects. Prior v13 checks run the actual Lancia race, Desert finish/results and
entry into the forested second course. A two-billion-instruction held-gas replay reaches
time-over and returns to attract mode without a missing-code or device error.
A fresh-credit check then accepts two coins and Start, returning to Car Select
at frame 11,160. The sound-enabled v14 extension starts and drives a second race:
its frame-12,800 capture shows 118 km/h in fourth gear at 15.83 seconds elapsed.
Completing that race, other courses and alternate paths still need tests.

The latest observed missing return is now included. A sound-enabled v14 check
matches the previous 25,580 normal execution/audio steps exactly and continues
past the old frontier through frame 14,001. This comparison uses the same native
Machine implementation; it does not establish independent hardware parity.

Independent CPU comparison now passes the two-billion-instruction held-gas
path: all nineteen input events, 86,328,770 ordered writes, 4,650,936,033
reference cycles and 10,701 rendered frames. CPU state, writes and events are
checked per instruction; the full framebuffer is checked at the end. This
sound-off test uses separately instantiated shared board implementations,
not the full Machine scheduler or independent MAME/PCB hardware. Extended-
precision FP, exact racing-scene pixels and physical timing remain open.

Indexed lookup and the opt-in instruction boundary hook reduce execution
overhead while preserving instruction guards and timing. With native SCSP sound
enabled, a quiet paced driving check completes 2,760 frames and 47.98148 seconds
of board time in 48.519993 seconds of wall time, including startup and shutdown.
SDL reports zero audio input shortfalls and zero chip or host sample drops.
The older execution path exhausted its queue during the same driving checkpoint.

The hook matches the old Machine through 600 million driving instructions,
every compared frame and every captured PCM sample. This hidden-window result
does not establish every course or visible presentation. A ten-second native
PCM sample has been heard on the Mac speakers. Opening audio after window/Metal
setup fixes the observed startup failure. The later v16 controller preview
exits normally after 12,974 frames with sound and CRT, with zero logged SDL
shortfall requests or chip/host drops. Live feedback from v15 praised the image
and motion but reported title-screen audio glitches. Clean later telemetry does
not resolve that audible issue.
Sound requires both the build option and runtime `--sound`.
See the [sound instructions](docs/segarally-build-validation.md#optional-sound-experiment).

Optional Accurate textures now combines measured Metal span stepping with a
certified reciprocal correction. All pixels in three saved complete scenes
match the CPU renderer, and 447 synthetic cases pass. The separate v17 candidate
builds and packages, but has not launched; its OFF/ON host comparisons remain
pending. The option stays off by default. Continuous flicker, live cost and
whole-game graphics fidelity remain unverified.

## Display controls

Newly built hosts use **F1** for paused Settings and **F6** to toggle
optional CRT effects. Choose Original or Arcade CRT, then adjust scanlines,
mask, glow, curvature and color fringing. Smooth/Sharp scaling, integer scale
and Accurate textures are separate controls. Fresh defaults preserve the
original display; changed preferences use a separate `display.settings` file.
The v15 and controller-enabled v16 previews have launched with sound and CRT.
A 2,600-frame off/on comparison preserves native frames, audio and game state.
Monitor accuracy and sustained effect cost remain unverified.
See [the player manual](docs/mac-player-manual.md#display-controls).

Recognized Xbox/SDL gamepads provide analog steering and triggers. Press **Y**
or **F1** for **Settings → Controls** to enable/disable controller input, remap
keyboard/controller bindings, and adjust dead zones and steering response.
Keyboard controls stay available; input preferences are separate from cabinet
saves. See [controller defaults](docs/mac-player-manual.md#controls).
The v16 live recording contains intermediate steering, accelerator and brake
values and replays exactly; it does not identify the physical source of each
input. Feedback on controller Y and menu navigation remains open.

## Personal inputs and repository boundaries

Supply an archive matching the committed Revision B (`srallycb`) manifests.
The importer checks sizes, CRC32 and SHA1; filenames alone do not establish the
revision. ROMs, extracted images, generated game C++, captures and apps stay in
ignored local directories. Source tools, native hardware code, manifests,
address annotations, tests and analysis are versioned.

Unsupported hardware and missing native targets stop with diagnostics. They
are not skipped or replaced with invented game behavior. The earlier lifted-C
phone prototype lives separately in `../segarally95-recomp`; its captures and
Daytona's results do not establish this runtime's fidelity.

Full MAME tracing currently needs three files absent from the supplied archive:
`epr-17891.ic12` (drive), `epr-16726.bin` (communication) and `epr-18022.ic2`
(billboard). Native bring-up does not claim to execute those missing firmwares.

## Source base

Started from [`alphanu1/daytona-arcade-recomp`](https://github.com/alphanu1/daytona-arcade-recomp)
at `b33baeb28a1e1c94d90a4a80811f0b2c7f61106b`. The upstream Git history and
BSD-3-Clause license are retained. Third-party pins and licenses are recorded in
[THIRD_PARTY.md](THIRD_PARTY.md). `daytona-upstream` is a read/reference remote;
this project currently has no publication remote. The
[inherited Daytona documentation](docs/daytona-upstream-readme.md) describes the
upstream reference build, whose targets remain available separately.
