# Evidence and status

Snapshot: **2026-10-14**. macOS is the active target; phone work is deferred.
This reference summarizes the latest recorded checkpoints. Exact hashes,
failure history and ignored evidence locations are in [HANDOFF.md](../HANDOFF.md).
The [design](sega-rally-design.md) records interpretation and hardware limits.

| Area | Established evidence | Limit |
| --- | --- | --- |
| Main native coverage | Frozen v14: 75,207 instructions, 52 native objects, no missing templates | Coverage is a reached graph, not a remaining-work percentage |
| Interactive Mac | v15 sound/CRT session exits normally after 30,328 frames; user reports excellent image and near-perfect-feeling motion | Qualitative feedback; complete courses and hardware fidelity remain open |
| Driving parity | Two billion held-gas instructions, 86,328,770 ordered writes, 4,650,936,033 cycles, 113,101 IRQs and 10,701 renders; nineteen replay events match | Sound off; independent CPU with shared boards and final-frame comparison; no full MAME race |
| Later game paths | Fresh credit/start after attract reaches a second race; sound-on frame 12,800 shows 118 km/h in fourth gear, 15.83 seconds elapsed | Complete second race and Game Over overlay not separately validated |
| Course progress | Desert clear/results show 1:12.15; forested second course enters, times out, then attract returns | Centered steering does not complete second-course bends; remaining courses unvalidated |
| Metal fidelity | Optional native repeated stepping is integrated into v15; three complete scenes fall from 2,519 differing pixels to 5, and 447 synthetic cases pass | Default OFF; sustained cost, continuous flicker and whole-game fidelity remain open |
| Sound generation | Native linked board captures 2,409,170 stereo frames: 54.6297052 seconds including nonzero 44.1 kHz PCM; shared-core drains agree | Whole-game audio parity unvalidated |
| Audio output | Earlier v15 playtest has four SDL estimated demands; later v16 manual test and fresh 7,200-frame neutral capture have zero demands/drops; all 5,519,841 neutral-capture PCM frames match recorded hashes | User reports title-screen glitches; clean later telemetry does not establish audible quality or resolve the cause |
| Controller input | Xbox One Elite 2 detected; 329 mapping/virtual-device checks, normal build, menu Metal checks and actual-host input isolation pass; live recording contains intermediate steering, throttle and brake values | Recorded cabinet inputs do not identify their physical source; Y/navigation feedback remains open; wheels/force feedback remain outside this implementation |
| Exact host recording | Actual manual v14 drive replays all 18,689 resumes, PCM hashes and final saves through frame 9,344; earlier scripted recorded/unrecorded checks also match | Same-Machine reproducibility; full-course and independent CPU/sound parity remain open |
| Independent host recording | Actual sound-off driving bundle passes 5,201 resumes, 500,035,460 instructions, 15,156,607 ordered writes, 1,130,001,001 cycles and 2,600 frames after immutable preflight | Independent i960 with shared boards; longer/manual captures, sound-on and hardware parity remain open; fault rows rejected |
| Hardware fidelity | Isolated device/reference tests and instruction-timed diagnostics | PCB timing, extF80 precision and full-machine behavior remain open |

The separate v16 controller app exited normally after a 12,974-frame windowed
test with sound, CRT and queue diagnostics. It supplied 9,946,388 stereo frames
with zero logged SDL shortfall requests or chip/host drops. A frozen prefix
contains 2,726 cabinet-input changes after scripted control ended, including
intermediate analog values. Settings-button feedback and title-screen audio
quality remain open. Before launch, its 2,600-frame scripted drive preserved
v15 native audio/image, saves and ordered inputs exactly. The app and complete
user recording remain preserved; completion alone is not a replay result.

## Measured Mac speed

The earlier quiet hidden-window Metal race comparison used the same 480 rendered frames,
87,173,426 main instructions and 8.344320 seconds of board time, with sound off:

