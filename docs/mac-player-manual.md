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

The current preserved v13 playtest is in
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

The current v13 speed playtest has **sound off**. The optional sound build and
`--sound` runtime switch are described in the [build guide](segarally-build-validation.md).
Nonzero PCM capture and a short SDL/CoreAudio output check are established.
A normal-paced sound build reaches the race but cannot yet supply samples fast
enough for continuous playback, so sound may break up. User listening confirmation
remains pending. Silence in the sound-off speed app is expected.

The measured indexed Mac checkpoint remains below real time. Game time can
advance more slowly than wall time; `--unthrottled` disables waiting for
diagnostic runs and does not guarantee arcade speed. Avoid additional builds
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
