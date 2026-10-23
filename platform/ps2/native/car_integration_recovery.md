# Car integration recovery boundary

This is an evidence map for the next native subsystem, not a recovered driving
model. No generic car physics or mechanically lifted implementation is used.
It follows the governing design's PS2 native-rewrite exception and its
requirement to prove arithmetic before optimization.

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
| `0x2abc0` | `0x2d2d4` | Table-query dependency receiving the pointer stored at `0x214354` and the object's signed halfword at `+0x88` |
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
The first and third values then have separate bounds involving pi/8. Those
post-fold bounds have not been included in the new angle component. The
middle angle is not subjected to that same final bound. Calling these axes
pitch, yaw and roll remains a hypothesis until their coordinate transforms and
recorded car states establish the mapping.

The routine mixes binary32 and binary64 arithmetic, floating registers,
comparisons, division, square root and atan. Rounding boundaries are observable;
replacing the whole routine with fused VU arithmetic would change the contract.
The reference interpreter's general host-double limitation still applies.
The bounded angle leaf is exceptional: its intermediate subtraction is exact
by the explicit dyadic proof, avoiding that uncertainty for its result alone.

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
explicit restricted contract and exhaustive folded-interval comparison. It
does not yet make the car integration routine native or the rewrite playable.
