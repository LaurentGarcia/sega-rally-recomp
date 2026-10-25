# Cabinet steering conversion

`cabinet_steering(uint8_t)` recovers the stateless conversion of a cabinet input
byte into the game's steering value. Every byte is accepted. The conversion
centers at 128, saturates at 80 counts on either side, and preserves the exact
binary32 result of the original arithmetic. It does not implement the subsequent
stateful filter, wheel calibration, force feedback or vehicle dynamics.

## Original contract

The verified Revision B program is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The conversion occupies ROM `[0x395d8,0x39668)`, 144 bytes, SHA-256
`1363e4086b6a24a4c452c6c7b7912e51e6bf12c6ac90b6117b3ac0278b22de88`.
It has one ordinary byte input at offline address `0x202050`, no data writes,
and returns its value in `g0`. The saved return address moves from `g14` to
`g1`; `g14` is cleared. Addresses and registers belong only to the offline
comparison adapter, not the native interface.

For signed offset `s = sample - 128` and magnitude `m = min(abs(s), 80)`, the
positive result is:

```
R32(R64(R64(m * binary64(0.84)) / binary64(80)))
```

The sign bit is flipped when `s < 0`; the center returns positive zero.
The result is not asserted to have angle units. The following original helper
at `0x39a90` separately filters this value through state at `0x2142c0` and
negates its output. That operation is recovered separately in the
[complete cabinet filter](cabinet_steering_filter.md).

## Native arithmetic and evidence

The independent implementation rounds the exact rational `21*m/2000` using
integer quotient/remainder arithmetic. Exhaustive comparison for all 256 bytes
establishes that this simplification produces the same final bits as the
original binary64 rounding sequence. No claim extends beyond that finite input
domain. For nonzero magnitudes, the binary exponent lies in `[-7,-1]`; the
largest shifted integer remains below 2^35, safely within `uint64_t`.

The private original-program oracle passes all 256 cases, executes 7,263 original
instructions (27–30 per case) and produces fingerprint `466a329bdf779c3a`.
It checks the single input read, absence of writes and return-register contract.
The portable test independently checks the rounded host arithmetic, monotonicity,
sign symmetry, center and both saturation plateaus. ASan/UBSan also pass.

The original ROM-free fixture executes all 256 cases on the EE in Play 0.72 HLE,
checking the same complete result fingerprint. Its ELF SHA-256 is
`5b1ffd567726386eb29f574ae268252d6c95bb7ceea1c7afda9520fd3b965d7a`,
with 1,222,476 file bytes and 177,120 load-segment memory bytes. The 12.038-second
observation obtained PASS and empty guest stderr. This is emulator evidence,
not a measured completion time or physical-console validation.

Capture 007 adds 32 sampled live calls after all 2,850 observer-enabled and
disabled checkpoints match. Each captured result equals both the native byte
mapping and isolated original execution. One live call spans 101 instructions
because its counter includes an interrupt handler; the isolated operation
still takes 27–30 instructions. The live reader preserves that distinction,
checks complete pairs and return/input identity, and bounds the total span.
The final source-bound report is
`build/ps2-native-cabinet-steering-live-001/result.json`.

```sh
python3 platform/ps2/tools/check_cabinet_steering.py \
  --program build/ps2/native/rom_cache/program.bin \
  --output build/ps2/private-cabinet-steering
python3 platform/ps2/build_ps2.py build --target core-probe \
  --core-manifest platform/ps2/tests/cabinet_steering_manifest.json \
  --output build/ps2/cabinet-steering-ee
```

All original bytes, disassembly, generated output and runtime records remain
under ignored `build/`. The native gameplay loop remains unconnected.
