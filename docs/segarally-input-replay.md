# Sega Rally cabinet input replay

The bounded native boot driver accepts `--input-replay FILE` together with
`--with-screen --with-io`. This follows the inherited design's **Inputs** and
**Reference & validation** contracts: change cabinet pins, execute real game
code, and retain reproducible input evidence. A replay reaching a missing-code
frontier remains incomplete, even if every input was applied.

The plain text format begins with `segarally-input-v1`. Each following row is
a decimal count of completed rendered frames, then twelve two-digit hexadecimal
bytes: `in0 in1 in2 switches analog0 analog1 ... analog7`. Blank lines and `#`
comments are accepted. Frames must strictly increase, from 0 through 1,000,000;
the file may contain at most 4,096 events. Every row is a complete snapshot.
Default neutral inputs remain in effect before the first row, and each snapshot
persists until the next row. Frame 0 applies before the first CPU instruction.
Later events apply after that many frames have finished rendering and before
the next call to resume the machine, matching the frontend's input boundary.
These are rendered-frame counts, not screen latch numbers or wall-clock time.

The fresh verified cabinet configuration needs two coins for one credit.
This measured coin/start sequence holds each button for ten rendered frames:

```text
segarally-input-v1
# frame in0 in1 in2 switches steering gas brake analog3..7
1200 fe 8f 00 ff 80 00 00 ff ff ff ff ff # coin
1210 ff 8f 00 ff 80 00 00 ff ff ff ff ff # release
1220 fe 8f 00 ff 80 00 00 ff ff ff ff ff # second coin
1230 ff 8f 00 ff 80 00 00 ff ff ff ff ff # release
1240 bf 8f 00 ff 80 00 00 ff ff ff ff ff # start
1250 ff 8f 00 ff 80 00 00 ff ff ff ff ff # release
```

This reaches Car Select on the frozen v3 generation. A single coin visibly
produces CREDIT 1/2 and does not buy a game. The complete versioned replay
`config/segarally_lancia_select.inputs` additionally steers right at frame 1800,
centers at 1820, presses gas at 1840 and releases at 1850. A captured menu
confirms the Lancia with automatic transmission is selected. On v3 it then
stops at callback `0x005e4380`, during a white transition at frame 1915. The
frozen v6 generation proceeds through GO and actual driving. Coin is active
low bit 0 of `in0`; start is active low bit 6. The first three analog bytes
control steering (center `80`), gas and brake. Other pins use the same raw
cabinet wiring as the SDL frontend; no game variables or saved memory are
modified. The run begins from the runner's fresh board state.

`config/segarally_lancia_drive.inputs` extends that selection sequence to
nineteen events: gas at frame 2300, a chase-view button pulse at 2400, left
and right steering pulses at 2500/2600, gas release plus brake at 2700, and
brake release at 2760. On v6 all events complete through 600 million native
instructions without a missing-code or device frontier. A capture during
acceleration at 500 million instructions shows 160 km/h in third gear after
the left steering pulse. A matched centered-steering replay shows 222 km/h
in fourth gear at the same displayed race time, 4.48 seconds. The original
replay reaches 13.53 seconds at 600 million instructions, showing 8 km/h
after braking and coasting. These are bounded driving observations, not a
complete race or hardware-fidelity claim. Use the selection-only replay for
interactive play so scripted steering and pedals do not override the player.

`config/segarally_lancia_hold_gas.inputs` preserves the same event boundaries
while keeping steering centered and gas pressed after selection. On v6 it
reaches the FINISH banner at displayed race time 1:12.13, then stops at the
actually reached callback `0x005bc680` after 1,245,399,197 instructions and
6,658 rendered frames. This establishes the course traversal, including a
checkpoint time extension, but not the finish/results transition. V7 compiles
the observed callback for the next bounded replay.

The runner preserves the submitted bytes as `boot-input.txt`, hashes them in
`boot.json`, and logs the exact frame/instruction/cycle boundary of each input.
The final event counts distinguish applied and pending inputs. Missing, duplicate,
backwards or skipped frame events fail explicitly. This diagnostic replay does
not establish full MAME or PCB input-timing parity.

The native boot runner accepts an explicit `--instructions` budget from 1 to
2,000,000,000; its default remains 1,000,000. Budgets up to one billion retain
the 180-second process timeout. Larger diagnostic runs have a 360-second
timeout so later course and race-end callbacks can be exercised without an
unbounded run. The live app and its current generation are unaffected.

