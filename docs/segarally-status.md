# Evidence and status

Snapshot: **2026-10-17**. Mac remains the principal gameplay validation target.
Preview 4 provides public Mac, Windows and Linux binaries; personal iPhone
packaging remains local. Windows audio was confirmed by a user on a PC with
the preceding SDL package. The Mac game now defaults to direct Metal
for game, CRT and Settings, with SDL retained for window/input/audio services.
The 2,600-frame driving comparison preserves native execution/audio/saves and
final images; a separate 2,100-frame visible run reports no audio shortfalls
or drops. Direct Vulkan now passes the same three-arm 2,600-frame driving
comparison on Linux/lavapipe with actual X11 presentation and zero validation
errors. Preview 4 adds native Wayland and a window-restoration fix in Linux,
retaining the earlier Mac and Windows binaries. Three 2,800-frame Wayland game
runs and a fresh archive/import run preserve execution, native PCM, images and
saves; dummy-audio shortfalls remain recorded. Direct Vulkan remains
opt-in; Windows/vendor-GPU execution is not established. The Windows game
cross-build, dependencies and Wine startup/import checks pass.
See [platform status](release-platforms.md) for current packages and
[native renderers](native-renderers.md) for backend evidence and remaining work.
This reference summarizes the latest recorded checkpoints. Exact hashes,
failure history and ignored evidence locations are in [HANDOFF.md](../HANDOFF.md).
The [design](sega-rally-design.md) records interpretation and hardware limits.

## Latest development checks

The [22,000-frame Practice finish/restart replay](course-regression.md) now
covers SHORT Practice Mountain through Stage Clear, results, name entry, Top 8
and attract, then a new Championship Desert start. The signed personal Metal
player matches all 4,401 CPU frame checkpoints, final saves and the final
Accurate-textures image. The 16,866,030-frame stereo PCM stream has finite samples
and contains nonzero audio. This hidden unthrottled run does not test speakers or
pacing, and it does not complete NORMAL Championship or Lakeside.

The same input sequence also completes a separate raw i960 comparison:
4,208,428,228 instructions, 134,770,594 ordered writes and 233,617 IRQ entries
agree with the inherited interpreter. All 1,901 cabinet-input boundaries and
terminal state/pixels correlate with the Metal recording. This comparison has
sound off and shared CPU helpers/board/TGP implementations; it is not a full
independent MAME-machine, sound or extF80 hardware certification. The linked
course regression documents that distinction and the frozen evidence.

A new 18,000-frame paced visible Mac attract run exposes intermittent host
delivery stalls: 20 audio shortfall requests and 20,854 host-dropped frames,
with no chip drops. Added opt-in timing separates event handling, PCM capture,
submission, recording, diagnostic writes and requested/actual sleep. A separate
instrumented repeat preserves native PCM, execution, final image and saves,
and records no shortfalls or drops. The long stalls did not recur, so their
cause remains unresolved; the repeat is not proof of a correction. Earlier
shorter audio passes and the measured buffer remedy retain their narrower scope.

Direct Vulkan now recreates stale presentation state after a hidden/minimized
window is restored, fixing a measured Wayland FIFO presentation hang. The new
native Wayland fixture passes 110 checks and 68 presentations under headless
Weston/software Vulkan, with no validation errors. X11 still passes 101
presentations and all nine images match its preceding baseline. A separate CI
job covers Wayland. Preview 4 publishes the Wayland-enabled Linux player;
Preview 3's older Linux binary remains X11-only.

The [exact-scene Metal/Vulkan analysis](native-renderers.md) reproduces 951
default-image pixel differences despite identical projected vertices and CPU
images. All 940 differences with unchanged polygon ownership change integer
texture coordinates and texels; 11 ownership changes involve cutout textures.
Unmodified Accurate textures matches across APIs and with CPU for that frame.
The analysis narrows the cause, without establishing temporal-flicker coverage
or a production shader correction. Default rendering is unchanged.

## Public source prerelease

