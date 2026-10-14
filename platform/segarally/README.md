# Native Sega Rally host

This dedicated host runs the generated Sega Rally i960 core and native Model 2A devices through `rt::Model2AMachine`. It reuses Daytona's SDL_GPU polygon and System 24 tile renderer, with Metal required on Apple targets. The active target is Mac; phone work is deferred. Native coverage remains partial, and unsupported hardware or missing code stops execution with a diagnostic.

Use the [project wiki](../../docs/README.md), [Mac player manual](../../docs/mac-player-manual.md) and [build guide](../../docs/segarally-build-validation.md) for current instructions and evidence. Optional native SCSP sound requires `SEGARALLY_SOUND=ON` with an audited sound generation, then `--sound` at runtime. The latest sound build also enables the exact instruction boundary hook; its normal-paced driving check reports zero audio input shortfalls or sample drops. The older v13 speed playtest has sound off. Listening confirmation remains open.

SDL3 is pinned to Daytona's revision `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c` (3.4.16). Clone that revision into ignored `extern/sdl3`. Generate the main CPU, TGP, and verified video imports into `build/segarally-timed` using the project pipeline first. The standalone frontend project does not require ImGui or the Daytona launcher.

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

Interactive launches use normal board-time pacing; `--unthrottled` is an explicit diagnostic option. The host and launcher accept `--input-replay FILE`, reusing the boot tool's `segarally-input-v1` parser and completed-render boundary semantics. Replayed cabinet snapshots remain unchanged between events. The final snapshot is held for one completed frame, then keyboard/touch take over with neutral inputs where no control is held. This interactive handoff intentionally differs from the headless boot tool, which retains its final snapshot indefinitely. Parsing failures stop before native execution. A paired software capture receives the identical inputs at every resumed step, including handoff.

`SEGARALLY_NATIVE_BUILD` overrides the generation directory. Packaging verifies every bundled image against its import audit. Personal ROM-derived images, generated code, SDL source, and build products remain in ignored directories.

## Deferred iPhone target

The device target is preserved for later work. `SEGARALLY_DEVICE` selects the connected iPhone for signing; the default is the previously authorized device. Resume device builds and testing only when requested again.

The iOS bundle is `com.lgarciacarro.segarally.native`, separate from the earlier driving prototype. Its SDL3 scene startup follows the working Daytona lifecycle. EEPROM and backup RAM persist in Application Support; the earliest startup log is available through file sharing as `Documents/segarally-native.log`. Current input support is keyboard and touch; gamepads are not connected yet. Touch controls use the actual coin, start, view, steering, pedal, gearbox, and handbrake inputs. Arrow triangles steer, the lower right triangle accelerates, the adjacent square brakes, the top left yellow mark inserts a coin, and the top left triangle presses start. Keyboard controls appear in `--help`.
