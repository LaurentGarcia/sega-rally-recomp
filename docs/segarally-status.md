# Evidence and status

Snapshot: **2026-10-14**. macOS is the active target; phone work is deferred.
This reference summarizes the latest recorded checkpoints. Exact hashes,
failure history and ignored evidence locations are in [HANDOFF.md](../HANDOFF.md).
The [design](sega-rally-design.md) records interpretation and hardware limits.

| Area | Established evidence | Limit |
| --- | --- | --- |
| Main native coverage | Frozen v13: 75,205 instructions, 52 native objects, no missing templates | Coverage is a reached graph, not a remaining-work percentage |
| Interactive Mac | Stable SDL3/Metal host, manual control after selection handoff; sound-enabled windowed session exits normally after 9,347 frames | User feedback on live audio quality pending |
| Driving parity | 600 million instructions, 19,641,024 ordered writes, 1,365,742,448 cycles, 32,576 IRQs and 3,142 renders; nineteen replay events match | Independent CPU with shared board implementations; no full MAME race |
| Later game paths | Held-gas v13 runs two billion instructions and 10,701 renders without a code/device frontier; returns to attract | Game Over overlay and a new credit/start sequence not separately validated |
| Course progress | Desert clear/results show 1:12.15; forested second course enters, times out, then attract returns | Centered steering does not complete second-course bends; remaining courses unvalidated |
| Metal fidelity | Optional polygon scanlines reduce error in three real scenes; default-off captures match the stable renderer; 243 synthetic cases pass | Still off by default; exact racing-scene raster parity and updated performance remain open |
| Sound generation | Native linked board captures 2,409,170 stereo frames: 54.6297052 seconds including nonzero 44.1 kHz PCM; shared-core drains agree | Whole-game audio parity unvalidated |
| Audio output | Windowed session supplies 7,165,805 stereo frames with zero drops or estimated input shortfalls; user heard a separate ten-second native PCM sample | Live-game listening feedback and physical fidelity remain open |
| Hardware fidelity | Isolated device/reference tests and instruction-timed diagnostics | PCB timing, extF80 precision and full-machine behavior remain open |

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

## Graphics evidence

The first paired car frame differs in 15,830 of 190,464 RGB pixels (8.31%),
with mean absolute channel error 0.7278/255. Re-rendering that frozen CPU scene
as triangle fans reduces the difference to 1,602 pixels and 0.040408/255,
attributing 94.45% of mean error to interpolation. This diagnosis does not
establish exact race rendering. A stable picture and correct polygon ordering
remain distinct from pixel fidelity.

The optional polygon scanline implementation now uses the CPU reference's two
edge chains for 4–8 vertex solids and textures. Rounded spans control coverage
over a conservative rectangle; texture shading remains on the GPU.

| Captured frame | Stable differing RGB pixels | Candidate differing RGB pixels | Stable mean channel error | Candidate mean channel error |
| --- | ---: | ---: | ---: | ---: |
| 1,634, first car | 15,830 | 822 | 0.727817 | 0.019426 |
| 2,329, race start | 13,896 | 1,244 | 0.577714 | 0.027496 |
| 2,599, driving | 21,236 | 599 | 0.698309 | 0.013448 |

Each image is 496×384; error is the mean absolute RGB channel difference on a
0–255 scale. These frozen scenes use the earlier sound-off generations. They
do not measure current v13 gameplay speed. The option remains off by default;
disabled captures match the committed stable renderer. The synthetic Metal
fixture passes 243 native-resolution cases, including rounded edges, clipping,
crossed chains, checker pixels and layered transparent/opaque texels.

The stray white pixel at (125,75), attributed to solid polygon 353, now matches
the CPU's black pixel. Ten pixels improve to exact matches versus the previous
prototype; one at (147,20) worsens and its cutout ownership remains unresolved.
Other differences include the untouched triangle path and texture precision.
The CPU's repeated floating-point additions are not reproduced exactly by
the per-fragment formula. Broader scenes, continuous flicker and performance
remain to test before enabling this path for play.

## Next validation work

Confirm visible-window gameplay sound, then measure
longer game paths with sound enabled. Preserve exact instruction guards, timing
and IRQ boundaries in further speed work. Validate fresh
credit/restart and the remaining courses, then extend independent replay
comparison beyond the current 600-million driving checkpoint. Real-race graphics
comparison, full MAME-machine parity and PCB/precision validation remain required.

Inherited Daytona results and the earlier lifted-C Rally prototype provide
reference code and historical evidence. They do not satisfy these Rally exit
criteria. No complete milestone or release readiness is claimed.
