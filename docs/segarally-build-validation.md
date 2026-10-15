# Mac build and validation guide

The experimental **v0.1.0-rc.1 source prerelease** builds a local Mac app from
verified personal Revision B (`srallycb`) ROMs. For a fresh checkout, follow
[getting started](getting-started.md) first. See [platform status](release-platforms.md) for phone and Windows/Linux build readiness.
Commands below run from the repository root.

## Inputs and tools

Use Apple's C/C++/Objective-C toolchain with C++20 support, Python 3.10 or newer,
CMake 3.25 or newer (including CTest), Git, `codesign` and `pgrep`. A Metal-capable
Mac and desktop/audio session are required to play. Apple Silicon is the recorded
validation platform; Intel execution and a minimum macOS version have not been
established by the Rally checks. The pipeline builds the current host architecture.

The dedicated Mac pipeline requires these pristine source checkouts:

| Directory | Upstream | Required commit |
| --- | --- | --- |
| `extern/sdl3` | [SDL3](https://github.com/libsdl-org/SDL) | `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c` |
| `extern/imgui` | [Dear ImGui](https://github.com/ocornut/imgui) | `f1cc2ae15e53a861a874c3034aae6798fde194ab` |
| `extern/lzma` | [7-Zip](https://github.com/ip7z/7zip) | `0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17` |

Exact clone/checkout commands are in [getting started](getting-started.md#2-get-the-source-and-pinned-dependencies).
The build checks revisions and refuses modified tracked dependency files.
The inherited generic setup may modify SDL for a different graphics backend;
use clean pins for this Metal pipeline. An ordinary build needs no MAME, Ghidra,
DXC, SPIRV-Cross or Docker. The root Daytona CMake project has separate dependency
requirements and is not this guide's build entrypoint.

Supply a ZIP archive containing all entries required by the manifests in
[config](../config). The importer checks every required input's size, CRC32 and
SHA1. The archive's name does not determine its revision. Keep ROMs outside the
checkout or in ignored `roms/`; the pipeline never downloads them. Generated game
C++, extracted images, compiled apps and captures remain ignored local output.

## Build the Mac candidate in one command

```sh
python3 scripts/segarally_mac.py --roms /path/to/srallyc.zip \
    --output build/segarally-player --jobs 4
```

Choose a new output directory. The pipeline runs these stages in order:

1. Verify/import and generate main i960 code with the expanded seed profile,
   indexed dispatch and exact instruction boundary hook.
2. Generate Rally's TGP program and import graphics inputs.
3. Import sound images and generate/audit the timed 68000 program.
4. Configure and compile the Release Mac host with sound, controls, Settings
   and recording support, plus its regression executables.
5. Run the dedicated Mac CTests, package assets/notices and verify the signature.

The eleven registered Mac CTests cover host audio, controls, display preferences,
input parsing, asset staging, launcher, notice packaging and pipeline behavior.
Their passing result is a build/regression gate, not complete gameplay validation.
The source and test registration live in
[platform/segarally/CMakeLists.txt](../platform/segarally/CMakeLists.txt).

The command prints the app path, build report and optional automatic-selection
launch command. It never launches the game. Open
`build/segarally-player/mac/Sega Rally Native.app` to play, or follow the printed
launch command. Direct no-argument startup enables compiled sound and the exact
instruction boundary hook; explicit diagnostic launches retain opt-in switches.
Current source builds enable CRT for fresh player preferences; other enhancements remain opt-in.

The package is ad-hoc signed for local use, not notarized or universal. No Apple
developer account is required for that local signature. Because it contains
personal ROM-derived inputs, it is not a public source-release asset. Project,
SDL, ImGui, selected 7-Zip and runtime notices are bundled; missing required
notices stop packaging. See [THIRD_PARTY.md](../THIRD_PARTY.md).

`--plan` prints the command lists without writing output or running build tools.
`--jobs` accepts 1 through 8. Existing output directories are rejected to preserve
apps and recordings. A failure leaves `build-result.json` and `logs/` in its output.
The report records source/dependency/input identities, commands, durations and
artifact hashes. Preserve the first failed report before choosing another output.

## Run the dedicated host tests again

The one-command pipeline already builds the test executables and runs CTest:

```sh
ctest --test-dir build/segarally-player/mac --output-on-failure
```

For a manually configured host, the test executables are excluded from its default
build. Build them explicitly before running CTest:

```sh
cmake --build build/segarally-player/mac --target test_segarally_audio \
    test_segarally_controls test_segarally_display_preferences \
    test_segarally_audio_output_capture test_segarally_input_replay -j 4
ctest --test-dir build/segarally-player/mac --output-on-failure
```

Avoid concurrent builds or heavy probes while measuring gameplay or listening.
Recorded run durations depend on the Mac and active work; they are not portable
performance guarantees.

## Verify, import and recompile

For development, the stages can be run separately. Keep a common generation
directory for main, TGP, video and sound inputs:

```sh
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-dev --verify-only
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-dev --seed-profile lift-labels \
    --indexed-dispatch --instruction-boundary-hook
python3 scripts/segarally_tgp.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-dev
python3 scripts/segarally_video.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-dev
python3 scripts/segarally_sound.py --roms /path/to/srallyc.zip \
    --build-dir build/segarally-dev
python3 scripts/segarally_sound_recompile.py --build-dir build/segarally-dev --timed
```

`lift-labels` is the expanded seed profile; `bootstrap` is the smaller default.
A seed candidate is not proof of execution or complete indirect coverage.
Generated code is never hand-edited: fix the generator or audited seed evidence,
then regenerate. Asset staging checks imported image identities and available
source/object provenance. Hashes identify consumed artifacts; they do not turn
an older identity-only audit into proof of how a file was generated.

## Build and launch the Mac host

For those manually generated inputs:

```sh
cmake -S platform/segarally -B build/segarally-dev-mac \
    -DCMAKE_BUILD_TYPE=Release \
    -DSEGARALLY_NATIVE_BUILD="$PWD/build/segarally-dev" \
    -DSEGARALLY_SOUND_BUILD="$PWD/build/segarally-dev" \
    -DSEGARALLY_SOUND=ON -DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON
cmake --build build/segarally-dev-mac -j 4
python3 platform/segarally/macos/launch.py --build-dir build/segarally-dev-mac \
    --sound --instruction-boundary-hook \
    --input-replay config/segarally_lancia_select.inputs
```

The launcher verifies inputs, packages/signs, then opens a window through
LaunchServices. `--no-launch` packages only and saves `launch-command.json`.
Quit the app before repackaging. Preserve recorded executables and assets exactly;
re-signing or rebuilding can invalidate their identity-bound recordings.
The [manual](mac-player-manual.md) explains saves and controller handoff.

### Optional instruction boundary hook

The generation flag, CMake option and explicit runtime switch above must agree.
Rebuild every native object after changing the hook contract. Stale/mixed artifact
layouts are rejected. Indexed routing changes address lookup; the hook keeps
execution inside a native chunk while retaining live-word guards, per-instruction
cycle charges, event synchronization and IRQ boundaries. The old outer loop remains
available for comparison in a compatible binary. See the
[architecture](segarally-architecture.md) and [status](segarally-status.md).

### Display controls and shaders

F1 opens paused Settings; F6 toggles optional CRT. The launcher accepts `--crt`,
`--accurate-textures` and `--original-display`. The last restores baseline display
preferences for a session; do not combine it with `--crt`. Omit
`--accurate-textures` when making baseline captures. Native `--capture` is before
CRT and UI but reflects the selected native texture mode.

Generated common shader headers are committed. If you edit shader sources,
regenerate through the separate DXC/SPIRV-Cross pipeline or documented container
fallback; do not hand-edit generated headers:

```sh
python3 scripts/build_shaders.py
python3 scripts/build_crt_shaders.py
```

These developer tools have additional requirements; inspect their source before
use. Shader regeneration is not part of an ordinary Mac build.

## Bounded checks and independent comparison

A bounded native boot uses the same generated inputs and shared devices:

```sh
python3 scripts/segarally_boot.py --build-dir build/segarally-dev \
    --instructions 100000000 --with-tgp --with-io --with-comm \
    --with-control --with-video --with-screen --with-uart --capture-frame
```

The default one-million budget stops inside reset work. `boot.json` records the
outcome; exit 3 means unsupported board access and exit 4 means missing native
code. Budget exhaustion is a checkpoint, not full-game completion. Input scripts
change cabinet pins; they do not replace game variables.

The independent i960 engine can compare the recorded driving path:

```sh
python3 tests/test_segarally_boot_parity.py --build-dir build/segarally-dev \
    --instructions 600000000 --timeout 1200 \
    --tgp --io --comm --control --video --screen --uart \
    --input-replay config/segarally_lancia_drive.inputs
```

This compares CPU state, ordered writes, IRQs and separately instantiated shared
devices. It is not a full independent MAME machine. Defaults are two million
instructions and 180 seconds; longer budgets/timeouts must be explicit. Record
checkout, import, input and final checkpoint identities with every claim.

### Independent sound-off recording consumer

A separate developer consumer tests supported sealed sound-off recordings using
the independent i960 engine and shared board implementations. It rejects fault
rows and unsupported options. See
[tests/test_segarally_recording_reference.py](../tests/test_segarally_recording_reference.py)
and the [replay contract](segarally-input-replay.md) for its exact CLI and scope.
This differs from production offline replay, which uses the exact same signed
executable to verify captured resumes, cabinet pins, PCM hashes and final saves.

## Optional sound experiment

The one-command pipeline includes sound. The CMake default is OFF for manual
builds; enable `SEGARALLY_SOUND` and supply the audited timed sound directory,
then use `--sound` for explicit runtime launches. The headless boot runner instead
uses `--with-sound`, requires `--with-uart` and accepts `--sound-build-dir`.

The direct host accepts `--audio-capture FILE.wav` with sound. Unthrottled runs
capture/drain without opening a playback device, so they do not validate listening.
Paced `--sound --audio-diagnostics` logs queue demands and host delays. A separate
`--audio-output-capture FILE.wav` also requires paced sound, audio diagnostics
and a distinct native capture; it records this application's SDL logical-device
mix before CoreAudio, not hardware loopback or microphone audio.

Playback primes a 60 ms base plus one actual device period, caps its queue at
240 ms and limits presentation-rate trim to ±0.5%. Device periods longer than
60 ms are rejected. Native PCM identity, queue health and physical listening are
separate evidence. Sound CPU MMIO remains at instruction-start boundaries;
full physical bus phases and whole-game audio parity remain unverified.

## Reference firmware

Full MAME reference work additionally needs the verified drive, communication
and billboard firmware, beyond what the native frontend executes:

| File | Device | Bytes | CRC32 |
| --- | --- | ---: | --- |
| `epr-17891.ic12` | Drive board | 65,536 | `9a33b437` |
| `epr-16726.bin` | Communication board | 131,072 | `c179b8c7` |
| `epr-18022.ic2` | Billboard | 65,536 | `0ca70f80` |

Supply these from your own archive inventory using the layouts expected by your
MAME version. A local reference inventory can be audited without starting a game:

```sh
mame -rompath /path/to/your/reference-roms -verifyroms srallycb
```

A passing ROM audit proves input readiness, not correct native execution or
full-race parity. Retain MAME source/build identity and align input/timing before
comparing traces. The inherited `scripts/run_trace.sh` defaults to Daytona and
replaces its output directory; it is not a ready-made Rally validation command.
The [status](segarally-status.md), [design](sega-rally-design.md) and
[handoff](../HANDOFF.md) retain detailed historical results and known limitations.
