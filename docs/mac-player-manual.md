# Mac player manual

This manual covers the experimental native Mac player. The prebuilt app needs
no Python, CMake, Xcode, compiler or network connection; its first-run importer
prepares game data from your own verified ROM ZIP. Developer source builds are
also supported. The recorded Mac
checkpoint has sound-enabled Metal replay evidence clearing Desert and Forest,
then entering Mountain and retiring near its finish. A separate one-lap SHORT
Practice Mountain drive finishes in 1:16.23, reaches name entry and the score
table, then returns to attract mode.
All-course completion, exact graphics and hardware fidelity remain unverified.
See [current evidence](segarally-status.md).

## Start the game

Download the published [Mac Player Preview 1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.1).
See [package and platform status](release-platforms.md) for checksums and exact
validation limits. The package targets Apple Silicon and Metal, with a macOS 13
deployment target; execution on macOS 13 and Intel Macs is unvalidated. It is
ad-hoc signed, not notarized. The withdrawn compiler-bootstrap previews are
separate from this prebuilt player.

Open the **ROM-import edition** DMG, copy **Sega Rally Native.app** to Applications,
and open it. Choose your personally supplied complete Revision B (`srallycb`)
ZIP in the native file picker. Its filename may be `srallyc.zip`; supported bytes,
not the filename, identify the revision. The app checks size, CRC32 and SHA1 for
all required files, assembles eight runtime images, and checks their fixed SHA256
identities before saving an import. No game download or runtime compilation is
involved. Subsequent launches reuse the verified local import.

The separate **personal edition** already includes its owner's verified game
data and skips the picker. Keep it private. The ROM-import edition contains no
ROM archive or extracted game assets, but both editions contain statically
recompiled game instructions. The project's BSD license covers original runtime
and tool code, not Sega game content.

The game opens windowed with normal pacing. Sound, instruction-boundary
execution and Arcade CRT are enabled on direct startup with fresh preferences.
Saved display choices take precedence. Allow the original reset checks to finish,
then press and release **C twice**, with a gap between presses, and press
**Return**. Xbox controls are below. Startup is initially silent; attract sound
begins after the reset checks. Accurate textures remains opt-in in Settings.

Cancelling the picker exits before starting the game. An unsupported ZIP reports
an error and lets you choose again. Existing imports and cabinet saves remain
intact. The importer does not move or modify your selected archive.

To choose a ZIP directly:

```sh
"/Applications/Sega Rally Native.app/Contents/MacOS/segarally_native" \
    --rom /path/to/srallyc.zip
```

Add `--import-only` to verify and save the data without starting the game or
opening a graphics/audio device. `--rom` enables player defaults; `--player`
applies them when launching an existing import from the command line. Use
`--save-dir DIR` to keep a separate import/save set. `--data DIR` is a developer
asset-directory override and cannot be combined with `--rom`.

## Controls

Click the game window to give it keyboard focus.

| Key | Action |
| --- | --- |
| Left / Right arrows | Steer left / right |
| Z | Accelerator |
| X | Brake |
| C | Coin |
| Return | Start |
| V | Change view |
| A / S | Gear down / up |
| Space | Handbrake |
| F2 | Test |
| F3 | Service |
| F1 | Open Settings; pause or resume |
| F6 | Toggle CRT effects |
| Escape | Cancel remapping, close Settings, or quit when the panel is closed |

These defaults come from the [Rally controls](../src/app/segarally_controls.cpp).
Keyboard steering and pedals currently use full-scale cabinet values; releasing
steering centers it. Gear inputs change on each key press. Optional touch controls can be shown with the
host's `--touch` flag.

The player detects recognized SDL gamepads on connection.
Xbox defaults are:

| Controller | Action |
| --- | --- |
| Left stick | Analog steering |
| RT / LT | Analog accelerator / brake |
| RB / LB | Gear up / down |
| A | Change view |
| X | Handbrake |
| Back / View button | Coin |
| Start / Menu button | Start |
| Y | Open or close Settings |

