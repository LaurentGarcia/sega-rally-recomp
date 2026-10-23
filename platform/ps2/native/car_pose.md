# Recovered car-to-render pose publication

`publish_car_pose` is an independently written value-copy operation between
`CarPoseState` and `RenderPose`. It transfers twenty raw 32-bit payload words
and one progress word. Two flag bits from the previous render pose are retained
in the game state; the published pose additionally sets flag bit 7.

The production function has no guest addresses, registers, mapped memory,
instruction dispatcher, TGP or scheduler. Its C++ references require valid,
distinct state objects. The original leaf has no null-pointer guards; none
from the earlier lifted implementation were imported.

## What the names establish

`position`, `motion` and `orientation` identify three observed groups of three
words at the game/render boundary. They remain raw integer bit patterns here.
This function does **not** establish coordinate units, axis handedness, angle
order, floating-point rounding, camera behavior, collision response or physics.
The eleven remaining payload words are neutral `attributes`. `progress` is
also an opaque copied word, not an implemented route or lap system.

All bits, including signed zero, infinity, NaN payloads and arbitrary non-float
values, are preserved. No mathematical transformation or VU approximation is
involved. Re-publication of an unchanged state/pose pair is idempotent.

## Reference identity and adapter

The verified Revision B program is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The address-only lead in
[`config/segarally_lift_seeds.json`](../../../config/segarally_lift_seeds.json)
is `game_start_race_obj_pose_publish`, ROM offset `0x2bc08`, 224 bytes.
That span's SHA-256 is
`7cb381207b3c9630fa42abb284168925c7e45959aaed6aa53e686c8dd47cd345`.
Fresh decoding of the owner's image confirms 56 instructions, ending with
the return branch at live-copy address `0x005cace4`. No lifted/generated
implementation or instruction bytes are copied into the native source.

The **host-only adapter** maps typed values to these original offsets:

| Native value | Source object offsets | Render pose offsets |
| --- | --- | --- |
| `position` | `0x14, 0x18, 0x1c` | `0x00, 0x04, 0x08` |
| `motion` | `0x2c, 0x30, 0x34` | `0x0c, 0x10, 0x14` |
| `orientation` | `0x38, 0x3c, 0x40` | `0x18, 0x1c, 0x20` |
| `attributes` | `0x74, 0x80, 0x84, 0x64, 0x68, 0x6c, 0x70, 0xb4, 0xb8, 0xbc, 0xc0` | Consecutive words `0x24` through `0x4c` |
| `flags` | Word `0x10` | Word `0x50` |
| `progress` | Word `0x88` | Word `0x54` |

The original object holds its destination pointer at `0x8c`. The typed
function receives the destination explicitly. Flags 14 and 15 of its previous
flags word replace the same two source flags. All other source flags remain;
bit 7 is additionally set in the destination only. The oracle checks the
original 24 stores, including the two byte writes and their order, as well as
the complete surrounding memory. The native API promises the same final value
state on distinct objects, not externally observable guest bus writes.

## Validation

```sh
python3 tests/test_ps2_car_pose.py
python3 platform/ps2/tools/check_car_pose.py \
  --program /path/to/verified/program.bin \
  --output build/native-pose-oracle-001
```

The portable test contains only original boundary vectors and checks every
65,536 source/destination flag-byte pair, raw-bit copying and re-publication.
The private differential verifies the complete program and function hashes
before running the existing test-only i960 interpreter on the host. Its
65,536 cases use randomized payload words and every flag-byte pair. All cases
match; reference execution totals 3,670,016 instructions and 1,572,864 ordered writes, fingerprint
`f77a27e7080bcf13`. The host adapter/interpreter are excluded from the native
game, and all game-derived evidence stays under ignored `build/`.

This validates isolated pose publication. A running race, its physics,
coordinate transforms and physical PS2 behavior remain separate work.
