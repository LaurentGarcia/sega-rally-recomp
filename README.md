# Sega Rally Championship native recompilation

Sega Rally Championship (1995, Model 2A), running as generated native C++ from
verified personal ROMs, with shared native hardware code, SDL3 and Metal.
**macOS is the active target; phone work is deferred.**

The Mac app provides keyboard and Xbox/SDL gamepad driving, remappable controls,
analog steering and pedals, persistent cabinet saves, native SCSP sound and
exact input recording/replay. Optional CRT effects and Accurate textures are
available in paused Settings; enhancements start off on a fresh setup.

## Build and play

With the [required tools and pinned dependencies](docs/segarally-build-validation.md)
in place, run from this checkout:

```sh
python3 scripts/segarally_mac.py --roms /path/to/srallyc.zip \
    --output build/segarally-player --jobs 4
```

Choose a new output directory. The command verifies the supplied Revision B
ROMs, generates native main/TGP/sound code, builds, runs host checks and packages
a signed app. Open `build/segarally-player/mac/Sega Rally Native.app` afterward.
No game data is included in the repository.

Direct no-argument startup enables compiled-in sound and the faster instruction
boundary path. The app opens windowed with normal pacing. Press and release
**C twice** for a fresh cabinet credit, then **Return** to start. Steer with
**Left/Right**, accelerate with **Z**, brake with **X**, and change view with
**V**. Press **F1** or controller **Y** for Settings. See the
[Mac player manual](docs/mac-player-manual.md) for controller defaults,
automatic selection, display controls and saves.

## Current checkpoint

Candidate 002 was built from the supplied archive at source revision `481bf63`.
All eleven dedicated Mac CTests pass, and the signed package is preserved at
`build/mac-candidate-002/mac/Sega Rally Native.app`. Its actual no-argument
startup runs 1,293 frames over a bounded 25-second check with Metal, sound and
the instruction boundary hook enabled, with enhancements off. One SDL estimated
input shortfall was logged. **Reported title-screen audio glitches remain
unresolved**; successful startup and queue measurements do not establish
listening quality.

The sound-enabled Metal candidate applies all 4,398 recorded input events through
20,000 frames, clearing Desert in **1:00.38** and Forest in **1:21.37**, entering
Mountain and retiring near its finish. Its exact signed-executable offline
replay matches all 40,006 resumes, PCM hashes and final saves. This unthrottled
run uses no playback device and is not a speed or listening check. **Full-game
completion is not established.** Earlier independent CPU comparison and the
preserved v17 manual replay remain documented in the status reference; neither
is full MAME-machine or PCB parity.

A separate paced capture exposes a 6.53 ms SDL output silence gap absent from
native PCM. Offline SDL scheduling reproduces the captured output, but a buffer
remedy remains under development and the reported title issue stays open.

Optional Accurate textures matches all pixels in three saved complete scenes
and passes 447 synthetic cases; its v17 host checks preserve audio, inputs and
saves. Whole-game graphics fidelity, extended-precision FP, physical timing,
remaining game paths and sustained display-effect cost remain open.

## Documentation

Start with the [project wiki](docs/README.md):

- [Mac player manual](docs/mac-player-manual.md): launch, controls, coins and saves.
- [Build and validation guide](docs/segarally-build-validation.md): reproducible
  import, generation, Mac builds and bounded checks.
- [Technical architecture](docs/segarally-architecture.md): CPU, devices, timing,
  graphics and audio.
- [Evidence and status](docs/segarally-status.md): measured checkpoints,
  historical comparisons and remaining validation.

The [design](docs/sega-rally-design.md), [project rules](rules.md) and
[handoff](HANDOFF.md) retain contracts, hashes and the detailed engineering record.

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
