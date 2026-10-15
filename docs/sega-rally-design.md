# Sega Rally recompilation design

This project extends the three-layer design in the inherited Daytona document:
generated game instructions, native board behavior, and the SDL3 platform host.
The sections **i960 static recompiler**, **Reference & validation**, and
**Platform layer & build** govern that reuse. Daytona-specific addresses and
hardware mappings are evidence to inspect, not defaults for Sega Rally.

Current development scope is Mac only, following the 2026-10-14 direction.
Complete and validate the native game before resuming iPhone work. The shared
SDL3/Metal architecture remains; device builds, installs and testing are deferred
until requested again.

## Required end state

- Generate Sega Rally's native game code reproducibly from verified user ROMs.
- Execute the actual game control flow, including menus, races, and results.
- Preserve geometry command, polygon-priority, texture, audio, timing, and input
  behavior using tested shared components from Daytona where the hardware agrees.
- Run through SDL3 and hardware Metal on Mac, with functioning keyboard/controller
  inputs and lifecycle handling. Resume the iPhone target after the native game
  is ready and device work is requested again.
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
   Mac. Record what passed and what remains incomplete; phone checks are deferred.

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

An individually audited startup table at `0x005af85c` supplies three additional
targets across seven entries. The importer verifies the unsigned index bound,
scaled load, indirect branch register, contiguous control flow, code/data
hashes and exact reset-copy aliases before adding candidates. Table candidates
remain distinct from executed targets; neither is evidence of complete game
coverage. A second table at `0x005ddfc0` has 32 masked entries and 17 nonzero
callbacks. The inherited decoder checks the index mask, saved object pointer,
disjoint intervening stores including longword width, reload and null guard.
A narrow continuation pass follows explicit absolute loads and moves through
a decoded straight-line whitelist to adjacent indirect branches. Known seed
and direct-transfer side entries invalidate its reaching definition. Each
candidate retains its source span hash and original seed provenance; unknown
indirect side entries and exclusive coverage are not established. Conditional
paths remain outside this proof. Actual callback frontiers are separately
recorded with instruction/cycle counts and ignored execution-report paths.
Unsigned compare/branch tables also admit intervening frame spills only when
they preserve both the comparison flags and index. The bounded index and
loaded branch destination are checked independently. Twenty-two audited tables
now contribute 116 candidate rows. Sixteen scoped object-update registrations
add a separately recorded candidate class: constructor/update roots must
preserve the object base and absolute callback definition through decoded
operations to its +4 pointer cell, with the caller's load/null guard/same-object
argument independently verified. Already-labeled targets retain inferred roles
for subsequent update-chain analysis. MOV explicitly propagates saved object
aliases and callback constants. A direct BAL may cross a decoded straight-line
leaf only when its BX return uses the preserved incoming link; every written
register and BAL's own g14 write kills matching facts. Each leaf span, hash,
call site and clobber set is retained in provenance. This recovers two known
constructor registrations without an assumed callee-save ABI. Unknown calls,
nested control flow and conditional paths remain outside this proof. The frozen first-car profile reaches its
first geometry at rendered frame 1,357 and a visible textured car/course scene
by frame 1,634; this is CPU-renderer evidence. Metal and race validation were
still pending at that checkpoint; later comparisons are recorded below.

Daytona's unchanged TGP generator and TgpBoard compile Rally's own 1,665-word
upload, not Daytona's microcode. A bus-level upload/startup probe returns `8`
after 108 native instructions and then waits on an empty FIFO. Tables and
collision data are independently checked against the pinned ROM map. Full
TGP command and race parity are still required.

The current bus exposes unsupported hardware accesses. Two observed unmapped
probes (`0x00f80000` and `0x01c00202`) follow the pinned reference's zero-read,
discarded-write contract with counters; these are not invented ready devices.
Without the optional control adapter, only timing-independent IRQ reset
operations are supported. The adapter implements delayed mask updates, four
timers and requests from attached sources. Buffer reset and fresh backup SRAM match
the reference's `0x07800f0f` word pattern and all-ones initialization.

