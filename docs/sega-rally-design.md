# Sega Rally recompilation design

This project extends the three-layer design in the inherited Daytona document:
generated game instructions, native board behavior, and the SDL3 platform host.
The sections **i960 static recompiler**, **Reference & validation**, and
**Platform layer & build** govern that reuse. Daytona-specific addresses and
hardware mappings are evidence to inspect, not defaults for Sega Rally.

## Required end state

- Generate Sega Rally's native game code reproducibly from verified user ROMs.
- Execute the actual game control flow, including menus, races, and results.
- Preserve geometry command, polygon-priority, texture, audio, timing, and input
  behavior using tested shared components from Daytona where the hardware agrees.
- Run through SDL3 and hardware Metal on the connected iPhone, with functioning
  touch/controller inputs and lifecycle handling.
- Validate actual game scenes and replayed inputs, including buildings, car
  decals, shadows, water, and foreground occlusion.

## Bring-up sequence

1. Verify and extract a specific ROM revision; retain hashes and the public
   board/ROM-map source. Generate reachable i960 code using Daytona's decoder
   and emitter, report coverage gaps, and compile the output.
2. Implement the Model 2A bus and boot services from public hardware contracts
   and executed code evidence. Match CPU state at bounded checkpoints. An
   unresolved indirect target is reported explicitly; it is not skipped.
3. Validate the graphics-processor program and command interface before selecting
   a native implementation. Do not substitute Daytona's program for Rally's.
4. Feed the shared geometry and renderer contracts with complete window,
   Z-adjust, face/link, texture, and ordering metadata. Verify saved real scenes
   and temporal stability, not only synthetic triangles.
5. Connect the verified game to the shared SDL3/Metal host. Bring up the correct
   sound-board behavior, inputs, saves, pause/resume, and frame pacing.
6. Run attract, course selection, driving, finish/results, and restart checks on
   the phone. Record what passed and what remains incomplete.

## Current evidence and unresolved work

The initial native bootstrap uses the inherited MAME-compatible host-double
FP state and operand semantics for differential analysis. New single/long
arithmetic, conversion, FP register/literal, and flushreg templates have
14,496 generated-code/reference comparisons, including additional long-real,
transcendental, and carry forms. This diagnostic mode deliberately
does not satisfy the inherited design's hardware extF80 requirement. There is
no claim that finite host-double parity proves the i960KB's extended precision,
exception handling, or all rounding modes. Resolve that gap with SoftFloat
state and tests before treating FP-dependent game behavior as hardware exact.
Other inherited reference details also remain diagnostic limitations:
`addc` currently preserves the reference's 32-bit intermediate sum (and its
carry loss), and `cpysre` follows its `>= 0` sign test for negative zero.
Reference equality is reported separately from hardware correctness.

The verified reset copy maps program bytes `0x1000..0x60fff` to writable
RAM `0x5a0000..0x5fffff`. Recompilation decodes at the RAM addresses to preserve
relative branches and loads, and checks every live instruction word before
execution. The opt-in label profile uses address annotations only; a separate
observed-target manifest records native missing-code evidence. No lifted
game implementation is copied into the new runtime.

Daytona's unchanged TGP generator and TgpBoard compile Rally's own 1,665-word
upload, not Daytona's microcode. A bus-level upload/startup probe returns `8`
after 108 native instructions and then waits on an empty FIFO. Tables and
collision data are independently checked against the pinned ROM map. Full
TGP command and race parity are still required.

The current bus exposes unsupported hardware accesses. Two observed unmapped
probes (`0x00f80000` and `0x01c00202`) follow the pinned reference's zero-read,
discarded-write contract with counters; these are not invented ready devices.
Only timing-independent IRQ reset operations are supported. Enabling a source
is still a frontier until the board clock, delayed mask update, timers, and
asynchronous sources are connected. Buffer reset and fresh backup SRAM match
the reference's `0x07800f0f` word pattern and all-ones initialization.

The older lifted-C prototype already reaches desert practice on the phone, but
contains incomplete game paths. It is a useful source of address annotations and
capture tooling. Its C implementation is not a substitute for verifying the new
generated core.

The rendering investigation reproduced whole windows disappearing while stopped.
Their camera-space depth crossed the wall depth by tiny amounts while their
arcade polygon priority remained constant. A single priority pass stabilized
windows in 16 captured frames, but an initial implementation without complete
metadata incorrectly hid a roof decal and drew a tan ground polygon. That
candidate was withheld from installation.

The prototype had dropped a real GEO Z-adjust command emitted during bootstrap
and did not retain window/culling metadata. Restoring the complete Daytona
priority/culling path and exact header palette fixes the roof, windows, and
tan shadow on 16 real captured frames. A direct raw-command audit matches
3,370 face/attribute records over 141 objects. The apparent cloud regression
was a comparison of different captures: the matched sky regions are identical
across all 16 frames. The corrected prototype was built and installed on the
phone; its launch was blocked by the locked device. This does not establish
full-game visual parity or on-phone play validation of that build.

## Repository boundaries

Only tools, native hardware code, tests, manifests, address annotations, and
written findings belong in Git. ROM bytes, generated game instructions, private
captures, emulator state, and compiled signed apps remain local under ignored
directories. Retain upstream licenses and record copied components with their
source revision. No leaked source or SDK is used.

The local development branch is `sega-rally`. Changes should be small enough to
review and preserve a testable checkpoint; do not declare a milestone complete
until its named behavior has been observed.
