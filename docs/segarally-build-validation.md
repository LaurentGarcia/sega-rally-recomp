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

The Display panel uses the same MIT-licensed Dear ImGui 1.92.9b pin as Daytona:
`f1cc2ae15e53a861a874c3034aae6798fde194ab`. The dedicated host requires this
checkout at `extern/imgui`; its SDL3 and SDL_GPU backends are built unchanged.
`scripts/setup.py` prepares the pinned dependencies. To prepare just ImGui:

```sh
git clone https://github.com/ocornut/imgui.git extern/imgui
git -C extern/imgui checkout --detach f1cc2ae15e53a861a874c3034aae6798fde194ab
```

CMake checks the revision and packages the ImGui MIT notice with the app.
Vendored sources remain ignored.

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
automatically either frozen v13 or v14 checkpoint.

Staging now records those actual artifact identities under `assets.json`'s
versioned `provenance` object. Hook generation retains its boundary-contract
verification. Ordinary main/TGP generated sources are labeled `identity_only`:
their hashes identify consumed files without claiming that older audits
authenticate how they were generated. Available inventories, reachability and
the declared TGP upload bytes are checked before changing the package. Audits
use paths relative to their originating checkout; stage worktree builds in
that checkout's context rather than weakening path checks.

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

### Display controls and shaders

Display controls are compiled into newly built hosts; no CRT effect is enabled
on a fresh setup. F1 opens the paused panel and F6 toggles CRT. The launcher and
direct host accept `--crt`, `--accurate-textures`, and `--original-display`.
The direct-only `--display-controls` opens the panel after the first native
frame. `--crt` and `--original-display` conflict. Saved `display.settings`
preferences load first; explicit flags then override them. Use
`--original-display` without `--accurate-textures` for baseline captures.

The CRT pass runs after native image composition and before touch/UI overlays.
It changes neither native game state nor the raw `--capture` texture. Accurate
textures selects the separate optional native renderer, so it can affect that
capture. The original linear blit and closed panel remain the fresh defaults;
UI and CRT GPU resources initialize only when needed.

Generated common shader headers are committed, so an ordinary host build needs
no shader tools. When changing shader source, regenerate through the common
DXC/SPIRV-Cross pipeline (or its Docker fallback):

```sh
python3 scripts/build_shaders.py
python3 scripts/build_crt_shaders.py
```

The first updates `shaders_gen.h` from `m2.hlsl`; the second updates only
`crt_shaders_gen.h` from `crt.hlsl`. Do not hand-edit generated shader output.
A separate CPU-only preference regression target is available in the configured
Mac host build:

```sh
cmake --build build/segarally-docs-mac --target test_segarally_display_preferences -j 2
ctest --test-dir build/segarally-docs-mac --output-on-failure -R '^segarally_display_preferences$'
```

The finite parser, range bounds, round trips, invalid files and separate save
writes pass 250 synthetic checks under ASan/UBSan. The hidden offscreen UI smoke
passes at 640×480, including visible Resume and no backend calls on closed
frames. No game or audio runs in that smoke. A separate 2,600-frame sound-enabled
host comparison preserves the native image, PCM, ordered resumes and saves
with CRT off/on. The final v15 app has also launched with visible CRT
presentation and sound; sustained effect cost remains unmeasured.

The CRT GPU fixture is separately enabled and requires a working Mac display:

```sh
cmake -S . -B build/metal-tests -DM2_BUILD_WINDOWED_APP=OFF -DM2_METAL_TESTS=ON
cmake --build build/metal-tests --target test_gpu_crt -j 2
ctest --test-dir build/metal-tests --output-on-failure -R '^gpu_crt$'
```

The synthetic assertions cover disabled/zero-effect behavior, individual
controls, bounds, letterboxing and native/enhanced source-row behavior. The
retained GPU gate passes 18 synthetic draws plus two scene-preview draws.
These are presentation checks, not monitor-fidelity or gameplay-speed tests.

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

### Independent sound-off recording consumer

The separate test consumer accepts a sealed sound-off recording with supported
options and no captured fault rows. It uses the canonical immutable preflight,
actual initial/final saves and the existing raw harness's two board instances;
production same-executable replay and raw parity defaults remain unchanged.
The first actual check passes six resumes, 563,041 instructions, 283,375 ordered
writes and two frames. Longer/manual captures and coupled sound remain open.

