# Main balance, controls and power update

`update_main_balance` implements the complete bounded value operation entered
at original ROM `0x37870`, including its power, input-feedback and event-state
dependencies. It reuses the recovered parameter selection, paired balance and
geometry publication operations. This closes the earlier gap before the
geometry tail; it is still one part of the main vehicle callback.

The native function takes explicit immutable assets, cabinet samples and a
persistent typed state. It updates the vehicle rate/phase fields, selected
parameters, force/slip, balance history, two accumulated triples, cabinet
feedback, event counters and random state. It returns an ordered bounded list
of sound requests. Both state and requests commit only on success. It performs
no allocation, mapped-memory access, processor dispatch or sound transport.

The request list preserves command identity, parameter and whether a parameter
was supplied. The original sound ring queue is a transport detail outside this
game operation. The offline comparison executes its successful serialization
path and checks requests in order. It rejects the original overflow/fault path;
it does not claim that native audio playback is connected.

## Value and asset contract

All arithmetic-bearing inputs and asset values are finite binary32, generally
bounded in magnitude by `2^20`. Orientation and steering are each bounded by
eight; rate and target rate lie in `[0,8800]`, gears in `[0,4]`, speed is
nonnegative, reference scale and gear ratios are positive. Selected parameter
words which are copied but not read numerically remain raw payloads.

The supplied assets include the ten parameter records, shaping constants,
three 64-value cabinet curves, five gear ratios, seven power constants, one
power curve of 82–104 values, ten event records and eight occasional commands.
No original table is embedded in the implementation. The two verified power
curves each contain 104 values; their selection remains explicit and does not
establish a vehicle model name.

Every separately rounded binary32 or binary64 expression uses the shared
`vehicle_arithmetic` value operations. The geometry sine polynomial is the
recovered firmware operation, not a host-library sine. Its input and the
half-pi-shifted cosine input must remain within its bounded domain. Divisions,
square roots, generated angles, curve indices and the final geometry-tail
domain must also be valid; input bounds alone do not guarantee acceptance.
Non-finite, overflowing or otherwise unsupported intermediate results reject
the complete update atomically. There is no epsilon or substitute physics.

Integer counter updates retain 32-bit wrap. The random state update retains
the multiply-and-fold result and its 31-bit mask. Sound conversion limits and
used biases are bounded by 127. At most 24 ordered requests fit the output;
exceeding that capacity rejects the update before publication.

## Original-program evidence

The main region `[0x37870,0x38674)` is 3,588 bytes with SHA-256
`3503a497f3ad7ac13f9adf9c3290d888caf3f8b3798a48ee4fba02e9df9f3162`.
The checker verifies the complete owner-supplied program and geometry-firmware
images. Its instruction allowlist includes the main region, power/gear and
cabinet callees, shaping/event helpers, random update and successful sound
serialization. Unexpected original instructions or ordinary-memory dependencies
fail the comparison. Geometry firmware starts through its verified startup and
has no external/table access in this operation.

Capture 014 records every callback for frames 2200–2800. All 601 balance calls
match independently executed original instructions and all captured output
blocks, including the persistent event state. They emit 363 ordered sound
requests. This corpus covers operating modes 0 and 1, cabinet modes 0, 1 and 3,
and all three occasional-event modes. It contains no manual gear selection,
nonzero variant selector or active shift-hold counter.

An additional 512 independently specified control variations exercise manual
and automatic selection, both variant values, positive and negative hold
counters, cabinet modes 0–4, all ten parameter selections, zero/nonzero contact
weights and either projection sign. All 512 are accepted and match the original
helper; no cases are filtered out. The combined comparison executes 1,360,606
original CPU instructions and 1,388,448 geometry-firmware instructions, with
1,769 ordered sound requests and fingerprint `9b1de7f1464e8006`.

The ROM-free composite fixture separately checks 192 authored control cases
and nine atomic failures, including a late failure after power/event work.
All 603 checks pass under AddressSanitizer and UndefinedBehaviorSanitizer.
These are host checks; no physical-console or frame-rate result is implied.

Private reports and capture-derived records stay below ignored `build/`.
Each live helper call currently starts with its observed caller state. This
proves the helper's complete transition on those inputs; it does not prove
successive native vehicle ticks from a single initial state. The remaining
main-callback composition and renderer connection are separate work.
