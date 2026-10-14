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

For example, this coin/start probe holds each button for ten rendered frames:

```text
segarally-input-v1
# frame in0 in1 in2 switches steering gas brake analog3..7
1200 fe 8f 00 ff 80 00 00 ff ff ff ff ff # coin
1210 ff 8f 00 ff 80 00 00 ff ff ff ff ff # release
1240 bf 8f 00 ff 80 00 00 ff ff ff ff ff # start
1250 ff 8f 00 ff 80 00 00 ff ff ff ff ff # release
```

This is an input probe, not a known successful driving replay. Coin is active
low bit 0 of `in0`; start is active low bit 6. The first three analog bytes
control steering (center `80`), gas and brake. Other pins use the same raw
cabinet wiring as the SDL frontend; no game variables or saved memory are
modified. The run begins from the runner's fresh board state.

The runner preserves the submitted bytes as `boot-input.txt`, hashes them in
`boot.json`, and logs the exact frame/instruction/cycle boundary of each input.
The final event counts distinguish applied and pending inputs. Missing, duplicate,
backwards or skipped frame events fail explicitly. This diagnostic replay does
not establish full MAME or PCB input-timing parity.