| Routing | Wall time | Frames per wall second | Board time / wall time |
| --- | ---: | ---: | ---: |
| Original | 14.163164 seconds | 33.8907 | 58.9156% |
| Indexed | 11.871813 seconds | 40.4319 | 70.2868% |

Indexed lookup alone gives about a 19% wall-speed gain, still below real time. All 66 progress,
input and exit rows match. It is one measured Mac checkpoint, not a performance
guarantee. Hidden acquisition does not measure visible VSync blocking; GPU
encoding time is CPU work, not GPU completion time. The game clock was unchanged.
The indexed v6 independent parity transcript also matches the original report
byte for byte.

The newer sound-enabled instruction boundary hook reaches the 2,760-frame
driving checkpoint in **48.519993 seconds**, including process startup and exit,
for **47.98148 seconds of board time**. SDL reports zero input shortfall requests
and zero chip/host sample drops; 2,282 audio frames remain queued at exit. The
previous outer-loop sound test reported 3,222 input-demand requests. These are
whole-process sound checks, separate from the sound-off race segment above.

The hook preserves the prior game and sound data: actual Machine comparison
matches all 3,142 frames through 600 million instructions, and the entire
54.6297052-second WAV matches the old output byte for byte. SDL stayed supplied
through this quiet hidden-window check. The user has heard a separate ten-second
native PCM sample; longer paths and physical audio fidelity remain separate checks.

The longer independent CPU comparison passes the held-gas path through the
known first-course results, second-course timeout and return to attract. All
nineteen input instruction/cycle boundaries, terminal PC, frame counts and
final framebuffer hash match the preserved native two-billion run. This uses
the separate reference CPU with shared board/TGP/geometry implementations,
sound disabled and a null instruction callback. It compares CPU state, writes
and events at instruction boundaries, with full video RAM/framebuffer checks
at the terminal checkpoint. It does not compare every rendered framebuffer
or extend the sound-enabled Machine scheduler test beyond its prior checkpoint.

The separate stable-host restart check uses fresh saves and holds scripted
controls through frame 11,160. After the known attract checkpoint, two coins
and Start bring up Car Select with countdown 08. All 27 inputs apply, with no
manual takeover or frontier, through 2,090,501,029 instructions. The menu
does not display a numeric credit balance, and this check stops before a
second race. Sound is off; unthrottled diagnostic cost is not a gameplay-speed
measurement.

The sound-enabled v14 extension now also starts and drives that second race.
It preserves the 26 credit-restart events, repeats the known selection timing
after the second Start, then holds gas. All 32 events apply through frame
12,800 without a frontier or keyboard takeover. The first 26 logged event
boundaries match the sound-off restart. The final Metal capture shows an active
race at 118 km/h in fourth gear, 15.83 seconds elapsed and position 15/15.
The cockpit view does not directly establish the car's identity. All 25,605
recorded resumes match scripted pins and their PCM hashes match the WAV;
9,812,993 stereo frames are generated with zero chip/host drops. This hidden
unthrottled test uses no physical audio device and is not a speed measurement.

The earlier LaunchServices sound attempt loaded its bundled selection replay
and initialized the machine, then exited at `AudioQueueStart` with CoreAudio
error `-66681` (`kAudioQueueErr_CannotStart`). Direct windowed and hidden probes
also failed while the standalone native PCM player succeeded. Moving only audio
opening after window/Metal setup, following Daytona's order, passes a new
60-frame windowed test with zero drops or estimated input shortages. The boot
PCM is silent. The signed windowed app subsequently applies all ten selection
events, hands control to the keyboard at frame 1,851 and exits normally after
9,347 frames. It supplies 7,165,805 stereo frames with zero chip/host drops or
estimated SDL input shortages and 2,255 frames queued at exit. This session has
no separate wall-speed measurement. Live-game listening feedback is pending.