`v0.1.0-rc.1` packages the source project and documentation for local Mac builds.
It does not distribute a playable app, ROMs or generated game code. See the
[release scope](releases/v0.1.0-rc.1.md) and [setup guide](getting-started.md).
The recorded gameplay checkpoints below retain their original local identities;
source publication and host regression checks do not extend their fidelity scope.
The [source history map](source-history.md) resolves original local commit IDs
to their public counterparts.

## Recorded Mac candidate

The preserved Candidate 006 gameplay checkpoint is generated from the supplied ROM archive at source revision
`0b9ba0eba47d6effa3378f9a2b7fae9010a1c11e`, with all eleven dedicated Mac CTests
passing. Its signed package is `build/mac-candidate-006/mac/Sega Rally Native.app`;
the executable SHA-256 is
`9fef7f0ef4e6dde4ff7b47d44666a0b360de79a7e1e8034295df84d6e138753e`.
The one-command pipeline
reproduces import, generation, sound-enabled Release build, tests and packaging;
see the [build guide](segarally-build-validation.md). This remains a local
development/test candidate, not a certified full-fidelity or public release.

The previously missing drive `epr-17891.ic12`, communication `epr-16726.bin`
and billboard `epr-18022.ic2` firmware now pass size, CRC32 and SHA1 checks from
additional personal archives. The staged Revision B reference set passes
installed MAME 0.289's `-verifyroms srallycb` audit; see the
[reference setup](segarally-build-validation.md#reference-firmware). No game ran
in that audit. Full-machine tracing/comparison and PCB fidelity remain open;
the installed MAME binary's source revision is not established.

The preserved candidate 003 visible 7,200-frame neutral check enables CRT and
Accurate textures and reports zero audio demands, chip/host drops or capture failures.
All 14,404 input/execution records, observations and 5,519,841 native stereo PCM
frames exactly preserve the earlier neutral capture; its final native image
also equals candidate 002's Accurate-textures image. All 5,516,288 captured SDL
output frames reproduce bit for bit using the current buffer policy and measured
delivery schedule, with zero short callbacks, missing frames or queue mismatches.
This verifies the captured starvation fix on one visible Mac run. Listening
confirmation, other devices and independent hardware fidelity remain separate.
Its longest post-prime producer pause was 18.46 ms; the earlier 52.92 ms pause
did not recur. The separate replay of the earlier delivery schedule establishes
that the new buffering covers that specific longer pause.
Integrity evidence: `build/validation/mac-release-audit/candidate003-audio-review.json`.

The preserved candidate 002 no-argument startup runs 1,293 frames over a bounded 25-second check
with Metal, native sound and the instruction boundary hook enabled. Enhancements
are off. One SDL estimated input shortfall is logged. This validates the startup
defaults, not listening quality. Earlier clean telemetry must not be read as
resolving every reported audible issue.

Candidate 002's paced 7,200-frame paired capture preserves all 5,519,841 native stereo
PCM frames byte for byte against the earlier neutral WAV. Its SDL logical-device
postmix footer reports zero capture drops, overflow or failure. One SDL input
shortfall request estimates 2,296 extra bytes, so this run fails its zero-demand
criterion. Independent artifact audit passes native PCM, recorded resumes and
capture integrity. The postmix is this application's SDL mix before CoreAudio,
not a microphone or hardware-loopback recording.

Analysis finds a 288-frame / 6.530612 ms output zero tail absent from native PCM,
after a 52.918 ms producer pause. An offline SDL schedule reproduces every
captured postmix frame exactly. This localizes an observed output-starvation
mechanism; it does not identify the producer pause's cause or resolve all
reported title glitches. Candidate 003 adds one actual output-device period to
the 60 ms queue target; the measured device now uses 83.22 ms. The same recorded
demand schedule has no shortage with this policy, and the actual visible test
above passes. The original failed playback criterion remains
preserved. Integrity evidence is
`build/validation/mac-release-audit/candidate002-audio-review.json`.

The preserved candidate 002 Metal sound-on course replay applies all 4,398 input
events through 20,000 frames, 3,662,512,354 instructions and 8,692,041,001 reference
cycles, ending at PC `0x005ca464`. It supplies 15,332,761 stereo PCM frames with
zero chip/host drops and no frontier or manual takeover. All 670 recorded
frame/instruction/cycle checkpoints match the CPU-only sound-off path. Desert
clears in 1:00.38 and Forest in 1:21.37; Mountain enters and retires near its
finish. That Championship replay does not establish Mountain completion, Lakeside or
full-game completion; the separate Practice evidence below has a different scope.
This is an unthrottled diagnostic without device playback, not a speed or
listening measurement. Default-OFF Metal pixels are not claimed equal to the
software renderer.

Independent artifact audit passes; the exact signed-executable offline replay
matches all 40,006 resumes, cabinet pins, PCM hashes and final saves. It takes
168.85 seconds, a replay duration rather than a gameplay benchmark. The app,
original recording and saves remain unchanged. This is same-Machine
reproducibility, not independent CPU/device or full MAME-machine parity.
Reports are `build/validation/mac-release-audit/championship002-integrity.json`
and `build/validation/mac-release-audit/championship002-offline-001/result.json`;
the latter SHA-256 is
`9b89fd17a7185d1cb2e2e39301cb0db4fa18427e8c3d8d11bd41113f2f0b5024`.

Candidate 006 also repeats the NORMAL 20,000-frame sound-on Metal path with
Accurate textures enabled. All 40,006 resume payloads, observations, the whole
native WAV and initial/final saves match candidate 002 exactly. Its N/C/frame/PC
terminal is unchanged; a default-OFF versus Accurate-ON image comparison is not
asserted. Report:
`build/validation/mac-candidate-003-courses/candidate006-normal-001/capture-001/result.json`,
SHA-256 `9e61b629bfa287f8f727aea435ef4a36a73ee09773063483d64e6933f1a79df1`.

## Practice coverage and bounded callback discovery

Sound-off CPU helpers use fresh FF cabinet seeds, actual frame-based inputs and
the game's ordinary Test/Service settings. With normal difficulty and the
operator's SHORT mode, Practice Mountain displays STAGE CLEAR, rank second,
with lap and total time 1:16.23. This is the game's one-lap mode, not evidence of
finishing the normal Championship Mountain race. A separate normal-mode Practice
Lakeside run reaches route segment 297 of 300 before the 80-second retirement;
its 14,000-frame terminal is 2,676,028,109 instructions / 6,084,441,002 cycles.
The preserved CPU images are under
`build/worktrees/course-driving/build/course-driving/short-practice-mountain-v20-attempt-001/guest/`
(`frame-10200.ppm`), and the Lakeside terminal is under
`build/worktrees/course-driving/build/course-driving/practice-lakeside-drive-attempt-001/guest/`.
Lakeside completion remains unverified. The consolidated SHORT Mountain CPU
path passes 18,000 frames / 3,456,982,099 instructions / 7,822,841,003 cycles,
ending at PC `005de474`. The observed sequence reaches STAGE CLEAR, name entry,
the MOUNTAIN STAGE TOP 8 scoreboard (DELTA AT first in 1:16.23), then attract at
frames 15,000 and 18,000. The final screen reads INSERT COIN(S) / CREDIT 0/2.
The 18,000-frame CPU report is
`build/worktrees/course-driving/build/course-driving/short-practice-v29-18000-frozen/report.json`,
SHA-256 `7bcd02faf5842621b1e1fd60c88161aaff96700cc42abd2401dbb513a9e395aa`.
Independent artifact review is
`build/validation/mac-release-audit/short-practice-v29-18000-review-001/result.json`,
SHA-256 `02eb5ff52213cf78f5713f385a29b4a66487fa4547987cd0b2e441685d2c0591`.

Candidate 006 repeats this complete SHORT path with sound, Metal and Accurate
textures. All 1,880 events apply without manual takeover, the N/C/frame/PC
terminal matches exactly, and the final 496×384 RGB image equals the CPU image.
It records 36,014 resumes and 13,799,492 native stereo PCM frames with zero
chip/host drops. This hidden unthrottled check has no device playback or CRT;
it does not establish listening quality or independent sound/device fidelity.
Report:
`build/validation/mac-candidate-003-courses/candidate006-short-mountain-001/capture-001/result.json`,
SHA-256 `e19040f8f5d95beec00193da3d1a691c6feef936bdfd3bbc0e805e5e6d192f66`.
Its exact signed-executable offline replay also passes all 36,014 resumes,
PCM hashes covering 13,799,492 stereo frames, results/fault fields and final
saves. The 18,000-frame terminal matches, with the signed app, original capture
and saves unchanged. It uses the early offline consumer without SDL, GPU or
device playback. This is same-Machine reproducibility, not independent CPU or
hardware validation. Its 153.832-second duration is not a gameplay benchmark.
Report:
`build/validation/mac-release-audit/short-mountain-final-offline-001/result.json`,
SHA-256 `c1b5827af4bed202159e3652e02163f787d424445346a20d76c475e342041fdb`.

The separate candidate 006 Lakeside gate also passes 14,000 frames, applying all
2,168 events and recording 28,011 resumes / 10,732,954 native stereo PCM frames.
The terminal instruction count (2,676,028,109), cycles (6,084,441,002) and frames
match the sound-off CPU helper, and its final image equals the CPU image. Host
PC is `005ca4b0`; the original CPU fixture did not record a terminal PC, so PC
equality is not asserted. Zero chip/host drops and unchanged frozen inputs are
verified, with no device playback. This reaches the previously observed
retirement, not a Lakeside finish. Report:
`build/validation/mac-candidate-003-courses/candidate006-lakeside-001/capture-001/result.json`,
SHA-256 `6e5a57043df39d339f1f94817a78ca36a461798d6108f36fe028af08523c82f5`.
Candidate 003's earlier visible audio check remains historical. Independent
artifact review of all three candidate 006 course gates passes at
`build/validation/mac-release-audit/candidate006-courses-review-001/result.json`,
SHA-256 `e3ca6adffb2f6b9bb69ecc4f0ccf1ef02413aa7806eb0cbc62db607f2b21b3a6`.

A single independent i960 comparison follows the SHORT Mountain input stream
for exactly 2,000,000,000 instructions. It matches 58,120,650 ordered writes,
4,539,611,659 reference cycles and 110,449 IRQ entries, ending at PC `005a37a4`
after 10,445 renders. All 1,877 supplied events apply, with none pending.
Read-only witnesses prove successful comparison at the new Practice start
callback `005c0fa0` and finish callback `005bfcf0`. The two first execute at
instruction counts 1,068,614,675 and 1,921,603,185, respectively. The comparison
uses unchanged I960Ref execution with independently instantiated shared board
models, sound off and idle UART receive. It does not test resident
Model2AMachine scheduling, the separate retirement callback, later post-result
callbacks, full MAME behavior or PCB fidelity. All 200 frozen inputs remain
unchanged; no retry is used. Report:
`build/validation/practice-short-v20b-i960-reference/attempt-001/result.json`,
SHA-256 `e4df250c8811ec7e8caa288a0712bc5496ed17a93ae67b670313d915d552a044`.

The subsequent dispatcher at `005b7690` exposes a specific static-discovery gap:
a literal mask bounds a four-entry indexed load, and its combined nonzero test
branches directly to the call through that loaded register. A separate decoded
proof recognizes this layout without widening the existing saved-object callback
proof. One hash-pinned manifest row derives four aligned, reset-copy targets;
operand, mask, scale, branch, index and tracked-register mutations are rejected
after recomputing evidence hashes. These remain table-derived candidates,
separate from observed execution. Neither live RAM-table immutability nor
exclusive incoming control flow is inferred. Existing live instruction-word
guards are unchanged. No whole-ROM pointer scan or generated-code edit is used.

## Recorded evidence

| Area | Established evidence | Limit |
| --- | --- | --- |
| Main native coverage | Candidate 006: 79,783 instructions, 54 chunks plus the dispatch table, no missing templates | Coverage is a reached graph, not a remaining-work percentage |
| Interactive Mac | Latest v17 sound/CRT/Accurate-textures session exits normally after 4,729 frames and replays exactly; v16 also exits normally after 12,974 frames | Qualitative feedback and bounded session evidence; complete courses and hardware fidelity remain open |
| Driving parity | Two billion held-gas instructions, 86,328,770 ordered writes, 4,650,936,033 cycles, 113,101 IRQs and 10,701 renders; nineteen replay events match | Sound off; independent CPU with shared boards and final-frame comparison; no full MAME race |
| Practice paths | SHORT Mountain Stage Clear, second in 1:16.23, name entry/Top 8 and attract; candidate 006 sound-on Metal matches the full 18k terminal/final CPU image; independent 2B comparison covers start/finish callbacks | Lakeside sound-on Metal also matches terminal N/C/frames and final CPU image, but retires; unthrottled/no device playback, no Lakeside finish or full Championship completion claim |
| Later game paths | Fresh credit/start after attract reaches a second race; sound-on frame 12,800 shows 118 km/h in fourth gear, 15.83 seconds elapsed | Complete second race and Game Over overlay not separately validated |
| Course progress | Candidate 002 sound-on Metal replay clears Desert 1:00.38 and Forest 1:21.37, enters Mountain and retires near its finish; all 670 CPU-only frame/instruction/cycle checkpoints match | Unthrottled, no device playback; that Championship path does not establish Mountain completion or Lakeside; full-game completion remains open |
| Metal fidelity | Optional native repeated stepping plus certified reciprocal matches all pixels in three saved scenes; 447 synthetic cases pass. Both v17 2,600-frame host checks pass: OFF matches v16; ON's final image matches software, with audio/inputs/saves preserved. v17 has launched windowed | Final-image comparison before CRT/UI, using the same runtime; no all-frame or independent CPU claim. Default OFF, continuous flicker and whole-game fidelity remain open |
| Sound generation | Native linked board captures 2,409,170 stereo frames: 54.6297052 seconds including nonzero 44.1 kHz PCM; shared-core drains agree | Whole-game audio parity unvalidated |
| Sound CPU/board comparison | Actual initialization matches persistent MAME CPU execution for 1,000,000 instructions, including 383 IRQ/RTE pairs and 35,955 SCSP writes; synthetic UART/nonzero-PCM cases also pass | The actual initialization's 51,158 PCM frames are silent; shared devices, instruction-start MMIO and idle serial do not establish music or independent sound-on recording parity |
| Authentic sound commands | Corrected independent MAME CPU comparison passes 14,947,325 instructions, 14,614 IRQ entries and all 1,929 cuts through 55 actual UART accesses; 728,884 PCM frames match, including nonzero audio. The native payload also matches the preserved neutral host WAV prefix | Shared device models and instruction-start MMIO; no independent chip, physical bus-phase, title-music or whole sound-on recording claim. The earlier observer failure remains preserved |
| Audio output | Candidate 003 fixes the reproduced 6.53 ms starvation gap; its visible 7,200-frame capture has zero demands/drops, exact native PCM and no missing SDL output frames in complete offline reproduction | One measured Mac/device run; broader listening confirmation, device changes and full sound fidelity remain open |
| Controller input | Xbox One Elite 2 detected; 329 mapping/virtual-device checks, normal build, menu Metal checks and actual-host input isolation pass; live recording contains intermediate steering, throttle and brake values | Recorded cabinet inputs do not identify their physical source; Y/navigation feedback remains open; wheels/force feedback remain outside this implementation |
| Exact host recording | Candidate 006 SHORT matches 36,014 resumes, 13,799,492 PCM frames and final saves; candidate 002 matches all 40,006 resumes, 15,332,761 PCM frames and final saves; preserved v17 matches 9,459 resumes and 3,625,487 PCM frames; v16 matches 25,953 resumes | Same-Machine reproducibility; full-course and independent CPU/sound parity remain open |
| Independent host recording | Actual sound-off driving bundle passes 5,201 resumes, 500,035,460 instructions, 15,156,607 ordered writes, 1,130,001,001 cycles and 2,600 frames after immutable preflight | Independent i960 with shared boards; longer/manual captures, sound-on and hardware parity remain open; fault rows rejected |
| Hardware fidelity | Isolated device/reference tests and instruction-timed diagnostics | PCB timing, extF80 precision and full-machine behavior remain open |

The latest v17 playtest closed normally at frame 4,729 after 890,065,301 main
instructions and 2,055,264,421 cycles, with zero logged audio shortfalls or drops.
Its exact signed-executable offline replay passes all 9,459 resumes, PCM hashes
covering 3,625,487 stereo frames and final saves in 38.943 seconds. This is a
replay duration, not a gameplay-speed measurement. The app, original recording
and live saves remain unchanged. There are 350 manual analog-input transitions
after scripted selection, including 112 within a render frame. No course-result
evidence or listening verdict accompanies this session.

The separate v16 controller app exited normally after a 12,974-frame windowed
test with sound, CRT and queue diagnostics. It supplied 9,946,388 stereo frames
with zero logged SDL shortfall requests or chip/host drops. A frozen prefix
contains 2,726 cabinet-input changes after scripted control ended, including
intermediate analog values. Settings-button feedback and title-screen audio
quality remain open. Before launch, its 2,600-frame scripted drive preserved
v15 native audio/image, saves and ordered inputs exactly. Its complete manual
recording now also replays exactly using the original signed executable, with
the app, assets, recording and live saves unchanged.

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
no separate wall-speed measurement or recorded listening verdict. Later v15
feedback reports title-screen audio glitches, as described below.

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

Subsequent raw GPU captures attribute both driving residuals to one-ULP
differences in the final reciprocal depth. A separate restricted reciprocal
prototype now passes 262,144 arithmetic rows on Metal: 130,898 signed normal
results and 131,072 seed/fallback cases match exact integer oracles, while
174 excluded inputs preserve a separately compiled legacy shader's bits.
Ordinary division differs on 13,317 sampled normals, at most one ULP. This
initially establishes arithmetic correctness separately from scene integration.
The actual span steps at the two remaining first-car pixels are now measured:
the same signed width produces a one-ULP-different reciprocal and three
one-ULP-different steps. Repeated float32 addition from identical starts
reconstructs the observed texture-coordinate and filtered-texel differences.
The full accepted image remains exact under instrumentation. The corrected
common-shader candidate now passes all 447 synthetic cases and matches all
571,392 pixels across the three complete CPU images, fixing precisely the five
remaining differences. Default-OFF images remain byte-identical. The exact
candidate is integrated behind the existing native-scale option.

A four-arm, 540-render comparison measures current/candidate ON medians of
2.230/2.309, 2.233/2.375 and 2.167/2.130 ms on Apple M5. Candidate/current OFF
ratios are 0.898/0.981/0.963, with substantial block variability. These are
completed offscreen render latencies, not GPU timestamps or game FPS; no
general speedup or precise overhead is established. The next check is the
new host's representative driving replay. See `HANDOFF.md` for frozen inputs,
raw images and independent audits. Temporal flicker improvement remains untested.

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
behavior at frame 1,200, so that historical check did not resolve the reported
cover-screen glitch. Candidate 003's captured starvation remedy is described
above; broader listening confirmation remains separate.

## Next validation work

- Preserve the exact v17 manual replay and extend course validation beyond the
  new sound-on Metal Desert/Forest clears and Mountain retirement. Retain explicit
  result images and matching cabinet inputs; full-game completion is still open.
- Collect listening feedback on the verified playback-starvation fix and capture
  any remaining audible events with both native and SDL output. The corrected
  independent sound comparison now reaches the first
  nonzero output; extend beyond that bounded interval and keep listening quality
  separate from sample/CPU equality.
- Confirm controller Y, menu navigation, remapping and neutral resume during
  actual play. Recorded analog cabinet values do not identify their source.
- Check continuous real-race texture flicker and measure native/CRT cost in a
  quiet interval. Saved-scene equality and offscreen timings do not establish
  visible gameplay behavior or speed.
- Complete Lakeside and Championship Mountain paths, then extend
  independent replay to those paths and coupled sound. Full MAME-machine
  comparison, PCB timing and extended-precision validation remain open.

Preserve exact instruction guards, timing and IRQ boundaries in further speed work.

Inherited Daytona results and the earlier lifted-C Rally prototype provide
reference code and historical evidence. They do not satisfy these Rally exit
criteria. No complete milestone or release readiness is claimed.