Open **Settings → Controls** with F1 or Y. Controller input is optional: disable
it there while retaining keyboard/touch input. Adjust stick and trigger dead
zones, steering response and inversion; the meters show the adjusted input.
Select a keyboard or controller binding, release the input used to select it,
then press the replacement key/button or move the desired stick/trigger.
Escape cancels capture. F1, F6, Escape and controller Y remain reserved.
Bindings can be cleared individually or restored to defaults. Changes are
saved in `controls.settings`, separately from display preferences and cabinet
saves.

Opening Settings pauses the game. Held keys, buttons and axes must be released
or centered before controlling the game again. Disconnecting the controller
releases its inputs; keyboard controls remain available. Wheels and force
feedback are not part of this controller implementation.

With a fresh verified cabinet configuration, press and release **C twice**,
with a gap between presses. One coin displays `CREDIT 1/2`; it does not buy a
game. Press Return when a full credit is available. Use steering and accelerator
on the selection screen; the recorded successful path selects the Lancia with
automatic transmission. Saved cabinet settings can change credit behavior.

## Display controls

Open **Settings** with **F1** or controller **Y**;
picture adjustments are on its **Display** tab. The game and
scripted cabinet inputs pause while it is open. Choose **Resume**, press F1,
or press Escape to return to play. Inputs held when closing the panel must be
released before they can control the cabinet, so Return used for Resume cannot
also press Start.

**Original** switches CRT effects off. **Arcade CRT** enables the preset;
**CRT effects** is the master switch. Adjust Scanlines and Phosphor mask from
0 to 1, Glow from 0 to 0.5, Curvature from 0 to 0.15, and Color fringing from
0 to 1 source pixel. These are optional display effects, not a claim to
reproduce a particular arcade monitor.

**Smooth** is the original linear scaling; **Sharp** selects nearest sampling
when CRT effects are off. **Integer scale** uses whole-number scaling when
there is enough space. **Accurate textures** is a separate native rendering
option that improves measured texture edges and interpolation but can reduce
performance. It does not add CRT scanlines. The current player starts Arcade
CRT enabled for fresh player preferences. Accurate textures remains opt-in;
Smooth remains the scaling default. Saved preferences take precedence. Explicit
diagnostic commands retain their existing opt-in defaults, and
`--original-display` disables the effects for that run.

Changed presentation preferences are saved in `display.settings` inside the
supplied save directory (`live-app-save` beside a launcher build). They are
separate from EEPROM and backup RAM. The panel restores valid preferences on
later launches; malformed or oversized files are ignored with a warning.

The developer launcher accepts `--crt`, `--accurate-textures`, and
`--original-display`. The last selects default presentation for that session;
do not combine it with `--crt`. For a baseline image check, use
`--original-display` and omit `--accurate-textures`. The direct host also accepts
`--display-controls` to open the panel after its first rendered frame.
Native `--capture` images are taken before CRT effects and the overlay; they
still reflect the selected native texture-rendering mode. Offline recording
replay remains independent of the display panel.

## Sound, speed and saves

The prebuilt player and sound-enabled source builds include native sound.
Direct startup, `--player` and `--rom` enable sound and the faster
instruction-boundary execution path. Explicit diagnostic commands keep those
features opt-in with `--sound --instruction-boundary-hook`.
Performance depends on the Mac and scene. `--unthrottled` is a diagnostic option
and does not guarantee arcade speed. Avoid additional builds or captures during
a playtest.

