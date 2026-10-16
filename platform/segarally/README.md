# Native Sega Rally host

This dedicated host runs the generated Sega Rally i960 core and native Model 2A devices through `rt::Model2AMachine`. Mac builds default to direct Metal for the game, CRT and Settings, using the shared polygon/tile data and shaders. SDL3 still handles window/input/audio services; `--sdl-gpu` selects the comparison renderer. Windows/Linux and iOS retain their existing SDL GPU path. Native coverage remains partial, and unsupported hardware or missing code stops execution with a diagnostic.

`SEGARALLY_NATIVE_METAL=ON` is the macOS CMake default. Use `OFF` for an SDL-only Mac build; other platforms do not yet accept this option. `--native-metal` explicitly selects direct rendering. See [native renderers](../../docs/native-renderers.md) for the actual game comparisons and remaining migration work.

Use the [project wiki](../../docs/README.md), [Mac player manual](../../docs/mac-player-manual.md) and [build guide](../../docs/segarally-build-validation.md) for current instructions and evidence. Optional native SCSP sound requires `SEGARALLY_SOUND=ON` with an audited sound generation, then `--sound` at runtime. The validated sound builds also enable the exact instruction boundary hook. The v14 continuation passes the former missing return at `0x005ae800` through frame 14,001. The later v16 controller app closes normally after 12,974 frames with sound, CRT and zero logged audio shortfalls or drops. Live feedback from v15 reported title-screen audio glitches; later clean telemetry does not resolve listening quality. The separate v17 graphics candidate passes both 2,600-frame host comparisons and has launched windowed with sound, CRT and Accurate textures. Earlier apps and recordings are preserved.

SDL3 is pinned to Daytona's revision `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c` (3.4.16). Clone that revision into ignored `extern/sdl3`. The Settings panel also requires Dear ImGui at `f1cc2ae15e53a861a874c3034aae6798fde194ab` in `extern/imgui`. The [build guide](../../docs/segarally-build-validation.md#inputs-and-tools) lists the dependency setup commands. Generate the main CPU, TGP, and verified video imports into `build/segarally-timed` using the project pipeline first. The standalone frontend does not use the Daytona launcher.

Mac builds enable `SEGARALLY_INPUT_RECORDING=ON` by default. This builds optional runtime recording/replay support and requires the public-domain 7-Zip sources at `0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17` in ignored `extern/lzma`, fetched by `python3 scripts/setup.py`. Set `SEGARALLY_INPUT_RECORDING=OFF` when configuring to retain the minimal host build without this dependency. Runtime recording remains opt-in.

```sh
bash platform/segarally/build.sh mac
python3 platform/segarally/macos/launch.py
build/segarally-mac/segarally_native --frames 180 --unthrottled \
    --capture build/segarally-mac/metal.ppm \
    --reference-capture build/segarally-mac/reference.ppm
```

`--reference-capture` runs a second instance of the same generated core with the existing CPU renderer, checks matching instruction counts, PCs, cycle counts, and actual screen events, then saves the corresponding software frame. The Metal capture reads the hardware target back through a GPU transfer buffer. These captures compare rendering; they do not prove complete game coverage or physical board timing.

Add `--hidden` to run a bounded Mac capture without activating a visible window. The same Metal target and pipelines render the captured frame; the live frontend can remain open in a separate process.

Pass `--fullscreen` to the host or Mac launcher to use desktop fullscreen. Escape closes the app; steering uses the arrows, acceleration uses Z, and braking uses X.

The Mac launcher builds an ignored `.app` from the verified executable/assets and uses LaunchServices to leave the window open independently of a shell session. Its diagnostic output is `live-app.log` in the build directory. Pass `--build-dir build/segarally-mac-v3` to launch a different prepared generation.

Add `--no-launch` to package and sign without opening or activating a window.
It refuses to overwrite a running app and records the future argument list in
`launch-command.json` beside the build. No saves or recording are created by
packaging; `--no-launch` cannot be combined with `--record-inputs`. A failed process check
stops packaging rather than assuming the app is closed.

