# Recovered race-progress update

`race_progress.cpp` is an independently written semantic update over typed
game state. It contains no guest registers, addresses, instruction dispatcher,
board bus, TGP operations or device scheduler. It does not implement a race,
select its own update frequency, or establish the game's complete state model.

## Confirmed behavior

One successful call decreases `remaining_ticks` only when it is positive,
increments both unsigned counters, and increments the selected entry in each
bank. Unsigned counters wrap modulo 2^32. Other state remains unchanged.
Negative remaining values, including `INT32_MIN`, remain unchanged.

Each bank has 64 entries. Invalid indices are rejected without changing state;
this is the new native API's explicit input validation. The original leaf has
no bounds check. Its behavior for out-of-range guest indices is outside this
native contract. No claim is made yet that every live game caller satisfies
the selected domain.

The source names the two counters and banks neutrally. The prior address label
suggests lap/progress timing, and one bank is consumed by a lap-time HUD path,
but those interpretations are not used to invent a race state machine.

## Evidence and offline adapter

The owner's verified Revision B program is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The address-only lead is
[`config/segarally_lift_seeds.json`](../../../config/segarally_lift_seeds.json),
`game_start_race_lap_tick`, ROM offset `0x1c8b8`, declared extent 136 bytes.
Its span SHA-256 is
`72e6be07282908df59b032efe70b47b330105791cfe44cc049d19784886589b1`.
The function is decoded afresh from that verified image; no lifted or generated
implementation is copied into this native source.

The live copy starts at `0x005bb8b8`. A positive remaining value executes 23
instructions; other values execute 22. The return branch ends at `0x005bb93c`,
before the trailing return instruction in the declared span. The update has
five 32-bit data stores and no hardware or floating-point operations.

Only the **host-only oracle adapter** maps the following addresses:

| Native field | Reference location |
| --- | --- |
| `remaining_ticks` | `0x20b0b0`, signed comparison |
| `counters[0]`, `counters[1]` | `0x2020bc`, `0x2020b8` |
| `first_bank` | 64 words beginning at `0x20afb0`, index from `0x20ac98` |
| `second_bank` | 64 words beginning at `0x2020d0`, index from `0x2020c0` |

The adjacent initialization routine at ROM `0x1c830`, 120 bytes, clears
64 words in both banks. Its SHA-256 is
`e58eeefc4928940f40881d941da12c81043533b6543cba1e3e627ad8dca7c09e`.
This verifies the bank capacity; it does not establish all caller invariants.

Run the portable native-only checks with:

```sh
python3 tests/test_ps2_race_progress.py
```

The private differential requires the owner's program image and a new ignored
output directory. It verifies the image and function hashes before execution:

```sh
python3 platform/ps2/tools/check_race_progress.py \
  --program /path/to/verified/program.bin \
  --output build/native-progress-oracle-001
```

The host adapter executes the existing test-only i960 reference interpreter,
with bounded fetches and a return sentinel. It compares the complete 64 KiB
surrounding memory, all five ordered writes and the function's return behavior
against the typed update. The interpreter and adapter are excluded from the
native game build. Program bytes, disassembly and oracle output stay private.

The initial differential matches **32,768 cases**. Reference execution totals
735,250 instructions and 163,840 writes, fingerprint `5acab57d56227b63`. Cases cover every
64×64 index pair with seven signed countdown boundaries, plus 4,096 random
states; counter wrapping is exercised repeatedly. The portable fixture adds
121 boundary/rejection checks and repeated ticks. These are isolated function
checks, not race integration, full-game fidelity or physical PS2 validation.
