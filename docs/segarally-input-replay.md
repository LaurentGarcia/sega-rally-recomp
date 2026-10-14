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
stops at callback `0x005e4380`, during a white transition at frame 1915; race
entry remains unverified. Coin is active
low bit 0 of `in0`; start is active low bit 6. The first three analog bytes
control steering (center `80`), gas and brake. Other pins use the same raw
cabinet wiring as the SDL frontend; no game variables or saved memory are
modified. The run begins from the runner's fresh board state.

The runner preserves the submitted bytes as `boot-input.txt`, hashes them in
`boot.json`, and logs the exact frame/instruction/cycle boundary of each input.
The final event counts distinguish applied and pending inputs. Missing, duplicate,
backwards or skipped frame events fail explicitly. This diagnostic replay does
not establish full MAME or PCB input-timing parity.

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
