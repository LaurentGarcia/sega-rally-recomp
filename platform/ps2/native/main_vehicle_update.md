# Continuous native main-vehicle update

`update_main_vehicle` composes the recovered main vehicle operations into one
persistent typed update. It consumes the previous native state, a bounded native
asset package and explicit cabinet/game-flow inputs, then publishes the next
state, a render pose and ordered sound requests. It contains no i960/TGP runtime,
original addresses, captured intermediate results or per-frame state replacement.

The host comparison advances **601 consecutive updates from one initial seed**
(render frames 2200–2800 in the sealed reference observation). Every update
matches all 161 canonical state fields, all 22 published pose words and the
ordered sound requests: **96,761 state comparisons, 13,222 pose comparisons and
458 sound requests**, fingerprint `57e2c98c5de78376`.

The same checker also passes when compiled for the EE and executed in Play:
all counts and the fingerprint match, and both exported checkpoints are byte
identical to the host outputs. The fixture includes file IO and comparison
work, so its total runtime is not a per-frame performance measurement. Physical
PS2 validation remains separate.

This establishes continuity for that observed sequence and the supported component
domains. It does not establish a recovered race initializer, every possible game
mode, console performance or a completed playable application. The optional
surrounding-car entry branch remains explicitly unsupported. The recorded
sequence does not take that branch.

## State and inputs

The operation owns position, pending displacement, motion, orientation, angular
motion, candidate state, contact results and history, steering filter history,
segment progression, gear/phase state, selected tuning values, cabinet feedback,
power/sound history, counters and random state. Stage adapters are temporary;
the canonical state has one owner for each persistent value. A failure leaves
both the caller's state and publication unchanged.

Per-update inputs are the cabinet frame counter, requested gear, cabinet screen,
game screen/mode variants, steering/accelerator/brake samples, flag sample,
cabinet kind, manual-selection setting and surrounding-car mode. The comparison
supplies these explicit inputs from the original observation. It never supplies
original collision results, forces, selected tuning, intermediate vectors or
internal histories after the initial seed.

Course guidance is derived from the immutable course-direction table and the
**previous native segment**, before this update selects a new segment. Negative
segments select row zero. All 601 observed original direction triples match that
lookup. Guidance is compared as an independent witness, not fed from the capture.
The fixed setup contains four divisors and gravity; their original values remain
unchanged across the whole sequence.

The render publication copies raw value bits. Its first three auxiliary values
are steering, phase step and phase, followed by four contact histories and four
published contact values. The pose comparison binds these values to the original
publication destination, rather than checking only a position vector.

## Evidence and limits

The additive full-callback observation preserves the previous streams and all
2,850 enabled/disabled reference checkpoints. Its continuous ordinary-memory
replay checks original reads against the single seed, callback writes and
explicit writes by surrounding original code. The private `VEHSTEP1` fixture
contains one seed, explicit external input words, expected-after words and
expected ordered sound bytes. Expected values are used only for comparisons.

The equality claim covers the typed semantic state and publication. It excludes
original stack temporaries, pointers, CPU/interrupt/device state and ownership
of the original sound transport ring. Native sound requests are compared by
expanding them through the immutable command assets into the observed byte
order; the original queue cursor is used only for that comparison.

The reference sequence uses course selector zero and one observed vehicle
configuration. Cabinet steering and pedals, game screen/mode and the flag input
change during the sequence; several other explicit inputs remain constant.
Passing this comparison does not expand the supported numeric or asset domains
of the individual recovered operations.

## Reproduction

All game data and capture outputs remain under ignored `build/` directories.
Given the sealed full-callback observation and a version-3 driving asset package:

```sh
python3 platform/ps2/tools/extract_main_vehicle.py \
  --capture build/ps2/driving-capture-014/capture \
  --output build/vehicle-records-new
python3 platform/ps2/tools/check_main_vehicle_update.py \
  --records build/vehicle-records-new/vehicle.bin \
  --assets build/ps2-driving-assets-v3-001/course0.bin \
  --output build/vehicle-proof-new --sanitize --negative-checks
```

The proof records capture, asset, compiled-source and executable hashes. It
requires the exact state/pose comparison counts and exports no checkpoint on a
comparison failure. The bounded negative cases alter seed size, truncate or
append record data, change an expected pose, change the second frame's steering
sample, and break consecutive frame identity. Each must fail at its intended
validation boundary.

After the entire sequence passes, the checker exports two private developer
checkpoints: the first native seed and the retained native state before render
frame 2400. These contain typed values and asset identity, never original
addresses. Loading one is an explicit development starting point; it is not a
recovered game startup sequence. The active checkpoint is produced by advancing
the native state from the first seed, not by copying a later original snapshot.

See [the native update API](main_vehicle_update.h),
[the bounded checkpoint format](main_vehicle_checkpoint.h),
[the main-vehicle recovery map](main_vehicle_recovery.md), and
[the offline full-step observer](../tools/driving_capture.md).
