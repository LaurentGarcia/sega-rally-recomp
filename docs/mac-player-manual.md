# Mac player manual

This is a development build of Sega Rally Championship, using the original
game instructions recompiled from verified personal ROMs. The current Mac
checkpoint can drive the Lancia through Desert and enter the second course.
All-course completion, exact graphics and hardware fidelity remain unverified.
See [current evidence](segarally-status.md).

## Start a prepared app

Run commands from the repository root. A prepared Mac host can be packaged and
opened in a normal window with:

```sh
python3 platform/segarally/macos/launch.py --build-dir build/segarally-mac
```

The launcher verifies the prepared assets, creates `Sega Rally Native.app`,
signs it and opens it through LaunchServices. It prints the app and log paths.
Windowed mode is the default; `--fullscreen` requests desktop fullscreen.
Normal board-time pacing is the default. Startup can take time while the game
executes its reset checks. The launcher activates an already running copy of
the same app; quit it before changing startup options.

When `--input-replay` is supplied, the launcher copies that file into the app's
resources before signing. Startup reads the bundled copy. Missing files,
directories and replays larger than 1 MiB are rejected before packaging.

The latest prepared sound playtest is in
`build/worktrees/boundary-hook/build/mac-boundary-sound-v13-ordering/Sega Rally Native.app`.
Launch it with its sound and execution options using:

```sh
python3 platform/segarally/macos/launch.py \
    --build-dir build/worktrees/boundary-hook/build/mac-boundary-sound-v13-ordering \
    --sound --instruction-boundary-hook \
    --input-replay config/segarally_lancia_select.inputs
```

These flags matter: opening the executable without them retains its default
sound-off, original execution mode. Quit a running copy before relaunching with
different options.

The earlier sound app exited during CoreAudio startup with
`AudioQueueStart: -66681`. The latest build opens audio after the window and Metal
renderer and passes a 60-frame windowed startup check. The longer timing results
below came from a separate direct-executable test. See
[current evidence](segarally-status.md) for the test scope.
The subsequent windowed launch applies the selection replay, hands control back
at frame 1,851 and exits normally after 9,347 frames with sound enabled and zero
reported audio drops or input shortages.

The earlier sound-off v13 playtest is preserved in
`build/worktrees/indexed-dispatch/build/mac-playtest-v13/Sega Rally Native.app`.
It was launched windowed with normal pacing, selection replay and sound off.
Its session uses `live-window-save` and `live-window.log` beside the app.
This is a local checkpoint path, not a distributable installation.

For a newly prepared host, automatic coin/start/Lancia selection followed by
manual play is available with:

```sh
python3 platform/segarally/macos/launch.py --build-dir build/segarally-mac \
    --input-replay config/segarally_lancia_select.inputs
```

The script inserts two coins, starts and selects the automatic Lancia. It holds
the final input snapshot through one completed frame, then hands controls to
the keyboard. Keyboard input takes effect after that handoff. The longer driving
and held-gas replays override controls during their recorded drive before the
interactive host hands back control. Use the selection-only replay for play.

## Controls

Click the game window to give it keyboard focus.

| Key | Cabinet input |
| --- | --- |
| Left / Right arrows | Steer left / right |
| Z | Accelerator |
| X | Brake |
| C | Coin |
| Return | Start |
| V | Change view |
| A / S | Gear down / up |
| Space | Handbrake |
| F2 | Service |
| F3 | Test |
| Escape | Quit |

These bindings come from the [dedicated frontend](../src/app/segarally_main.cpp).
Keyboard steering and pedals currently use full-scale cabinet values; releasing
steering centers it. Gear inputs change on each key press. Gamepads and wheels
are not connected to this host. Optional touch controls can be shown with the
host's `--touch` flag.

With a fresh verified cabinet configuration, press and release **C twice**,
with a gap between presses. One coin displays `CREDIT 1/2`; it does not buy a
game. Press Return when a full credit is available. Use steering and accelerator
on the selection screen; the recorded successful path selects the Lancia with
automatic transmission. Saved cabinet settings can change credit behavior.

## Sound, speed and saves

The latest sound build uses the exact instruction boundary hook. Its normal-paced
driving check completes 47.98 seconds of board time in 48.52 seconds including
startup and shutdown, with no reported audio input shortfalls or sample drops.
The [build guide](segarally-build-validation.md) explains both build options
and the runtime switches `--sound --instruction-boundary-hook`. The older
sound-off speed app remains silent by design. The user has heard a ten-second
native PCM sample on the Mac speakers. The completed windowed session supplies
7,165,805 stereo frames with no drops or input shortages; live listening feedback
remains pending.

Other paths and machines may run more slowly. `--unthrottled` disables waiting
for diagnostic runs and does not guarantee arcade speed. Avoid additional builds
or captures during a live speed playtest.

The launcher normally keeps EEPROM and backup RAM in `live-app-save` beside
the build and writes `live-app.log`. Preserve the log before relaunching because
the host opens it for replacement. The direct host accepts `--save-dir DIR`
for an isolated save set. Use a new ignored directory for a fresh-state check;
keep the existing saves for comparison.

If the window stops with a `frontier` title, the machine reached missing code
or unsupported hardware. Preserve the log, build path, replay name and visible
game state. Such a stop is an explicit incomplete path. If controls appear
inactive, check focus and whether the selection replay has handed over. Phone
builds and device testing are deferred.
