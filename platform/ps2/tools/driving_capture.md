# Offline driving observation

This separate host tool runs the original translated reference with the tracked
cabinet input replay, then records bounded native-recovery evidence. It does not
make the PS2 rewrite playable. Captures, generated programs, asset packs and
reference images remain under ignored `build/`.

The opt-in `RALLY_OFFLINE_DRIVING_CAPTURE` hook observes the existing exact
pre-instruction boundary after synchronization and interrupt decisions. It never
changes registers or game memory. Snapshot reads are restricted to aligned,
bounded ordinary RAM; device/FIFO reads are forbidden. The macro is rejected on
EE, and neither the observer nor this host executable belongs to a native game
source role. Normal player builds do not enable the hook.

## Reproduce the bounded replay

Supply the locally generated, audited Revision B reference and its matching
six-region asset pack. The defaults use the existing private PS2 reference:

```sh
python3 platform/ps2/tools/capture_driving.py \
  --native-build build/ps2/native \
  --pack build/ps2/assets/game.rpk \
  --inputs config/segarally_lancia_drive.inputs \
  --output build/ps2/driving-observation
```

The default executes 2,850 rendered frames, applying all nineteen coin, start,
selection, pedal and steering events at their established frame boundaries.
It observes integration calls during completed frames 2200 through 2800.
Every observed outer entry/return is recorded, while detailed snapshots select
up to four calls at input/60-frame checkpoints and at most 128 calls. Other hard
bounds limit frame intervals, instructions per call, total calls, events and
output bytes. This is a bounded sample, not a full-course capture.

The tool builds one host executable and runs it with capture enabled and
disabled. It requires exact equality of every per-frame instruction, cycle,
modeled board-time, PC, general-register hash and framebuffer hash checkpoint.
This checks the observer against the same reference, not against an independent
PCB or a complete MAME machine. It is not an exhaustive comparison of all
device/internal state.

`validation.json` binds source/generated inputs, program/asset identities,
input replay, executable and capture file hashes. A successful observation has
`status: passed`, `exact_checkpoint_match: true` and a completed `capture.json`.
Temporary streams or failed validation reports must not be used as live evidence.

## What the records establish

`cadence.jsonl` pairs outer calls using their actual return PC and caller frame
pointer; `selectors.jsonl` records the dispatcher field selecting the route.
`events.jsonl` contains selected entry/exit, direction-blend and course-query
boundaries. Query returns use their own saved return PC/frame pointer because
the same helper has multiple callers. Records include general registers, raw
floating-register bits, arithmetic control and named RAM windows.

The legacy field name `object_words` means a 0x400-byte RAM window from the
observed object address. It does not prove the object's allocation size or
field layout; observed object addresses are only 0x100 apart. Optional dependency
windows outside the allowed RAM ranges are omitted. The recording is not a
complete integrator input schema.

The validated `driving-capture-004` run recorded 1,202 completed alternate
integrator calls: objects `0x500100` and `0x500200` each once per observed frame,
with selector one. Player/AI ownership remains unknown. It captured 32 detailed
calls and 288 events; both [direction blending](../native/direction_blend.md)
and [contact-neighbor selection](../native/contact_neighborhood.md) match every
sample. The primary integrator and its three-fold/pi-over-eight orientation tail
were absent. This distinction matters: neither an assumed primary entry nor a
different orientation rule can substitute for the path that executes.

```sh
python3 platform/ps2/tools/check_direction_blend.py \
  --program build/ps2/native/rom_cache/program.bin \
  --capture build/ps2/driving-observation/capture \
  --output build/ps2/private-direction-comparison
python3 platform/ps2/tools/check_driving_contact.py \
  --capture build/ps2/driving-observation/capture \
  --output build/ps2/private-contact-comparison
```

Both consumers verify complete paired boundaries and the observer report's exact
hashes. Their results establish the captured component behavior; the remaining
geometry queries, collision calculation, alternate orientation smoothing,
full race integration and physical PS2 execution remain separate work.
