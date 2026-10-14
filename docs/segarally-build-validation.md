# Mac build and validation guide

Run commands from this checkout's root. Use separate ignored directories for a
new generation and host so preserved playtests remain available. These commands
are a reproduction guide; do not run builds or heavy probes during an active
player speed test. Phone work is deferred.

## Inputs and tools

Supply your own ROM archive containing the verified Revision B (`srallycb`)
inputs. The archive may be named `srallyc.zip`; its contents, sizes, CRC32 and
SHA1 must match the committed manifests. Renaming an incompatible set does not
make it valid. See the manifests under [config](../config).

The scripts use Python 3 and a C++20 compiler (`CXX` or `--compiler` selects it).
The dedicated Mac project requires CMake 3.25 or newer, Apple's C/C++/Objective-C
toolchain and the pinned SDL3 source at `extern/sdl3`. The SDL revision is
`fa2c02bb6e21974a89ea9824bc53c9932abe5f9c` (3.4.16), enforced by
[CMake](../platform/segarally/CMakeLists.txt). Record dependency revisions and
licenses in [THIRD_PARTY.md](../THIRD_PARTY.md); vendored checkouts stay ignored.
This guide does not assume a published Rally remote or an automatic installer.

If `extern/sdl3` is absent, prepare the pinned dependency once:

```sh
git clone https://github.com/libsdl-org/SDL.git extern/sdl3
git -C extern/sdl3 checkout --detach fa2c02bb6e21974a89ea9824bc53c9932abe5f9c
```

An existing SDL checkout must report that exact revision with
`git -C extern/sdl3 rev-parse HEAD`. Preserve any local changes before changing
its revision.

## Verify, import and recompile

Use the same `--build-dir` for every import and generated component:

```sh
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-docs --verify-only
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-docs --seed-profile lift-labels --indexed-dispatch
python3 scripts/segarally_tgp.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-docs
python3 scripts/segarally_video.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-docs
```

`lift-labels` uses the expanded audited seed profile. The default `bootstrap`
profile is smaller. Indexed dispatch is opt-in; omit its flag to reproduce
the original routing. Record the checkout revision and resulting import,
coverage and source hashes: fresh generation from a newer checkout is not
automatically the frozen v13 checkpoint.

ROM-derived images, generated C++ and objects remain under ignored `build/`.
Import success proves input identity; coverage success proves the generated
graph, not every course or hardware behavior. Fix the generator or seed evidence
when a missing target is reached, then regenerate and validate.

## Build and launch the Mac host

Direct CMake configuration permits an isolated host directory:

```sh
cmake -S platform/segarally -B build/segarally-docs-mac \
    -DCMAKE_BUILD_TYPE=Release \
    -DSEGARALLY_NATIVE_BUILD="$PWD/build/segarally-docs"
cmake --build build/segarally-docs-mac -j 4
python3 platform/segarally/macos/launch.py --build-dir build/segarally-docs-mac \
    --input-replay config/segarally_lancia_select.inputs
```

The launcher audits assets and packages/signs the app before opening it in a
window. [The player manual](mac-player-manual.md) explains controls and saves.
The convenience `platform/segarally/build.sh mac` uses
`SEGARALLY_NATIVE_BUILD` but always builds the host in `build/segarally-mac`.

### Optional instruction boundary hook

For the exact execution candidate, add `--instruction-boundary-hook` to the
recompile command and generate into a separate build directory. Retain
`--seed-profile lift-labels --indexed-dispatch` and import TGP/video into that
same directory. Configure its Mac host with
`-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON`, then pass
`--instruction-boundary-hook` to the launcher or direct host to enable it.
Omitting the runtime flag keeps the old outer loop in that compatible binary.
Sound can be enabled alongside it with the options below.

This requires rebuilding every native object. The pipeline records matching
source, header, generated-code and object hashes; staging and runners reject
stale or mixed artifacts. The boot runner accepts the runtime flag with
`--with-control`. The independent CPU harness links the matching build layout
but keeps its boundary callback disabled; actual Machine scheduling equivalence
is tested separately by `tests/test_segarally_boundary_game.py`.

