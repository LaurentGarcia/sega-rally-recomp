# Evidence and status

Snapshot: **2026-10-14**. macOS is the active target; phone work is deferred.
This reference summarizes the latest recorded checkpoints. Exact hashes,
failure history and ignored evidence locations are in [HANDOFF.md](../HANDOFF.md).
The [design](sega-rally-design.md) records interpretation and hardware limits.

| Area | Established evidence | Limit |
| --- | --- | --- |
| Main native coverage | Frozen v13: 75,205 instructions, 52 native objects, no missing templates | Coverage is a reached graph, not a remaining-work percentage |
| Interactive Mac | Stable SDL3/Metal host, actual driving after selection replay handoff | Current speed playtest has sound off |
| Driving parity | 600 million instructions, 19,641,024 ordered writes, 1,365,742,448 cycles, 32,576 IRQs and 3,142 renders; nineteen replay events match | Independent CPU with shared board implementations; no full MAME race |
| Later game paths | Held-gas v13 runs two billion instructions and 10,701 renders without a code/device frontier; returns to attract | Game Over overlay and a new credit/start sequence not separately validated |
| Course progress | Desert clear/results show 1:12.15; forested second course enters, times out, then attract returns | Centered steering does not complete second-course bends; remaining courses unvalidated |
| Metal fidelity | Startup warning matches CPU renderer pixel for pixel; stable fan path is used | Exact racing-scene raster parity remains open |
| Sound generation | Native linked board captures 2,409,170 stereo frames: 54.6297052 seconds including nonzero 44.1 kHz PCM; shared-core drains agree | Whole-game audio parity unvalidated |
| Audio output | Ten-second recorded excerpt plays through the SDL/CoreAudio API without input shortfalls; a normal-paced game reaches 2,760 frames with unchanged main state | Game output queue runs dry: 3,222 estimated input-demand requests; user listening confirmation pending |
| Hardware fidelity | Isolated device/reference tests and instruction-timed diagnostics | PCB timing, extF80 precision and full-machine behavior remain open |

## Measured Mac speed

A quiet hidden-window Metal race comparison used the same 480 rendered frames,
87,173,426 main instructions and 8.344320 seconds of board time, with sound off:

| Routing | Wall time | Frames per wall second | Board time / wall time |
| --- | ---: | ---: | ---: |
| Original | 14.163164 seconds | 33.8907 | 58.9156% |
| Indexed | 11.871813 seconds | 40.4319 | 70.2868% |

This is about a 19% wall-speed gain, still below real time. All 66 progress,
input and exit rows match. It is one measured Mac checkpoint, not a performance
guarantee. Hidden acquisition does not measure visible VSync blocking; GPU
encoding time is CPU work, not GPU completion time. The game clock was unchanged.
The indexed v6 independent parity transcript also matches the original report
byte for byte.

## Graphics evidence

The first paired car frame differs in 15,830 of 190,464 RGB pixels (8.31%),
with mean absolute channel error 0.7278/255. Re-rendering that frozen CPU scene
as triangle fans reduces the difference to 1,602 pixels and 0.040408/255,
attributing 94.45% of mean error to interpolation. This diagnosis does not
establish a production whole-polygon fix or exact race rendering. A stable
picture and correct polygon ordering remain distinct from pixel fidelity.

## Next validation work

Improve runtime speed enough to supply continuous audio and confirm the Mac
listening result before treating live sound as working. Continue speed work with
exact instruction guards, timing and IRQ boundaries preserved. Validate fresh
credit/restart and the remaining courses, then extend independent replay
comparison beyond the current 600-million driving checkpoint. Real-race graphics
comparison, full MAME-machine parity and PCB/precision validation remain required.

Inherited Daytona results and the earlier lifted-C Rally prototype provide
reference code and historical evidence. They do not satisfy these Rally exit
criteria. No complete milestone or release readiness is claimed.
