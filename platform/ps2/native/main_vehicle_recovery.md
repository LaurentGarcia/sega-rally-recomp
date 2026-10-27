# Main vehicle update recovery

The vehicle update at ROM `0x34f40` is separate from both integrators in the
[earlier dependency map](car_integration_recovery.md). Recovering the alternate
routine at `0x2e650` does not recover this callback. The distinction remains
important when interpreting the earlier component evidence.

The callback's supported path is now joined in the typed
[`update_main_vehicle`](main_vehicle_update.md) operation. It passes 601
consecutive updates from one initial state on the host and in an EE executable
running in Play. The native development application advances that state from
live cabinet inputs after loading one private checkpoint. This is a bounded
vehicle update and diagnostic presentation, not a completed race or recovered
game startup. The optional surrounding-car entry branch remains unsupported.

## Identity and object initialization

The verified Revision B image and its hash are recorded in
[cabinet steering conversion](cabinet_steering.md). Fresh linear decoding of
`[0x34f40,0x371f4)` reaches the return at `0x371f0`, followed by padding and the
next function at `0x37200`. This span contains 8,884 bytes and 1,713 instruction
sites, SHA-256
`16112f4f88469998d0cbae72c4d5cb41e22f282d64e2e00120b26a8f1a29c470`.
It has 184 static FIFO write sites and 68 static read sites, excluding callees.
Those are decoded sites, not transactions or runtime cost per frame.

The initialization sequence at `0x1bae8` initializes the object pool at
`0x500000`, allocates its first entry, stores that pointer in global `0x2140cc`
at `0x1bb18`, and calls `0x34140`. That setup assigns callback `0x5d3f40`
(the relocated `0x34f40`) to the object's `+4` field. The pool builder advances
ordinary entries by `0x100` bytes. A prior observer's `object_words` window is
`0x400` bytes, so it spans more than one such entry and must not be described as
a recovered object layout.

The validated capture 004 already records 601 executions of this callback in
frames 2200–2800. In the same window, the separate dispatcher at `0x2bcf0` runs
4,207 times, the alternate integrator runs 1,202 times, and `0x2cce0` never runs.
Coverage alone does not associate arguments with the callback or prove player
ownership. Explicit callback records and input/state captures are needed for
that association; raw address order is insufficient.

Capture 007 supplies that association for this replay. Its 2,850 enabled/disabled
frame checkpoints match exactly. In frames 2200–2800, object `0x500000` calls
`0x5d3f40` exactly once per frame (601 times); objects `0x500100..0x500700` each
call the separate dispatcher. All 34 detailed snapshots retain `0x500000` in
global `0x2140cc` and `0x5d3f40` in that object's callback word. The complete
cabinet filter returns to `0x5d3f90`; its result matches object `+0x74` in all
26 snapshots sharing the 13 sampled filter frames. This connects the cabinet
input path to the main object's callback, rather than identifying it from
allocation order. It does not establish every object's role in other modes.
Private `callback-validation.json` binds these checks to the validated capture.

## Dependency order