The ROM-free recording checks require the pinned `extern/lzma` revision
`0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17` for SHA-256, alongside the
normal reference-test dependencies. Run them through CMake:

```sh
cmake -S . -B build/recording-tests -DM2_BUILD_WINDOWED_APP=OFF
cmake --build build/recording-tests --parallel 2 --target \
    test_segarally_recording_reference test_segarally_recording_bundle \
    test_segarally_reference_resume
ctest --test-dir build/recording-tests --output-on-failure \
    -R '^segarally_(recording_reference|recording_reference_build|recording_bundle|reference_resume)$'
```

`tests/test_segarally_recording_reference.py` verifies the actual preserved raw
host, its `compile_commands.json`, measured source/object/link artifacts,
staged assets and audited generation. Supply `--generation`, `--host-build`,
`--host-evidence` and a new exclusive `--output` directory. It links existing
verified native/TGP objects; it does not regenerate or compile native chunks.
The evidence directory must contain the original `compiled-object-contract.json`,
`linked-artifact-identities.json`, `native-link.txt` and `recording-link.txt`.
Missing evidence is rejected, not replaced by a supplied digest. TGP source
identity remains `identity_only`, not independently authenticated generation.

The default prepares only. To execute, add `--bundle DIR --run`, an explicit
`--max-instructions` (at most 20 million) and `--timeout` (at most 180 seconds).
The first actual check uses one million and 90 seconds. Output retains the
build/run commands, frozen identities and comparison transcript; no SDL/GPU,
audio output or live-save writes occur. Preserve the original raw host and its
matching measurements; a changed or signed executable needs matching evidence.

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

### Optional polygon scanlines

Newly compiled hosts accept `SEGARALLY_GPU_POLYGON_SCANLINES=1` in the direct
process environment to enable the diagnostic polygon span path. It remains
off by default, and preserved playtest binaries do not acquire it from source
changes. Compare native 496×384 captures first; enhanced resolution has separate
sampling behavior. The [status table](segarally-status.md#graphics-evidence)
records current improvements and remaining differences.

The synthetic fixture needs no game data. It does require the pinned SDL3
checkout and a working macOS Metal/display session:

```sh
cmake -S . -B build/metal-tests -DM2_BUILD_WINDOWED_APP=OFF -DM2_METAL_TESTS=ON
cmake --build build/metal-tests --target test_gpu_polygon_scanlines -j 2
ctest --test-dir build/metal-tests --output-on-failure -R '^gpu_polygon_scanlines$'
```

`M2_METAL_TESTS` defaults to off so ordinary headless tests do not require a GPU.
This explicitly enabled path builds SDL without requiring the inherited launcher.
The fixture tests native-resolution coverage and layered cutouts against the
CPU rasterizer; it is not a whole-game texture or performance benchmark.

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
The older outer-loop execution reaches 2,760 frames with unchanged main state
and no sample drops, but records 3,222 SDL requests for extra input. With the
instruction boundary hook enabled, the same checkpoint completes in 48.519993
seconds including startup and shutdown, with zero extra input requests and
zero chip/host drops. These counters estimate SDL input demand, not exact
hardware output samples. The user confirmed hearing a separate ten-second native
PCM sample through the Mac speakers. The earlier windowed session exits
normally after 9,347 frames and supplies 7,165,805 stereo frames with no drops
or estimated input shortages. Live-game listening feedback and full-game sound
parity remain pending.

The current v14 app's recorded windowed session closes normally at 9,344
frames, supplying 7,163,505 stereo frames with zero chip/host drops and one
estimated SDL input-shortfall request for 552 bytes. Separately, its hidden
sound-enabled continuation passes the old missing return through frame 14,001
with exact prior normal execution/PCM observations. That unthrottled test
opens no playback device. See the [manual](mac-player-manual.md) for the
prepared app path and [status](segarally-status.md) for precise evidence limits.

Full MAME reference work is separately blocked by three files absent from the
supplied archive: `epr-17891.ic12`, `epr-16726.bin` and `epr-18022.ic2`.
Native bring-up does not require pretending those firmwares have run.