The 315-5649 register chip, Model 2A byte-lane/cabinet wiring, and 93C46
EEPROM are separate native devices. The EEPROM requires explicit monotonic
nanosecond timestamps; serial edges never advance time themselves. The bounded
runner now derives time from the CPU's pinned-reference cycle deductions at
25 MHz. Every opcode charges before its memory effects; call/return helpers
also charge during interrupt entry/return. This preserves a separate completed
instruction count and monotonically increasing device time through reset/IAC.
Unknown costs fail generation, and stale untimed objects are rejected by the
runner. Costs have independent interpreter comparisons, but retain MAME's
fixed estimates and omit burst stalls and full scheduler behavior. The chip's
inherited serial-status constant is a documented
MAME shortcut, not a simulated UART-ready signal. Drive commands and lamps
are latched; the missing drive-board firmware is not executed.

The communication adapter reuses the inherited CommBoard simulation with
explicitly disconnected endpoints. Shared RAM, mirrors and CN/FG register
semantics match the pinned reference. A null transport would skip the
reference's no-peer initialization, so the adapter supplies closed endpoints
without opening sockets. It never manufactures a successful link and does
not execute the missing Z80 firmware. Frame callbacks run from the board's
video event.

The optional screen path follows the inherited **Frame loop** and
**Reference & validation** sections with a more explicit diagnostic clock.
`Model2AMachine` owns that common machine for both the bounded runner and the
platform frontend. Relative instruction budgets and frame-boundary stepping
resume the same CPU/device state. Unsupported code or device accesses stop
the machine persistently; repeating a run cannot silently retry a partly
executed instruction. EEPROM and backup images load only before execution.
Every bus access and instruction boundary synchronizes one shared event queue;
callbacks run at their deadline, and IRQ edges wait for a CPU boundary. Equal
deadlines preserve insertion order across timers, delayed mask writes and
screen events. This does not reproduce MAME's complete CPU timeslice machinery.
The pinned screen starts its first vblank at 17,384,000 ns and ends it at
19,024,000 ns. Subsequent frames take 17,384,000 ns. The frame latch updates
before geometry parsing; vblank IRQ and communication follow parsing. Rendering
occurs at vblank end before the screen frame counter increments. Soft reset
preserves screen phase. A visible-line duration of 15,744,000 ns would be the
wrong initial vblank deadline.

