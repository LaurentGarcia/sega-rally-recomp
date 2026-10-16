# Practice finish and restart regression

[`config/segarally_short_mountain_finish_restart.inputs`](../config/segarally_short_mountain_finish_restart.inputs)
is a frozen 1,901-event cabinet-input replay for **SHORT Practice Mountain,
followed by a new Championship Desert race**. It runs for 22,000 completed
game frames from fresh cabinet saves, using verified Revision B game data.

The replay enters the game's Test/Service menu, selects NORMAL difficulty and
SHORT game mode, then drives the Lancia with automatic transmission. It changes
only cabinet inputs. Game instructions, physics, timers and clocks are unchanged.
The SHORT operator setting remains active in the restarted Championship race.
This is not a completed NORMAL Championship or a complete Lakeside race.

## Run on Mac

Use a built player or the installed Mac app, and supply the location of your
verified game data. Run these commands from the repository root. The new run
directory must not already exist; keep its saves separate from your normal
player saves because the replay changes operator settings and records a score.

```sh
mkdir -p build/course-regression
mkdir build/course-regression/run-001

"/path/to/Sega Rally Native.app/Contents/MacOS/segarally_native" \
  --data "/path/to/verified/assets" \
  --save-dir "$PWD/build/course-regression/run-001/saves" \
  --native-metal --sound --instruction-boundary-hook \
  --hidden --unthrottled --crt --accurate-textures \
  --input-replay "$PWD/config/segarally_short_mountain_finish_restart.inputs" \
  --frames 22000 --instructions 10000000000 \
  --record-inputs "$PWD/build/course-regression/run-001/recording" \
  --audio-capture "$PWD/build/course-regression/run-001/native.wav" \
  --capture "$PWD/build/course-regression/run-001/native.ppm" \
  --log "$PWD/build/course-regression/run-001/host.log"
```

Do not interact with cabinet controls during the replay. The final input at
frame 21,999 keeps scripted ownership through the bound. A graphical desktop
session is still required for this hidden Metal run. Unthrottled sound is
generated and captured without device playback; the image is captured before CRT
presentation. The test does not measure speaker quality, presentation pacing or
CRT appearance. Omit `--reference-capture`: it runs a second software-rendering
machine on every frame, which is unnecessary for this regression.

## Observed coverage

| Completed frame | Observed state in the preserved CPU captures |
| ---: | --- |
| 10,200 | Mountain Stage Clear, rank second, lap/total 1:16.23 |
| 12,000 | Name entry |
| 14,100 | Mountain Top 8 includes the new 1:16.23 score |
| 14,400 | Return to the title sequence |
| 18,000 | Attract mode, INSERT COIN(S) |
| 19,000 / 19,020 / 19,040 | New coin, coin and start inputs |
| 21,600 | New Championship Desert race visibly moving |
| 22,000 | Requested frame bound reached normally |

The current CPU run preserves all 602 instruction/cycle checkpoints and 99
shared numbered images from the previous 18,000-frame SHORT Practice result.
The previous final image also equals the new image at frame 18,000.

The signed personal Mac package's direct Metal run reaches **4,208,428,228
instructions, 9,561,241,000 reference cycles, PC `005dde3c`**, exit zero.
All 1,901 events apply, with none pending and no manual takeover. All 4,401 CPU
frame checkpoints, final EEPROM/backup saves and the final Accurate-textures
image match the frozen CPU run. Its 16,866,030 native stereo float PCM frames at
44.1 kHz are finite, nonzero and have zero chip/host drops. An offline artifact
audit verifies all 44,014 sealed resume records, their continuity and every
cabinet-pin value. The 203.28-second elapsed diagnostic run is not a gameplay
speed measurement. These are comparisons using the same native Machine, not
independent MAME-machine or PCB certification.

## Frozen evidence

The local evidence root is
`build/worktrees/rc-courses-001/build/course-coverage/`. Game captures, PCM,
recordings and generated code stay outside source Git; only the original
cabinet-input replay is included here.

| Artifact under that root | SHA-256 |
| --- | --- |
| `short-mountain-finish-restart.inputs` (same bytes as the tracked config) | `94a36fd909030d7330bdbe16fbfb1d33e189b7d2d11641c7b6b8329d51510b2e` |
| `short-mountain-restart-001/result.json` | `203c325b4d369bbe9b7ad85d362c78c43e68b2dd0695a8e5f3b6f59904d60942` |
| `cpu-coverage-review.json` | `4fabc182e58a35ead7e4e60441b18e84bbacac378113388b6dbd2a20dc7c9926` |
| `baseline-compatibility.json` | `9390858805abd193afc1772fb0e00b04070be16e841a894d3c13fcd70372c6bb` |
| `metal-short-restart-003/result.json` | `c5ff8bc0fa5b64d7e97855b366377c2f1cd7edc1e7cfeed306993a29da60c153` |
| `recording-review.json` | `66067af8322f5cb20d06a2f8ae496c7aac76023aa0501b79ccaffb82c8adcf0f` |
| `metal-short-restart-003/native.ppm` | `04e98438a536efd887a21186e1bd1dfd99127f657966446b43555c97eb958f64` |
| `metal-short-restart-003/native.wav` | `136f964e55600a0bb6271c7e820c8925dfbb8a7d6372bc682412da4e8e16002a` |
| `metal-short-restart-003/recording/resumes.txt` | `66897a2fd3f23f0aa8f9ccaedc458ff311b559099cabaf83a48bfad605bd05d5` |
| `metal-short-restart-003/recording/complete.txt` | `0a003962bab02e6fbba5a2d44680aa9264b1c315f328ac9cbe84fda5c12e46ed` |

The tested signed executable is
`build/installers/mac-direct-metal-personal-001/stage/Sega Rally Native.app/Contents/MacOS/segarally_native`,
SHA-256 `9d21b0cb7ee5ad84f5effd5a20f58d1534ac6ffc0f7ccc1cf12686260f795738`.
The public import package shares the game implementation; this exact executable
identity refers to the local personal package.

Earlier attempts remain preserved. `metal-short-restart-001` terminated in the
sandbox before recording a game run; its result hash is
`f66d70950fbf99719310182f88b29016206c2b263186a6005f4f3715300bbf87`.
The second attempt unnecessarily enabled the second CPU renderer and was stopped
at frame 20,222 after 858.05 seconds; its frame-bound assertion correctly failed.
Its result hash is `6f7c32da28b37708cb353e601c222fae6841e46915302f650040e94cd0297b12`.
Neither failure was relabeled as the completed 22,000-frame pass.

Separate NORMAL input explorations remain incomplete: the best Championship
run clears Desert in 0:59.60 and Forest in 1:20.85, then retires at Mountain route
segment 293/300. A Lancia Practice Lakeside run completes lap one in 1:19.75 but
retires on lap two. The CPU review binds those attempts, including unsuccessful
steering, anticipation, braking and predictive-input experiments. These do not
establish a missing native-code defect or full-game completion.
