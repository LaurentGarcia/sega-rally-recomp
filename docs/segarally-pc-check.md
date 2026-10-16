# Check a Windows or Linux player

Use [segarally_pc_check.py](../scripts/segarally_pc_check.py) on the actual PC
to collect a bounded, reproducible report from a released player. This optional
diagnostic needs Python 3.10+; playing the game does not. It needs no compiler,
Vulkan SDK, rebuild or network connection. The script and the
[driving input file](../config/segarally_lancia_drive.inputs) can be downloaded
separately, or run from an existing source checkout.

Keep the extracted player package intact. Supply your own supported ROM ZIP,
or use `--data` with an existing verified image directory. Personal packages
can use their adjacent `assets` directory without either argument. Every check
requires a new output directory and creates isolated saves; it does not change
your normal cabinet saves or display preferences. Outputs include private game
images, audio and recording data. Keep them local rather than committing or
uploading the complete directory.

## Scripted comparison on the PC

From the source checkout, run this in Windows PowerShell, adjusting the paths:

```powershell
py -3 scripts/segarally_pc_check.py --player "C:\Games\Sega-Rally\segarally_native.exe" --rom "C:\Games\srallyc.zip" --output "pc-check-001" --expect-hardware
```

On Linux:

```sh
python3 scripts/segarally_pc_check.py --player "$HOME/Games/Sega-Rally/segarally_native" --rom "$HOME/Games/srallyc.zip" --output pc-check-001 --expect-hardware
```

If downloading the files separately, replace `scripts/segarally_pc_check.py`
with its saved path and add `--input-replay /path/to/segarally_lancia_drive.inputs`.
Run the executable directly through the checker, rather than passing a launcher
script to `--player`. The current Linux release requires X11, or XWayland on a
Wayland desktop; native Wayland was not compiled into that release.

The checker runs SDL Vulkan and direct Vulkan sequentially with fresh saves,
sound, Arcade CRT, input recording, native PCM capture and logical-device audio
capture. Each run is limited to 2,800 game frames, approximately 49 seconds of
board time, and 600 seconds of wall time. Do not operate the controls during
scripted comparison. A slow machine can take longer than board time; these
diagnostic runs are not a performance benchmark.

`--expect-hardware` requires the selected direct Vulkan device to match a loader
inventory entry reported as an integrated or discrete GPU. Software, virtual
and unknown devices produce a finding. Device names alone are not treated as
hardware proof, and the released SDL path does not identify its selected
physical device. The report records container/Wine markers and graphics/audio
environment overrides. It does not infer a physical monitor or speaker from a
successful Vulkan inventory query.

Add `--validation` only if the Khronos Vulkan validation layer is installed;
the released player intentionally fails if the requested layer is unavailable.
The ordinary check does not install anything. A new `--output` is required for
every attempt, including failed attempts.

## Exercise keyboard or controller input

Connect the controller before launching. For a separate session, use:

```powershell
py -3 scripts/segarally_pc_check.py --player "C:\Games\Sega-Rally\segarally_native.exe" --rom "C:\Games\srallyc.zip" --output "pc-controls-001" --interactive --renderers native --expect-hardware
```

Use the equivalent `python3` command on Linux. This records your inputs instead
of injecting the driving script, with a 7,200-frame limit and the same wall-time
bound. Press and release **C twice**, then **Return**. Exercise left and right
steering, gas and brake. Open **F1** Settings, check the selected controller and
bindings, then resume. Observe window resizing and focus loss, and exit with
**Escape** when finished. A normal early exit is accepted in interactive mode.

The report identifies the controller logged at startup and the cabinet pin
changes actually recorded. Those pin changes alone cannot distinguish a real
gamepad from keyboard or injected input. Record which device you used, whether
Settings paused correctly, and whether sound was audible without glitches in
`operator-observations.txt`. Speaker, controller-source and visual observations
remain explicit human observations rather than automatic pass claims.

## Read the result

`result.json` contains executable and input hashes, selected device evidence,
commands, exit state, recording seals, frame and audio metrics, saves and
artifact hashes. Each recording's completion seal is checked against the
actual manifest, transcript, observations and final saves; initial seed and
transcript context identities are checked too. This verifies artifact integrity,
not replay against an independent arcade reference.

For scripted comparisons, the checker compares terminal execution, native PCM,
final saves, recorded observations and the final raw image between renderers.
Logical-device output can differ with host scheduling and is reported separately.
Audio drops, device input shortfalls, silence, an incomplete scripted run or
renderer differences produce `completed_with_findings` and exit code 2. Startup,
capture, seal or process failures produce `failed`, also with exit code 2.
`passed` exits with code 0. Logs and partial evidence are retained on failure.

`native.ppm` is captured before CRT and Settings; it does not prove the displayed
CRT appearance. Output audio is this application's logical mix before the
physical backend, not a microphone recording. `--audio-driver dummy` is available
for an explicitly simulated diagnostic environment and is recorded in the report.
A short `--frames 120` plumbing check can legitimately report silence during
startup; it does not replace the full scripted or manual test. No result here
certifies an entire race, hardware precision or physical speaker fidelity.
