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

The frozen v13 graph contains 75,205 native i960 instructions in 52 compiled
objects. It runs the actual Lancia race, Desert finish/results and entry into
the forested second course. A two-billion-instruction held-gas replay reaches
time-over and returns to attract mode without a missing-code or device error.
Other courses, alternate paths and a fresh second-credit start still need tests.

Independent CPU comparison passes 600 million driving instructions, all nineteen
recorded input events, 19,641,024 ordered writes and 3,142 rendered frames.
It uses separately instantiated shared device implementations, so it is not
full MAME-machine or PCB proof. Extended-precision FP, exact racing-scene pixels
and physical timing remain open requirements.

Indexed lookup and the opt-in instruction boundary hook reduce execution
overhead while preserving instruction guards and timing. With native SCSP sound
enabled, the latest quiet Mac check completes 2,760 frames and 47.98148 seconds
of board time in 48.519993 seconds of wall time, including startup and shutdown.
SDL reports zero audio input shortfalls and zero chip or host sample drops.
The older execution path exhausted its queue during the same driving checkpoint.

The hook matches the old Machine through 600 million driving instructions,
every compared frame and every captured PCM sample. This hidden-window result
does not establish every course or visible presentation. A ten-second native
PCM sample has been heard on the Mac speakers. Opening audio after window/Metal
setup fixes the observed startup failure. The subsequent windowed session exits
normally after 9,347 frames with zero audio drops or estimated input shortages;
live-game listening feedback remains pending.
Sound requires both the build option and runtime `--sound`.
See the [sound instructions](docs/segarally-build-validation.md#optional-sound-experiment).

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
