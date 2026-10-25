# Car integration recovery boundary

This is an evidence map for the next native subsystem, not a recovered driving
model. No generic car physics or mechanically lifted implementation is used.
It follows the governing design's PS2 native-rewrite exception and its
requirement to prove arithmetic before optimization.

The original map below covers one integration route. Actual driving observation
now distinguishes it from the alternate selected by object field `+0xe8`.
The dispatcher calls `0x2cce0` for value zero and `0x2e650` for value one;
value two takes an inline path. These values do not yet establish car type or
player/AI identity. In the bounded nineteen-event Lancia driving replay,
frames 2200 through 2800 reached the alternate and its shared velocity/blend
1,202 times, but never entered the first integrator. Native work must follow
this executed path rather than assume every static label is the live caller.

Capture 007 also identifies a **separate main-object callback** at `0x34f40`,
with 601 calls on object `0x500000` and a verified connection to cabinet steering.
The [main vehicle dependency map](main_vehicle_recovery.md) records this path.
Recovering the alternate integrator alone cannot implement the controlled car.

## Verified identity and dependencies

The address-only integrator lead is Revision B ROM `0x2cce0`, 6,508 bytes,
SHA-256 `588b922ab1a5d4462e3e84ab1c93855402eeb302ad71793aac2aa156d794165d`.
The complete program identity is recorded in [angle folding](angle_fold.md).
Fresh decoding produces 1,299 instructions in this declared span, terminating
at the return at `0x2e648`. This is static coverage, not proof that every path
has executed during a race.

| Direct callee | Caller locations | Established dependency |
| --- | --- | --- |
| `0x30e00` | `0x2cdd4` | Prior velocity-step address label; mutates the same object |
| `0x2f8a0` | `0x2ce54` | Prior contact-solve label; receives object plus candidate vectors |
| `0x30320` | `0x2d294` | Calls `0x2b0b0`, with a conditional `0x30380` path; the latter also exchanges TGP data |
| `0x2abc0` | `0x2d2d4` | Query preparation receiving the descriptor selector stored at `0x214354` and the object's signed halfword at `+0x88`; see [neighborhood recovery](contact_neighborhood.md) |
| `0x30140` | `0x2d328` | Four-iteration contact loop; calls `0x2acc0`, updates per-slot values and packed result bits |
| `0x27b28` | `0x2e568`, `0x2e578`, `0x2e588` | One-step angle folding; first isolated arithmetic component |

These labels describe analysis leads, not verified physical meanings for every
parameter. The two unlabeled helper ranges were decoded directly rather than
assigned invented gameplay names.

The integrator itself contains 133 static stores and 52 static reads at the TGP
FIFO. Counts exclude callees and are **instruction sites**, not transactions per
update. FIFO results subsequently participate in state arithmetic and decisions;
substituting a modern matrix or collision library would require a numerical and
semantic proof. Rendering-only VU equivalence does not supply that proof.

The first argument is an object pointer. The second survives as a descriptor
table pointer: at `0x2d70c`, an object word at `+0xe4` selects byte
`12 + 16*index` in that table. Its bits select one of eight tuning words in the
live-copy table at `0x5cabe0`. Neither the descriptor index bounds nor the
complete object layout have been established yet.

Other directly referenced tuning/global inputs include `0x214124`,
`0x2141c4`, `0x2141cc`, `0x2141d0`, `0x2141d4`, `0x2141ec`, `0x2141f0` and
`0x214354`; callees bring additional inputs. These addresses belong only in
offline extraction/oracle adapters. A native interface must receive explicit
typed tuning and collision data rather than retain mapped-memory accesses.

## What can and cannot be named yet

The existing pose-publication mapping identifies the object groups at `+0x14`,
`+0x2c` and `+0x38` as position-, motion- and orientation-like triples. The
integrator updates these groups, consumes other triples at `+0x20` and `+0x44`,
and uses divisors at `+0x50` through `+0x5c`. This alone does not prove units,
mass/inertia interpretation, gravity scale, axis order or integration timestep.

At the end, three explicit calls fold the orientation values once around pi.
The first and third values then have separate bounds involving pi/8. The
[orientation finalization component](orientation.md) now composes the three
folds and those bounds, with an explicit restricted input domain and private
original-tail comparison. The middle angle is not subjected to that same
final bound. Calling these axes
pitch, yaw and roll remains a hypothesis until their coordinate transforms and
recorded car states establish the mapping.

The routine mixes binary32 and binary64 arithmetic, floating registers,
comparisons, division, square root and atan. Rounding boundaries are observable;
replacing the whole routine with fused VU arithmetic would change the contract.
The reference interpreter's general host-double limitation still applies.
The bounded angle leaf is exceptional: its intermediate subtraction is exact
by the explicit dyadic proof, avoiding that uncertainty for its result alone.

The shared velocity helper now has an independently recovered
[planar direction blend](direction_blend.md), and the contact-query preparation
has a typed [cyclic neighborhood selector](contact_neighborhood.md). Their
isolated original-program and EE checks do not implement the surrounding
transforms, intersection search or force calculations.

The alternate's orientation update is different from the first integrator's
three-fold/pi-over-eight tail. It folds a stack-resident candidate triple, then
uses wrapped differences and separate smoothing/step rules. The existing
`finalize_orientation` function must not be substituted for this path. The
separate [alternate orientation component](alternate_orientation.md) now
matches its original arithmetic and all 34 sampled live states within its
explicit domain. The [executed-path dependency note](velocity_step_recovery.md)
corrects an earlier attribution beyond the alternate's return. The surrounding
integration and force calculations remain unrecovered.

## Next validation boundary

1. Capture bounded entry/exit states from actual race updates, recording both
   arguments, all touched object/table inputs and each dependent query result.
2. Recover the descriptor/contact-query inputs and outputs independently,
   with documented bounds, units where proven and original-case replay.
3. Establish each arithmetic boundary, including exceptional inputs and actual
   rounding-control assumptions, before replacing integration calculations.
4. Connect typed components only after live caller domains and cadence are
   established; compare recorded full-course behavior afterward.

The recovered [angle operation](angle_fold.md) is a real dependency with an
explicit restricted contract and exhaustive folded-interval comparison. The
[orientation tail](orientation.md) adds the established three-channel order
and asymmetric bounds. These components do not yet make the car integration
routine native or the rewrite playable.