The video adapter owns stable memory for the shared Video renderer and exposes
the exact Model 2A ranges, mirrors and byte lanes. Geometry uses independently
verified polygon and texture ROM images; CPU texture/luma RAM remains distinct.
All four tile mirrors notify the renderer. Reference maskless texture and
CRTC writes retain their partial-write behavior. Framebuffer test-mode storage
is mapped, but its presentation is an explicit unsupported frontier. Normal
screen rendering uses either the inherited CPU reference renderer or the same
Daytona SDL_GPU renderer through the dedicated SDL3 host. Metal is required on
macOS and iOS. Separate machine instances check execution counts and frame
boundaries during render comparison. The startup warning at frame 513 matches
all RGB pixels; no polygon or racing-scene parity is inferred from this tile
screen. The host provides touch/keyboard inputs, persistence, pause/resume,
and board-time pacing. A separate iOS bundle preserves the earlier prototype.
The first paired car scene at frame 1,634 has matching CPU state but 15,830
different RGB pixels out of 190,464, with mean absolute channel error 0.7278/255;
graphics fidelity remains under investigation. Hidden Metal captures preserve
the visible Mac instance. The Mac launcher validates assets, packages them in
SDL's `Contents/Resources` base path, and starts a normal application through
LaunchServices with a persistent diagnostic log.
An isolated same-scene CPU triangle-fan rerender reduces the GPU difference
to 1,602 pixels and mean channel error 0.040408/255. This attributes 94.45% of
mean error to polygon interpolation: the CPU uses outer scanline edges for
four-to-eight-vertex polygons; the default GPU path uses triangle fans.
The optional `SEGARALLY_GPU_POLYGON_SCANLINES=1` path now uploads the same two
edge chains and uses a conservative rectangle plus rounded-span rejection for
solid and textured polygons. It preserves the common shader, ordering, scissor
and tile composition. First-car differences fall to 822 pixels and mean channel
error 0.019426/255; two driving captures also improve. The candidate with the
option disabled matches the committed stable renderer in those captures.
The extension to triangles follows their separate CPU slope/plane setup and
raises the native-resolution synthetic fixture to 447 cases. It repairs the
previously omitted triangle at (147,20). In a saved 3D-only reconstruction,
differences fall from 969 to 844; these counts exclude tile/character RAM and
cannot be compared directly with the complete-scene counts above. The one
newly worsened pixel at (95,31) is a measured repeated-addition versus direct-
offset rounding difference; an ignored diagnostic loop restores the CPU bits,
with unmeasured cost. Extending that diagnostic to four-or-more-vertex polygons
confirms GPU cutout ownership changes caused by accumulated rounding. Combined
repeated stepping leaves three differing pixels in the same 3D-only snapshot,
including one slightly worsened pixel; all 447 synthetic cases and the exact
disabled-mode image guard pass. The initial candidate failed that guard and is
excluded. A subsequent three-complete-scene
gate reproduces historical CPU/stable outputs and raw tile/character composition
exactly before comparison. Combined repetition reduces differing pixels from
697/1,241/581 to 3/1/1, with one newly worsened pixel per scene. Those saved
v5/v6 states support complete scene replay without executing the game.
A clean implementation now lives behind the existing default-OFF option. Its
regenerated common shaders pass all 447 synthetic cases and reproduce those
three accepted complete images exactly, while OFF preserves the historical
stable images. Repetition applies only at native scale 1; enhanced scales keep
their previous direct interpolation. No diagnostic switches remain in this path.
An initial bounded offscreen measurement of the diagnostic, not the clean
implementation, finds repeated-path completed-render
medians of 2.119/2.126/2.527 ms on Apple M5. The first-car diagnostic comparison
shows about 51% higher latency; driving controls vary too much to generalize
the loop cost. This includes encoding/submission/completion wait and excludes
presentation and readback; it is neither GPU timestamp timing nor gameplay FPS.
Residual texture precision, live cost and whole-game fidelity remain open;
the option stays off by default. See [the evidence table](segarally-status.md#graphics-evidence).
Polygon scanline rasterization is distinct from the optional CRT presentation
pass. Newly built hosts offer adjustable scanlines, phosphor mask, glow,
curvature and color fringing after native composition. Effects default off;
the original linear blit is retained. Common HLSL generates the backend shader
headers, without a Metal-only implementation. F1 opens a lazy ImGui Display
panel and pauses guest execution and cabinet replay; resuming clears playback
backlog and resets host pacing. Presentation preferences use only the separate
bounded `display.settings` file. Raw captures precede CRT/UI, and offline replay
still returns before SDL or preference loading. Disable effects and native
texture enhancements for baseline image comparisons. The CRT fixture passes
18 synthetic draws and two saved-scene preview draws; the ROM-free UI and
preference checks pass. A 2,600-frame sound-enabled off/on host comparison
preserves all ordered resumes, PCM, saves and the native image exactly. The
final v15 app has launched with a visible CRT swapchain pass and sound. These
results do not establish monitor fidelity or sustained live effect cost.
The host also accepts the bounded cabinet replay and holds its final snapshot
through one complete frame before handing control to keyboard/controller/touch. Both
rendering instances receive identical inputs. Normal board-time pacing is the
Mac launch default; unthrottled runs are explicit diagnostics.

The v16 host adds optional SDL gamepad input and a Controls tab within the
paused Settings panel. It retains Rally's cabinet ranges and wiring while
adapting Daytona's button/half-axis model. Bindings, dead zones and steering
response persist separately from cabinet state. Menu/focus/replay transitions
require held controls to return to neutral before rearming. The pure mapper
and virtual SDL devices pass 329 checks; the actual host passes pause/focus
input-isolation checks, and the menu passes both small and normal window sizes.
The connected Xbox One Elite 2 is detected by an external SDL probe. Physical
driving/navigation feedback is separate from those checks. The final v16
2,600-frame sound/CRT scripted comparison preserves native PCM, pixels, saves
and ordered resume observations exactly; no guest timing change is involved.

Manual capture needs a separate host-resume transcript because controls are
sampled between bounded execution calls, including mid-frame. An IRQ can also
complete a frame at an unchanged instruction count. The tested transcript core
preserves each requested budget, all cabinet pins, before/after witnesses and
return reason. The Mac host now adds opt-in recording and early same-executable
offline replay, binding loaded images/options and initial saves, per-resume PCM,
detailed faults and final saves. Its synthetic bundle/launcher tests pass.
Actual sound/hook v13 boot and driving captures also replay exactly, with
recorded/unrecorded input boundaries, complete PCM, final Metal image and saves
matching through frame 2,801. Offline replay opens no SDL/GPU/audio device and
changes no bundle/live saves. Full-course manual and independent CPU/sound
validation remain open. See
[the input reference](segarally-input-replay.md#exact-host-resume-transcripts).
The frozen v6 generation now executes the actual Lancia race with recorded
cabinet throttle, steering, view and brake events. A bounded 600-million main
instruction run renders 3,142 frames without an unsupported frontier; matched
captures show acceleration, automatic shifting, steering and braking responses.
The linked native sound board completes the same run with identical main state
transcripts and final pixels. The Mac Metal host has visibly passed GO with
selection-replay handoff to manual controls. These observations do not close
the full-race, hardware-precision, exact raster or on-phone validation gaps.
The separate held-gas replay now completes Desert and its result screen
(1:12.15), enters the forested second course, and reaches TIME IS UP. It supplies
centered steering rather than following the second course's bends. Six additional
actually executed callbacks extend the frozen v13 graph to 75,205 instructions;
the existing static table and registration proofs are unchanged. A subsequent
two-billion-instruction run returns to attract mode without a missing-code or
device frontier. A separate fresh-save stable-host check subsequently accepts
two more coins and Start and reaches Car Select at frame 11,160, with all 27
scripted inputs applied. The v14 sound-enabled extension preserves the restart
event boundaries and begins a second race, with its frame-12,800 capture
showing 118 km/h in fourth gear at 15.83 seconds elapsed. All 32 cabinet events
apply and the complete captured PCM matches per-resume hashes. The Game Over
overlay, complete second race and remaining courses still require validation.
A later sound-enabled v13 recording reaches a missing return at `0x005ae800`
after 12,785 frames. Adding that observed target and its canonical ROM alias
extends the v14 graph to 75,207 instructions without changing prior addresses
or compiler heuristics. A new capture matches all 25,580 completed normal
resume/PCM rows, excludes the old partial frontier CPU outcome, and continues
normally through frame 14,001 and 2,675,194,732 instructions. This same-Machine
comparison across generations does not replace independent CPU or hardware
validation, and production recording identity checks remain strict.
Independent v6 driving comparison matches 600 million instructions, all nineteen
input events, 19,641,024 ordered writes and 3,142 renders against the reference
CPU with shared board implementations. It is not complete MAME-machine parity.
The frozen v13 held-gas path also passes two billion instructions,
86,328,770 ordered writes, 4,650,936,033 reference cycles, 113,101 IRQs and
10,701 renders. All nineteen input boundaries and the final framebuffer hash
`2e2d4fb9e7f002ad` match the preserved actual native run. This raw independent
CPU harness uses a null hook callback and shared boards/TGP/geometry, with
sound off. Full video RAM and framebuffer equality are terminal checks;
per-instruction state/write/event checks do not establish per-frame pixel,
Model2AMachine scheduling, coupled-sound or full MAME/PCB parity.

The newer whole-recording consumer separately checks actual Machine resume
boundaries under the declared phase contract. A fresh sound-off v14 driving
bundle matches 5,201 resumes, 500,035,460 instructions and 15,156,607 ordered
writes through frame 2,600, with exact cycle/PC/input witnesses and unchanged
bundle bytes. It still uses independent i960 execution with shared boards and
rejects sound-on/fault captures. This closes the earlier short-boot-only scope
for that one recorded drive, not complete race or coupled-sound validation.

The slow sensation of speed is measured in a hidden, normally paced Mac Metal
run with sound disabled: 480 race frames take 14.163 seconds of wall time for
8.344 seconds of board time, or 33.89 FPS and 58.92% normal speed. Core execution
accounts for 99.43% of wall time; the CPU cost of GPU encoding is 0.122 ms/frame
and is not a GPU completion measurement. Hidden-window acquisition does not
measure visible VSync blocking. Optimization must preserve per-instruction
timing, live instruction guards and exact event/IRQ boundaries; changing the
game clock is not a speed fix.
An optional instruction-boundary hook now keeps exact generated execution
inside chunks while retaining every boundary synchronization, IRQ delivery,
live-word guard and cycle charge. All 51 instruction bodies remain byte-identical
apart from separate capability markers. Generator/source/object audits and
per-chunk version/layout checks reject stale or incompatible builds; ordinary
builds and the old outer-loop path remain available.
Actual old/new Machine comparison matches 600 million driving instructions,
19,641,024 ordered writes, 32,576 IRQ entries and all 3,142 framebuffers. The
independent CPU transcript remains byte-identical to the prior checkpoint.
The sound-enabled old/new comparison also matches each PCM drain, with the full
54.6297052-second WAV byte-identical to the earlier capture. A separate quiet
normal-paced SDL/CoreAudio test reaches 2,760 frames and 47.98148 seconds of
board time in 48.519993 seconds including process startup/exit, with zero audio
input shortfalls or chip/host sample drops. The old sound path recorded 3,222
estimated input-demand requests at that checkpoint. Hidden presentation and
SDL counters do not establish user hearing, every course or physical audio
fidelity. The user has separately heard a ten-second native PCM excerpt on the
Mac speakers. An earlier v13 windowed session supplies 7,165,805 stereo frames and exits
normally after 9,347 rendered frames with no drops or estimated SDL input
shortages. Live-game listening feedback and physical fidelity remain open.
The host now opens its physical audio device after window/Metal initialization,
following Daytona's order and before establishing the pacing origin. This
startup-only move resolves the observed `AudioQueueStart: -66681` failure in a
60-frame windowed probe with zero audio drops or estimated input shortages.
The probe's boot PCM is silent, and the underlying CoreAudio failure mechanism
is not established by that ordering comparison.
Bundled assets are verified Xcode resource inputs so signing follows copying,
including incremental builds; post-signing bundle mutation is not allowed.
The Mac launcher also copies the selected cabinet replay into app resources
before signing, so startup does not depend on opening the original workspace
file. This packaging change preserves replay bytes and execution behavior.

Nine verified graphics ROMs produce polygon, texture and unused video-table
analysis images under ignored build output. Only polygon and texture images
are connected to Geo. The lookup-table image is not a claimed new hardware
implementation. The importer validates every input before writing output.

Timer/IRQ reference details matter: acknowledgment has no write mask, partial
timer writes combine with the last read latch, delayed enable replacement uses
the active mask, disabling a source does not clear its latched request, and
soft reset does not cancel an already pending delayed-enable callback. These
are preserved reference contracts, not conclusions from PCB measurements.

The main-board UART now uses explicit physical edges at 500 kHz, beginning
with a rising edge at 1 us. Its ready outputs feed the IRQ controller while
CPU line delivery stays at instruction boundaries. The real recovery sequence
briefly programs idle synchronous mode before resetting into asynchronous 8N1
at x16; rejecting that programming sequence would stop valid startup. Actual
synchronous transfer remains unsupported. Without the optional sound diagnostic,
SCSP RX remains unconnected and idle, with no fake responses. The shared 68000 generator now has an explicit Model
2A map: 512 KiB RAM, SCSP at `0x100000`, 512 KiB program at `0x600000`, and
8 MiB sample ROM at `0x800000`. The address-space adapter preserves big-endian
byte lanes and ordered longword accesses. Board reset copies only the first
16 program bytes to RAM; the generated game code clears RAM itself and must
install its vectors. Every generated instruction checks its live words.
The actual reset path executes 278,301 instructions before its first SCSP
access, a high-byte write at `0x100400` from PC `0x600184`. This establishes
static sound-code execution and the memory map, not chip or audio parity.
The native SCSP component now uses shared big-endian sound RAM, the common
event queue, explicit MIDI pins and interrupt callbacks that retain distinct
clear/assert/hold states. All 32 voices and the DSP follow the pinned public
source; buffers are bounded and drainable at 44.1 kHz. An explicit reference
random provider makes the source's noise sequence reproducible without
claiming physical RNG fidelity. Independent unmodified source comparisons
cover samples, DSP, registers, DMA, timers and serial behavior. A real
UART-to-SCSP pin test also transfers every byte in both directions concurrently.

Sample deadlines follow the reference stream's `floor(time * 44100) + 1`
sample count for positive time: the first sample is observable at 1 ns,
later samples at the ceiling of their rational index. No work occurs at zero.
Timers round reference attosecond deadlines upward to nanoseconds. The native
queue produces samples proactively; MAME's lazy stream updates and speaker
resampling remain outside this comparison. Reference envelope/LFO/noise/DSP
limitations, immediate DMA and the missing 1Fs interrupt are retained.
An opt-in diagnostic now accounts 68000 instruction totals at 11.2896 MHz,
including RAM/SCSP waits, modeled prefetch transfers, dynamic instruction costs
and phase-dependent autovector IACK. Unsupported exception/timing paths stop.
The common queue executes exactly one sound instruction or IRQ entry per
callback. Reset consumes its modeled cycles before the first instruction,
without advancing the caller's clock during construction. SCSP and the main
UART exchange real serial pin edges; no command reply is substituted.

This follows the inherited **Audio** section's static-program approach with a
Rally-specific scheduling contract. MMIO still occurs at each instruction's
start boundary, with no value-bearing prefetch or individual bus-phase model.
Independent public microcode tests also compare architectural registers,
stack banks, PC, defined SR and complete synthetic memory: 11,551 completed
instruction cases, 280 IRQ entries and two resets pass with fresh generated
fixtures. Undefined division-overflow flags, reset state and unsupported
exceptions remain explicit exclusions. A separate ROM-free test now runs the
production emitter's synthetic program against continuous pinned MAME CPU
execution on separate board instances. Two 2 ms cases compare every event
boundary, including full RAM, CPU state, queue order, physical UART echo,
IRQ/RTE and nonzero PCM: 305,780 checks pass in 5.738680 seconds including
generation and compilation. The reference retains its own prefetch state;
native execution still uses live instruction guards. SCSP, UART, memory-map
and queue implementations and the instruction-start MMIO contract are shared.
This validates the tested coupling, not independent chip, whole-game audio or
physical bus-phase timing. The diagnostic
is disabled by default, including in the Metal frontend. Daytona's measured
instruction-rate shortcut is specific to its different Model 1 board and is
not reused for Rally.

Three verified nibble-table dispatchers now contribute 48 static sound-code
candidates, distinct from twelve observed entries. The combined reached graph
is rechecked for known side entries; the separate byte-index dispatcher remains
observed-only because its full index range is not bounded to valid jump slots.
Dynamic division costs follow the pinned microcode, with 1,379,624 independent
emitted-instruction comparisons. Zero-divide exception delivery remains
unsupported and undefined overflow N/Z flags are excluded from parity claims.
The coupled sound CPU now runs through the main v2 title checkpoint at
266,518,444 main instructions, with 19,707,819 sound instructions and real UART
traffic. This does not establish host playback or complete game audio parity.

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
