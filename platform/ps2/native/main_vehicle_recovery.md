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
| `0x2b0b0` called at `0x34fc4` | Segment-selection helper receiving position and signed-index storage |
| `0x351a8..0x35218` | Separately rounded additions build candidate position and orientation triples for the [four-point contact pass](main_contact_pass_recovery.md) at `0x34380` |
| `0x2abc0` / `0x2acc0` at `0x35704` / `0x35710` | Shared course-query preparation and result interpretation |
| `0x37200` | Four explicit calls from the update; slot arithmetic and physical meaning still require recovery |
| `0x37870`, `0x34ce0` | Further state/arithmetic dependencies; not replaced by generic force or drivetrain models |
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

The callback contains additional geometry commands, conditional state updates,
transcendental operations and mixed binary32/binary64 arithmetic. Units, axis
meanings, exceptional behavior and full caller domains remain unproven. A full
typed update requires those dependencies and comparisons across actual control
sequences. The native application still has no connected driving loop.

Private disassembly, address-reference studies and input identities remain in
`build/ps2-native-player-research-001/`; no original instruction bytes or
mechanically translated implementation belong in the source repository.