## Bounded checks and independent comparison

First reproduce a bounded boot with the generated components:

```sh
python3 scripts/segarally_boot.py --build-dir build/segarally-docs \
    --instructions 100000000 --with-tgp --with-io --with-comm \
    --with-control --with-video --with-screen --with-uart --capture-frame
```

The default one-million budget stops inside reset work. `boot.json` records
the bounded outcome; exit 3 means unsupported board access and exit 4 means
missing native code. Budget exhaustion is a checkpoint, not a complete boot
or race. `--step-frames` resumes the same machine one rendered frame at a time.

The measured driving checkpoint can be compared with the independent CPU:

```sh
python3 tests/test_segarally_boot_parity.py --build-dir build/segarally-docs \
    --instructions 600000000 --timeout 1200 \
    --tgp --io --comm --control --video --screen --uart \
    --input-replay config/segarally_lancia_drive.inputs
```

The harness defaults to two million instructions and a 180-second timeout.
Longer runs accept an explicit `--timeout`; permitted budgets extend to two
billion instructions. This compares CPU state, ordered writes, interrupts,
device state and frames using shared device implementations. It is not complete
MAME-machine or PCB parity. Keep input hashes and final checkpoint counts beside
the report; see [replay format](segarally-input-replay.md).

Relevant source-level regression checks include:

```sh
python3 tests/test_segarally_bootstrap.py
python3 tests/test_segarally_table_seeds.py
python3 tests/test_segarally_continuations.py
python3 tests/test_segarally_native_templates.py
python3 tests/test_i960_runtime_timing.py
python3 tests/test_segarally_tgp_import.py
python3 tests/test_segarally_video_import.py
```

For paired renderer evidence, the prepared executable accepts:

```sh
build/segarally-docs-mac/segarally_native --frames 180 --hidden --unthrottled \
    --capture build/segarally-docs-mac/metal.ppm \
    --reference-capture build/segarally-docs-mac/reference.ppm
```

This captures matching generated-machine states through two renderers. Choose
a recorded race checkpoint for race claims; 180 startup frames do not establish
racing-scene graphics fidelity. Hidden capture avoids activating another visible
window but still consumes CPU/GPU time.

## Optional sound experiment

Sound requires both audited generated sound inputs and an explicit build option:

```sh
python3 scripts/segarally_sound.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-docs
python3 scripts/segarally_sound_recompile.py --build-dir build/segarally-docs --timed
cmake -S platform/segarally -B build/segarally-docs-audio \
    -DCMAKE_BUILD_TYPE=Release \
    -DSEGARALLY_NATIVE_BUILD="$PWD/build/segarally-docs" \
    -DSEGARALLY_SOUND=ON -DSEGARALLY_SOUND_BUILD="$PWD/build/segarally-docs"
cmake --build build/segarally-docs-audio -j 4
python3 platform/segarally/macos/launch.py --build-dir build/segarally-docs-audio --sound
```

Sound stays off without `--sound`. The headless runner instead accepts
`--with-sound` and optional `--sound-build-dir`, alongside `--with-uart`.
The direct host accepts `--sound --audio-capture FILE.wav`; unthrottled mode
drains/captures PCM without opening a playback device. A successful WAV capture
therefore does not validate speakers. Source/assets/generated-code changes
require renewed sound audits; the host checks provenance during incremental
builds. A ten-second captured-race check through the real SDL/CoreAudio output
submitted 441,000 stereo frames with no queue drops or reported input shortfalls.
The separate normal-paced game check reaches 2,760 frames with unchanged main
state and no chip or host sample drops, but records 3,222 SDL requests for extra
input. The runtime is too slow for continuous playback; these estimated requests
diagnose an empty queue, not exact hardware underrun samples. User listening
confirmation and full-game sound parity remain pending.

Full MAME reference work is separately blocked by three files absent from the
supplied archive: `epr-17891.ic12`, `epr-16726.bin` and `epr-18022.ic2`.
Native bring-up does not require pretending those firmwares have run.