The independent CPU/device harness accepts the same file with
`tests/test_segarally_boot_parity.py --input-replay FILE --screen --io` and its
usual TGP/video/control options. Separate replay cursors drive each board;
CPU registers, cycle-stamped writes, IRQs, device states and framebuffer are
still compared. It freezes the file as `parity-input.txt` with a SHA-256 in
`parity-input.json`. The v3 neutral check at frames 0, 100 and 500 matches the
native machine's input instruction/cycle boundaries exactly and passes through
100 million instructions. This test uses the inherited independent CPU engine
with shared board implementations; it is not a full emulator or PCB replay.
The complete Lancia-selection replay also passes 376,736,993 instructions,
6,518,236 ordered writes and 832,720,369 cycles through that same callback.
Every input event's instruction/cycle boundary matches the native machine.
The v5 selection replay also passes independent CPU/device comparison through
GO: 451,072,711 instructions, 11,880,004 ordered writes, 1,012,631,901 cycles,
23,911 IRQs and 2,329 rendered frames. Its missing callback `0x005c0d60` is
compiled in v6. The full nineteen-event v6 driving replay now also passes:
600,000,000 instructions, 19,641,024 ordered writes, 1,365,742,448 cycles,
32,576 IRQs and 3,142 rendered frames, hash `f6c10714158f5366`. All nineteen
event boundaries match the shared machine, with no missing-code frontier.
Longer course and race-end comparisons remain outstanding.

## Exact host resume transcripts

`tools/srally_boot/host_input_transcript.h` provides a separate diagnostic
format, `segarally-host-resume-v1`. It is a tested core API; the player has no
capture or playback flag for it yet. The existing frame replay above is
unchanged.

The interactive host samples controls before each call to
`run_until_frame`, with a budget of at most 100,000 instructions. Several
calls can occur within one rendered frame. Also, IRQ entry can finish a frame
between calls without executing another opcode: the instruction count stays
equal while reference cycles, board time, frame count and PC advance. A manual
recording keyed only by frame or instruction count would lose these boundaries.

The transcript therefore records every actual resume call: its sequential
index, requested budget, all twelve cabinet input bytes, return reason, and
before/after instruction count, reference cycles, board nanoseconds, rendered
frame count and PC. Playback must repeat those exact calls and, with sound,
drain PCM after each call. Paused or quit-only host polls do not execute the
machine and need no row. Repeated input bytes and equal instruction counts
must not be merged.

Streams are limited to 250,000 resumes, 64 MiB and 1,024 bytes per line. An
explicit terminal marker binds the final count and witness; truncation, trailing
data, discontinuities and failed writes are rejected. A complete transcript
can end at a missing-code or device frontier. Completion establishes a complete
record, not successful gameplay, matching fault details or validated replay.

The context binds a SHA-256 provenance digest and sound/hook flags. The caller
must validate the manifest against the actual loaded images, generated build,
runtime options and initial EEPROM/backup bytes before approving that binding.
Fresh default save bytes matter too. The parser checks syntax and equality;
a supplied digest or callback does not verify those external facts. The bounded
`rt::Sha256` helper now hashes loaded byte snapshots, streams and files through
the pinned public-domain SDK, with 179 known-answer and I/O checks. Host
integration, manifest validation, PCM evidence and detailed terminal faults remain
work to implement. The independent CPU harness must reject unsupported startup
seeds, resume phases or configurations, and currently rejects coupled sound.

The ROM-free tests cover 98 stream/contract checks and 84 Machine checks in
both execution modes. A real timer IRQ in the synthetic fixture completes a
frame at the same instruction count, then the next opcode reads the newly
recorded input. Captured, untouched and replayed Machines agree on CPU state,
frame output and saves. This follows the inherited design's **Inputs** and
**Reference & validation** contracts; it is not a MAME or full-course result.

```sh
cmake -S . -B build/host-tests-resume -DM2_BUILD_WINDOWED_APP=OFF
cmake --build build/host-tests-resume --target \
    test_segarally_host_input_transcript test_segarally_host_input_machine -j 2
ctest --test-dir build/host-tests-resume --output-on-failure \
    -R '^segarally_host_input_(transcript|machine)$'
```