| Region or callee | Established operation or remaining contract |
| --- | --- |
| `0x34f40..0x34ff0` | [Typed entry wrapper](main_update_entry.h): flag update, steering filter and segment selection; the conditional surrounding-car branch rejects explicitly |
| `0x39a90` called at `0x34f8c` | Recovered [cabinet steering filter](cabinet_steering_filter.md); result stored at object `+0x74` |
| `0x33e90` called at `0x34fb0` | Conditional surrounding-car dependency outside the supported whole-update path |
| `0x2b0b0` called at `0x34fc4` | Recovered [segment selection](segment_selection.md), including ordered search and the unclamped miss result |
| `0x34ff0..0x35218` | Recovered [candidate preparation](main_candidate.md): pending-middle clamp, metric gate, conditional state update and candidate triples for the [four-point contact pass](main_contact_pass_recovery.md) |
| `0x34ff0` through contact return at `0x3521c` | [Joined candidate/contact operation](main_contact_step.md), deriving all contact inputs from the newly prepared vehicle |
| `0x3521c..0x36760` | [Complete front stage](main_front_update.md): secondary point transforms, contact re-queries, four slot updates, force/moment preparation and history/event updates |
| `0x2abc0` / `0x2acc0` at `0x35704` / `0x35710` | Shared course-query preparation and result interpretation |
| `0x37200` | Recovered [main contact-slot update](main_contact_slot.md), called four times; bounded scalar/history/type/response behavior, with explicit private normalization coefficients |
| `0x37870` | [Complete balance, controls and power operation](main_balance_update.md), including filtering, shaping, event histories, emitted sound requests and the recovered geometry tail |
| `0x36760..0x371a4` | [Typed motion stage](main_motion_update.h): angular/motion/position updates, mode gating, feedback/events, damping and metric cap |
| `0x34ce0` | Counter, feedback and event scheduling through return at `0x34e38`, integrated into the motion stage; sound requests and the diagnostic marker are typed effects |
| `0x371b0`, `0x371c0`, `0x371d0` | Three calls to the recovered one-step angle fold, without the earlier integrator's final pi/8 clamps |
| `0x34078` at `0x371dc` | Recovered [main-object pose publication](main_car_pose.md), including raw flags/index words and an external four-word state group |

The last publication helper differs from the recovered `0x2bc08` operation.
It copies full source words at `+0x10` and `+0x88` into destination `+0x50`
and `+0x54`, and copies the four global words at `0x2138a0..0x2138ac` into
destination `+0x40..0x4c`. The earlier helper's persistent flag merge is not a
valid substitute. The native `make_main_car_pose` operation receives explicit
typed state and four external values, returning the existing `RenderPose`
value type without mutating either input. Original addresses belong only to
its offline comparison adapter. The whole-update proof now checks this final
publication as well as the persistent state passed into the next update.

The `0x34ce0` helper's code span is `[0x34ce0,0x34e3c)`, 348 bytes. Extending it
to `0x34f40` includes following data, not additional helper instructions. Its
three observed return instructions are at `0x34d0c`, `0x34d50` and `0x34e38`.
It must not be described as a generic force or drivetrain calculation.

The complete `0x37870` helper spans 3,588 bytes and has nine direct/BAL
dependencies plus geometry FIFO operations. Both vector pointer arguments are
in/out: the tail modifies the second and stores a geometry result through the
first. Earlier pair, record-copy and geometry-tail tests covered only parts of
that operation. The complete native balance helper now derives the tail inputs
through the preceding power, filtering, limiting and event paths; its separate
original-program comparison covers all 601 observed helper calls and 512
authored control variations. The joined update retains those histories between
calls. Emitted requests preserve sound semantics without reproducing the
original transport queue in the native game.

## Current comparison scope

### Joined state transition

The previously missing caller span `0x3521c..0x371a4`, entry wrapper and final
publication are now connected. Original region sizes identify recovery
boundaries; they are not runtime costs or completion percentages.

| Caller region | Native operation in the joined update |
| --- | --- |
| `0x3521c..0x35b5c` | Front stage: secondary candidate/current transforms, conditional queries, differences, normalization and square-root/atan-derived history |
| `0x35b5c..0x35c50` | Front stage: four contact-slot calls and their surrounding packing |
| `0x35c50..0x36760` | Front stage: response/filter/geometry preparation for the complete balance helper |
| `0x36760..0x36cc4` | Motion stage: angular increments and clamps, feedback, events and correction branches |
| `0x36cc4..0x36ddc` | Motion stage: acceleration, gravity, orientation, mode gating and position update |
| `0x36ddc..0x371a4` | Motion stage: counter/events, damping and metric cap; optional surrounding-scene inputs remain outside the joined path |
| `0x371a4..0x371f4` | Final angle folds and raw pose publication |

Capture 014 preserves all 2,850 enabled/disabled reference checkpoints and
records every callback in observation frames 2200–2800. Continuous ordinary
memory replay validates the single seed, callback writes and surrounding-code
changes. The native checker seeds its persistent state once, then advances it
through **601 consecutive updates**, comparing **96,761 state words, 13,222
published pose words and 458 ordered sound requests**. All match, fingerprint
`57e2c98c5de78376`. Captured intermediate forces, contacts, tuning values and
histories are never inputs to later native updates.