The current playback buffer includes one output-device period of headroom in
addition to its 60 ms base. This fixes a measured starvation mechanism without
changing native PCM. A visible 7,200-frame validation capture has no shortfalls,
drops or missing output samples. Broader listening quality and hardware fidelity
remain separate checks; see [the evidence record](segarally-status.md).
Device periods longer than 60 ms are unsupported; see the
[audio details](segarally-build-validation.md#optional-sound-experiment).
Add `--audio-diagnostics` to a paced launcher command with `--sound` when
investigating; it timestamps queue demand and host delays without changing audio
generation or buffering. See [current evidence](segarally-status.md) for the
measured scope.

A direct Finder launch stores cabinet saves and display/control preferences in
`~/Library/Application Support/sega-rally-recomp/SegaRallyNative/`.
Imported images live in its `imported-game/import-…/` subfolders; `current.txt`
selects the completed import. Keep the whole application-support folder when
backing up saves. A failed import does not replace a previous import or EEPROM.
The developer launcher uses `live-app-save` beside its build and writes `live-app.log`.
These save sets are separate. Preserve the log before relaunching because it is
replaced. The direct host accepts `--save-dir DIR` for an isolated save set.
Keep existing saves when testing a fresh configuration.

If the window stops with a `frontier` title, preserve the log, build path,
replay name and visible game state. This means a native-code or hardware path
still needs implementation. If controls appear inactive, check window focus
and whether the selection replay has handed over. Other platform readiness is tracked in [platform status](release-platforms.md).

## Developer builds and scripted selection

To build your own personal app from source, follow [getting
started](getting-started.md#build-from-source-instead) and the [build
guide](segarally-build-validation.md#build-the-mac-candidate-in-one-command).
The source-build tools below are optional and are not required by the prebuilt
player. Commands run from a prepared repository checkout.

For automatic coin/start/Lancia selection followed by manual control, use:

```sh
python3 platform/segarally/macos/launch.py \
    --build-dir build/segarally-player/mac --sound --instruction-boundary-hook \
    --input-replay config/segarally_lancia_select.inputs
```

The selection replay chooses the automatic Lancia, then hands over to keyboard
or controller. Release held buttons and center the stick when control passes
to you. The longer driving replays are for validation and keep scripted control
for their entire sequence.

New builds also provide two Practice selection scripts. Replace the
`--input-replay` file above with one of these:

| Practice course | Selection script |
| --- | --- |
| Mountain | [segarally_practice_mountain_select.inputs](../config/segarally_practice_mountain_select.inputs) |
| Lakeside | [segarally_practice_lakeside_select.inputs](../config/segarally_practice_lakeside_select.inputs) |

Both choose the automatic Lancia through the original game menus, then release
all controls for manual driving. Lakeside uses the game's built-in gear/brake
selection sequence. These timed scripts assume fresh default cabinet settings;
saved settings or credits can change the menu sequence. Use a fresh build/save
directory and preserve existing saves. These scripts retain the default NORMAL
game mode; the separate one-lap finish validation uses the original cabinet
Test menu's SHORT game-mode setting. Older captured builds may predate the Practice callback fixes; preserve their
exact executables when comparing recordings.

This developer launcher verifies assets, packages and signs the app, then opens it through
LaunchServices. It prints app and log paths. Add `--fullscreen` for desktop
fullscreen. An already running copy is activated; quit it before changing
startup options. Add `--no-launch` to package only and record the future command
in `launch-command.json`. This cannot be combined with `--record-inputs`.

Explicit diagnostic commands keep sound and the hook opt-in; add
`--sound --instruction-boundary-hook`, or use `--player` for player defaults. A direct no-argument launch uses the standard
application save folder; the launcher uses `live-app-save` beside its build.
These are separate save sets, as described above.

Preserve any app used to record a drive. Repackaging or signing it again can
change its identity and make the old recording fail verification. Historical
preview paths and their validation records are retained in [HANDOFF](../HANDOFF.md).

## Record and replay a drive

A prepared host built with recording support accepts
`--record-inputs build/recordings/drive-001` on its launch command. Quit any
running copy first and choose a new recording directory. A selection replay
can start the session before keyboard takeover; the recording retains the
actual controls used between frames and initial saves, plus hashes that check
whether replay generates the same sound.
Quit normally to finish the bundle. A crash leaves an incomplete recording;
frontier and fault endings remain explicitly identified.

Replay with the exact captured executable using
`--replay-recording DIR --data ASSETS`, where ASSETS is its matching staged
assets directory. Offline replay opens no window or audio device and changes
no live saves. Preserve the recorded app and assets: rebuilding, re-signing or
repackaging can change their bytes, and changed identities are rejected.
Exact replay has been checked on recorded sound-on driving and Practice sessions,
including a 20,000-frame Championship path and an 18,000-frame SHORT Practice
Mountain finish/name-entry/scoreboard path. Replay verifies recorded inputs,
execution outcomes, PCM hashes and final saves using the original executable.
It opens no playback device and is not a speed, listening or independent-hardware
test. A completed capture does not imply all courses are implemented or validated.
See the [input guide](segarally-input-replay.md#exact-host-resume-transcripts) and
[status reference](segarally-status.md) for exact recorded checkpoints.
