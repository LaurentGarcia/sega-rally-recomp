# Mac player manual

This is a development build of Sega Rally Championship, using the original
game instructions recompiled from verified personal ROMs. The current Mac
checkpoint has sound-enabled Metal replay evidence clearing Desert and Forest,
then entering Mountain and retiring near its finish. A separate one-lap SHORT
Practice Mountain drive finishes in 1:16.23, reaches name entry and the score
table, then returns to attract mode.
All-course completion, exact graphics and hardware fidelity remain unverified.
See [current evidence](segarally-status.md).

## Start the game

Build a personal Mac app with the [one-command build](segarally-build-validation.md#build-the-mac-candidate-in-one-command):

```sh
python3 scripts/segarally_mac.py --roms /path/to/srallyc.zip \
    --output build/segarally-player --jobs 4
```

After the build succeeds, open `build/segarally-player/mac/Sega Rally Native.app`.
The current candidate 006 package is
`build/mac-candidate-006/mac/Sega Rally Native.app`, built at source `0b9ba0e`
with all eleven dedicated Mac CTests passing. It includes the Practice finish
and name-entry continuations and the earlier verified audio-buffer fix.
New sound-enabled Mac builds start with sound and the faster native execution
path when opened directly, without command-line arguments. The game opens in
a window and uses normal pacing. Allow its original reset checks to finish,
then insert two coins with **C** and press **Return**. Xbox controls are below.
CRT effects and Accurate textures are optional Settings controls.

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
Test menu's SHORT game-mode setting. Earlier captured apps, including candidate
003, predate the Practice callback fixes.

The launcher verifies assets, packages and signs the app, then opens it through
LaunchServices. It prints app and log paths. Add `--fullscreen` for desktop
fullscreen. An already running copy is activated; quit it before changing
startup options. Add `--no-launch` to package only and record the future command
in `launch-command.json`. This cannot be combined with `--record-inputs`.

Explicit command-line launches still require `--sound --instruction-boundary-hook`
to enable those features. A direct no-argument launch uses the standard
application save folder; the launcher uses `live-app-save` beside its build.
These are separate save sets, as described below.

Preserve any app used to record a drive. Repackaging or signing it again can
change its identity and make the old recording fail verification. Historical
preview paths and their validation records are retained in [HANDOFF](../HANDOFF.md).

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

New controller-enabled builds detect recognized SDL gamepads on connection.
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

New controller-enabled builds open **Settings** with **F1** or controller **Y**;
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
performance. It does not add CRT scanlines. All effects and Accurate textures
start off on a fresh setup; Smooth remains the default.

Changed presentation preferences are saved in `display.settings` inside the
supplied save directory (`live-app-save` beside a launcher build). They are
separate from EEPROM and backup RAM. The panel restores valid preferences on
later launches; malformed or oversized files are ignored with a warning.

The launcher accepts `--crt`, `--accurate-textures`, and
`--original-display`. The last selects default presentation for that session;
do not combine it with `--crt`. For a baseline image check, use
`--original-display` and omit `--accurate-textures`. The direct host also accepts
`--display-controls` to open the panel after its first rendered frame.
Native `--capture` images are taken before CRT effects and the overlay; they
still reflect the selected native texture-rendering mode. Offline recording
replay remains independent of the display panel.

## Sound, speed and saves

The one-command Mac build includes native sound. Opening its app directly
enables sound and the faster instruction-boundary execution path. For explicit
command-line launches, retain `--sound --instruction-boundary-hook`.
Performance depends on the Mac and scene. `--unthrottled` is a diagnostic option
and does not guarantee arcade speed. Avoid additional builds or captures during
a playtest.

The current package retains candidate 003's verified playback-buffer fix for a
captured 6.53 ms audio gap. It adds one output-device period of headroom, about
23 ms on the tested Mac. A complete visible 7,200-frame candidate 003 capture
has no shortfalls, drops or missing output samples while preserving the prior
native PCM byte for byte.
This resolves the captured starvation mechanism; broader listening feedback on
the earlier title-screen report is still welcome. Device periods longer than
60 ms are unsupported; see the [audio details](segarally-build-validation.md#optional-sound-experiment).
Add `--audio-diagnostics` to a paced launcher command with `--sound` when
investigating; it timestamps queue demand and host delays without changing audio
generation or buffering. See [current evidence](segarally-status.md) for the
measured scope.

A direct Finder launch stores cabinet saves and display/control preferences in
`~/Library/Application Support/sega-rally-recomp/SegaRallyNative/`.
The launcher uses `live-app-save` beside its build and writes `live-app.log`.
These save sets are separate. Preserve the log before relaunching because it is
replaced. The direct host accepts `--save-dir DIR` for an isolated save set.
Keep existing saves when testing a fresh configuration.

If the window stops with a `frontier` title, preserve the log, build path,
replay name and visible game state. This means a native-code or hardware path
still needs implementation. If controls appear inactive, check window focus
and whether the selection replay has handed over. Phone testing remains deferred.

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
The sound-on scripted validation now matches recorded and unrecorded runs
through frame 2,801, including their WAV, final image and saves. The later v14
manual session also replays all 18,689 resumes, PCM hashes and final saves
through frame 9,344 with the exact signed app. The later v16 analog-input session
replays all 25,953 resumes and PCM hashes through frame 12,974 with its original
signed app and final saves unchanged. The preserved v17 session matches all
9,459 resumes, PCM hashes covering 3,625,487 stereo frames and final saves
through frame 4,729 with its exact signed executable. Candidate 002 also replays
its 20,000-frame sound-on course run exactly: all 40,006 resumes, PCM hashes and
final saves match. The source run applies 4,398 input events and clears Desert
and Forest before retiring near Mountain's finish. It is unthrottled and opens
no playback device, so it does not measure live speed or sound quality. These
checks establish repeatability
for the captured drives. Candidate 006's separate SHORT Practice Mountain
recording also replays exactly through all 18,000 frames: 36,014 resumes,
13,799,492 stereo PCM frames and final saves match with the same signed app.
That drive includes the one-lap finish, name entry, score table and attract
return. It opens no playback device and does not measure live speed or listening
quality. Remaining course finishes and full arcade comparison still need
validation. See the [input guide](segarally-input-replay.md#exact-host-resume-transcripts).
