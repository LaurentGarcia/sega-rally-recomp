# Bounded orientation finalization

`finalize_orientation(OrientationAngles&)` is an independently written native
operation recovered from the car integrator's final state update. Its three
fields are named `first`, `middle` and `third`: channel order is established,
but pitch/yaw/roll, axis direction and handedness are not.

This is a recovered **tail region**, not a separate original function and not
a recovered driving model. It follows the native-rewrite boundary and exact
arithmetic requirement in the governing design. Production code contains only
typed values, integer bit operations and the existing bounded angle operation.

## Contract

All three input values must be binary32 values with magnitude no greater than
`round32(2*pi)`. An out-of-domain value, infinity or NaN rejects the entire
triple without changing any channel bit. This is a native API restriction;
the original program does not implement rejection. Live callers have not yet
been proven to satisfy this domain.

The operation performs one [angle correction](angle_fold.md) independently on
each channel. It then bounds the first and third channels to signed
`round32(pi/8)`. The middle channel receives only the angle correction.
The bound tests use a strict comparison against binary64 `pi/8`, after the
corrected value has been rounded to binary32.

For example, `first = round32(pi)` folds to the negative side before being
bounded, so the final first channel is `-round32(pi/8)`. Bounding before folding
would produce a different sign. The largest binary32 below `pi/8` is unchanged;
the nearest binary32 to `pi/8` lies slightly above the binary64 constant, but
bounding it writes the same bit pattern. Untouched values preserve signed zero
and subnormal bits. No floating arithmetic or floating-point control mode is
needed by the native implementation.

The bit threshold follows binary32 ordering: binary64 `pi/8` lies strictly
between binary32 encodings `0x3ec90fda` and `0x3ec90fdb`; the latter is its
nearest/even binary32 result. Negative values use the same magnitude threshold.
The prior exact-difference proof for the bounded fold remains applicable.

## Original-program evidence

Revision B program image: 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.

The tail is ROM `[0x2e560, 0x2e638)`, 216 bytes, SHA-256
`44898c50a385def77f4e81d33bc88544534a0820c0d667de4ada13e25badbc6d`.
It calls the already verified angle leaf at `0x27b28` three times. The private
oracle executes the live-copy addresses and stops before the integrator's
register-restoration/return epilogue. It permits instruction fetches only in
the tail and angle leaf, and data access only to the three object words at
`+0x38`, `+0x3c` and `+0x40`.

Within the tested domain, the original tail executes 69–106 instructions,
reads those words 5–7 times and writes them 3–5 times. The native contract
matches final channel values; it does not reproduce register changes,
intermediate store ordering, exception flags or a mapped-memory interface.
Those effects are outside this typed native boundary.

The containing routine, dependencies and unresolved caller invariants remain
documented in [car integration recovery](car_integration_recovery.md). The
operation is not yet connected to a native race update.

## Validation

```sh
python3 tests/test_ps2_orientation.py
python3 platform/ps2/tools/check_orientation.py \
  --program /path/to/verified/program.bin \
  --output build/native-orientation-oracle-001
```

The private original-program differential passes **294,300 triple evaluations,
294,260 unique input triples, 882,900 channel operations and 25,111,815 original
instructions**. It observes 1,743,128 allowed reads and 1,154,528 allowed writes;
any other data access fails immediately. Result fingerprint:
`40f5b5992d4371cc`. Runtime on the validation host was 0.232 seconds.

Coverage includes the Cartesian product of 30 signed mathematical endpoints,
all represented exponent classes with selected fractions, adjacent encodings
around the clamp/fold/accepted-range boundaries, 131,072 deterministic random
triples and a stride-127 sweep of the folded interval in both signs. The sweep
is deliberately bounded and is **not exhaustive over all triples**. The
separate angle operation already has exhaustive folded-interval validation.

Portable tests also compare against an independent binary64 numerical
reference and test atomic rejection in each channel: **321,331 checks** pass,
including a host AddressSanitizer/UndefinedBehaviorSanitizer run.
EE-compatible tests use
27,000 endpoint combinations with independently calculated expected bits,
four specific channel/order cases and 27 rejection cases. The host-only oracle
and interpreter remain outside the native target. Private evidence and all
original-program bytes stay under ignored `build/`.

The standalone native EE fixture passed **27,031 checks** in Play 0.72 with
the HLE BIOS. Executed ELF SHA-256:
`cc3fbf750b7876d10caf39017d75842bd78a29fc10cd99901ab54a61312bd21b`.
Its bounded emulator run lasted 17.084 seconds, including startup and log
collection; that duration is not a kernel performance measurement. This is
native EE code executed in an emulator, not a physical-console result or a
full-race validation.
