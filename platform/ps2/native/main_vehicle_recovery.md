# Main vehicle update recovery

The vehicle update at ROM `0x34f40` is separate from both integrators in the
[earlier dependency map](car_integration_recovery.md). Recovering the alternate
routine at `0x2e650` does not recover this callback. The distinction must remain
explicit when selecting work for the native driving loop.

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
| `0x39a90` called at `0x34f8c` | Recovered [cabinet steering filter](cabinet_steering_filter.md); result stored at object `+0x74` |
| `0x33e90` called at `0x34fb0` | Conditional render-state consumer; full effect not recovered |
| `0x2b0b0` called at `0x34fc4` | Recovered [segment selection](segment_selection.md), including ordered search and the unclamped miss result |
| `0x34ff0..0x35218` | Recovered [candidate preparation](main_candidate.md): pending-middle clamp, metric gate, conditional state update and candidate triples for the [four-point contact pass](main_contact_pass_recovery.md) |
| `0x34ff0` through contact return at `0x3521c` | [Joined candidate/contact operation](main_contact_step.md), deriving all contact inputs from the newly prepared vehicle |
| `0x2abc0` / `0x2acc0` at `0x35704` / `0x35710` | Shared course-query preparation and result interpretation |
| `0x37200` | Recovered [main contact-slot update](main_contact_slot.md), called four times; bounded scalar/history/type/response behavior, with explicit private normalization coefficients |
| `0x37870` | Recovered [parameter selection and paired balance](paired_balance.md), plus [final geometry/publication tail](main_balance_geometry.md); intervening filtering and event-related dependencies remain unrecovered |
| `0x34ce0` | Counter, feedback and event scheduling through return at `0x34e38`; calls `0x261b0` and BAL dependency `0x26e18` remain outside the native recovery |
| `0x371b0`, `0x371c0`, `0x371d0` | Three calls to the recovered one-step angle fold, without the earlier integrator's final pi/8 clamps |
| `0x34078` at `0x371dc` | Recovered [main-object pose publication](main_car_pose.md), including raw flags/index words and an external four-word state group |

The last publication helper differs from the recovered `0x2bc08` operation.
It copies full source words at `+0x10` and `+0x88` into destination `+0x50`
and `+0x54`, and copies the four global words at `0x2138a0..0x2138ac` into
destination `+0x40..0x4c`. The earlier helper's persistent flag merge is not a
valid substitute. The native `make_main_car_pose` operation receives explicit
typed state and four external values, returning the existing `RenderPose`
value type without mutating either input. Original addresses belong only to
its offline comparison adapter. This isolated recovery does not establish
the complete main-callback behavior.

The `0x34ce0` helper's code span is `[0x34ce0,0x34e3c)`, 348 bytes. Extending it
to `0x34f40` includes following data, not additional helper instructions. Its
three observed return instructions are at `0x34d0c`, `0x34d50` and `0x34e38`.
It must not be described as a generic force or drivetrain calculation.

The complete `0x37870` helper spans 3,588 bytes and has nine direct/BAL
dependencies plus geometry FIFO operations. Both vector pointer arguments are
in/out: the tail modifies the second and stores a geometry result through the
first. The recovered pair reduction, raw record copy and final geometry/publication
tail do not establish the whole helper's result or side effects. The tail starts
after the last CPU callee; its explicit inputs remain dependent on unrecovered
earlier filtering, limiting and event paths.

## Current comparison scope

Capture 012 adds a separate main-vehicle stream while retaining all 2,850
enabled/disabled frame checkpoints and the earlier capture streams unchanged.
Its 26 complete sampled callbacks contain 104 contact-slot calls, 26 paired
balance regions and 26 parameter selections. Saved caller frames bind each
child to its enclosing callback even when the child's raw return register has
changed after an interrupt. These snapshots validate isolated components;
the native application does not execute the complete callback.

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

The callback still contains additional geometry commands, conditional state updates,
transcendental operations and mixed binary32/binary64 arithmetic. Units, axis
meanings, exceptional behavior and full caller domains remain unproven. A full
typed update requires those dependencies and comparisons across actual control
sequences. The native application still has no connected driving loop.

Private disassembly, address-reference studies and input identities remain in
`build/ps2-native-player-research-001/`; no original instruction bytes or
mechanically translated implementation belong in the source repository.