The sound-on recording checks match a 60-frame boot and a 2,801-frame drive,
including 5,603 exact driving resumes and the full 48.69424036-second float WAV.
Offline replay runs inside the sandbox without SDL/GPU/audio output or live-save
writes. The capture runs are hidden and unthrottled without a physical audio
device; these results do not establish listening quality or gameplay speed.

The v13 signed-app playtest records 25,581 resumes before reaching missing
native target `0x005ae800` at 2,437,360,267 instructions and 12,785 frames.
Normal close seals the frontier recording. It supplies 9,802,212 stereo frames
with zero chip/host drops or estimated SDL shortages. This path is beyond the
two-billion checkpoint. A single observed return seed extends v14 coverage by
two addresses, including the canonical ROM alias. The fresh sound-enabled
continuation matches all 25,580 completed normal resumes and PCM observations
before the old partial frontier row, then exits normally at frame 14,001:
2,675,194,732 instructions and 6,084,875,601 cycles. Its 28,013 PCM observations
match the complete 10,733,721-stereo-frame WAV. The old partial CPU outcome is
excluded from the equality claim. This is a same-Machine comparison across
generations, not an independent hardware test or a v14 offline replay.

The v14 windowed app subsequently applies all ten selection events, hands over
at frame 1,851 and closes normally after 9,344 frames. It supplies 7,163,505
stereo frames, with zero chip/host drops and one SDL input-shortfall request
for 552 extra input bytes. There is no WAV or separate speed measurement for
this session. The automated continuation, rather than this shorter live
session, establishes progress beyond the former missing return.
The live bundle contains 18,689 resumes and actual post-handoff accelerator,
steering, brake, view and handbrake changes. Its signature, assets, initial/
final saves and completion record pass read-only audit. Exact signed-executable
offline replay subsequently matches every resume, PCM count/hash and final
save, preserving the entire app, bundle and live saves. This opens no output
device and establishes reproducibility of the recorded manual drive.
See the [input guide](segarally-input-replay.md#exact-host-resume-transcripts).

## Graphics evidence

The first paired car frame differs in 15,830 of 190,464 RGB pixels (8.31%),
with mean absolute channel error 0.7278/255. Re-rendering that frozen CPU scene
as triangle fans reduces the difference to 1,602 pixels and 0.040408/255,
attributing 94.45% of mean error to interpolation. This diagnosis does not
establish exact race rendering. A stable picture and correct polygon ordering
remain distinct from pixel fidelity.

The optional scanline implementation uses the CPU reference's two edge chains
for 4–8 vertex solids and textures. Triangles use their separate edge slopes
and parameter planes. Rounded spans control coverage over a conservative
rectangle; texture shading remains on the GPU. The three complete-scene
measurements below precede the triangle extension.

| Captured frame | Stable differing RGB pixels | Candidate differing RGB pixels | Stable mean channel error | Candidate mean channel error |
| --- | ---: | ---: | ---: | ---: |
| 1,634, first car | 15,830 | 822 | 0.727817 | 0.019426 |
| 2,329, race start | 13,896 | 1,244 | 0.577714 | 0.027496 |
| 2,599, driving | 21,236 | 599 | 0.698309 | 0.013448 |

Each image is 496×384; error is the mean absolute RGB channel difference on a
0–255 scale. These frozen scenes use the earlier sound-off generations. They
do not measure current v14 gameplay speed. The option remains off by default;
disabled captures match the committed stable renderer. The expanded synthetic
Metal fixture passes 447 native-resolution cases, including rounded edges,
clipping, crossed chains, triangle subpixel/empty spans, checker pixels and
layered transparent/opaque texels.

The stray white pixel at (125,75), attributed to solid polygon 353, now matches
the CPU's black pixel. Ten pixels improve to exact matches versus the previous
prototype; one at (147,20) worsens, but its ownership is now attributed.
The CPU's triangle 800 owns that pixel with RGB (74,84,6); both GPU paths omit
its fragment before alpha shading. The old GPU instead fills it from polygon
804 with RGB (38,44,0), although that polygon's CPU span is empty. Corrected
4–8-vertex coverage rejects 804 and exposes polygon 850's black pixel.
This polygon fix therefore does not exclude the legitimate CPU span. Exact
hardware subpixel quantization is not established. Instrumentation matches both unmodified GPU
outputs over all 190,464 RGBA pixels. An initial mixed shader/source fixture
was invalidated before this attribution; see the handoff for evidence.

The triangle extension now makes (147,20) exact. In the saved 3D-only
reconstruction, differing pixels fall from 969 to 844 and mean channel error
from 0.0215019 to 0.0188207/255. Of 127 changed pixels, 126 become exact;
one at (95,31) worsens slightly through accumulated interpolation rounding.
Both paths select the same triangle and plane coefficients; an isolated
repeated-addition shader restores the CPU result. Across the saved 3D layer,
the diagnostic makes five pixels exact, reducing differences from 844 to 839
without worsening another pixel. All 447 synthetic cases also pass in this
mode. The loop is not in production and its cost is unmeasured. This snapshot
lacks tile/character RAM, so its counts are separate from the complete-scene
table above. Default-off output remains byte-identical in this comparison.
The CPU's repeated floating-point additions are not reproduced exactly by
the per-fragment formula. Broader scenes, continuous flicker and performance
remain to test before enabling this path for play.

The next diagnostic confirms that four-or-more-vertex interpolation rounding
crosses texture cutout thresholds at three large-error examples. Actual GPU
ownership measurements agree with the CPU prediction. Repeated stepping for
those polygons reduces the same frozen 3D scene to eight differing pixels;
combining it with triangle repeated stepping leaves three, with mean channel
error 0.000042002688/255 and maximum channel error 4. Of 843 changed pixels,
842 become exact; one previously exact pixel at (434,94) worsens slightly.
The remaining differences are at (294,94), (434,94) and (98,331), all owned by
opaque quads. The subsequent GPU probe confirms identical owners. The first
two differ in accumulated U/z; a one-ULP change to the CPU step reproduces the
measured GPU words, though the GPU step itself remains inferred. The last has
identical repeated parameters but a measured one-ULP reciprocal-z difference
which moves U across an integer truncation boundary. These are arithmetic
differences at the measured points, not evidence of changed polygon ownership.

The first instrumented candidate changed one baseline pixel even with its
diagnostic mode disabled and was rejected. The second separates the new
branch from baseline polygon shading: its disabled output matches all 190,464
RGBA pixels and all 447 synthetic cases pass. No production shader change
has been made; subsequent broader-scene and timing evidence follows below.
Three residual pixels in one snapshot do not establish whole-game
or hardware fidelity, or resolve continuous flicker.

The broader complete-scene gate now passes too. Historical CPU/stable images
and exact game-state witnesses match, then exported raw tile/character RAM
reconstructs all decoded layers, flags, registers and palette pens. Default
OFF and diagnostic mode-0 guards are exact in all three complete RGBA images.

| Complete scene | Current optional differing pixels | Combined repeated stepping | RGB absolute-error sum, current → repeated |
| --- | ---: | ---: | ---: |
| 1,634 | 697 | 3 | 9,568 → 24 |
| 2,329 | 1,241 | 1 | 15,147 → 13 |
| 2,599 | 581 | 1 | 7,015 → 11 |

These are 496×384 full images, including the HUD. Across them, 2,517 pixels
become exact and three previously exact pixels worsen, one per scene. The two
driving residuals remain unattributed. An independent audit recomputes the
counts and decodes the tile/character layers from raw bytes. These historical
v5/v6 scenes do not establish current-v14 whole-game fidelity or continuous
flicker. Production remains unchanged.

One bounded offscreen timing run on Apple M5 (10 GPU cores) passed all image
guards, with 32 samples per arm/scene after warmup:

| Scene | Default OFF | Current optional | Diagnostic direct | Diagnostic repeated |
| --- | ---: | ---: | ---: | ---: |
| 1,634 | 1.168 ms | 1.491 ms | 1.408 ms | 2.119 ms |
| 2,329 | 1.319 ms | 1.541 ms | 2.257 ms | 2.126 ms |
| 2,599 | 1.913 ms | 2.585 ms | 2.258 ms | 2.527 ms |

These are median completed-render latencies including CPU encoding, submission
and a GPU completion wait. Readback, file I/O and presentation are excluded;
per-draw fences serialize completion. The first-car repetition costs about 51%
against its diagnostic direct control, consistently across blocks. Driving
controls vary too much to infer a general loop cost or speedup. This is not
pure GPU timing or live gameplay FPS. A clean candidate now passes fresh image
guards: all 447 synthetic cases and all three complete saved scenes match the
accepted diagnostic ON output, while actual default OFF matches historical
stable/current OFF exactly. This clean common shader is integrated behind the
existing option; it repeats only at native scale. Application integration and
paced gameplay measurement follow.

### Optional CRT and Display controls

Newly built hosts now include a separate CRT presentation pass and F1 Display
panel. CRT scanlines, phosphor mask, glow, curvature and color fringing are
adjustable; F6 toggles the effect. Fresh defaults keep CRT and Accurate textures
off, the panel closed, and the original linear scaling. Display preferences
persist separately in `display.settings`. Guest execution and cabinet replay
pause while the panel is open; native captures and offline replay retain their
separate paths.

The CRT gate passes 18 synthetic draws and two saved-scene draws. Display
preferences pass 250
ASan/UBSan checks, and the hidden 640×480 offscreen UI smoke verifies a visible
Resume button, unchanged preferences and zero backend calls on closed frames.
Earlier failed fixture attempts remain preserved: the sandbox could not list
displays, and the initial UI fixture omitted ImGui's layout-only first frame.
No production behavior was changed to bypass either check. Evidence remains
ignored under `build/validation/display-panel-smoke/ui-004/` and the CRT
candidate's `run-01/`.

The v15 host and both platform CTests pass. A 2,600-frame sound-enabled
off/on comparison preserves native images, PCM, ordered resumes and saves
exactly. That hidden run precedes the final held-key release guard and does
not alone establish visible CRT presentation. The final app separately
launched windowed with sound, and its log confirms CRT swapchain initialization.
Sustained effect cost, monitor accuracy and whole-game parity remain open.
Keep effects disabled for baseline image comparisons.

The final host's separate held-key test passes eight guest resumes across
12 host iterations, including three paused iterations. Keys held on F1 panel
close remain neutral until released; fresh presses then control the cabinet.
This checks the actual host transition, not ImGui Resume-button activation.
The user subsequently reported a good image and near-perfect-feeling motion;
the 30,328-frame session ended normally, with title-screen audio glitches and
four SDL estimated input shortfalls. A subsequent paced 2,600-frame sound/CRT
diagnostic preserves native PCM, image, saves, inputs and ordered resumes
exactly. Its 44 telemetry rows contain no estimated demand shortfalls or drops.
Startup includes 158.83 ms maximum pacing lateness and a 114.82 ms presentation
wall interval; neither is GPU execution time. The run leaves neutral title
behavior at frame 1,200, so the reported cover-screen glitch remains open.

## Next validation work

Localize the reported title-screen audio glitches with paced queue/timing
telemetry and native PCM, then measure longer game paths with sound enabled. Preserve exact instruction guards, timing
and IRQ boundaries in further speed work. Validate
the complete second race and remaining courses, then extend independent replay
comparison to those paths and coupled sound. Real-race graphics
comparison, full MAME-machine parity and PCB/precision validation remain required.

Inherited Daytona results and the earlier lifted-C Rally prototype provide
reference code and historical evidence. They do not satisfy these Rally exit
criteria. No complete milestone or release readiness is claimed.