Per-update inputs are explicit cabinet and surrounding game-flow values.
Course guidance comes from immutable asset data indexed by the previous native
segment; all 601 observed direction triples agree with that lookup. It is an
expected-only witness, not a captured input. Sound requests are compared through
their immutable command encoding; original queue ownership, stack temporaries,
pointers and CPU/device state are outside the typed equality claim.

The host proof passes ASan/UBSan and six malformed-input/divergence checks in
`build/ps2-native-main-vehicle-live-final-001/result.json`. The same persistent
checker passes all 601 updates as an EE executable in Play, with the same
fingerprint and byte-identical exported checkpoints; its record is
`build/ps2/native-vehicle-ee-001/runtime-validation.json`. That fixture includes
file IO and comparison work, so its total time is not a game-frame performance
measurement. Physical-console validation remains separate.

This proof covers the recorded course-zero vehicle configuration and the
supported component domains. Game startup, race flow, other vehicles, all game
modes, surrounding-car interaction, original presentation and playable-speed
validation remain separate work. The development app loads one typed checkpoint
and advances native state with live controls; that checkpoint is not a recovered
race initializer. See the [whole-update contract](main_vehicle_update.md) and
[integration milestone](../../../docs/ps2-native-rewrite.md#next-integration-milestone).

### Sampled component comparisons

Capture 012 adds a separate main-vehicle stream while retaining all 2,850
enabled/disabled frame checkpoints and the earlier capture streams unchanged.
Its 26 complete sampled callbacks contain 104 contact-slot calls, 26 paired
balance regions and 26 parameter selections. Saved caller frames bind each
child to its enclosing callback even when the child's raw return register has
changed after an interrupt. These snapshots validate isolated components; the
later continuous proof above establishes their joined state transition.

The contact-slot comparison matches all 104 recorded calls, including 99 active
and five inactive updates, plus 25,852 synthetic cases. Pair reduction matches
all 26 recorded regions plus 101,130 synthetic cases. Parameter selection matches
all 26 recorded copies plus 6,144 synthetic selections; every live sample chooses
record 2, so other indices rely on synthetic coverage. The candidate-preparation
original comparison accepts 16,315 of 17,044 synthetic evaluations and matches
all 26 sampled live candidate states; the remaining
729 exercise its explicit rejection of unsupported metric squared-subnormal
inputs and are not original-execution successes. Every sampled candidate prefix
has a zero pending delta, so live evidence covers the zero-metric branch; clamp
and active-consumption behavior are covered synthetically. Runtime fixture and live-proof
details are recorded in the [native rewrite guide](../../../docs/ps2-native-rewrite.md).

Capture 013 adds 26 segment-search pairs and 26 geometry-tail pairs without
changing any previous stream or the 2,850 replay checkpoints. Each sampled
segment result matches fresh original execution; all samples hit, so miss and
signed-wrap behavior rely on synthetic comparisons. The geometry tail matches
all seven output words in each sample, including three calls taking its
negative first-longitudinal branch.

The joined candidate/contact checker binds the two streams through the exact
one-instruction call transition, saved caller words, candidate storage and
unchanged object state. It matches all 26 composed results, including nine
active responses, without supplying captured intermediate offsets or replies
to the native operation. The 46-word result includes a marker preserved by the
inspected contact callee rather than sampled again at return; the initial
selected scalar is an unused/reset zero. These derived fields are labeled in
the private evidence report rather than represented as direct observations.

These historical component comparisons remain narrower than the later
continuous proof. Neither establishes all physical units, exceptional original
behavior or every caller domain. The native development driver now connects the
joined callback to live controls and diagnostic geometry, while the full game
and its original rendering/audio presentation remain unfinished.

Private disassembly, address-reference studies and input identities remain in
`build/ps2-native-player-research-001/`; no original instruction bytes or
mechanically translated implementation belong in the source repository.