Interactive launches use normal board-time pacing; `--unthrottled` is an explicit diagnostic option. The host and launcher accept `--input-replay FILE`, reusing the boot tool's `segarally-input-v1` parser and completed-render boundary semantics. Replayed cabinet snapshots remain unchanged between events. The final snapshot is held for one completed frame, then keyboard/controller/touch take over after held inputs are released or centered. This interactive handoff intentionally differs from the headless boot tool, which retains its final snapshot indefinitely. Parsing failures stop before native execution. A paired software capture receives the identical inputs at every resumed step, including handoff.

For an audio diagnosis, the raw host accepts `--audio-output-capture output.wav`
alongside paced `--sound --audio-diagnostics --audio-capture native.wav`.
The two capture paths must differ, and the output WAV and its `.events.txt`
sidecar must be new. `native.wav` contains the original 44.1 kHz stereo SCSP
samples. `output.wav` contains only this app's logical SDL device mix after
resampling and stream/device gain, before the physical backend; it does not
record other apps or prove what the speakers played. Its WAV header preserves
the actual output rate and channel count. The sidecar records device format,
callback timestamps/frame ranges, producer queue levels and exact requested
rate-ratio bits. A bounded callback ring is drained to disk on the main thread;
overflow or a format change makes the capture fail explicitly. Capture stops
at the host loop's end without waiting for the remaining queued audio to play;
the sidecar records that queued input tail. Without this flag no postmix tap or
capture buffers are created. The ROM-free `segarally_audio_output_capture`
CTest uses only SDL's dummy audio driver.

For a fresh recorded session, add `--record-inputs build/recordings/drive-001` to the Mac launcher. The directory must be new; an already running app must be closed before recording can capture startup. `--input-replay FILE` may still supply menu selection before manual takeover. The recording stores the final twelve cabinet bytes at every actual instruction-bounded host resume, including changes between rendered frames. It preserves initial EEPROM/backup bytes, all ten Machine options, actual loaded image hashes, raw assets-report identity, executable identity, PCM bit hashes and fault/final-save evidence. Source-generation identities in the assets report remain `identity_only` evidence rather than independent authentication.

Replay with the exact captured executable, for example:

```sh
"build/segarally-mac/Sega Rally Native.app/Contents/MacOS/segarally_native" \
    --replay-recording build/recordings/drive-001
```

The early replay mode initializes no SDL, GPU or audio device and does not write live saves. It validates the complete bundle before executing any instruction, then compares exact resumes, return/fault details, PCM counts/bits and final save bytes. Changed executable or asset bytes are rejected. Preserve the complete recorded `.app` and matching staged assets; rebuilding, repackaging, or changing resources and re-signing can change their identities. A completion marker distinguishes quit, frame limit, instruction limit, native frontier and runtime fault. A crash or failed capture leaves an incomplete bundle that replay rejects. A complete recording or matching same-Machine replay does not establish a completed course, independent CPU parity, MAME fidelity or PCB fidelity. Recordings contain private state and remain in ignored build directories.

`SEGARALLY_NATIVE_BUILD` overrides the generation directory. Packaging verifies every bundled image against its import audit. Personal ROM-derived images, generated code, SDL source, and build products remain in ignored directories.

## Deferred iPhone target

The device target is preserved for later work. `SEGARALLY_DEVICE` selects the connected iPhone for signing; the default is the previously authorized device. Resume device builds and testing only when requested again.

The iOS bundle is `com.lgarciacarro.segarally.native`, separate from the earlier driving prototype. Its SDL3 scene startup follows the working Daytona lifecycle. EEPROM and backup RAM persist in Application Support; the earliest startup log is available through file sharing as `Documents/segarally-native.log`. Current input support is keyboard and touch; gamepads are not connected yet. Touch controls use the actual coin, start, view, steering, pedal, gearbox, and handbrake inputs. Arrow triangles steer, the lower right triangle accelerates, the adjacent square brakes, the top left yellow mark inserts a coin, and the top left triangle presses start. Keyboard controls appear in `--help`.
