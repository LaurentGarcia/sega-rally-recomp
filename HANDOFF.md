# Handoff

## Native texture reciprocal correction integrated (2026-10-14)

The optional native-scale texture path now uses the tested integer-certified
reciprocal at the two measured discrepancy sites: polygon span width and
final reciprocal depth. The shared HLSL helper handles signed normal inputs
with normal reciprocals, retaining the original division outside its exact
domain. No epsilon, texture-coordinate bias, platform shader fork or C++
renderer change is included. The default remains OFF; enhanced-scale and
default fan expressions remain unchanged. This follows **Geometry (TGP) and
rendering** and **Reference & validation**.

All 447 synthetic cases pass, comparing 85,137,408 pixels. Three fully
composed saved scenes at frames 1,634, 2,329 and 2,599 now match every CPU RGBA
byte: zero differing pixels, correcting the previous 3/1/1 residuals without
new differences. CPU/raw tile-character composition and all three default-OFF
historical image guards pass. All 188 frozen identities remain unchanged.
Evidence in
`build/worktrees/mac-triangle-fidelity/build/validation/reciprocal-gpu-001/candidate-001/report.json`,
SHA-256 `100226d212d19d03285ac03669787a385bd2884fdfed0b5cd2c73af63ec687a6`.
Independent raw-image review:
`build/validation/reciprocal-candidate-independent-review/result.json`, SHA-256
`a00dd1068af99117a72c3261d772383c87dc174fdfdae130998a9d34e18da6a5`.

A separate quiet four-arm cost run verifies twelve accepted images, then
collects 384 measurements in 48 rotated blocks, with 144 warmups: 540 total
renders and twelve upfront readbacks under a 30-second watchdog. On Apple M5,
current-ON versus candidate-ON completed-render medians are 2.230/2.309,
2.233/2.375 and 2.167/2.130 ms. Candidate-OFF/current-OFF median ratios are
0.898/0.981/0.963. Blocks vary substantially, up to 2.35 times in a control,
so these figures do not establish a precise helper cost or general speedup.
They include host encoding, submission and fence completion, excluding
presentation/readback; they are not GPU timestamps or gameplay FPS. All 202
cost inputs remain unchanged, with no competing game/build observed.
`cost/run-01/report.json` SHA-256:
`9b7ab2c5a82615917c8d1f03987b8cfc53ef80313954a27e53f97dff89317919`.
Independent statistics/image review:
`build/validation/reciprocal-cost-independent-review/result.json`, SHA-256
`bf94d9433993a78dd44ee8ffd33e56f5ad0e5b364b31816b9c5eb4abed47889e`.

The three integrated files match the tested candidate byte-for-byte, including
the generated common shader header. The prepared native-domain arithmetic
and original full-scene evidence remain frozen. No other backend device,
phone, continuous-flicker or whole-game fidelity result follows from these
saved scenes. Next build a separate Mac host and verify a representative
driving replay with both texture settings, preserving the signed v16 app.

## Authentic main-to-sound UART prefix recaptures exactly (2026-10-14)

A test-only forwarding observer now captures real main-program UART accesses
from the preserved audited native build. Its first bounded run records seven
writes and 163 sound-state cuts. The first data byte is observed as `ff` at
1,442,891,360 ns; the actual sound program reads it from SCSP MIDI at
1,443,240,416 ns, boundary cycle 16,293,607, clearing IRQ level 3 to zero.
The normal budget stop reaches 14,412,000 main instructions and board time
1,444,941,000 ns. Sound reaches 1,260,947 instructions, 16,312,806 cycles and
660 IRQ entries. No command value is invented or injected past the UART.

Replaying that immutable transcript into a separate native sound/UART instance
matches every captured architecture, cycle-statistics, RAM, IRQ, ordered data
and PCM cut. Both paths preserve UART reset/first clock token before sound
construction, physical MIDI wiring and register accesses after fully draining
the clock through their timestamp. Same-time accesses keep recorded order.
The fixture supports side-effecting reads, but this captured prefix contains
only writes. Data counters are cumulative; transfer hashes cover each interval
between cuts. Constructor-reset bus transfers remain outside the observer.
The omitted main/control/screen events do not change this measured prefix's
sound observations; that does not prove every longer projection equivalent.

This follows **Audio** and **Reference & validation**. Capture passes in
2.112635 seconds and native recapture in 0.660414 seconds, each on its first
execution with unchanged inputs and empty stderr. All 63,722 stereo frames
are silent: their interval hashes independently equal positive-zero float32
bytes. This validates authentic command transport and native recapture, not
independent sound CPU/chip behavior, nonzero music or the title-screen symptom.
Independent sound-on recording consumption remains unsupported. No production
runtime, generated body or live app changes are included.

Evidence under `build/worktrees/sound-init-reference/build/`:
`uart-native-capture-001/result.json` SHA-256
`ebe3f63303ba5eafacbba12ecd9cbdc4a4c5624f8d493e78524e001cf7a49c12`;
`uart-native-recapture-001/result.json`
`fc7fbb3bfb76225c72cfd573add35a990ec2ffbc30a35a3f7fcb63657a210022`.
The 68,885-byte transcript SHA-256 is
`215804ec9b993026eeec668b36127c27522411d58bb0d89f486c060006dbabd2`.
Independent review verifies all 926 measured inputs and six prepared artifacts:
`build/validation/sound-uart-native-independent-review/result.json`, SHA-256
`f8557cd913ece1a8939e1d9018eed2fb5e0d133a5a51f5bbb4eb651a72e4560e`.
All 111 worktree-relative source inputs map exactly to MAIN; remaining inputs
retain their original external/build identities. This is the original
worktree run, not a second MAIN execution. Integration record:
`build/validation/sound-uart-native-integration/result.json`, SHA-256
`3510a658bc2d1e134519bb8d7c5a62a3fcd95d4401784e1fbb59056fcfc33abe`.
Next compare this authentic transcript with the persistent independent sound
CPU, then extend observed traffic to nonzero playback.

The fourth v16 session closes normally at frame 6,406, with 4,911,133 stereo
frames and zero chip/host drops or positive SDL demands. Its recording and
`build/validation/mac-controller-v16/play-004/live.log` remain preserved and
have not been replayed. The reciprocal candidate's 447 synthetic cases and
three complete CPU-image comparisons pass. Its six-render scene gate had
already been submitted when the next live-play hold arrived and completed
after tool approval, possibly overlapping that session. No timing inference
is made from it. Future pending device gates must be collected or cancelled
before declaring a new live session free of background device work. Candidate
cost measurement and production integration remain pending.

## Actual sound initialization and interrupts match independent CPU (2026-10-14)

The unchanged audited sound program now matches continuous pinned MAME CPU
execution for 1,000,000 instructions and 13,096,293 cycles, ending at PC
`602f7a`, board time 1,160,029,674 ns. Startup reaches PC `6001f2`, SR `2000`,
after 639,524 instructions and 8,662,138 cycles. Both sides execute 383 IRQ
entries and 383 RTEs, with 35,955 SCSP writes and four reads. All 1,045,029
paired observations match full CPU/stack/SR/RAM state, queue order, separate
RNG state, ordered data accesses and raw float PCM. The 51,158 stereo frames
are all silent; no external UART commands are supplied. This establishes
initialization and interrupt agreement, not actual music or a title-glitch fix.

This follows **Audio** and **Reference & validation**. The test authenticates
all 2,663 immutable-ROM instruction addresses and 660 opcode words. The
test-only provider requires an explicit 660-word opt-in; its default bound
remains 64. Preflight confirms the original 31-opcode reset header is
byte-identical, rejects 660 words without opt-in and rejects the larger bound
outside provider mode. Production runtime, generated native source, emitter
instruction semantics and the playable app remain unchanged. The observer
stamps IRQ-only callbacks before clock advancement; CPU-space IACK rows are
validated separately because native acknowledgements do not use AddressSpace.
Reference prefetch remains persistent. Constructor reset bus capture and
physical per-access timing remain outside scope, as in the prior reset test.
Shared board/chip implementations and instruction-start MMIO still apply.

The first bounded execution passes in 10.286094 seconds, exit zero, empty
stderr and unchanged inputs. Sealed state counts 39,985,918 checks; stdout
includes one subsequent successful file close. There is no retry or relaxed
comparison. Evidence in
`build/worktrees/sound-init-reference/build/init-run-001/result.json` has SHA-256
`7728c60956c09403342609b8294821b66c8d2ef59242e5b6419f3ff1346899b8`;
`state.json` is
`930b8aced1d30095d02ac01d4fe9763f578c7d1ccf0cb5ce657f839a312e4f42`.
All 118 source/image inputs map exactly to MAIN and the preserved external
inputs; the ten prepared artifacts remain unchanged. This is the original
worktree execution, not a new MAIN build. Integration review:
`build/validation/sound-init-reference-integration/review.json`, SHA-256
`a481c5bc513035c89480aaa85d3ff19297a9a715156a9919acfc04163b01991e`.

Next capture authentic main UART register traffic, including side-effecting
reads, and prove native recapture before using it with the independent sound
CPU. Preserve constructor wiring and equal-time ordering; do not substitute
invented commands or prequeue every register access as a clock callback.
Independent sound-on recording consumption remains unsupported.

The user requested two more windowed launches of the preserved v16 app.
`controller-v16-002` closes normally at frame 3,861; its final log reports 135
positive SDL demand callbacks and 1,103,328 estimated extra input bytes, with
zero chip/host drops. All 64 periodic rows through frame 3,840 still report
zero demands; the first nonzero observation is the final row. The final
interval takes 3.528078 host seconds for 0.365064 board seconds, and no further
PCM push measures that terminal gap. These are estimated SDL source-demand
observations, not an audible glitch count or title-screen diagnosis. Read-only
review: `build/validation/mac-controller-v16/play-002/audio-review.json`, SHA-256
`95ea927f191e496bae21ad05656aa6a39acec58707f6bccd2a3308843eaf7711`.
The exact wait/event causing the terminal gap remains unlocalized. The subsequent
`controller-v16-003` session also closes normally: frame 6,490,
1,198,352,751 instructions, 2,820,851,951 cycles and PC `005a379c`. Its log at
`build/validation/mac-controller-v16/play-003/live.log` reports 4,975,983 stereo
frames and zero chip drops, host drops or positive SDL demands. These new
recordings have not been replayed. No heavy tests/builds ran during live play;
both recordings and the signed app remain preserved. Process absence and the
normal terminal log now release the live hold. The reviewed reciprocal
candidate's bounded synthetic and complete-scene gates are next.

## Metal span-step discrepancy directly measured (2026-10-14)

The remaining first-car pixels (294,94) and (434,94) now have direct GPU span
evidence. Owner 827 has the same edge endpoints, parameters and signed width
`4390d31a` on both paths. Its actual Metal reciprocal is `3b624288`, one ULP
above the CPU's nearest-even `3b624287`. Float32 subtraction/multiplication
reconstructs all three measured GPU steps: `37496822/bd5aed80/3aff150e`, versus
CPU `37496821/bd5aed7f/3aff150d`. The earlier inference identified only U/z;
the measurement shows all three step components differ. Starting values are
identical. The actual 121 and 261 serial additions reproduce every measured
GPU endpoint; using CPU steps reproduces the CPU endpoints. Only accumulated
U/z remains different there because the other components round to matching
final words. Fixed U changes from 73,026 to 73,025 and 11,616 to 11,615;
the corresponding filtered texels change from 147 to 148 and 160 to 159.
Final reciprocal depth is identical at these two pixels.

This follows **Geometry (TGP) and rendering** and **Reference & validation**.
One complete saved frame, including decoded tiles/characters, matches all
761,856 accepted RGBA bytes before seven isolated owner-witness renders.
The same fragment calculation supplies color and raw uint evidence; no
reciprocal replacement is included. The original clipped-start screen pixel
is occluded, so its valid start witness comes from the isolated owner passes.
All 107 frozen inputs remain unchanged. Eight renders and sixteen readbacks
complete normally in 0.688115 seconds with empty stderr; that elapsed time
only establishes watchdog completion, not shader cost. An independent raw
readback audit confirms the complete-color guard and arithmetic reconstruction.

Evidence: `build/worktrees/mac-triangle-fidelity/build/validation/reciprocal-gpu-001/actual-span-001/`.
`attribution.json` SHA-256 is
`6300e034d959bd63a1be630f070a5bfdd4d3ab0658e66e6a99a2977856931655`;
`run01/result.json` is
`252d66bde560f4de92723c686e53664609ffd72b5867e59cb006fa84c35c2812`.
Independent review: `build/validation/actual-span-independent-review/result.json`,
SHA-256 `cb7e998668d48f6c999f132f8ff12ffdd82877892c7510c5b6ab45a0d44022dd`.
Next evaluate the frozen reciprocal helper at the proven span-width and final-z
sites in an isolated common-shader candidate. Require default-off, complete
scene and synthetic guards, then measure cost before integration. No production
edit, temporal-flicker improvement, precise compiler-lowering mechanism or
hardware-parity claim follows from this attribution.

## Actual sound-program reset prefix matches independent CPU (2026-10-14)

The authenticated real sound program now matches persistent pinned MAME CPU
execution through its first SCSP write. Before that write both sides reach
278,301 instructions, 4,506,428 cycles and PC `600184`, with all 512 KiB RAM
cleared and no IRQ activity. Exactly one write to `100400`, data `0300`, mask
`ff00`, completes instruction 278,302 at 4,506,445 cycles and PC `60018a`.
Execution stops there. All 291,684 paired queue observations match CPU/stack/SR
state, full RAM, queue ordering, RNG and sample drains; 262,147 reference data
transfers are recorded, with ordered native/reference data comparisons after
reset. The 17,604 stereo frames match bit for bit. The single run completes
in 3.10043 seconds with exit zero and empty stderr. Sealed state records
11,061,054 checks; stdout includes the subsequent successful file-close check,
for 11,061,055. No retry occurs.

This follows **Audio** and **Reference & validation**. The frozen native source
is included unchanged under a test-only namespace wrapper; a bus observer
distinguishes modeled data accesses from live guards using `timing_active()`.
The reference uses 38 authenticated program addresses and 31 public opcode
handlers, within the existing 64-opcode bound. Its continuous prefetch state
is retained, and access evidence is consumed at every boundary before clearing
the bounded log. No production runtime, emitter, game instruction or app changes.

Constructor reset bus accesses are not intercepted on the native side; reset
architectural state is compared, with general registers zero by harness
agreement. Native bus stamps are instruction-boundary cycles, while reference
stamps retain microcode phases. At the first write those are 4,506,428 versus
4,506,437 cycles, both observing board time 399,166,313 ns under the declared
instruction-start MMIO contract. They are deliberately not asserted equal or
described as physical timing. SCSP/memory/queue models remain shared. No UART
commands, nonzero music, independent chip, speaker or whole-recording result
is established; the title-screen symptom remains open. The old timed boot
probe's `on_cycles` scheduler is not used as a reference for this newer board.

Evidence: `build/worktrees/sound-reset-reference/build/actual-reset-run-001/`.
`result.json` SHA-256 is
`f51b1a65d12eb99fcfcf7a4a4562f31e44b21f605263efe87ce8eb305a021ee1`;
`state.json` is
`8997393a7a148c78a73d7a862cdfed56660bce4dc600668601bf6b3f44a8ff79`.
Independent read-only review verifies all 116 source/image inputs, ten prepared
artifacts and the sealed outputs. The three integrated test files and their
mapped main-tree dependency closure match the tested bytes, recorded in
`build/validation/sound-reset-reference-integration/result.json`, SHA-256
`050223cba9087f882398d2dd86dd3885ea582a18f1e01c00e9714b4708226a0a`.
Next extend the independently executing CPU through sound initialization and
actual serial traffic before enabling a sound-on independent recording path.

## Restricted reciprocal passes actual Metal arithmetic gate (2026-10-14)

The frozen common-HLSL helper now passes the actual Metal arithmetic gate:
262,144 rows in two 512×512 RGBA32_UINT draws/readbacks. All 130,898 signed
normal-input results match the independent integer nearest-even oracle.
All 131,072 perturbed/invalid-seed cases match for both the certified path and
exact fallback. All 174 excluded inputs preserve the separately compiled
legacy fragment's exact bits, including signed zeros, subnormals, large
normals, infinities and NaNs. Every input echo and unique row tag matches.
Ordinary Metal division differs from the oracle on 13,317 sampled normals,
at most one ULP; the candidate has zero mismatches. This follows **Geometry
(TGP) and rendering** and **Reference & validation**, without epsilon changes
or a platform-specific shader fork.

The same DXC release and semantic flags as `scripts/build_shaders.py` produce
SPIR-V, DXIL and MSL. Inspection retains the domain guard, unsigned limb
arithmetic, bounded corrections and exact fallback; existing ordinary DXIL
divides still carry fast flags. Only Metal execution is measured. Pinned SDL
compiles MSL with its unchanged nil options. Two initial Docker architecture
metadata preflights failed before any container or download: inspect returned
an OCI index, not architecture metadata. Both attempts remain preserved. The
successful isolated toolchain explicitly selects linux/amd64, verifies x86_64
execution, and records image, archive, package and binary identities. Only
the ignored fixture directory is mounted in its retained idle container.

The one approved run exits normally with empty stderr in 0.450522 seconds,
well inside its 30-second watchdog. This is a correctness-run elapsed time,
not a shader cost or gameplay measurement. All 37 frozen inputs remain
unchanged. An independent read-only audit recomputes every raw result with a
generic rational-to-binary32 encoder and verifies the same counts and hashes.
No game, scene replay, audio device, app rebuild or production edit occurs.

Evidence is under
`build/worktrees/mac-triangle-fidelity/build/validation/reciprocal-gpu-001/`.
`run01/result.json` SHA-256 is
`250e6fe4c9aded9cf8febe4d70c1dba4cb916741cc49c985b8385a7dfc29083d`;
`run01/render/result.json` is
`9e3a2f724dc8c68cea3cf9291a5b17bddbbd9741e9c6b2e6d1bc4510de3d19fa`.
The separate review is
`build/validation/reciprocal-gpu-independent-review/result.json`, SHA-256
`60f86c7112e9f3d9229f9482ac3bb80349357abc4346dea20dea5eb2b2279f22`.
Next measure actual GPU span steps at the remaining first-car residuals
(294,94) and (434,94), then evaluate an isolated common-shader scene candidate
with default-off/image guards and cost. The arithmetic pass alone does not
establish complete-scene, continuous-flicker, all-backend or hardware fidelity.

## Continuous sound CPU/board reference fixture passes (2026-10-14)

The test-only provider now runs pinned public MAME M68000 microcode with its
own persistent registers, flags, prefetch, reset, IRQ and cycle state against
a freshly production-emitted synthetic program. Each side has separate RAM,
SCSP, UART, queue and RNG instances. Two 2 ms cases reverse the insertion
order of a UART send and the first CPU deadline. Each executes 3,158
instructions, 22,586 cycles, two IRQ-only entries and RTEs, and 2,000 UART
clock edges. All 5,265 sampled boundaries per case match CPU state, complete
512 KiB RAM, queue order, UART snapshots/pin timestamps and sample drains.
The actual serial bytes `a5` and `5a` echo back; each case produces 89 stereo
frames, including 87 nonzero. The final fixture passes 305,780 checks in
5.738680 seconds including generation and compilation.

This follows **Audio** and **Reference & validation**. The reference executes
no native CPU instruction/reset/IRQ helper. Device implementations and the
instruction-start MMIO contract remain shared; recorded source cycle phases
do not move the board clock inside an instruction. The authenticated immutable
synthetic ROM profile rejects unsupported execution. This is not full MAME
board scheduling, independent chip, actual sound-program, speaker or hardware
parity. The production runtime and independent recording consumer are unchanged;
the latter still rejects sound-enabled input. Next assess a bounded actual
sound-program reset/SCSP checkpoint before extending to recordings. The
reported title-screen glitch remains unresolved.

The first attempt failed before CPU execution: zero-filled unsupported vectors
seeded address zero and accidental RAM-code reachability. Only the synthetic
image was repaired, marking those vectors unsupported and initializing D1
explicitly. The first coupled pass is preserved. Review then corrected the
reference IACK evidence field from zero to the actual autovector 25; CPU
semantics were already correct. The final fixture asserts the returned vector
and mask. The ordinary default matrix subsequently passes 11,551 instruction
cases, 280 IRQ entries and two resets with zero errors in 20.880636 seconds.
Both generated headers remain byte-identical to the accepted fresh matrix
(`53bf9dfb...` reference and `9a90675b...` native).

Evidence: `build/worktrees/sound-board-reference/build/sound-reference-evidence/`.
`final-001/result.json` SHA-256 is
`149546365b3fff683c20de7e1408823feee42bf23818edb6cbfafbba6e2707a2`;
`default-001/result.json` is
`bc10ea14bb974bb31a624cdc94011f46860033c6986db797a4044b70da77d7a7`.
The four integrated test files and all 102 recorded input files match both
tested source closures. Integration report:
`build/validation/sound-board-reference-integration/result.json`, SHA-256
`45b36add2fccbcd4d4fbd5266f8f5f8b161f0383887b5aee6969973da10ad57f`.
No generated code, game assets, captures or playtest-app changes are committed.

## Complete v16 manual session replays exactly (2026-10-14)

The original signed v16 executable now replays the completed controller session
offline. All 25,953 resumes match their input pins, before/after witnesses,
return/fault fields and PCM counts/raw-float hashes through frame 12,974.
Terminal state is 2,381,700,896 instructions, 5,638,541,403 cycles and
PC `005a37a4`; PCM hashes covering all 9,946,388 stereo frames and final
EEPROM/backup bytes match. Replay finishes normally in 109.633421 seconds,
under the 300-second watchdog, with empty stderr. It initializes no
SDL/controller/GPU/audio device and writes no live save. App, all eight actual
assets, complete recording,
live saves and log remain byte-identical before/after.

This follows **Reference & validation** and closes same-Machine reproducibility
for this actual analog-input session. It does not establish independent CPU,
chip, board, complete-course, hardware or audible-quality parity. The recorded
pins still do not identify which physical input opened Settings. Evidence:
`build/validation/mac-controller-v16/manual-replay-001/result.json`, SHA-256
`0ded284d70289ad3d925bc433f36ce2b906adc8259f586baa765ad792e4d381f`.
The exact signed executable and immutable bundle were used without identity
bypass or disabling sound. Independent sound-board work remains separate.

## Sound CPU comparison includes architectural state (2026-10-14)

The existing pinned-public-microcode differential now also compares D/A
registers, active/inactive stack banks, architectural PC, defined SR bits and
the complete synthetic 16 MiB memory image, retaining all previous timing and
ordered-write checks. The freshly generated current fixture passes 11,551
instruction cases, 280 IRQ entries and two resets with zero errors. Generation,
compilation and execution take 21.185635 seconds on this Mac. This follows
**Audio** and **Reference & validation**; no game ROM or runtime code changed.

The first attempt reused an obsolete native fixture and failed 1,011 cases.
The unchanged old test against those same inputs reproduced every failure
count/opcode. Read-only provenance then showed the old fixture still used
high-word-first ORI.L writes and obsolete fixed bit-operation timings; retained
v3 fixtures and the current production emitter already contain the fixes.
Fresh generation used the unmodified current emitter and pinned reference
sources in a new directory. The independent microcode header remains exactly
`53bf9dfb...`; native fixture SHA-256 changes from `1ea26c9d...` to
`9a90675b3093e7f6a67879e18a44b6d2ed14ec3165540c51d937c991a9d75188`.
No generated file was hand-edited and no comparison was weakened. Keep both
failed runs: the lesson is to regenerate fixtures with their current emitter,
not to treat a convenient old header as the current baseline. A prepared
aggregate diagnostic was unnecessary after the fresh matrix passed and was
not compiled or run.

Explicit limits remain: 16 zero-divide frontiers, 16 prefetch frontiers,
169 odd-address exceptions, 456 unsupported timing and 16 unemitted cases.
One DIV overflow case excludes undefined N/Z; reset excludes undefined CCR
and general registers. Instruction cases start supervisor with X=0; IRQ cases
start supervisor. There is no value-bearing prefetch, exact bus phase, SCSP,
coupled-board or hardware claim. Next build the separate independent sound
board comparison before attributing the title-screen symptom to CPU timing.

Evidence: `build/worktrees/m68k-state-reference/build/state-reference-evidence/`,
with `fresh-001/result.json`, SHA-256
`324b344400c4eb2967c84b064656512500097fa89df5e33fcab5d0bce0d7126b`,
and `review-summary.json`, SHA-256
`3b67500335adbb80cfb67006eb49c617e96e5087835bbe1b7d5867c8e0f72773`.
The integrated C++ fixture matches the tested bytes. The Python wrapper's
documentation/audit scope is updated; generation and execution logic remain
unchanged, verified by AST comparison after excluding those two string changes.
All ten shared generator/runtime source files match the tested worktree;
`build/validation/m68k-state-integration/result.json` records the integration.
Playtest apps and all old evidence remain preserved.

## Restricted reciprocal prototype passes CPU arithmetic check (2026-10-14)

An ignored common-HLSL prototype certifies the nearest-even reciprocal for
signed normal inputs whose reciprocal is also normal. Its exact guard is
`00800000 <= absbits <= 7e800000`. Powers of two are packed directly; other
inputs use one exact 24-by-24-bit product, at most two neighboring candidate
corrections and a cold 24-step integer fallback. Correctness does not assume
the hardware seed's accuracy. Zero, subnormal, overflowing, infinite and NaN
cases keep the original source operation; backend bit equality is unproven.
This follows **Geometry (TGP) and rendering**: one shader path, no epsilon or
per-platform fork, preserving signed span widths and operation ordering.

A mechanical CPU translation passes all 8,388,608 normalized mantissas, both
signs, exponent boundaries, perturbed/invalid seeds and exact product bounds:
260,232,785 checks, including 204 excluded-input forwarding/CPU comparisons.
Strict compilation and unchanged-source checks pass. Filesystem timestamps
bound the whole compile/test interval at approximately 1.53 seconds; there is
no GPU or game-performance inference. HLSL bodies are mechanically retained,
with loop attributes removed and two division literals explicitly float.

Evidence is under
`build/worktrees/mac-triangle-fidelity/build/validation/reciprocal-source-audit/`:
`audit.json`, SHA-256
`8823e29943d31b76dacb1013ecb07f88556a79d2e39b71df8dd9a5403da5bbc9`,
and `cpu-oracle-001/run01/result.json`, SHA-256
`f8491c511aa250e8d57a9e923982e0ebfcd7775ada5200bb00e588f3125335a6`.
No production shader changed. Next compile the isolated helper through the
shared shader toolchain, check actual GPU arithmetic and excluded-class guards,
then directly attribute the remaining span-step differences before integration.
CPU success alone does not establish shader semantics, cost or pixel fidelity.

## Longer neutral sound capture completes (2026-10-14)

After the controller window closed, the prepared fresh neutral attract check
ran alone through 7,200 frames in 126.378242 seconds including process startup
and exit. It reaches 1,399,415,787 instructions, 3,129,161,000 cycles and
PC `005ca464`, with a normal frame-limit exit. Both neutral events apply at
frames 0 and 7,199, all twelve cabinet pins remain neutral and no manual
handoff occurs. The sound-enabled recording seals 14,404 ordered resumes.

The finite float32 stereo 44.1 kHz WAV contains 5,519,841 frames. Every drain's
sample count and SHA-256 matches its recording observation; the first 2,400
resumes and 7,360,272 PCM bytes exactly match the previous paced capture before
its first coin at frame 1,200. Distinct recording/executable identities remain
intact. All 121 telemetry rows show zero positive SDL demands or host drops,
and the native log reports zero chip drops. Interval maxima are 23.750417 ms
producer gap, 7.252333 ms Machine execution, 2.693875 ms presentation wall time
and 42.259833 ms pacing lateness. These are host observations, not GPU timings
or exact speaker-sample accounting. Frozen inputs are unchanged.

The terminal image shows the tree-lined course with CREDIT 0/2 during attract.
There are no screenshots labeling earlier title intervals and no verified
listening assessment. This run therefore does not resolve the reported audible
title glitch; do not change buffering or chip behavior from its clean counters.
No retry or concurrent build/device test occurred. Next investigate independent
sound CPU/board state, and use a reported audible interval to localize any
further output experiment. The v16 playtest app remains unchanged.

Evidence: `build/validation/audio-diagnostics-v15/neutral-title-001/result.json`,
SHA-256 `be731ef578c01e9e57d544454dfac755921a8cd100046b08e33031785f6e1d34`;
separate artifact audit `neutral-review-001/result.json`, SHA-256
`40e161c41a3544e8d2c4b7aa7a4b00f509ef8537de5c074e9db26529cf975f7d`.
The original terminal PPM, its PNG review conversion, full WAV, seeds and
ordered recording are retained under ignored build output.

## Completed v16 controller playtest (2026-10-14)

The windowed sound/CRT session exits normally at frame 12,974 after
2,381,700,896 instructions and 5,638,541,403 cycles, PC `005a37a4`.
The sealed recording contains 25,953 linked resumes and observations. Actual
signed executable, assets-report identity, startup/final saves, row witnesses,
terminal seal and preserved live prefix all validate. PCM counts total
9,946,388 stereo frames, matching the host log; this session did not save raw
PCM, so its per-row PCM hashes have not been independently recomputed. All 217
telemetry rows report zero positive SDL demands or host drops; final native
audio also reports zero chip drops. The two final queue counts were sampled
at different times while playback drained and are not an inconsistency.

The frozen prefix through frame 11,847 includes 2,726 cabinet-input changes
after scripted handoff: steering has 250 distinct values, throttle 226 and
brake 132. Intermediate values establish analog-path activity, not the physical
input source or whether Y opened Settings. No persisted `controls.settings`
was present in a separate read; this is not proof of the complete historical
in-memory settings. The completed bundle has not been replayed. Title-screen
audio quality remains unresolved despite clean counters in this session.

Evidence: `build/validation/mac-controller-v16/live-final-001/result.json`,
SHA-256 `c786db6836999a081a4f5b82d84b5bd747d079de3bfb598c5d9da0a9f41a900e`;
the earlier `live-prefix-001` and original preview artifacts remain unchanged.
No build, device probe or rendering test ran during the live playtest. The app
and complete recording remain preserved. Design and replay documentation now
also reflect the completed 500-million-instruction independent driving check.

## Optional controller input and Settings menu (2026-10-14)

The dedicated Mac host now supports recognized SDL gamepads with hotplug,
analog steering/triggers and configurable keyboard/button/half-axis bindings.
The input adapter follows Daytona's half-axis model but retains Rally's exact
cabinet wiring, full keyboard/touch ranges and neutral-to-fourth shifter. Xbox
defaults use left stick, RT/LT, RB/LB, A view, X handbrake, Back coin and Start.
Y opens Settings; F1/F6/Escape and Y remain reserved. The Controls tab provides
enable/disable, separate dead zones, steering response/inversion, live meters,
per-binding capture/clear and defaults. Bounded `controls.settings` is separate
from cabinet/display saves. This follows **Inputs** and **Platform layer & build**.

Settings uses the existing lazy ImGui/Metal panel, with Display/Controls tabs
and a fixed Resume footer. Only the selected controller supplies navigation.
Capture input is isolated from UI activation; Escape cancels binding before
host quit. Pause, focus/background and scripted-input handoff suppress held
keys/buttons/axes until release or neutral. Focus loss releases actions while
retaining gear. No guest clocks, sound synthesis, shader or generated-code
changes are part of this input feature. The Mac launcher also forwards the
existing opt-in audio diagnostics for the next listening session.

The actual external SDL device probe recognizes Xbox One Elite 2 Controller,
type xboxone, 19 buttons and six axes. The sandbox probe returned no devices;
the external read establishes detection, not gameplay or physical navigation.
The ROM-free mapping/preferences/virtual-gamepad fixture passes 329 checks,
including ASan/UBSan with uninstrumented SDL and leak detection disabled.
The normal sound/hook/recording v16 build and all three platform CTests pass;
launcher regressions pass 12 plus four checks. All ten integrated source files
match `build/validation/mac-controller-v16/integration/sources-v2.json`.

The actual-host call-substitution fixture passes 16 iterations, 12 guest
resumes and three paused iterations, covering held Return/shift/steer/gas,
focus loss/regain and fresh-press gear advancement. It does not inject global
OS inputs. The Settings UI passes eight Metal draws/readbacks at 640x480 and
992x768, both with a reachable Resume footer and zero closed backend calls.
Root also reviewed both Controls images. Physical controller driving remains
the next user test. The v16 build is separate from preserved older apps.

Evidence: `build/validation/mac-controller-v16/`; input source/test records in
`build/worktrees/mac-controller-core/build/controls-evidence/`; UI report in
`build/worktrees/mac-settings-controls/build/settings-evidence/report.json`.
Preparation findings: inactive mapping initially returned neutral gear, fixed
and regression-tested before integration; equal keyboard/controller labels
could share an ImGui ID, fixed with distinct scopes before the visual run.
Disabled/disconnected controllers cannot begin an unusable binding capture.
The previous F2/F3 manual/help labels were reversed relative to the retained
mapping; text now says F2 Test, F3 Service. Title-screen audio diagnosis remains
open, and its longer neutral capture is still prepared but unrun.

The final v16 representative 2,600-frame sound/hook/CRT replay passes: native
WAV/frame, saves, all 15 applied input boundaries and complete ordered
resume/observation payloads equal the preserved v15 comparison. All ten source
files, 87 static link inputs and 203 generation files are bound in
`build/validation/mac-controller-v16/final-result.json`, SHA-256
`9d4b2652f7c73b2944c1a1365530f6dd532cc8bb18ca08ab5e489fffaeed148b`.
The raw binary SHA-256 is
`0fe2f5aa3571ebe48ed0af2b38266414ee0d64310ee4534877256cda649add73`.

The separate v16 app has now launched windowed with sound, CRT, opt-in audio
telemetry and Lancia selection, recording to
`build/recordings/controller-v16-001`. The ongoing snapshot reaches frame 1,080;
it is not a completed performance or controller assessment. Startup confirms
Metal/playback/CRT and initially reports no controller before asynchronous
discovery; physical Y/navigation feedback remains requested, not assumed.
`preview-live-result.json` binds the signed app and retained log snapshot.
All test/build lanes are idle during the user's playtest. Do not repackage,
re-sign, rebuild or interfere with this running app or its recording.

## Independent recorded drive through frame 2,600 (2026-10-14)

The prepared sound-off checkpoint now passes an actual captured drive. A fresh
v14 raw-host capture completes 5,201 resumes and 500,035,460 instructions through
frame 2,600, with 1,130,001,001 cycles and PC `005e6964`. All 15 applied script
events match every recorded pin snapshot; four later events remain pending.
The final image shows the Lancia driving at 160 km/h in third gear, with 4.50 s
on the race timer. This confirms the captured state, not image fidelity.

Independent replay then matches all resumes, 15,156,607 ordered writes and
terminal witnesses in 450.991573 seconds. Bundle bytes and all measured inputs
remain unchanged. Scope is independent i960 execution with shared board/TGP/
video implementations and sound off; full-machine, sound and hardware parity
are still open. Evidence: `build/validation/independent-drive-v14/`, with
`capture-001/result.json`, `capture-001/visual-review.json` and
`replay-001/result.json`. The original PPM is preserved; a PNG conversion was
needed because the review viewer could not decode PPM.

Both commands requested `nice -n 10`, but the replay's stderr records
`setpriority: Operation not permitted`. The program still ran and passed;
reduced scheduling priority was not established. Do not describe this as a
quiet performance benchmark or repeat the completed parity run solely for that
priority request. The long run is complete and no replay process remains.

## Opt-in audio timing diagnostics (2026-10-14)

`--audio-diagnostics` now observes paced `--sound` playback without changing
guest clocks, PCM, queue policy or rendering. It reports once per 60 frames,
on new positive SDL demand and at exit: queue depth, cumulative counters,
callback time and interval maxima for producer gaps, Machine execution,
presentation wall time and pacing lateness. Timestamp work in the callback is
opt-in; no logging, allocation or sample generation runs there. Separately
sampled atomics are observations, not an exact output-sample ledger. Invalid
no-sound/unthrottled combinations and offline replay flags reject before SDL.
This follows **Audio**, **Reference & validation** and **Platform layer & build**.

The actual SDL dummy-stream fixture passes timestamp-enabled/disabled demand,
priming, bounds, clear/restart and exact float WAV checks. The integrated three
source files exactly match those tested; current MAIN host headers also pass a
strict syntax compile. One paced visible 2,600-frame sound/hook/CRT replay
preserves the earlier full WAV, native frame, startup/final saves, all 15 applied
input boundaries and 5,201 resume/observation rows plus terminal footer.
It records 44 telemetry rows, no estimated SDL shortfalls and no chip/host drops.
Queue observations range from 1,876 to 8,715 frames and end at 2,280. Startup
records maximum producer gap 118.55 ms, presentation wall time 114.82 ms and
pacing lateness 158.83 ms; these are host observations, not GPU execution time.
The maximum measured Machine step is 7.565 ms.

The title-screen glitch remains open. This replay inserts coins at frame 1,200
and starts at 1,240, so it does not cover a complete recurring neutral title
sequence. A longer neutral attract capture is prepared but has not run.
The previous user session's four demands remain unlocalized. Do not alter
buffering or sound-chip semantics from these counters alone.

Evidence: `build/validation/audio-diagnostics-v15/`, including `frozen.json`,
`main-integration/result.json` and `review.json` (SHA-256
`0717a1c0211ce22ae589a53965c5c83140ccd8004fa8b205ab20e0dabcd4aded`).
The diagnostic binary reuses authenticated frozen v15 objects and libraries;
old apps remain unchanged. An initial preparation mixed newer recording headers
with old objects; dependency verification rejected it before execution. Rebuilding
the new host translation unit against the complete frozen header closure fixed
that preparation error. Only the successful binary was run.

New requested work: optional Xbox controller input and a configurable Controls
menu, reusing Daytona's input components while retaining Rally's cabinet pin
layout. This proceeds separately from the audio investigation. Mac remains the
active target, and the existing motion/board timing remains unchanged.

## Display input isolation and live sound feedback (2026-10-14)

The final v15 host passes a bounded input-isolation regression using only
validation-call substitutions for SDL polling and keyboard state. Twelve
scripted host iterations include three paused iterations and exactly eight
guest resumes. Return/S/right/Z held across F1 panel close stay neutral until
release; a new press then supplies Start, gear 1, steering and gas. All 197
measured inputs and the final host source remain unchanged. This tests the
actual close transition through F1, not ImGui Resume-button activation. Evidence:
`build/worktrees/mac-triangle-fidelity/build/validation/host-keygate/run-01/result.json`,
SHA-256 `12b0b71d2b2d493353abdbc439b4f888e9e48c69c0efc2bdd0dd1bc15003241e`.
The initial validation shim used a variable name conflicting with `log()`;
renaming only that shim variable fixed compilation. The failed log is retained.

The user then closed the window normally after 30,328 frames and 5,738,008,246
instructions. Feedback is positive for the image and near-perfect-feeling
motion, with audible glitches on the title/cover screen. The completed log
contains 23,250,561 stereo frames, zero chip/host drops, four SDL estimated
shortfall requests totaling 25,248 input bytes, and 1,658 queued frames at exit.
These estimates are not exact missing device samples and have no frame/time
localization. They support checking playback interruption, not declaring its
cause or ruling out generated-audio errors. Final log SHA-256:
`47aeed451474ef661d4b33fb1fbd797703cbc928eb763b1309e0246cc66a2474`, retained with
`build/validation/mac-display-v15/preview-final-result.json`.

Next add opt-in queue/timing diagnostics and reproduce a quiet paced title
sequence before changing audio buffering or board semantics. Long independent
sound-off drive preparation and residual texture arithmetic analysis continue
separately; heavy execution waits for that quiet audio check. The v15 app and
its saves stay preserved. Mac remains the active target.

## Longer sound-off recording checkpoint prepared (2026-10-14; not run)

An explicit `--long-checkpoint` opt-in permits at most 600 million instructions
and 2,400 seconds; existing 20-million/90-second defaults and the 180-second
short ceiling stay unchanged. Optional matched-instruction progress is flushed
without guest synchronization changes, and first mismatches report counters/PCs.
All four focused CTests pass: 23 bundle, ten limit/binding, 165 existing bundle
and 249 phase checks. All six integrated source files match the frozen tested
files exactly. No long sound-off recording exists yet: current sound-off
bundles cover only two boot frames, and sound-on bundles remain untouched.

A measured consumer is prepared from existing audited objects for a fresh v14
raw-host Lancia script through frame 2,600, with fresh saves and exclusive output.
Future capture/replay commands use `nice -n 10`, 600/2,400-second hard timeouts,
immutable identity checks and first-failure stop. Five-million-instruction
progress is enabled. The roughly 18-minute estimate for 500 million instructions
comes only from boot; driving/TGP cost and interactive contention may be higher.
Evidence and exact commands: `build/validation/independent-drive-v14/`.
Capture, final driving-image inspection and long independent replay remain
pending; execution is deferred for the active v15 playtest/audio diagnosis.

## Short independent whole-recording validation (2026-10-14)

A separate test-only sound-off consumer now reuses canonical immutable bundle
preflight and the raw harness's two independent board instances. Actual host,
assets/options/startup seeds, measured source/object/link inputs and all 75,207
generated instruction addresses are checked before CPU execution. Each opcode
compares architectural/cache/FP state, cycle-stamped writes, IRQs and device
state without adding an end synchronization. All four focused CTests pass:
22 complete-bundle checks, seven build-binding tests, 165 existing bundle checks
and 249 phase checks. Independent read-only review found no blocker. MAIN integration preserves all
ten non-CMake source identities; its normal Debug build and all four focused
CTests pass (21.48 seconds). The root CMake file also retains the new CRT target.

One fresh v14 raw-host sound-off capture then passes all six resumes through
two frames: 563,041 instructions, 283,375 ordered writes, 910,200 cycles and
PC `0000053c`. The one-shot independent run takes 1.222 seconds; the complete
bundle and measured inputs remain unchanged. Evidence:
`build/validation/independent-recording-v14/actual-result.json`, SHA-256
`d83412b50d2e9d880fe7c031aa5c673100a55b9be7ee91f3a2dd616ef810b681`.
This is independent i960 execution with shared board/TGP/video implementations;
TGP generation evidence retains its `identity_only` limit. Longer/manual
sound-off captures remain unvalidated. Sound-on and captured fault rows reject
intact; production exact-executable replay and raw parity defaults are unchanged.

## Optional CRT controls and windowed v15 preview (2026-10-14)

The dedicated host now has a lazy F1 Display panel and F6 CRT toggle. Common
HLSL supplies adjustable native-row scanlines, output-pixel RGB mask, spatial
glow, curvature and color fringing after native composition and before UI.
Smooth/Sharp scaling, integer scale and the previously verified Accurate
textures option are separate controls. Fresh defaults retain the original
linear blit with CRT and Accurate textures off. Preferences use bounded
`display.settings`, separate from cabinet saves. This follows **Geometry**,
**Inputs** and **Platform layer & build**: one shader source, native timing,
optional enhancements, and reuse of Daytona's pinned MIT ImGui backend.

The panel pauses guest execution and replay application, clears audio backlog
at transitions, and resets host pacing on resume. A review found that Return
held while activating Resume could also become guest Start. Cabinet key
mapping now suppresses keys held on panel close until their release; global
F1/F6 navigation remains available. The subsequent bounded held-key
regression passes, as recorded above. Native captures precede display effects,
and offline recording replay still returns before SDL/preferences startup.

Metal checks pass 18 synthetic draws plus two saved Lancia scene previews;
preferences pass 250 ASan/UBSan checks; the final small-window UI smoke fits
Resume at 640×480 and proves closed frames do not call either backend. The
normal sound/hook v15 build and both platform CTests pass; existing launcher
and recording-launcher regressions pass 10+4 tests. Two hidden 2,600-frame
sound-enabled host runs produce byte-identical native images, PCM, ordered
resumes, observations and initial/final cabinet saves with CRT off/on. That
comparison precedes the final held-key guard; its exact executable is retained.
Hidden rendering alone is not evidence that a visible swapchain pass ran.

The final separate v15 app was signed and opened windowed on the user's
request, with sound, CRT and Lancia selection. Its log confirms Metal on Apple
M5, CRT swapchain initialization and manual takeover at frame 1,851. The saved
ongoing-session snapshot passes frame 7,140 without a frontier; it is not a
completed audio-quality or sustained-performance verdict. Old apps, recordings
and generated inputs remain unchanged. See `build/validation/mac-display-v15/`
for source identities, build logs, `gameplay-001/result.json` and
`preview-live-result.json`; the app is under
`build/worktrees/mac-display-v15/build/mac-v15/Sega Rally Native.app`.

Failures retained: the first UI fixture omitted ImGui's layout-only first
frame; a later layout clipped Resume until constrained to the window area.
The initial MAIN configure could not stage generation from another checkout's
relative audit paths. A new worktree uses all 203 hash-verified generation
files at their original relative paths; audit checks were not relaxed.
Copying a source file with an older preserved timestamp skipped one rebuild;
explicitly touching that source forced the corrected panel object to compile.
Next measure sustained live effect cost and remaining native pixel differences;
monitor authenticity, complete MAME race parity and all-course coverage remain
open. The iPhone target stays deferred.

## Native Metal repeated stepping integrated (2026-10-14)

The common renderer now includes clean repeated span stepping behind the
existing default-OFF polygon scanline option. It retains clipped-left and
checker alignment, and repeats only at native scale 1; higher scales retain
their existing direct interpolation. Diagnostic modes, primitive IDs and
probe switches are absent. The common HLSL is regenerated for all three
backends; only Metal execution has been tested. This follows **Geometry**:
preserve polygon ordering/composition and verify pixels before changing defaults.

Fresh clean-shader checks pass 447 synthetic cases (85,137,408 pixel comparisons)
and all three saved complete scenes. Their CPU/composition witnesses match,
actual default-OFF images equal both historical stable and current OFF images,
and ON equals the accepted diagnostic images exactly: 3/1/1 differing pixels,
RGB error sums 24/13/11. Independent source review found no blocker; all 216
preserved diagnostic/input identities remain unchanged. Clean source and final
image identities were checked again after integration.

Evidence: `build/worktrees/mac-triangle-fidelity/build/validation/clean-repeat-candidate/report.json`,
SHA-256 `07c72b87af9ce1a57abc6ee234224fa4eb05559b5094eb34934c86baf8ec2285`;
source review: `build/validation/full-scene-repeat-independent/clean-source-review.json`,
SHA-256 `49941e4fe90350694a7f865bfb458e425487fc793f72e7bd2436e20278d4fe6f`.
Previous diagnostic timings do not measure this newly compiled candidate.
Next integrate into a separate Mac app and measure paced gameplay with sound;
continuous flicker, remaining precision residuals and whole-game fidelity stay open.

## Independent ordered-resume phase component (2026-10-14)

`tools/srally_boot/reference_resume.h` adds a test-only sound-off interpreter
stepper for ordered host resumes. It applies all twelve input bytes once per
public call, preserves requested budgets and before/after witnesses, returns
Frame before an exact Budget tie, and retains queued IRQ edges until the next
preparation. An IRQ may complete a frame without executing another opcode.
Initial EEPROM/backup sizes are both checked before either board is changed.
The production Machine, raw reference harness and interpreter body are unchanged.
This implements the inherited **Inputs** and **Reference & validation** contracts.

A measured long absolute LDA leaves board time at 0 ns in Machine but 40 ns
in the unmodified interpreter, despite identical N=1/cycle=1 and CPU state:
the interpreter fetches its displacement after charging. The adapter validates
and caches only proven decoder words at the original boundary phase. Relocated
RAM live-word guards retain byte checks, access order and failures before
charging; genuine data accesses/flags/writes retain their original bus clocks.
ROM prefetch here is oracle byte validation, not a native/hardware fetch claim.
Unknown lengths, executable ranges, live-word changes and fetch faults reject;
an exception poisons the stepper. Captured DeviceAccess/RuntimeError rows are
unsupported because operand decoding changes the interpreter fault IP phase.
No phase is normalized silently. Coupled sound remains rejected intact.

The registered ROM-free fixture passes 249 checks in both hook modes, including
IRQ/cache state, ordered cycle-stamped IRQ writes, unchanged-count frame returns,
changed next-resume pins, real data clocks and guard/data faults. Existing
transcript and Machine fixtures pass 98 and 84 checks: all three focused CTests
pass after a normal CMake build (9.4 seconds, parallelism two). New-source strict
syntax/warnings and independent read-only review pass. The frozen integrated
files exactly match those tested in the isolated worktree.

Early synthetic probes guarded ROM code too; the formal fixture corrects that
scope to native embedded ROM words versus relocated RAM guards and still
reproduces the measured phase discrepancy. A data-fault fixture initially used
0x10000000, which Machine video legitimately maps; 0x20000000 supplies the
actual unmapped case. Whole-fixture warnings-as-errors encountered inherited
reference/geometry warnings; only the new translation unit/header are claimed
strict-clean. Original failed probes are preserved as ignored evidence.

Evidence: `build/validation/ordered-resume-reference/result.json`, SHA-256
`28ec789b02df84608b4aa400fa372376b59684fc43b247a78a1485faa89b48fc`.
That initial checkpoint supplied the low-level phase component; the later
whole-recording integration above adds actual byte/profile/board binding.
Capability booleans alone remain insufficient provenance proof. No sound-on
recording has been converted, accepted or independently validated.

## Repeated stepping across complete scenes (2026-10-14)

The diagnostic now passes three historical complete scenes, including HUD/tile
composition. A new ignored helper reuses authenticated mac-v5/timed-v6 objects
with two resets: neutral to frame 1,634 and the driving replay through 2,329
and 2,599. All frame/instruction/cycle/PC/polygon-count witnesses and original
CPU/stable PPMs match. Exporting actual tile RAM (64 KiB), character RAM
(512 KiB), geometry and 3D regions leaves the machine witness unchanged.
Fresh Video instances restored from these bytes reproduce the CPU image,
decoded layers/flags, registers/pens and stable GPU image. Current default OFF
equals stable RGBA; diagnostic mode 0 equals the current optional renderer.
No scene was skipped or invalidated. This follows **Geometry** and **Reference
& validation**: verify scene/composition inputs before attributing differences.

| Complete scene | Current optional differing pixels | Combined repeat | RGB error sum, current to repeat | Maximum channel error, current to repeat |
| --- | ---: | ---: | ---: | ---: |
| 1,634 | 697 | 3 | 9,568 to 24 | 168 to 4 |
| 2,329 | 1,241 | 1 | 15,147 to 13 | 8 to 5 |
| 2,599 | 581 | 1 | 7,015 to 11 | 52 to 4 |

Across three 496x384 images, differences fall from 2,519 pixels to five.
2,517 pixels become exact; three previously exact pixels worsen, one per scene.
The first-car residuals are attributed in older entries below. Driving residuals
at (449,281) in frame 2,329 and (479,369) in frame 2,599 remain unattributed:
CPU/current RGB (204,188,153) becomes (209,192,157), and (162,149,122) becomes
(166,153,125). This is historical scene fidelity, not current-v14 gameplay,
continuous flicker or whole-game/hardware parity. Production, original captures
and the accepted diagnostic shader remain unchanged. Its earlier 447-case
synthetic proof applies to the same shader bytes.

A single bounded offscreen timing run on Apple M5 (10 GPU cores) then passed
12 image guards and completed 144 warmup plus 384 measured draws, with 32
samples per arm/scene. Default OFF/current optional/diagnostic direct/repeat
completed-render medians in milliseconds are 1.168/1.491/1.408/2.119 at 1,634,
1.319/1.541/2.257/2.126 at 2,329, and 1.913/2.585/2.258/2.527 at 2,599.
The first-car repeated path costs about 51% more than diagnostic direct in
this run, with higher latency in every block; driving control variability
prevents a universal incremental-cost or speedup claim. Timing includes CPU
encoding, submission and completion wait, excludes readback and presentation,
and serializes draws with fences. These are static-scene host elapsed times,
not pure GPU time or gameplay FPS. Ordinary desktop activity remained during
the coordinated build/game pause; no retry was performed.

Evidence under `build/worktrees/mac-triangle-fidelity/build/validation/full-scene-repeat/`:
`report.json`, SHA-256
`84c65bf1e12ddf4a5a434bf6841215b1b8fad548710fe9339fc155af674394cb`;
`neutral.log`, SHA-256
`05784ddeff940a30f6b344698fc02a53c9e3de31d37c72c73ea7182988e4905e`;
`race.log`, SHA-256
`40f4e825538f1afad88ff7e80c6f47a6d183a44038b9e7c2af6954dfd877addd`.
Independent audit recomputes all four variant metrics per scene, guards and
raw tile/character decoding, flags, registers and pens:
`build/validation/full-scene-repeat-independent/review.json`, SHA-256
`45362d44bea216a5ac673a2140d9459a2d04530312e1552992b189463e5c663b`.
Full saved scenes now support replay without executing game instructions.
Timing evidence is under the same directory at `timing/run-01/report.json`,
SHA-256 `ccc17cdd940b77a2b81badc5f18885d086b784883ec95445f950f37ea58bc1d5`;
`samples.csv`, SHA-256
`7ebcac2ea23367c68bcf51273badc087c17c3e97f0c68d9abfc3e15e95aa33a1`.
Independent timing audit recomputes all groups and verifies 138 hash bindings:
`build/validation/full-scene-repeat-independent/timing-review.json`, SHA-256
`4be56403bb5f963e30463f014e6f8cc71d5607b41f85948c9cdd9c6188ca7bf9`.
Next is a clean candidate without diagnostic instrumentation, retaining the
default-OFF guard and common shader. Its compiled output needs the synthetic
and saved complete-scene checks before application integration and live pacing.

Polygon scanline rasterization is separate from visible CRT scanlines. CRT
postprocessing and chromatic aberration are not implemented in the Rally host.
Any future display effects belong after native image composition, remain
optional/off by default, and are disabled for image parity checks, following
the inherited **Geometry** enhancement policy. No CRT feature was added here.

## Second race starts and drives with sound (2026-10-14)

`config/segarally_second_race.inputs` preserves the first 26 credit-restart
events through frame 10,770. It removes that script's terminal neutral event
and shifts the known selection sequence by the Start-frame difference of
9,520: steer right at 11,320, center at 11,340, gas at 11,360, release at
11,370, then held gas at 11,820. Neutral at 12,799 retains scripted controls
through the 12,800-frame stop. There are 32 events. Input SHA-256:
`b6be8889b49047ef830d4ceb545a50a8d27d21df3f0d8cb390f73d688ce22001`.
This follows **Inputs** and **Reference & validation**: only cabinet pins
change; game variables and generated instructions remain untouched.

The unchanged raw v14 host, sound/hook enabled with fresh all-FF saves,
completes normally after 2,392,873,232 instructions, 5,562,921,029 cycles and
12,800 frames at PC `0x005d4e08`. Board time is 222,516,841,160 ns. All 32
inputs apply without manual takeover, and the first 26 logged event boundaries
match the earlier sound-off restart exactly. All 25,605 recorded resumes have
matching scripted pins and continuous witnesses; their PCM hashes match the
complete float WAV of 9,812,993 stereo frames. There are 18,104,188 nonzero
scalar samples and zero chip/host drops. The hidden unthrottled run opens no
playback device; it is not a listening or gameplay-speed check.

The final Metal frame is visually verified as active second-race driving:
118 km/h, fourth gear, elapsed/lap time 15.83 seconds, remaining time 45 and
position 15/15 on the dirt course. Its cockpit view does not show the car body,
so the final image alone does not verify Lancia identity. The input history,
matching restart prefix and active racing HUD establish a new race after fresh
credits; a frame-limit exit alone would not establish that. Complete second-race
or remaining-course validation is still open.

Evidence: `build/validation/second-race-v14/capture-001/`. `result.json`
SHA-256 `3656dc5a5f465c2cb0405ac3d1cf204b38bc9fccd8308df90789734770b7d56c`;
`visual-review.json` SHA-256
`80d2a2df0d71ac9c8e1fd8beca9164ee926c5f2c9c0ef54267ce7704e35b5c46`;
PPM SHA-256 `c365a81ef942ee9f1c931ef3d90733ac4201c4009fe7b0a6473555496520aa71`;
WAV SHA-256 `2e658bf0af6a555ddbd42ce13ee0f690d1acc67ed890ab206ef47621fe210ee5`.
Frozen runner `build/validation/second-race-v14/run.py` uses exclusive outputs,
a three-billion-instruction ceiling, 600-second timeout and 512 MiB artifact
bound. Preparation and independent review precede execution. Raw executable,
assets, old captures and helper identities remain unchanged. The lower-level
result intentionally retains `visual_second_race_verified: false`; the separate
visual review records the later inspection rather than rewriting raw evidence.
The independent artifact audit verifies all rows/pins, prior event boundaries,
PCM hashes and recomputed nonzero/peak/RMS statistics, fresh/final saves and
unchanged frozen inputs. It separately confirms the racing HUD in the image:
`build/validation/second-race-v14/independent-review.json`, SHA-256
`acec64c1cd9ff81dd921e92677402aac9e5caad87236e0f5459cf01c01a6645b`.
The 114.573042-second capture cost is diagnostic, with concurrent correctness
work, and supplies no normal-paced performance result.

Next: compare the repeated-step renderer in the three historical complete
scenes with composition guards, then measure cost in a quiet interval. A
separate sound-off ordered-resume reference fixture is being developed to
measure exact Budget/frame/fetch phases before independent manual replay can
be claimed. Validate later course paths without game-logic shortcuts. Phone
work remains deferred; no full-race MAME/PCB or release-readiness claim follows.

## Manual v14 replay and final pixel attribution (2026-10-14)

The exact signed v14 executable replays the completed manual session inside
the sandbox without SDL/GPU/audio-device initialization or live-save writes.
All 18,689 resumes match inputs, witnesses, return/fault details, PCM counts
and raw-float hashes, including 7,163,505 stereo audio frames. The normal quit
witness is 9,344 frames, 1,720,524,703 instructions, 4,060,943,401 reference
cycles, board time 162,437,736,040 ns and PC `0x005ca570`. Final EEPROM/backup
bytes agree, and the entire signed app/assets, recording, live saves and log
are unchanged. Exit is 0 and stderr is empty. Evidence:
`build/validation/manual-v14-offline-replay-001/result.json`, SHA-256
`ca78def96fbe9d0cb0f6e30305e0753ce8c52d798b996bf182b1949b4bb76bfb`.
Its 77.833496-second diagnostic cost is not a visible gameplay measurement.
This closes same-Machine replay for the actual manual-input session; it does
not establish independent CPU/sound/hardware or listening fidelity.

The final-three Metal probe also passes the exact disabled-mode guard across
all 190,464 RGBA pixels. CPU, current and combined-repeat images remain
byte-identical to accepted attempt02. All GPU owners match the CPU, excluding
an ownership discrepancy at these points. At (294,94) and (434,94), owner 827
has identical clipped-start parameters and final reciprocal z, but different
accumulated U/z. Float32 reconstruction with a one-ULP-different U step
reproduces both measured GPU words. The actual step was not exported, so its
value and upstream cause remain inferred. At (98,331), owner 784 has identical
repeated parameters, but measured reciprocal z differs by one ULP. That alone
moves fixed U across integer truncation from 42239 to 42240 and changes the
filtered texel from 87 to 88, explaining the color difference.

Retained SDL source supplies nil runtime Metal compilation options; generated
MSL uses ordinary division for span reciprocal and z. This source path provides
no explicit strict-math guarantee, and host compiler flags do not configure it.
Neither effective runtime options nor hardware instruction lowering was
measured; do not call a specific compiler mechanism proven. The repeated loop
remains diagnostic, with no production edit, broader-scene result or speed
claim. Evidence under
`build/worktrees/mac-triangle-fidelity/build/validation/final-three-gpu-review/`:
`attribution.json`, SHA-256
`0ae24038bdd664d56d8fb919c4e4d6f7877bdfb8f40ad090db089ec4434587bd`;
`direct.log`, SHA-256
`83b5bf82548f031af96ef20af64db02bdb68724ff8e173663aac0cd85a6b8f0c`.

## v14 continuation and completed windowed session (2026-10-14)

The single observed return seed is built and validated. Isolated worktree
`build/worktrees/observed-frontier-005ae800` preserves the audited generation
path, adds only canonical `0x0000f800` and runtime `0x005ae800`, and builds all
host objects freshly from source. Full main boundary and timed sound contracts
pass; all eight payloads and the sound/TGP/video generation inputs retain their
old bytes. The 75,207-instruction graph has no missing templates and retains
all previous addresses. Raw v14 executable SHA-256:
`81b1d88f34f74d13d40ed637dfeb35a1bb1c7868c3e9d7c994988c66bef4c0d1`.
Generation/build provenance is in the worktree's
`build/frontier-005ae800-evidence/completion.json`.

The bounded sound/hook continuation passes all 25,580 completed normal old
resume and observation rows exactly, including budgets, pins, witnesses,
return/fault details and PCM hashes. It excludes the old partial CPU frontier
row. A separate comparison matches that row's first 622 scalar audio samples;
this is audio-byte evidence only, not equality of terminal CPU phases. The
new run reaches its normal frame limit at 14,001 frames, 2,675,194,732
instructions, 6,084,875,601 reference cycles, board time 243,395,024,040 ns
and PC `0x005c7160`, without a missing-code frontier. All eleven input events
apply, including the neutral sentinel which keeps physical controls excluded.

All 28,013 PCM observations match sequential float WAV bytes: 10,733,721
stereo frames at 44.1 kHz, 19,665,219 nonzero scalar samples, zero chip/host
drops. This hidden unthrottled check uses no physical audio device. Evidence:
`build/validation/frontier-005ae800-v14/continuation-001/result.json`, SHA-256
`cb27113fd873150e223a74f3c4d137d89a892d25d1265970ba391a3b39316c39`;
WAV SHA-256
`78302323fb65322fd6003e9d3331d03a3d156dc9bd4f73ee21800d32b270fc02`.
Independent read-only review confirms all comparisons, valid seal/final saves,
matching initial seeds/options/images, the unchanged original capture and
current raw executable/assets:
`build/validation/frontier-005ae800-v14/independent-review.json`,
SHA-256 `2f87c5865b2524eb2eb7a78a58e9f84d75345b3dfe525e59ee3cd832d358a6ef`.
The frozen runner's initial text-header parsing error was caught and corrected
before execution; both independent reviews used real sealed bundles. No game
run is discarded on that account. This is same-Machine cross-generation
prefix/continuation evidence, not independent CPU/hardware or speed proof.
No v14 same-executable offline replay has been run.

The signed app at
`build/worktrees/observed-frontier-005ae800/build/mac-recording-v14/Sega Rally Native.app`
opens in a normal window with Metal Apple M5, sound, instruction hook, selection
replay and capture `build/recordings/mac-v14-20261014-001`. Its log confirms all
ten events and manual takeover at frame 1,851. The session subsequently closes
normally (`quit`) at 9,344 frames, 1,720,524,703 instructions, 4,060,943,401
cycles and PC `0x005ca570`, native result 0. It supplies 7,163,505 stereo frames
with 12,886,297 nonzero scalar samples, zero chip/host drops and one estimated
SDL input-shortfall request for 552 extra input bytes; 995 frames remain queued.
No WAV, listening-quality verdict or quiet speed measurement accompanies this
live session. No game process remains at the status check. The longer automated
continuation establishes passage beyond the former frontier. Preserve both
signed apps, their captures and original logs before any relaunch.

The read-only live audit passes strict/deep signature verification, manifest
identity, all eight packaged/raw/generation images, assets report, selection
bytes, all-FF initial seeds, final live-save equality and the seal/context/
continuity of 18,689 rows. After handoff there are 409 input snapshot
transitions, nine distinct nonneutral snapshots and 12,939 nonneutral resume
rows. Accelerator, both steering directions, brake, view and handbrake change;
no gear change is captured. The first deviation from the neutral selection
tail is row 4,069 before frame 2,034, at instruction 397,840,727 and cycle
884,017,400, with accelerator fully pressed. This is a manual-input capture,
not the neutral continuation stream. With no live WAV, its PCM digests have
not been independently recomputed, and offline execution remains pending.
Signed executable SHA-256:
`6ac1c2cf576d636efde6e02255c7c7caacbe731ac4d340e45c50bc097466dde9`.
Audit: `build/validation/frontier-005ae800-v14/live-launch-001.json`, SHA-256
`a93b78ce4dbf98a71494ba70dbaa2dfb450d7b22c45bc3d103d6e3498911e35d`.
Audited files remain unchanged.

## Polygon repeated stepping diagnostic (2026-10-14)

Actual GPU owner measurements now confirm the three large-error cutout examples
from the earlier CPU review. The direct-offset path selects polygons 654, 795
and 650 at (398,226), (415,199) and (439,230); repeated four-or-more-vertex
stepping restores CPU owners 650, 658 and 795. The shader remains a diagnostic
using the common HLSL generation path. No production source changes or default
option changes accompany this evidence.

The first candidate is invalidated: disabled diagnostic mode changes one pixel
at (243,322), from RGB (81,48,32) to (78,45,29). Generated MSL shares a product
temporary where the old expression was separate; precision perturbation is
plausible, but the exact cause is not established. Preserve this failed source,
binary, log and raw frames. The second candidate isolates the repeated branch
from unchanged baseline polygon shading and passes the full 190,464-RGBA-pixel
disabled-mode guard and all 447 synthetic cases (85,137,408 pixels).

In the frozen 496x384 3D-only reconstruction, current optional scanlines have
844 differing pixels, absolute RGB error sum 10,754 and maximum channel error
168. Repeated four-or-more-vertex stepping leaves eight differences, error sum
54 and maximum 4. Combined triangle/polygon repeated stepping leaves three,
error sum 24 and maximum 4. Of 843 changed pixels, 842 become exact; one at
(434,94) worsens from the CPU/current RGB (105,65,46) to (101,62,44).
The remaining pixels are (294,94), (434,94) and (98,331). CPU ownership is
opaque quads 827, 827 and 784 respectively, so these owners do not involve a
cutout decision. GPU oz/reciprocal-z attribution remains pending.

Evidence under `build/worktrees/mac-triangle-fidelity/build/validation/`:
`polygon-repeat-diagnostic/report.json`, SHA-256
`d21800f440ce8952edf8bb467cc81157ff2aaa8284224253405550a811b72650`;
`polygon-repeat-diagnostic/ownership.json`, SHA-256
`866ddaaf854283e27c77e92df11e0864a057f67f81c655bb207467979d46691e`;
`final-three-cpu-review/evidence.json`, SHA-256
`98f8eff577f097847cd82a6fc09f7c44b0086be241ebff36dd97f911eaa4abcd`.
The source-only GPU follow-up has not been compiled or executed. Loop cost,
broader composed scenes and continuous flicker remain unmeasured. Do not
promote this single-scene result to full-game accuracy or enable it for play.

Next: replay the preserved v14 manual capture offline; measure the final three GPU
interpolants before changing arithmetic; validate second-race/remaining-course
paths and broader composed graphics before any production renderer change.
Keep heavy builds and GPU captures quiet during future live playtests. Phone
work remains deferred. Production recording identity checks stay strict.

## Recorded live frontier at attract return (2026-10-14)

The signed recording-enabled v13 app reaches missing native target
`0x005ae800` at 2,437,360,267 instructions, 5,556,809,161 cycles and 12,785
rendered frames. The previous two-billion held-gas checkpoint does not cover
this later path. Normal close seals 25,581 resumes as `frontier`, with board
time 222,272,366,440 ns and native result 3. All ten selection inputs applied;
manual takeover occurred at frame 1,851. The live session supplied 9,802,212
stereo frames, including 17,920,545 nonzero samples, with zero chip/host drops
or SDL input-shortfall requests. There is no WAV capture or confirmation of
live listening quality. These counters do not establish speaker fidelity.

Recording: `build/recordings/mac-v13-20261014-001`. Transcript SHA-256
`8bc8c8ad19397ebe3a4f36240460449b4362ab68dfc7e9f3df67f114eb7a813b`;
observations SHA-256
`c7842f9e6c7cf818c290f89d764696b73cdaa884f284d97667ba473ceded5c9e`.
The signed executable and app resources remain preserved. Exact-executable
offline replay inside the sandbox reproduces all 25,581 resumes, PCM hashes,
fault details, final saves and the same frontier; bundle bytes remain unchanged.
Evidence: `build/validation/live-recording-replay-001/audit.json`, SHA-256
`dbe7a0d11aa827310f5e98890db797d73b2861b4c1d66c9dca62015ae9877932`.
Its 107.897885-second cost is a diagnostic run, not a game-speed measurement.

Read-only disassembly identifies a single RET at canonical ROM address
`0x0000f800`. Seeded `comm_attract_inner_8` loads the target, performs
bookkeeping and branches through g0 at `0x005ae7fc`; v13 includes that
predecessor but omits the return. One actual-observation seed is added in
`config/segarally_observed_seeds.json`, bound to the sealed recording and signed
executable. The isolated standard generation now contains 75,207 instructions:
only canonical `0x0000f800` and runtime `0x005ae800` are added, all 75,205 old
addresses remain, and indirect sites/invalid stops remain 419/13. No compiler
heuristic or game instruction changes. The subsequent full host build and
continuation checks pass as recorded above. Production recording identity
checks stay strict.

An input audit proves every captured twelve-pin snapshot matches the canonical
ten-event selection replay, including neutral manual handoff. No additional
manual or mid-frame change occurred. A new bounded capture can therefore use
that prefix plus a final neutral sentinel, compare all 25,580 completed normal
rows exactly and then continue. The old final MissingCode row executed only
81,380 of its 100,000-instruction budget; shortening the new budget is not an
exact phase match because Budget and MissingCode have different synchronization
points. Exclude that partial row from the normal-prefix claim. Proof:
`build/validation/live-recording-replay-001/input-frame-representability.json`,
SHA-256 `12fb56a3d403a1fe4ba28e7bdde3a122e63a600811289dd75e36f303127aa301`.

## Actual v13 sound capture and offline replay pass (2026-10-14)

Fresh detached worktree `build/worktrees/mac-recording-v13` at `9197003`
clones the original sound/hook generation to its exact audited relative path.
All 203 file inventories, sizes, SHA-256 values and bytes match before/after
build. Release recompiles all 54 generated sources plus app/runtime under one
host target; old pipeline objects are unused. Eight staged images match the
original bytes. Build/object/command identities and logs are under the
worktree's `build/recording-v13-evidence/`; source/audits and older apps remain
preserved. Raw executable SHA-256:
`4d1e4283faf35776c65c5501f7ae2602d3f9c23e8a16dd5934c7f122b739dabc`.

All six bounded actual-game gates pass: boot capture/offline replay,
unrecorded/recorded driving, driving offline replay and exact comparison.
Boot matches 121 resumes, 10,923,622 instructions, 26,117,011 cycles and 60
frames. Driving matches 5,603 resumes, 536,820,288 instructions,
1,217,355,612 cycles, 2,801 frames and PC `0x005d08f0`. Recorded/unrecorded
runs have identical twenty input boundary lines, final Metal PPM, EEPROM/backup
and complete float WAV: 2,147,416 stereo frames at 44.1 kHz (48.69424036
seconds), 2,857,178 nonzero float samples and zero chip/host drops.

Evidence is under `build/validation/recording-v13/`: `boot-001`,
`boot-replay-001`, `baseline-001`, `recorded-001`, `drive-replay-001` and
`comparison-001`. Offline replay runs inside the sandbox with only
`--replay-recording`/`--data`, leaves bundle/live saves unchanged and opens no
SDL/GPU/audio device. Input identities match before/after every gate. Separate
read-only `independent-review.json` and `independent-boot-review.json` check
every observation's raw-float hash/count against sequential WAV bytes and
confirm actual initial saves are all-FF constructor defaults (128/16,384 bytes).
Captured final bytes match live saves. This is same-Machine/raw-PCM evidence,
not independent CPU/sound, full-course, physical audio or speed validation;
hidden unthrottled captures use no physical audio device.

The new signed app launches normally at
`build/worktrees/mac-recording-v13/build/mac-recording-v13/Sega Rally Native.app`
with sound/hook, canonical selection replay and recording
`build/recordings/mac-v13-20261014-001`. The live log reports Metal Apple M5,
playback enabled and manual takeover at frame 1,851. The later frontier capture
is recorded above; listening feedback remains pending. Signed executable SHA-256:
`aa1aeb4a70795bf568b5a8c4f8f0bf70dfdb054a358c26173e0e57c7ffcb5efe`.
Raw executable bytes remain unchanged; packaged assets/report and selection
input match their source bytes. Replay must preserve the corresponding app
identity. Packaging evidence: `build/validation/recording-v13/live-launch-001.json`.

## Remaining interpolation and cutout precision (2026-10-14)

The ignored repeated-add triangle diagnostic changes five pixels in the frozen
496x384 3D-only scene, all to exact CPU matches with none worsened. Differing
pixels fall from 844 to 839; mean absolute RGB channel error falls from
0.018820704525 to 0.018768201165/255, with maximum channel error still 168.
Mode-0 instrumentation changes no pixels. The checker audit includes the initial
one-pixel alignment adjustment followed by doubled increments; all 447 synthetic
cases (85,137,408 pixels) also pass in repeated mode. The shader loop remains
diagnostic: no production change or measured speed benefit follows.

The three largest residual examples point to accumulated interpolation changing
the cutout decision in four-or-more-vertex polygons. CPU-only evaluation at
(398,226) changes V 53023 to 53024 under a direct span offset, crossing alpha
`0x004000e0` to `0x003f00e0`. At (415,199), U 2036 to 2037 crosses the same
threshold. At (439,230), the opposite crossing predicts acceptance of polygon
650 instead of the CPU's visible polygon 795. Host evaluation of the current
shader equations predicts the saved GPU colors at these points, but actual
GPU ownership/interpolant measurements remain pending. Do not present these
predictions as measured GPU attribution or a continuous-flicker fix.

Reports under `build/worktrees/mac-triangle-fidelity/build/validation/`:
`repeated-triangle-snapshot/report.json`, SHA-256
`462e1d1728bc25442f18413c885e0e5c1e1b206304a9e81ae6500dda4bf0f6a8`;
`residual-cpu-review/evidence.json`, SHA-256
`94bc72e0a3701514972b6943815125df235133e29d20560a6d0218220d59ac30`.
The quiet performance protocol is prepared but unexecuted. A separate
`polygon-repeat-diagnostic` source candidate preserves checker steps and
clipped-left setup; shader generation, compilation and GPU execution are
deferred during live play and the subsequent native-coverage build.
Frozen production files remain
unchanged. Next: measure these GPU owners, then broader error and cost before
considering a production interpolation change.

## Mac recording and offline replay integration (2026-10-14)

`--record-inputs DIR` now creates an exclusive new recording bundle around the
existing host resume/drain calls. It binds the actual executable (resolved by
`_NSGetExecutablePath`), loaded images, raw staged-assets report, all ten Machine
options and post-load initial EEPROM/backup bytes. Each resume retains the
budget, twelve final cabinet inputs, before/after witnesses, full return/fault
details and raw float PCM count/hash. Explicit completion seals final saves and
quit/frame-limit/instruction-limit/frontier/fault; failed writes, presentation
errors or abandoned captures remain incomplete. Default game and audio-drain
ordering is unchanged.

`--replay-recording DIR [--data DIR]` runs before SDL, log or save-directory
initialization. It preflights the entire bundle and exact executable/assets,
then executes the same budgets/inputs/options/seeds in the same Machine without
GPU, device audio, pacing or live-save writes. Both passes use immutable bounded
text snapshots. An initial implementation reopened paths after preflight; review
replaced that with the verified bytes. Limit terminals cannot masquerade as a
zero-instruction startup quit: frame limits require a Frame return, instruction
limits require executed instructions. The same-executable check rejects changed
executable bytes; rebuilding, re-signing or repackaging can change that identity.
Preserve the recorded app and its matching staged assets.

Both MAIN CTest registrations pass: 165 bundle checks and four launcher checks.
The ten existing Mac packaging/launcher tests also pass.
They cover nonzero coupled SCSP PCM, equal-instruction frame returns, changed
final backup, detailed device/runtime faults, actual byte identities, resealed
false observations and partial/failed captures. Strict standalone compilation
and host syntax checks pass with recording absent, recording only and
recording+sound+hook. The integrated build reports only the existing unused
`gpu_front_hud_` field warning. Mac build support defaults ON and requires the
pinned public-domain 7-Zip checkout; `SEGARALLY_INPUT_RECORDING=OFF` removes that
dependency/capability. Runtime recording remains opt-in; iOS is unchanged.

Evidence: `build/mac-input-recording.patch`, source identity list and standalone
commands; MAIN integration metadata/logs under
`build/validation/mac-input-recording/`. MAIN adds the zero-instruction terminal
regression to the frozen candidate's 160 checks, reaching 165. Source-generation
hashes remain identities, not an independent provenance or full-course proof.
Next is a fresh audited v13 host build and actual sound-enabled capture/replay.
No generated game build, new play session or preserved-app replacement has been
validated by this commit; the tests above use synthetic programs.

## Fresh credit/start after attract (2026-10-14)

`config/segarally_credit_restart.inputs` preserves all nineteen held-gas rows,
releases gas at frame 10,710, pulses two coins at 10,720/10,740 and Start at
10,760, releasing each after ten frames. A final neutral row at 11,159 keeps
scripted control active until the 11,160-frame exit. This exercises cabinet
pins and real game code under **Inputs** and **Reference & validation**.

The preserved window-first v13 Mac binary, sound off and hook enabled, applies
all 27 events and exits 0 after 11,160 renders, 2,090,501,029 instructions and
4,850,177,003 cycles at PC `0x005a37a4`. The actual capture shows CAR SELECT,
countdown 08 and the default Toyota automatic choice. This establishes a new
accepted start after the earlier timeout/attract path; it does not establish
a second race or a numeric final credit balance (not shown on that menu).
Fresh saves stayed inside the evidence directory, with no manual takeover.
The hidden/unthrottled run cost 93.387532 seconds under development load;
it is not a quiet pacing or sound-performance test.

No native-driver cap was changed: the host already accepts frame/instruction
limits. This run requests 11,160 frames, an explicit 2.2-billion instruction
ceiling and a 600-second process timeout. The older boot wrapper's two-billion
cap cannot reach these late inputs. The preserved playable app is unchanged.
Evidence is in `build/validation/restart-credit-v13/`; replay SHA-256 is
`ae9998a13f2f6300d74ecc6cbd0e2ffd3c2ffef94d03cc64ad8a6f86dde1d993`.
`run.log` SHA-256 is
`5a929aa284c5e55b0f9134466a2378a96262903387c7d43ef3e11b1e59b47ab9`;
`result.json` is
`666dd52bf272729d836e49e20105c29fef8d971d02a6a076fd84324b1e4828ae`;
`visual-review.json` is
`11cc718fb0d5c8e90963e91995f4b47f11d346ca6479f04213329af02fbb8294`.
The final PNG is only a lossless format conversion of the PPM for inspection.
Next: confirm a second race and repeat longer paths with sound/capture enabled.

## Two-billion-instruction held-gas CPU comparison (2026-10-14)

The preserved audited raw parity binary passes 2,000,000,000 main instructions,
86,328,770 ordered writes, 4,650,936,033 reference cycles, 113,101 IRQ entries
and 10,701 renders. All nineteen held-gas events apply, with no missing-code
or device frontier. Terminal PC is `0x005a37a8`, framebuffer hash
`2e2d4fb9e7f002ad`. The bounded process exits 0 in 1,077.580024 wall seconds;
this is validation cost under concurrent development, not a gameplay benchmark.
The original source identities remain unchanged before/after execution.

A separate saved-log audit matches all nineteen frame/instruction/cycle/input
boundaries, final instruction/cycle/IRQ/frame counts, PC and framebuffer hash to
`build/segarally-timed-v13/evidence/lancia-hold-gas-2b-complete`. That evidence
is in MAIN's build tree, not the suggested boundary-hook worktree. The old
native log does not count writes, so its write total is not independently
compared. Both runs use the same held-gas bytes and imported image identities.
This extends independent CPU evidence through the already observed Desert
results, second-course timeout and return to attract.

Scope follows **Reference & validation**: separate CPU engines and device
instances, shared board/TGP/geometry implementations, sound off, UART idle RX,
null instruction callback. CPU state, writes and events are checked at each
opcode; full video RAM and framebuffer comparison is terminal-only. This is
not every-frame pixel comparison, Model2AMachine scheduling or full MAME/PCB
parity. The previous sound-enabled scheduler/PCM comparisons remain separate.

Evidence directory: `build/validation/raw-parity-hold-gas-2b/`.
`run.log` SHA-256 is
`408238534c38ca4736c9a06dc6ba26891f1ea23d62b6db0880553e09365e2cec`;
`result.json` is
`6194f802d48e4fd0c9554ffc3116c521f06ab243658796764aea157f2630ea9d`;
`native-evidence-comparison.json` is
`afd2900bc100eb32ea50f7a3dd0e8e13401f64f54b6ec0c497eeb40a36250a74`.
The preserved binary SHA-256 remains
`04e3bc4d1c18fa8786ab901ee696eb6a961faeaa6e2a9f920a7effc5ef23a3cf`.
The separate fresh-credit/start check is recorded above. Exact host capture
with coupled sound remains required; neither follows from CPU parity alone.

## Residual triangle interpolation rounding identified (2026-10-14)

At (95,31), CPU and optional GPU both select triangle 787 with identical
uploaded plane coefficients. CPU repeated additions yield U/V 5630/42001,
texel 84 and RGB (20,24,0). A single 38-pixel offset from the same clipped
start yields V 42002, texel 83 and RGB (17,21,0), exactly matching the GPU.
An ignored shader toggle that repeats the additions restores all three CPU
interpolant bit patterns and the exact color, with the same owner and planes.
This measured discrepancy is interpolation rounding, not coverage, ownership,
alpha acceptance or plane setup. Mode-0 instrumentation matches all 190,464
normal-output RGBA pixels.

No production change follows: the loop's cost and broader scene behavior are
unmeasured. All five frozen triangle source hashes remain unchanged. Evidence
is in `build/worktrees/mac-triangle-fidelity/build/validation/pixel-95-31-gpu/`;
`attribution.json` SHA-256 is
`b7dbefa62d432c301093d2063615bb0424bae85b8ee3039b16d0d6387acfe8a4`.
`direct.log` is
`8a7635e89015ac7a3ca9e0eed6467a69006c883d4a26e7e07d59028e3f0cc16b`;
`repeat.log` is
`2676683c1b709a17360e375be9181bc1e11c9464e2d568facc679a7e9102ec8c`.

## Optional Metal triangle coverage (2026-10-14)

The optional scanline path now covers triangles as well as 4–8-vertex
polygons. Following the design's **Geometry (TGP) and rendering** and
**Reference & validation** sections, triangle setup follows the CPU reference:
stable y sorting, direct edge-slope divisions, rounded half-open coverage and
determinant parameter planes. It remains distinct from polygon edge chains.
Conservative rectangles supply fragments; the common shader selects the span.
All six shader entries were regenerated for SPIR-V, DXIL and MSL.
`SEGARALLY_GPU_POLYGON_SCANLINES=1` is still required; the default and preserved
playable executable are unchanged.

Both the standalone and MAIN's registered Metal fixture pass 447 invented cases
and 85,137,408 pixel comparisons. The fixture retains the prior 243 cases and adds triangle windings,
near-subpixel edges, flat tops/bottoms, collinear/empty spans, clipped starts
and varying-UV layered cutouts/checker patterns. Native scale 1 is tested;
enhanced scales are not validated.

On the frozen first-car **3D-only reconstruction**, differing RGB pixels fall
from 969 to 844 and mean absolute channel error from 0.0215018761 to
0.0188207045 on the 0–255 scale. Of 127 changed pixels, 126 become exact,
including (147,20), now RGB (74,84,6). One at (95,31) worsens from matching
CPU RGB (20,24,0) to (17,21,0); ownership/precision attribution is still open.
The saved snapshot lacks tile/character RAM, so these counts must not be
compared with the earlier fully composed 822-pixel result. Default-off output
matches the previous candidate over all 190,464 RGBA pixels. No performance,
continuous-flicker, new driving-scene or full raster parity claim follows.
The shader still differs from CPU repeated-addition interpolation.

Frozen source, exact compiler/link commands, shader/output identities and
limits are in `build/worktrees/mac-triangle-fidelity/build/validation/report.json`,
SHA-256 `ef465deffa36963a1c1c0eab200de8a6c68c384f0a951e28ce33dd05afd3a372`.
The reviewed patch SHA-256 is
`3f5860e4043736e68c5fdedb31e57d3e03ed08fbd2c966e70be31ba704070a31`.
MAIN copies and integration evidence are under
`build/validation/mac-triangle-fidelity/`, including the preserved registered
CTest log and its linked executable identity. Next are the residual (95,31)
attribution, broader composed driving scenes and a quiet performance comparison.

## First-car pixel ownership diagnosis (2026-10-14)

Pixel (147,20) is attributed from the frozen scene and authenticated old/new
GPU instrumentation. The CPU winner is textured triangle 800, with RGB
(74,84,6) and a passing cutout sample. Its rounded row span is `[138,148)`,
which includes pixel 147. Both GPU paths produce no triangle-800 fragment
even before alpha shading. The sample is very close to an edge; this supports
a subpixel coverage discrepancy, not a claim about exact hardware quantization.

The old GPU winner is polygon 804, RGB (38,44,0). Its reference span is
`[148,148)`: empty, so the old fan erroneously owns the pixel. Corrected
4–8-vertex coverage rejects 804 and lets polygon 850 shade black. The polygon
fix did not discard the legitimate CPU owner; it exposed the separate triangle
coverage error. Both instrumentation mode-0 outputs match their unmodified
counterparts over all 190,464 RGBA pixels.

An initial attribution fixture mixed a stable generated shader header with
modified renderer C++/HLSL. Its results were invalidated. All four renderer
files were restored and authenticated from the preserved prototype patch before
the measurements above. The triangle extension described above follows this
diagnosis. The default remains off and the preserved playable app is unchanged.

Ignored evidence is under
`build/worktrees/mac-raster-fidelity/build/validation/`. SHA-256 identities:

- `pixel-147-20-gpu/probe-both.log`: `af50a1c5b88a0344cb9af3b7307e313cff678ee528b91c8bf60cb0d495a7063e`
- `pixel-147-20-cpu-review/evidence.json`: `afa9fd8a48e1e773e58a4dc8e877cd59dbffac468ffa76ac740d0fc0f057a3e6`
- `pixel-147-20-cpu-review/triangle-coverage.json`: `4015a5ce25891ddf8c216c0871db9ea68d916393a8e7969f7685204519e0483e`
- `pixel-147-20-cpu-review/polygon-804-coverage.json`: `80f77a10d2676173afed4a4b3e16576c88ac14ca96883b1aa71611ee15d825af`

## Staged build identities for future recordings (2026-10-14)

`stage_data.py` adds a versioned provenance object to `assets.json`: coverage
and available reachability/generated-source identities, TGP audit/generated
source/verified firmware identities, video audit identity and public reference
revision IDs. Hook sources remain checked by the existing boundary contract.
Ordinary main/TGP generated hashes are explicitly `identity_only`; their
existing audits do not authenticate the complete generation source. Optional
legacy artifacts that are absent do not acquire invented hashes.

Present inventories, paths, reachability and declared TGP firmware are checked
before destination mutation. The firmware must match the verified program's
declared upload slice/count. Seventeen synthetic provenance tests and the
thirteen existing asset tests pass; the new registered CTest passes too.
Invalid incremental staging preserves the existing package bytes and mtimes.
The SDK attribution now explicitly describes the separate SHA-256 library use.

Actual ordinary v6 staging succeeds with six images at
`build/validation/staging-provenance/ordinary-v6`. The v13 sound/hook build
uses paths relative to its original `boundary-hook` worktree. Calling MAIN's
CLI directly on it rejects the program path; rebinding only the staging module
also leaves sound helpers in the wrong context and rejects their paths. With
the new module and existing helper modules using the original worktree context,
all eight images stage successfully and 52 generated source identities are
recorded. No confinement rule was relaxed. Evidence is
`build/validation/staging-provenance/sound-hook-v13-results.json`; staged
`assets.json` SHA-256 is
`7134f44e0c162f6ba1069ddee1c6d8f6deaa90aa8cdce2f9f81c070a3e814e5d`.
The preserved playable app/assets were not changed or relaunched.

Next is actual host capture/replay wiring with initial save snapshots and
per-resume PCM comparison. Staging identities and transcript parsing alone do
not establish a playable recording, independent CPU parity or a full course.
The graphics lane is separately using the frozen first-car scene to resolve
pixel (147,20); no full-game run is needed for that attribution.

## Bounded capture-identity hashing (2026-10-14)

`rt::Sha256` and its byte-span/stream/file helpers reuse the pinned public-domain
7-Zip SDK instead of adding a cryptographic implementation. The unchanged
`Sha256.c`, `Sha256Opt.c` and `CpuArch.c` are linked by a separate CMake library;
the public per-context API selects software hashing without global preparation.
`THIRD_PARTY.md` records this use. The player is not linked to it yet.

All 179 known-answer, binary/chunking, bounded-I/O and startup-snapshot checks
pass, including the integrated CMake/CTest target `sha256`. Strict standalone
C11/C++20 warnings-as-errors also pass. A transient fixture failure was a
53-byte typo in the canonical 56-byte vector; correcting the fixture, not the
backend, resolved it. Helpers use a fixed 64-KiB buffer, reject oversized or
failed reads, and hash the actual supplied bytes rather than reopening a saved
path when a caller passes a loaded snapshot.

Exact standalone commands and dependency identities are preserved in
`build/input-replay-capture-sha256-tests.sh` and
`build/input-replay-capture-sha256-identities.txt`. Capture integration still
needs a validated manifest, actual initial EEPROM/backup snapshots, unchanged
resume/drain calls and replay checks. A digest identifies bytes; it does not
establish complete game coverage or independent parity.

## Optional Metal polygon scanlines integrated (2026-10-14)

The corrected four-file GPU candidate and ROM-free fixture are integrated from
`build/worktrees/mac-raster-fidelity`. The default remains the triangle fan;
`SEGARALLY_GPU_POLYGON_SCANLINES=1` explicitly selects two-chain span coverage
and interpolation for 4–8 vertex solids and textures. Conservative rectangles
provide candidates, then the shared shader rejects pixels outside the CPU's
rounded half-open spans. Tile composition, scissor and polygon ordering stay
intact. MAME-derived setup/shading notices and `THIRD_PARTY.md` are updated.

Stable `38abb7c` GPU files compiled separately produce byte-identical output
to the candidate with the option disabled at frames 513, 1634, 2329 and 2599.
At first-car frame 1634, mean absolute RGB channel error falls from 0.727817
to 0.019426 on the 0–255 scale, and differing pixels fall from 15,830 to 822.
The two driving frames improve from 13,896 to 1,244 and 21,236 to 599 differing
pixels. These are frozen sound-off v3/v6 scenes, not current v13 speed tests.
The full first-car CPU image matches the preserved reference byte for byte.

Polygon 353's white pixel at (125,75) now matches CPU black. Relative to the
previous prototype, ten pixels become exact matches and one at (147,20)
worsens. Relevant spans contain that pixel; exact cutout ownership remains
unresolved. The full frozen polygon/raster memory is preserved for analysis.
Triangle coverage and CPU repeated-addition texture precision remain open.
No full fidelity, continuous flicker or performance claim; do not enable the
candidate by default or replace the preserved playable app yet.

All six shader entries regenerated for SPIR-V, DXIL and MSL; Metal executed
the candidate. The integrated CMake fixture passes 243 cases and 46,282,752
pixel comparisons, including varying-UV layered cutouts, outside the sandbox.
The new `M2_METAL_TESTS` option defaults off. An initial attempt to enable the
inherited windowed app exposed its absent ImGui checkout; tests now initialize
SDL independently with the launcher disabled, avoiding that unrelated dependency.
The Debug build emits only the existing unused `gpu_front_hud_` warning.

Evidence is in the isolated `build/validation/report.json` (SHA-256
`7ff6dc28f93598e66d7bef0064bb44c92431c73e84d5407d1956406b292099f4`) and
`provenance.json` (SHA-256
`7b9ed128e2d366b80b5cf322552a21397b6ea3dbf484554cfdc1a54cccdf2256`),
with exact compiler/linker commands and linked input hashes. Main-tree
`build/validation/mac-raster-fidelity/integration-results.json` records matching
source hashes and the registered test. The original uncommitted prototype is
preserved byte for byte as `original-prototype.patch` in that main evidence
directory, SHA-256 `88ce63e1875dba182f66722bf69ebc8e0b3023714d32a485324e211c6dd5039b`.
Next: resolve residual ownership with the saved scene, measure candidate GPU
cost, and test broader scenes before a player-facing graphics change.

## Exact host resume transcript core (2026-10-14)

The new bounded `segarally-host-resume-v1` core records every actual
`run_until_frame` call, including its budget, twelve input bytes, before/after
instruction/cycle/time/frame/PC witnesses and return reason. It preserves
equal instruction counts across IRQ/frame preparation, requires a completion
marker and rejects malformed, discontinuous, oversized or partial streams.
It is separate from the unchanged frame-based selection/driving replay.
No player flag or runtime behavior changes in this commit.

Both registered CMake tests pass: 98 stream/contract checks and 84 synthetic
Machine checks across the old outer loop and exact instruction hook. The
Machine fixture establishes an actual timer IRQ/frame boundary at equal
instruction counts, then verifies the next opcode reads the correct pins.
Recorded, unrecorded and replayed state, frame output and saves match. The
integrated tests were built in `build/host-make`; its existing CMake cache
needed reconfiguration before the new target names were available. The build
reports only the existing unused `gpu_front_hud_` field warning.

Provenance approval is a caller contract, not a parser guarantee. Next work
is to bind actual loaded images, generated code/options and initial save
snapshots, wire capture/playback around unchanged resume/drain calls, and
compare PCM and terminal fault details. The independent CPU harness lacks
startup-seed/resume-phase support and coupled sound; unsupported captures must
be rejected. No full-course or independent parity claim follows from these
ROM-free tests. The four main-tree GPU prototype files remain untouched and
outside this commit; their corrected candidate is being tested separately.

## Windowed sound session exits cleanly (2026-10-14)

The signed window-first app completed 9,347 frames, 1,726,555,073 instructions
and 4,062,247,215 reference cycles at PC `0x005d2648`, then exited with result 0.
All ten selection events applied and controls handed over at frame 1,851.
The session produced 7,165,805 stereo audio frames at 44.1 kHz, including
12,889,932 nonzero channel samples. Peak was 0.0759811401 and RMS 0.0105479909.
There were zero chip drops, host drops, estimated SDL input-demand shortfalls
or extra requested bytes; 2,255 frames remained queued at exit. These are
actual visible-session counters, not a wall-speed or hardware-fidelity proof.
The user confirmed the separate ten-second sample; the optional live-game
listening question remains unanswered.

Evidence: `build/worktrees/boundary-hook/build/mac-boundary-sound-v13-ordering/`
contains `live-session-results.json` (SHA-256
`2f43f95f97d22577b8d70298f0d450ccb8abd8696bf878e77fe57ee400f947f6`) and
`live-app.log` (SHA-256
`5be11928cce49ff9cb8ba4f5465a808f8c342a6d1a592ff1af33ea0d0173886c`).
An outside-sandbox process check confirms both the live process 28011 and the
older frozen process 8289 are absent. No restart was performed. Graphics
validation may resume; preserve the completed log before another launch.

Parallel work now isolates the scanline raster correction in
`build/worktrees/mac-raster-fidelity`, based on `38abb7c`, preserving the four
uncommitted main-tree prototype files. The original patch is saved at
`build/validation/mac-raster-fidelity/original-prototype.patch`, SHA-256
`88ce63e1875dba182f66722bf69ebc8e0b3023714d32a485324e211c6dd5039b`.
A separate input-capture audit found that frame-only replays cannot faithfully
record current mid-frame manual input. Instruction-boundary event timing,
initial save/configuration provenance and sound-board coupling must be covered
before manual recordings can serve as independent full-course parity evidence.

## Window-first audio startup passes (2026-10-14)

Moving only `audio.open()` after successful window, Metal and touch setup fixes
the observed startup failure in a bounded windowed test. This follows Daytona's
device-opening order and keeps opening before the board-time pacing origin.
No CPU, device, PCM, shader, audio parameter or generated object was changed.
The exact host source is integrated in the main tree and matches the isolated
candidate SHA-256 `1af69d360c94d1a764d144f77ca220c8f7cd8cf22b2250efeb2370ff5d3827eb`.

The candidate completes 60 frames, 10,923,622 instructions and 26,117,011 cycles
at PC `0x005a27c8`, exits normally, and submits 46,071 stereo frames with zero
chip/host drops and zero estimated SDL input-demand shortfalls. There are 2,933
frames queued at exit. Boot PCM is silent; this verifies startup/output without
claiming a live-game listening result. The preceding same-context windowed and
hidden probes failed `AudioQueueStart`; both failures remain preserved.

Evidence is `build/worktrees/boundary-hook/build/mac-boundary-sound-v13-ordering/`
with `ordering.diff`, `build-provenance.json`, `windowed-60-results.json` and
`windowed-60.log`. Provenance records the compiler/linker arguments and reused
object hashes. Candidate executable SHA-256 is
`102532a8259b23a6c4b40f969acacca939cd23b3ad0a6ead2a824a980d4f4d03`;
the earlier tested executable remains preserved.

The signed app in the same ordering build directory is now running windowed
with sound, the exact hook and normal pacing. All ten bundled selection events
applied, manual control returned at frame 1,851, and execution passed the
race-start checkpoint and frame 2,760 without an initialization/frontier error.
PID 28011 was alive when checked; `live-app.log`, `live-app-save` and
`package-provenance.json` record the session. The signed executable SHA-256 is
`9a9c6f4e78e633edd41f15341535203f9560f8098ee80ff822b6406f3c0fa169`;
all 26 code/data sections match the tested raw candidate, all eight assets and
the bundled replay match, and strict/deep signature verification passes.

The window is left for the user's test; an optional live-game listening question
is pending. Do not confuse the confirmed standalone sample with live-game
listening, or the 60-frame counters with a completed live-session audio report.
Heavy builds, replays and captures remain held during the playtest. Next work
is to record that feedback and the eventual exit counters, then test longer
sound-enabled paths and remaining course/restart coverage. The original frozen
pre-game PID 8289 was not terminated; its bundle/evidence remain separate.

## Native PCM heard on Mac speakers (2026-10-14)

The user confirmed hearing the standalone ten-second native Sega Rally audio
sample. The current CoreAudio check submitted all 441,000 stereo frames with
zero reported input-demand shortfalls or drops; its log is
`build/validation/audio-output/current-speaker-check.log`. This establishes
audible output for that captured sample, not continuous game playback or
hardware sound fidelity.

Both direct windowed and direct hidden startup probes also returned
`AudioQueueStart: -66681`, each run explicitly outside the tool sandbox. The
current hidden failure prevents attributing the error to foreground mode.
The standalone player's success makes a game startup comparison useful.
The default output reported outside the sandbox is MacBook Pro Speakers,
two channels at 48 kHz; no device or OS permission setting was changed.
Next: test Daytona's window/GPU-before-device startup order, then confirm
visible gameplay sound. Preserve each failed probe rather than overwriting it.

## Near-real-time Mac sound checkpoint (2026-10-14)

The quiet normal-paced sound-and-hook run completes frame 2,760 with identical
main state: 529,303,426 instructions, 1,199,537,000 cycles and PC `0x005dde44`.
All 64 progress/input rows match the older device trial; eighteen input events
apply, with the frame-2,760 event pending at exit as before. The external
monotonic wrapper measures 48.519993 seconds from launch through exit for
47.98148 seconds of board time, including initialization and final saves.
That is 98.890% board/wall speed for the whole process, not a separately timed
race segment or a guarantee of visible-window performance.

SDL reports zero input-demand shortfalls, zero requested extra bytes, zero
chip/host drops and 2,282 queued frames at exit. The 2,115,984-frame WAV is
byte-identical to the prior normal-paced capture. The earlier outer-loop run
reported 3,222 demand requests for 10,744,248 bytes; its log-timestamp timing
was less precise than the new monotonic measurement. No game clock, sample,
pitch or audio gain was changed to obtain the new result. Human listening is
still unconfirmed; the counters establish supplied SDL input, not exact
hardware-output samples or full sound-board fidelity.

Evidence is `build/worktrees/boundary-hook/build/mac-boundary-sound-v13/`
with `normal-paced-monotonic.json`, `normal-paced-results.json`, the log/WAV
and the earlier source/binary and PCM-comparison reports. The tested executable
remains SHA-256
`87a6aa3ecfe0b2748b132b8a192c8d975633ca903849184a37664089a9013da6`.
The source/test integration is committed as `7b574fa`; the isolated source
hashes match those integrated files. The GPU prototype remains excluded.

The first visible launch did not reach SDL initialization: its log contained
only `Sega Rally native host startup`. All 1,587 startup sample stacks were
blocked at `ifstream` / `fopen` / `__open_nocancel` while opening the external
selection replay. That replay is a regular 429-byte file with the expected
SHA-256 `797df95862beda36fd9ab54f8c440b701897bc0561b0cf8630f29e30b4f25f6e`.
The sample proves the blocking file open; an operating-system privacy prompt
is a hypothesis, not an established cause. Preserve `live-window.log` and
`live-startup-sample.txt` under the sound build as the failed-launch record.

The Mac launcher now reads a bounded regular replay before changing the bundle,
copies its exact bytes to `Contents/Resources/input-replay.inputs` before signing,
and passes that resource path at launch. Packaging without a replay removes a
stale copy; activating an existing process leaves its package unchanged.
Ten lightweight launcher tests pass with synthetic assets and mocked OS calls,
covering copy-before-sign ordering, flags, the 1 MiB size bound, invalid inputs,
stale replay cleanup and already-running activation. These tests establish
packaging behavior, not whether the blocked visible launch is resolved.

Closing the original frozen process required an escalated command that remained
at approval and was canceled without a confirmed execution result. The old
process and bundle were left untouched. A separate prepared build directory,
`build/worktrees/boundary-hook/build/mac-boundary-sound-v13-live`, contains the
same tested executable, eight verified assets and bundled canonical selection
replay. All 26 executable code/data sections match before and after signing;
strict/deep signature verification passes. Its `package-provenance.json` records
the compiled-source integration separately from the newer launcher packaging
source. One windowed, normally paced launch with sound and the hook enabled
successfully loaded all ten replay events, clearing the original external
replay open. It then exited before window creation when SDL audio returned
`CoreAudio error (AudioQueueStart): -66681`. The local SDK identifies this as
`kAudioQueueErr_CannotStart`, distinct from its permissions error. This does
not invalidate the earlier successful direct-device timing test; it prevents
a claim that the new visible sound app has run. Preserve this log while
comparing launch context and output-device startup. Race progression and
listening confirmation remain pending.

## Exact instruction boundary hook and driving equivalence (2026-10-14)

The opt-in `M2_INSTRUCTION_BOUNDARY_HOOK` path lets exact generated chunks
continue between instructions while the Machine callback retains the original
clock/IRQ preparation at each boundary. Frame and budget stop priority,
same-count IRQ redispatch, resumed calls, live guards and sticky frontiers are
preserved. Count-based Lockstep callbacks cannot share that event authority.
Ordinary builds retain their original layout and generated-symbol dependencies;
compatible hook builds can run either old or new scheduling for comparison.

Generation requires `--instruction-boundary-hook`, records a source/header and
generated-source/object hash contract, and emits separately compiled version/
layout markers for every chunk. Dedicated CMake uses
`SEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON`; the host, launcher and bounded boot
runner also require the runtime flag to enable it. Inexact generator/CPU modes,
stale objects, missing markers and mismatched host capability are rejected.
All prebuilt Rally-object consumers were reviewed. The raw independent CPU
wrapper needed matching compile definitions and an early marker check too;
without this correction it would have linked incompatible Lockstep layouts.
Its callback remains disabled, so that wrapper does not test Machine scheduling.

The isolated `build/worktrees/boundary-hook/build/segarally-boundary-v13`
generation matches frozen v13's 75,205 instruction addresses, 51 instruction
chunks and 52 objects. All non-capability reachability fields match; stripping
only the new markers leaves every instruction chunk byte-identical. Coverage
SHA-256 is `d9c578b3f1bc7e016029143cce8ba2358fa991d5b098a5c10ab4402f8f71980d`.
The root integration byte-checks all 25 source/test files against this tested
worktree; `build/validation/boundary-integration/source-hashes.json` records them.
The four unrelated GPU prototype files remain outside this change.

Strict and ASan/UBSan fixtures pass, including 28 old/new Machine comparisons
and two incompatible-build rejections. They cover IRQ reentry, entry-produced
edges, frame/budget ties, same-count resume, exceptions and missing targets.
The only initial fixture correction was pairing FAST_GEN with its prerequisite
FAST_CPU in the negative case; it was not a runtime defect. Forty-five Python
cases pass across capability/audit/packaging/default-generation and sound-link
checks. All four newly registered Release CTests pass; compile commands verify
assertions remain active in the core and Machine test targets.

Actual old/new Machine comparison passes 600,000,000 driving instructions,
1,365,742,448 cycles, 19,641,024 exact ordered attempted writes, 32,576 IRQs,
all nineteen input events and all 3,142 framebuffer comparisons. CPU registers,
cache, FP bits, observable device counters and saves match. Private event-queue
snapshots and sound are excluded from that harness. Two-million-instruction
boot and mixed-resume checks also pass. Driving evidence is under the worktree's
`build/validation/boundary-game-driving`; report SHA-256 is
`19b1e08a46ee141c680a96f984ae121aaf694fe0e3b3340db88b5ac8e18f36c1` and transcript
`b308f1b658b2b639b6e3f975818bf4f6a11b9e0ca08bec620f8af285a5e47db7`.

The separate raw independent CPU comparison also passes 600 million instructions
and is byte-identical to the previous v6 transcript, SHA-256
`329abd243d6299723a28ad2ca071da4b6cbcdbbe8da2bccb474127a98346ddb4`.
Its binary SHA-256 is
`04e3bc4d1c18fa8786ab901ee696eb6a961faeaa6e2a9f920a7effc5ef23a3cf`;
52 source/header hashes are recorded in the native build's
`evidence/raw-parity-600m/provenance.json`, SHA-256
`72b6cc4659ac670d53b4eb8f329fed6e7b81c310cb4aeb62a60c6b5b7975986d`.
Both comparisons still share board implementations and do not establish full
MAME-machine or physical-hardware fidelity.

The stable-GPU sound host under the worktree's `build/mac-boundary-sound-v13`
also passes the sound-enabled 600-million comparison against its old-loop
reference Machine. Main state and PCM match every resumed batch. The full
2,409,170-frame WAV is byte-identical to the previous sound capture, SHA-256
`7353902ba5af44d8af2a25b8f0b6fab033c3bd9544e1d18e35eba74ba2df8fc7`.
Its software and Metal images independently match their prior captures; they
still differ from each other. The executable SHA-256 is
`87a6aa3ecfe0b2748b132b8a192c8d975633ca903849184a37664089a9013da6`.
This run was unthrottled with no audio device; it does not prove playback pacing.
Next is a quiet, normal-paced real-output run through frame 2,760 with fresh
saves and the same input replay. Measure actual elapsed time and SDL input
demand before selecting the next windowed playtest; if the queue still runs
dry, profile the remaining runtime cost. The new path remains opt-in.

## Mac documentation and real audio output checks (2026-10-14)

The repository now has a wiki index, Mac player manual, build/validation guide,
technical architecture guide and evidence/status reference under `docs/`.
The root and dedicated-host READMEs point to them and state the active Mac-only
scope. Phone work is deferred. Commands cover verified import, exact indexed
generation, pinned SDL3 setup, isolated Mac builds, optional sound and bounded
parity. The 600-million-instruction example explicitly allows 1,200 seconds;
the harness's 180-second default is too short for the previously measured run.
All 47 local documentation links/anchors resolve, command paths and switches
were checked against the source, and whitespace checks pass. No external wiki
was published; no publication remote is configured.

The sound-off indexed v13 user window closed normally at 9,730 frames,
1,796,003,836 instructions, 4,228,699,002 cycles and PC `0x005a37a4`, result 0.
All ten selection events applied; manual takeover was frame 1,851. This log
does not establish which course the player reached.

A separate ten-second native race-audio excerpt was paced through the current
`AudioOutput` and the actual SDL/CoreAudio default output. It submitted 441,000
stereo frames with no host drops, no reported extra input demand and a maximum
queue of 4,017 frames. The sample used unity gain, peak 0.0672225952 and RMS
0.0104243613. Evidence and source/binary hashes are in
`build/validation/audio-output/results.json`. Counters were read before final
tail drain. The API accepted audio; physical listening remains unconfirmed.

The isolated signed sound package is
`build/worktrees/mac-audio-v13/build/mac-sound-v13/Sega Rally Native.app`, built
from `a4b8094` with stable fan graphics, frozen v13 indexed main code, 2,663
native sound instructions and eight verified assets. Strict bundle signing,
the audio fixture and help check pass. It has not been visibly launched.
Its hidden normal-paced real-device replay completes 2,760 frames with
529,303,426 instructions, 1,199,537,000 cycles and PC `0x005dde44`, matching
the prior main-state checkpoint. Eighteen replay events applied; the final
event at frame 2,760 remains pending because the bounded host exits there.

The live check produces 2,115,984 stereo frames, including 2,794,333 nonzero
channel samples, peak 0.0676727295 and RMS 0.00714519913. Chip and host sample
drops are zero, but SDL records 3,222 demand requests for 10,744,248 additional
input bytes; the queue ends at 774 frames. This establishes live presentation
starvation, despite the successful pre-recorded output check. The counters are
estimated input demand including resampler padding, not exact hardware missing
samples. A larger fixed buffer does not correct sustained execution below real
time. Evidence is `device-check.log` and `device-check.wav` beside the package;
precise wall-duration timestamps were not captured. Continuous live playback
and user hearing are not established.

Next: reduce exact main dispatch overhead, verify old/new Machine execution on
the actual driving replay, then repeat sound-enabled PCM and device checks.
An opt-in instruction-boundary hook is isolated in
`build/worktrees/boundary-hook`; strict/sanitizer synthetic tests and build-audit
tests pass, but real-game equivalence and performance remain pending. Its
generation must retain `--seed-profile lift-labels` to match frozen v13; an
initial proposed command omitted that flag and was corrected before generation.
The prototype is not included in this commit or either preserved playtest.

## Exact indexed dispatch and the next Mac playtest (2026-10-14)

`m2recomp --indexed-dispatch` now emits a sparse page/word index for exact
instruction membership and chunk selection. It replaces binary address search
and linear chunk scanning only. Every generated instruction, live RAM word
guard, cycle charge and boundary check remains byte-identical. Unaligned PCs,
displacement words, missing pages and other holes remain uncompiled. The flag
is opt-in in the Sega Rally pipeline, including fixed-point generations, and
cannot be combined with `--fast_inaccuracy`. Default generation is unchanged.
Ten bootstrap/routing tests pass, including exact holes, page/chunk boundaries,
high aliases and incompatible-mode rejection; independent code review found
no blocker.

The indexed v6 replay matches the independent CPU/device comparison through
600,000,000 instructions, 19,641,024 ordered writes, 1,365,742,448 cycles,
32,576 IRQs and 3,142 renders, hash `f6c10714158f5366`. All nineteen input
events match. Its complete parity transcript is byte-identical to the original
v6 report, SHA-256
`329abd243d6299723a28ad2ca071da4b6cbcdbbe8da2bccb474127a98346ddb4`.
Evidence is `build/validation/indexed-v6`; all fifty original instruction
objects were byte-checked before reusing them with the new routing table.
This remains independent CPU parity with shared board implementations.

The quiet hidden Metal race comparison improves from 33.8907 to 40.4319 FPS,
or 58.9156% to 70.2868% board/wall speed. For the same 480 frames, 87,173,426
main instructions and 8.344320 seconds of board time, wall time falls from
14.163164 to 11.871813 seconds and core time from 14.082547 to 11.789808 seconds.
That is 1.193x wall speed / 1.194x core speed; it is still below real time.
All 66 progress/input/exit rows are byte-identical. The existing hidden-window
and GPU-encoding measurement limits still apply. Full comparison and source/
binary hashes are in `build/validation/mac-profile/indexed-comparison.json`.

The frozen v13 held-gas run also completes its full two-billion instruction
budget without missing code or device errors: 4,650,936,033 cycles, 113,101
IRQs, 10,701 renders and final PC `0x005a37a8`. The capture shows the Lancia
back in attract mode with INSERT COIN(S), credit 0/2 and the Sega logo. Its
boot report SHA-256 is
`63e395e19955da2ea4653de79a3aac7609330823554e6959025aae2bb8c8ae74` under
`build/segarally-timed-v13/evidence/lancia-hold-gas-2b-complete`.
The Game Over overlay was not separately captured and a fresh second-credit
start remains untested. The independent-parity wrapper now permits an explicit
two-billion budget for future full-course comparison; its two-million default
and 180-second default timeout are unchanged. Argument probes verify bounds
and defaults; full-course independent parity has not yet been run.

The new signed app is
`build/worktrees/indexed-dispatch/build/mac-playtest-v13/Sega Rally Native.app`.
It contains frozen v13's 75,205 instructions / 52 native objects, all 51
instruction chunks verified byte-identical, plus the indexed table. Table
SHA-256 is `5c56f93d67d67e8e7a18802bd9e8347ea1de6bc536e5ad8729701772f4ef1e1f`;
app executable `06270afe9577f7a76b964f2dfb09ccbef7c3b12570a7b0981092dcbdd10efb61`.
It uses the committed stable frontend and fan renderer, normal pacing and
sound off. Asset hashes and strict bundle signing checks pass. After parity
passed, LaunchServices opened it in a normal window using the selection-only
replay and fresh `live-window-save`; `live-window.log` records the session.
Heavy probes/builds are stopped while the user tests. The older v6 app remains
preserved. Next: collect this playtest, preserve exact per-instruction semantics
while reducing remaining runtime overhead, and complete full-course parity,
audio playback and real-race GPU interpolation checks.

## Optional native SCSP audio capture and SDL output (2026-10-14)

The Mac host can now build with `SEGARALLY_SOUND=ON` and opt in at runtime
with `--sound`. Sound assets and generated CPU inputs are hash-audited before
linking, including incremental builds. The shared machine drains bounded
44.1 kHz stereo samples without advancing any CPU or device clock. SDL output
primes 60 ms, caps its queue at 240 ms, clears on lifecycle/frontier stops and
limits presentation-rate trim to +/-0.5%. Sound remains off by default.
`--audio-capture` writes exact float WAV; unthrottled diagnostics drain and
capture without opening an audio playback device.

The actual nineteen-event drive completes 600,000,000 main instructions,
1,365,742,448 cycles and 3,142 renders at PC `0x005a379c`. It captures 2,409,170
stereo frames (54.6297052 seconds), with 3,380,462 nonzero channel samples,
peak 0.08428192138671875, RMS 0.007353817195170514 and no chip sample drops.
Both machine instances agree on PCM at every drain; they share the generated
CPU and are not an independent CPU oracle. The software image is byte-identical
to the previous coupled driving capture, SHA-256
`7f3007acd63730959cab164e8833109fbb8217b183c0c62f588974a1b572c686`.

Ignored evidence is `build/segarally-mac-audio/capture-results.json`, SHA-256
`59e8782c2eb50d4994bed94f2be92b8abbbe153e91d1c122d471dba3fefef825`;
WAV `7353902ba5af44d8af2a25b8f0b6fab033c3bd9544e1d18e35eba74ba2df8fc7`,
capture executable `ac433056f5da6464d74b70fa2af3024157b42a6c28c1ea240ad1ab09b52f6d86`.
That executable predates the separately tested atomic SDL demand counters.
The counters observe estimated extra input bytes requested by SDL; resampler
padding means they are not exact missing hardware output sample counts.

Machine sample-drain and SDL/WAV fixtures pass strict and ASan/UBSan checks;
host assets pass ten tests and sound-link provenance passes six. Registered
root machine/machine-audio CTests and the dedicated Mac Release SDL CTest pass,
with assertions explicitly enabled for the new fixtures. Physical listening,
device pacing and whole-game audio parity are still untested. The sound CPU's
documented instruction-boundary MMIO limitation is unchanged. No audio probe
is active; keep sound off in the imminent speed-focused Mac playtest.

## Desert results, second course and measured Mac speed (2026-10-14)

Mac remains the only active platform. The design now records that current
scope explicitly; iPhone configuration is preserved without further device work.
Six actual execution frontiers extend coverage through the finish cinematic,
Desert results, next-stage loader and second-course time-up:
`0x005bf790`, `0x005bf880`, `0x005bf7c0`, `0x005bae40`, `0x005bc8d0`,
and `0x005bfff0`. Every observed row retains its input replay hash, original
execution profile, instruction/cycle counts and dispatch site. Static proofs
are unchanged: 116 table rows, ten continuations and sixteen registrations.

The held-gas replay displays DESERT STAGE CLEAR, rank 15th and clear/total time
1:12.15, with the Lancia close-up. The v9 result frontier occurs at
1,268,125,837 instructions / 2,947,096,677 reference cycles / 6,781 renders.
Its ignored evidence is
`build/segarally-timed-v9/evidence/lancia-centered-gas-005bf7c0`; boot report
SHA-256 is `6bc3f0a783df786a94836b92483eb39136a508e0a3a3a7b13f025730e03dc2fa`.
The v11 run enters the forested second course, then runs out of time at
1,659,787,795 instructions / 3,881,862,423 cycles / 8,931 renders. Its capture
shows the Lancia off the road at 19 km/h, total time 1:37.57 and lap time 25.42.
The replay does not steer through the second course's bends; this is observed
course entry and time-up behavior, not second-course completion.
V12 displays the large TIME IS UP lettering before its next actual callback.
Game Over and restart remain under test.

The frozen v13 profile contains 75,205 instructions / 52 native objects with
zero missing templates. Source-set SHA-256 is
`ddc5a290001d54544e0afa7004ce9aa82b08cdb8d26545abfb7f494b343fe88f`,
coverage `0eebd441903e486e82e8cbab925c758499dd021e128c7230bc68c41399a194bf`.
Earlier profiles, including the user-tested v6 app, remain preserved.

Quiet hidden-window Metal profiling measures 480 race frames (2280..2760) in
14.163164 seconds of wall time for 8.344320 seconds of board time: 33.8907 FPS,
58.9156% normal speed. Core execution is 14.082547 seconds, or 99.4308% of wall
time and 29.3386 ms/frame. Geometry takes 0.1525 ms/frame, TGP 0.0521 ms/frame,
and GPU CPU encoding 0.1222 ms/frame. Those nested core timings are not additive
to core cost; encoding is not GPU completion time. The run uses the stable fan
renderer, sound disabled, and normal board-time pacing. Hidden acquisition
does not measure visible VSync blocking. Evidence and instrumented source hashes
are in `build/validation/mac-profile/profile.json`.

An isolated exact instruction-index candidate replaces binary address search
and linear chunk selection while retaining every instruction body and timing
boundary. Its synthetic tests pass and all fifty v6 instruction chunks are
byte-identical, but actual performance and independent replay parity are still
pending; it is not integrated or in the playable app. A requested native stack
sample could not attach because approval arrived after its bounded process
exited; no stack result exists and no conclusion is based on one. Next: finish
Game Over/restart coverage, measure and validate exact dispatch, then native
audio presentation and real-race Metal comparisons. Complete MAME-machine race
parity, hardware FP precision and remaining-course completion are still open.

## Mac-first validation and finish-line coverage (2026-10-14)

Current user direction is to defer the iPhone build and concentrate on making
the decompiled game ready on Mac. Phone work is stopped; an isolated checkpoint
at `build/worktrees/iphone-drive` completed configuration only, without a new
device binary, installation or launch. Resume phone work only when requested.
The user reports that feel and visuals are good, with a slow sensation of speed.
Measure active wall time, board time, core execution and presentation costs
before changing performance behavior; the game clock is not a tuning knob.

The independent CPU/device comparison passes the full v6 driving replay:
600,000,000 instructions, 19,641,024 ordered writes, 1,365,742,448 reference
cycles, 32,576 IRQs, 3,142 renders and final hash `f6c10714158f5366`. All nineteen
input events match the shared machine's exact instruction/cycle boundaries.
The physical UART comparison observes 54,629,697 edges. Evidence is the frozen
profile's `boot-parity.txt` and `parity-input.json`; the test remains independent
CPU parity with shared board implementations, not full MAME-machine or PCB proof.

A second versioned replay, `config/segarally_lancia_hold_gas.inputs`, holds
centered steering and full gas after selection. Exact SHA-256 is
`c031b82a2c7b17a7fc33de2e86e7fe9f4afdfc423a65098bce6cf5507e9cab61`.
At one billion instructions the car reaches the mountain section and has
received a checkpoint time extension. The bounded extended run reaches the
FINISH banner at 229 km/h, fourth gear and race time 1:12.13, then stops at
actual callback `0x005bc680` from CALLX `0x005bba60`: 1,245,399,197 instructions,
2,894,016,626 reference cycles, 70,084 IRQs and 6,658 renders. This is finish-line
approach evidence, not completed results. The user's manual windowed run also
reaches this callback, at its own 1,104,021,132 instructions / 5,965 frames;
different input histories correctly give different execution counts.

The runner's explicit maximum budget increases from one to two billion
instructions, while its one-million default and existing 180-second timeout
through one billion are unchanged. Larger runs have a bounded 360-second
timeout. Python/native argument probes verify zero, negative and over-limit
rejection, two-billion acceptance, and unchanged defaults/help.

The separate v7 generation adds only the observed finish-line callback and
the reviewed leaf proof: 74,427 instructions, 51 objects, zero missing
templates, 116 table rows, ten continuation rows and sixteen registrations.
Source-set SHA-256 is
`111ee1a80f2469fec8c7c9b00e404b695a7f3bf6bf2f57cecac8b9ccbabf0a75`,
coverage `8975656db7c418b193cdc6ece286b09b15437dd6764b7809b6cebf3a1a8a10da`.
Next: finish/results and restart coverage, measured Mac performance, native
sound presentation, and Metal comparisons on real race scenes. Keep the
stable playable build separate from these diagnostics.

## Decoded returning-leaf callback proof (2026-10-14)

The scoped registration pass now tracks saved object aliases and constant
definitions through MOV and direct BAL calls to a proven returning leaf. The
leaf must consist entirely of reached, whitelisted straight-line instructions
and return by BX through its original incoming link. Exact decoded register
writes and BAL's implicit g14 write invalidate facts; there is no callee-save
ABI assumption. Known interior entries, missing bytes, unknown or nested calls,
branches and overwritten links reject the candidate. Every call keeps its own
leaf span, SHA-256 and clobber set in the generated audit.

The actual graph adds exactly two registration proofs, `0x005e29d0` from
constructor `0x005e2b20` and `0x005e4380` from `0x005e4550`, bringing the count
from 14 to 16. A second pass adds nothing. Both were already separately observed
targets; this improves justified coverage inference, not the frozen v6 code.
Their 60-byte leaf has SHA-256
`6196e10030caee5ecd5a1a586df152d4637972f853e6f9f8301c7d0b9a7470aa` and writes
g0, g1, g2, g4, g5, g6, g7 and g14. Actual proof report SHA-256 is
`a9ff17ef57817744942bb9f452f7da8d35cce59c0484266c7a077a73bd4e5b24`.

The review also corrected wide-load clobber footprints: LDL/LDQ round their
register base down, whereas EMUL writes its encoded destination and successor.
Treating an odd encoded LDL base literally could incorrectly preserve an
object or callback register. Mutation checks now cover those cases in both
the registration and table proof. CHKBIT's ignored destination-mode bit is
accepted because the executor does not write a general register.

All 25 proof tests pass under strict warnings and ASan/UBSan; the applied main
tree passes the same 25 tests plus eight bootstrap checks. Independent review
reproduced the two-target closure. Conditional diamonds, including the separately
identified `0x005ca740` case, remain unsupported. Next: use this helper in a
separate v7 generation for the newly observed finish-line callback, preserving
v6 and its ongoing independent driving comparison.

## Native Lancia driving and full-screen Metal launch (2026-10-14)

The frozen v6 generation reaches actual driving: 74,181 native instructions,
51 compiled objects, zero missing templates. Two actual entries extend v4:
`0x005e2828`, the reached return continuation after Lancia selection, and
`0x005c0d60`, the race callback reached after GO. These are recorded execution
frontiers, not broader inferred code-pointer rules. Source-set SHA-256 is
`4aa8f1552e2f56d7477d858cead503bf708c8b2b448d5200142a58829a065d06`,
coverage `74da94b686fd6fa703f2dd04ead968174f50f22a53ea6447be918313d86bedf6`.

The nineteen-event cabinet replay is versioned as
`config/segarally_lancia_drive.inputs`, SHA-256
`d7055e6747358f61509abbd49c449eea9437f8c11012aec5be52117190f5869c`.
All events complete at 600,000,000 main instructions / 1,365,742,448 reference
cycles / 32,576 IRQs / 3,142 renders, with no code or device frontier. The
capture shows the chase-view Lancia farther down the gravel course, elapsed
13.53 seconds and 8 km/h after brake release/coasting. A 500-million capture
during gas shows 160 km/h in third gear after left steering; the matched
centered-steering capture shows 222 km/h in fourth gear at the same 4.48-second
race time. Automatic gear changes and a steering-dependent trajectory are
observed. Evidence remains ignored under `build/segarally-timed-v6/evidence/`.
No game-state or generated-instruction patches are involved.
Matched 530-million-instruction captures isolate the brake ADC: gas is released
in both, while brake `ff` produces 113 km/h in second gear and brake `00`
produces 185 km/h in fourth at the same displayed race time of 7.22 seconds.

The linked sound profile adds only actually reached dispatcher target
`0x601852`, bringing it to 2,663 instructions / 61 combined seeds with no
template or timing gaps. It completes the same 600-million driving run with
39,901,316 sound instructions, 616,747,443 modeled sound cycles, 53,848 sound
IRQs and 11,750 main UART pin transitions. All 26 compared main/input/IRQ/frame/
TGP/status transcript lines and final PPM bytes match the sound-disabled run.
This does not enable frontend audio playback or establish full audio parity.
Evidence is `build/segarally-coupled-v7/evidence/lancia-drive-600m`; boot report
SHA-256 `4810eab3cd9f8727c479ec441c7a7b0f17e2c94b8d9f8786eb638551eb641f5b`,
sound audit `b7d1f0dcb030a08e2c474fb674f79fc729ce047e90a697ff86fa3a2dc08fb032`.

Independent v5 selection parity passes through GO and its then-missing callback:
451,072,711 instructions, 11,880,004 ordered writes, 1,012,631,901 cycles,
23,911 IRQs and 2,329 renders, hash `efb9df857cb6e7c8`. Every cabinet event
matches the shared machine's instruction/cycle boundary. This comparison uses
independent CPU instances with shared board implementations, not a complete
MAME-machine or PCB race. Next: extend this comparison through v6 driving and
exercise longer course, finish/results and restart paths.

The host and Mac launcher now accept `--fullscreen`. Strict host compilation,
Python compile and diff checks pass. A fresh signed Mac app at
`build/segarally-mac-v5/Sega Rally Native.app` was rebuilt against frozen **v6**
(the build-directory name is older), launched through LaunchServices with
Metal on Apple M5, desktop fullscreen and normal pacing. The selection-only
replay completed all ten events and handed control back at frame 1851; the
live log continued beyond GO to frame 2460 without a frontier. Z accelerates,
X brakes, arrows steer, V changes view, and Escape exits. The stable committed
GPU shaders remain in this visible app; the interpolation prototype is separate.
The app subsequently exited normally at frame 2618 / 503,836,827 instructions
with result 0; no missing-code or device failure was logged.
At the user's request the same packaged stable v6 executable was then opened
in a normal window, with fresh `live-window-save`, `live-window.log` and the
selection-only replay. Startup confirms Apple M5 Metal and progressing frames.
The new driving build has not yet been installed on the locked phone. Full-race
completion, physical timing and exact graphics fidelity remain unverified.

## Interactive replay and measured Metal interpolation difference (2026-10-14)

The SDL3 host and Mac launcher accept the same `--input-replay FILE` format.
The host holds the final snapshot for one completed frame, then returns to
current keyboard/touch input. The paired CPU renderer receives identical input
at every resumed step. A hidden three-frame press/release check applies frame
zero before execution, releases at frame one (285,550 instructions / 475,600
cycles), and hands over at frame two; GPU/CPU captures match exactly. Invalid
schema fails before SDL initialization. Strict host compilation, Python compile,
help and diff checks pass. Interactive Mac launches now use normal board-time
pacing; `--unthrottled` remains explicit. No visible app was relaunched here.

The frozen first-car scene contains 1,683 polygons for rendered frame 1,634;
the earlier 1,692 figure describes the next parsed vblank before another render.
An isolated rerender of its exact primitive and memory state reproduces the CPU
reference byte for byte. Changing only that diagnostic CPU renderer to use
vertex-zero triangle fans reduces GPU disagreement from 15,830 to 1,602 pixels,
and mean absolute channel error from 0.727817 to 0.040408 out of 255. Thus
whole-polygon versus triangle-fan interpolation explains 94.45% of measured mean
error. The production renderer has not been changed. CPU polygons with four to
eight vertices interpolate between outer scanline edges, whereas the inherited
GPU path splits all polygons into triangle fans.

Residual errors remain separate: 32 pixels exceed channel error 32, including
eight over 128. Three of those eight lie within 0.0014 pixels of a projected
edge; the others also require texture/alpha/precision investigation. This is not
exact raster parity or proof of physical hardware fidelity. Evidence is under
ignored `build/validation/car-interpolation`. Primitive-stream SHA-256 is
`45d4b1346131d3924743113b1e5d57089fd3dafceb0b49c998f0721b357c01ca`;
diagnostic harness `e75223f7070413ba572428ddd16bf6a7d628152a1909b4988ff5946ac882c4c8`.
Next: prototype whole-polygon texture interpolation in the common GPU shader,
preserving existing polygon order, tile passes and Metal rendering; compare
actual scenes and performance before changing the production default.

## Expanded object-update coverage for the race transition (2026-10-14)

The frozen v4 generation contains 73,945 instructions, 51 compiled objects
and no missing templates. Twenty-two audited tables contribute 116 candidate
rows, alongside ten continuation and fourteen object-registration rows. The
new scoped proof follows constructor/update roots, preserving the object in
g0 and an absolute callback constant through decoded straight-line operations
to a word store at object+4. The pinned caller independently loads that cell,
guards null, passes the same object and calls the loaded pointer. Known side
entries, base/value clobbers and unrecognized control flow reject the proof.

Accumulated registrations qualify an update root even if its address already
has an ordinary label; the initial fixed-point implementation missed that
case by only retaining roles on newly added seeds. A synthetic already-labeled
update chain verifies the correction. The final reached graph reproduces every
registration and continuation with provenance. Seventeen proof tests pass both
normally and under ASan/UBSan, and all eight table and eight bootstrap tests
pass before the next recorded replay result.

Actual frontier entries `0x005ca5b0`, `0x005e29d0` and `0x005e4380` are compiled
separately from inferred registrations. The earlier hypothesis that conditional
branches explained the last two missed registrations was wrong: their known
constructors save the object in r4 and use BAL to a returning leaf before the
store. The straight-line proof intentionally stops at that call and does not
track the saved object alias. Read-only evidence identifies a narrow leaf/alias
extension for the next iteration; it is not part of this checkpoint.

Source-set SHA-256 is
`19f05087de9ba5db3ad5e7601e4e1cefed204f5e0ecf241834561f47ea806d2a`,
coverage `098329d8657f7a65fa045b1634da022db89e93f729bf12d918336bed89469258`,
proof helper `048d8a91e9674d9bac26bf88cb154a6653f7ae2d0918d7488a45408d3f035b03`.
Generation success does not establish race entry. Next: run the exact versioned
Lancia-selection replay on this frozen profile, preserve its next frontier and
let the platform and sound lanes consume separate stable copies.

## Linked sound preserves the Lancia selection replay (2026-10-14)

Adding the actually reached sound entry `0x6016ce` from the known byte dispatcher
allows all ten recorded cabinet events to complete with sound attached. The
coupled machine reaches exactly the sound-disabled main frontier `0x005e4380`:
376,736,993 instructions, 832,720,369 cycles, 1,915 frames and pixel hash
`54fd87f7d2db4325`. Eighteen input/CPU/IRQ/frame/TGP/frontier transcript lines
match the sound-disabled replay. Sound runs 26,148,973 instructions, 376,043,201 cycles
and 30,949 IRQs; main UART has 214 pin transitions. The image is still the white
transition after selecting Lancia AT, not a running race.

Evidence is under ignored `build/segarally-coupled-v4/evidence/
lancia-at-main-5e4380`. Boot report SHA-256 is
`b58ae85eb53d5bbffca37616c7215b2a66917ba19bf1525f8e77319c57fed7a7`,
sound audit `7d5a3b2bfb2c975789377b822d7998de70c6031471a2d70e3f1a60d4fa161592`.
The exact replay SHA-256 is
`797df95862beda36fd9ab54f8c440b701897bc0561b0cf8630f29e30b4f25f6e`.
Next: continue the observed main update callback; sound and frontend playback
limitations remain as documented.

## Real coin/start and Lancia selection with independent parity (2026-10-14)

The frozen v3 core accepts real cabinet inputs and reaches Car Select. The
fresh cabinet needs two coins: the first probe visibly showed CREDIT 1/2,
so its Start pulse did not leave attract mode. Two separate coin pulses at
frames 1200/1220 and Start at 1240 reach the menu, captured at frame 1777.
Steering right at 1800 and pressing gas at 1840 selects Lancia AT; the coupled
sound lane's frame 1841 visibly highlights that option. The exact ten-event
schedule is now versioned as `config/segarally_lancia_select.inputs`.

The sound-disabled native run stops during the subsequent white transition
at missing callback `0x005e4380`, called from `0x005de478`, with g0 `0x00510a00`.
It reaches 376,736,993 instructions, 832,720,369 cycles, 19,541 IRQs, 1,915
renders / 1,916 parses and pixel hash `54fd87f7d2db4325`. Geometry first appears
at frame 1235; 665 frames contain geometry. Race entry and driving remain
unverified. Evidence is under ignored `build/validation/input-replay/`.

Independent CPU/device parity passes the exact replay through the same
frontier, checking 6,518,236 ordered writes and 33,308,814 physical UART edges.
All ten input events match the shared machine's instruction and cycle boundaries
exactly. The coupled sound run also matches the first nine event boundaries,
then stops at its newly observed sound callback `0x6016ce` just after the gas
assertion; the release event is still pending there. No game state was patched
and neither missing callback was skipped. Next: compile the observed callback
entries, proceed through race entry and make the live host replay this proven
selection sequence before handing control to the user.

## Coupled sound reaches the first extended 3D sequence (2026-10-14)

A separate copy of the 73,657-instruction v4 main profile, under ignored
`build/segarally-coupled-v4`, keeps the sound diagnostic independent of ongoing
main generation. One additional actually reached byte-table entry, `0x601682`
from `0x601662`, expands the sound profile to 2,635 instructions / 59 unique
seeds with no missing timing or templates. No new inferred byte-table bound
was introduced. Sound source SHA-256 is
`b7e39697f4e23f008e10c4131ac719f23b21d03dfc1f7c367084c86cc14af441`.

The coupled neutral run reaches main callback `0x005e29d0` after 397,603,063
instructions / 880,982,159 main cycles. It renders 2,027 frames, including
671 with geometry. Sound runs 28,781,289 instructions / 397,837,458 cycles,
32,818 IRQs and 1,554,053 sample frames, with 142 main UART pin transitions.
The final capture is still attract mode with CREDIT 0/2. It is not verified
driving or host audio playback. Evidence is in that copy's
`evidence/neutral-main-5e29d0`; boot report SHA-256 is
`d9c6c849fef831a8c8efd77d92d9ccc78a00cda54c668987192da3b9c1d6c7c5`
and sound audit `13e92c05a32fec4889afb9fb6f6d4e5c8687b5cfc68224aa0929a80b8aebd092`.
Next: exercise the exact two-coin/start/car-selection replay through this linked
board, while the main coverage lane follows newly observed object callbacks.

## Cabinet replay in independent CPU/device parity (2026-10-14)

The independent harness now accepts the same bounded cabinet replay, with
separate cursors for native and reference boards. It verifies the input pins
alongside CPU registers, ordered cycle-stamped writes, IRQs and device state,
and preserves exact replay bytes and a SHA-256. Strict harness compilation
passes. The v3 neutral frame 0/100/500 run agrees through 100,000,000 instructions,
1,719,294 ordered writes, 223,328,073 cycles, 4,585 IRQs and 513 rendered frames,
with final hash `c2b1e0b7602037ac`. Its applied events also match the shared
machine's exact instruction/cycle boundaries: frame 100 at 17,787,072 /
43,501,001 and frame 500 at 97,263,830 / 217,341,000. Ignored evidence is
`build/validation/input-replay/neutral-parity.log`. This is a neutral replay
check; coin/start behavior and driving parity still need the expanded profile.

## Sound dispatch coverage through the native title checkpoint (2026-10-14)

The coupled diagnostic now reaches the v2 main CPU's known `0x005caba0`
frontier after 266,518,444 main instructions and 588,916,392 cycles. Sound
continues to that point with 19,707,819 instructions, 265,945,223 cycles,
21,450 IRQ entries and 1,038,849 sample frames. The shared clock reaches
23,556,655,680 ns; 120 main UART TX pin transitions are observed. This is
native serial-linked execution, not audible playback or a verified race.

Three individually audited nibble dispatchers contribute 48 static candidates.
The decoder verifies mask width, shift direction/count, indexed address and
branch registers, all 16 jump slots, source/table hashes and known side entries.
Proofs are repeated against the combined reached graph. A separate byte-index
table has 256 possible indices but only 128 valid jump slots; it remains
observed-only, with no inferred seven-bit bound. Twelve actual observed entries
remain separate from the table candidates. The resulting 58 unique seeds
compile 2,595 instructions with no missing templates or timing.

Newly reachable DIVU/DIVS use pure cost helpers derived from the pinned public
M68000 microcode's 16-step paths, including signs and overflow. Independent
production-emitted comparisons pass 1,379,624 cases under ASan/UBSan, checking
exact cycles, results and defined flags. Overflow N/Z are undefined and excluded;
divide-by-zero exception delivery still stops explicitly. The broader suite
passes 11,551 instruction comparisons, 282 reset/IRQ phases, 1,166 sanitized
runtime checks, five table-proof tests with semantic mutations and six generation
tests. Daytona's default generated output remains byte-identical, SHA-256
`a0b69c9763dbd5560b58247049a41d4300d9926e19bf20a731cc76a6c61f1042`.

Evidence is under ignored `build/segarally-timed-v2/evidence/
sound-linked-main-5caba0`. Boot report SHA-256 is
`e4fbaa697620de97f7533d23bda5b012c15dd3b22fb11aad1a0e2d5e52471628`,
sound audit `1ac5418ce0a5f94a715bf5fe34c723a3fdcea6e4ed2496eb220186927cdc749d`,
generated source `233d070ac1d5f1fd7d446bc761aaba7ba6a3592e32e8bddbb624370468c215b0`.
All 26 audited source hashes match this checkpoint. MMIO retains instruction
boundary timing; prefetch values, bus phases and interrupt capture timing are
not proven. Next: couple this sound profile to a newer stable main generation
without overwriting frozen first-car evidence, then follow real gameplay inputs.

## Repeatable cabinet inputs for native gameplay probes (2026-10-14)

The bounded runner accepts `--input-replay FILE` with screen and I/O enabled.
Complete cabinet snapshots apply after explicitly numbered completed rendered
frames, before resuming the next frame. No game memory is patched. Input bytes
are frozen and hashed beside the execution report; each applied event records
its exact instruction/cycle boundary, and pending events remain visible when
execution stops. The format and coin/start example are documented in
`docs/segarally-input-replay.md`; the example is not a verified driving replay.

Thirty-eight strict and ASan/UBSan checks cover active-low press/release,
persistent state, every cabinet input byte, initial frame zero, duplicates,
skipped/backwards frames, malformed/range-limited numbers and bounded event
counts. CMake's input replay test and strict runner syntax pass. A frozen v3
neutral replay applies snapshots at frames 0, 100 and 500 and reaches exactly
the prior 100-million-instruction checkpoint: 223,328,073 cycles, 4,585 IRQs,
513 frames, PC `0x005a379c`, pixel hash `c2b1e0b7602037ac`. Evidence is under
ignored `build/validation/input-replay`; the existing v3 generation and its
saved first-car/parity evidence were not overwritten. Next: use real coin/start
edges with expanded callback coverage, and validate the resulting gameplay path.

## Visible Mac Metal car scene and packaged launch (2026-10-14)

The frozen v3 core now runs in a normal Mac application and leaves the last
car/course frame visible at missing callback `0x005e4980`. The verified asset
package belongs in `Contents/Resources/assets`: SDL's bundled base path does
not point at `Contents/MacOS`. The first bundle failed for that reason; a
detached process also disappeared without a conclusive cause. The launcher
now uses LaunchServices, keeps a startup log, activates an existing instance
without replacing its files, and ad-hoc signs newly packaged Mac applications.
The current live instance was left open during hidden comparison runs.

At rendered frame 1,634, separate Metal/CPU instances agree on 320,920,953
instructions, 710,177,427 cycles and PC `0x005aae90`. Their pixels do not yet
match: 15,830 of 190,464 RGB pixels differ (8.31%), with mean absolute channel
error 0.7278/255. There are 839 pixels with a channel error over 32 and 406
over 64. CPU hash is `04dde2bc10bbfabe`, GPU hash `cb05ac08103a1d01`. Actual
paired images and difference/metrics are under ignored `build/segarally-mac-v3`.
The apparent car fidelity is not a claim of exact rasterization or full-race
correctness. Texture and edge differences remain under investigation.

The host adds `--hidden` for bounded Metal captures and `--log` for startup
diagnostics. Strict host compilation, seven packaging tests, Mac/iOS Release
builds and strict external verification of the signed v3 iPhone app pass.
The new phone artifact has not been installed or relaunched since the locked
device denied launch. Next: extend callback coverage, exercise actual coin/start
inputs, resolve measured graphics differences and verify phone gameplay.

## Independent CPU parity through the first car scene (2026-10-14)

The frozen v3 native core and independent CPU/device instances agree through
321,115,347 instructions, 4,869,617 ordered cycle-stamped writes, 710,610,287
reference cycles and 16,545 IRQ entries. They stop at the same callback
`0x005e4980`. All 1,634 rendered frames / 1,635 parses and final pixel hash
`04dde2bc10bbfabe` match, as do 28,424,411 physical UART edges. This includes
the car/course scene. RX remains idle in this comparison; the diagnostic
sound CPU is not attached. Shared TGP/renderer code and reference timing
limitations remain outside independent full-MAME/PCB validation.

Reproduce with `python3 tests/test_segarally_boot_parity.py --build-dir
build/segarally-timed-v3 --instructions 400000000 --timeout 900 --tgp --io
--comm --control --video --screen --uart`. The live Mac Metal process has
also reached the same native frontier and keeps its last frame visible;
pixel comparison of the GPU path is being checked separately.

## First native car and course capture (2026-10-14)

The generated native core now renders the textured Martini Lancia, a second
rally car, forest, gravel road and sky. The actual CPU-renderer capture is
preserved under ignored `build/segarally-timed-v3/evidence/
first-native-3d-005e4980/boot-frame.png`. Its source PPM SHA-256 is
`1c1fc6f0a09cfa6d9758ff48237f41fdd4d529215790905de16f02a5135f3096`.
This is a first native 3D milestone, not a complete attract loop, playable
race, Metal comparison or phone capture.

The bounded run reaches 321,115,347 i960 instructions, 710,610,287 reference
cycles, 4,681,221 native TGP instructions and 1,634 rendered frames. Geometry
first appears at frame 1,357 (one-based); 278 frames contain geometry and the
last has 1,692 polygons. Execution then stops at missing callback `0x005e4980`,
called from `0x005de478` with RIP `0x005de47c`. It is not skipped or patched.

Eleven audited dispatch tables contribute 61 candidate rows. New unsigned
compare/branch proofs allow only frame spills preserving the index and flags;
classic proofs now check distinct bounded-index and loaded-destination
registers independently. Read-only scouting found further bounded tables,
but only the frozen generation's verified subset is part of this checkpoint.
Twelve decoder-proof tests, eight classic-table tests and eight bootstrap
tests pass; the proof suite also passes ASan/UBSan. All ten continuation rows
retain source provenance and reproduce against the final reached graph.

The frozen profile contains 68,953 instructions, 47 compiled objects and zero
missing templates. Native source-set SHA-256 is
`26ee00f68fcb1d38e354e5614f4b986656221acce3035f4a92924f4b51dcf840`;
coverage SHA-256 is
`803268bf29ebf20a93972065a4e3a007624e284816708e5f287493e30b4c6aab`;
proof helper SHA-256 is
`91a2ef4ace8ad442cf7a76445b52ec4f083d9586209240e10c291af792b043b0`.

Keep this v3 generation frozen while the Mac Metal comparison and new signed
iPhone build consume it. Independent CPU/device parity through this longer
scene is running separately. Next: show the live Mac build, compare the car
scene's Metal pixels, install on an available unlocked phone, and then extend
callback coverage from the preserved frontier.

## Timed sound core and optional shared-board diagnostic (2026-10-14)

The 68000 generator now supports explicit `--timing model2a-boundary` for
Model 2A. It accounts instruction totals, dynamic costs, mapped RAM/SCSP waits,
prefetch transfer counts, reset and phase-dependent autovector IACK against
the pinned public M68000 microcode. Unsupported timing and exception paths
stop before proceeding. MMIO remains at the instruction's start boundary;
prefetch values, individual bus phases and interrupt capture timing are not
modeled. These are diagnostic timing contracts, not PCB or whole-game parity.

Independent microcode comparisons pass 11,409 synthetic instruction cases
across 1,528 public decode patterns, checking total cycles, waits, bus counts
and ordered write address/data/masks. Another 282 comparisons cover two resets
and 20 starting phases across seven IRQ levels with RAM/ROM handlers, including
final PC/SSP and ordered stack writes. Strict and ASan/UBSan runs pass. Sixteen
prefetch-map frontiers and 151 odd-address exception cases are explicitly
excluded. The wider comparison caught upper-word bit-operation costs and
longword write order: timed read-modify-write and predecrement stores write
the low word first. Runtime guards and rational clocks pass 1,164 checks;
six generator tests pass. Default Daytona generation remains byte-identical,
SHA-256 `a0b69c9763dbd5560b58247049a41d4300d9926e19bf20a731cc76a6c61f1042`.

Actual timed Rally sound startup compiles 886 instructions and completes a
one-million-instruction bound at 13,096,293 modeled cycles, 383 IRQ entries,
35,955 SCSP writes and 51,158 sample frames. Sanitized execution produces the
same JSON as the normal run. Generated source SHA-256 is
`d1293a200f6d9039f2c903b1cd213edefaee4d546fb567e82c62a46c62f56d95`.
Observed IRQ seeds retain real vector-store evidence; no vector RAM image or
game reply is substituted. The original untimed reset probe still reaches
its first SCSP write after 278,301 instructions.

`Model2ASound` owns images, the bus, chip and native CPU. A shared-clock callback
executes exactly one instruction or one IRQ entry, then schedules its next
absolute rational deadline. Construction accounts reset without advancing the
caller's clock. Missing code and errors remain stopped; destruction cancels
all callbacks. Thirty-five scheduler checks pass strict, CMake and ASan/UBSan,
including real physical MIDI reception, DMA hold acknowledgment, equal-time
insertion order and DMA Hold followed by MIDI Assert on the same level. The
initial held-bit implementation was wrong for that last transition: Assert
must replace Hold, as pinned diexec does. A mutant retaining Hold fails with
`acknowledgment cleared ASSERT_LINE`. External peripheral callback exceptions
propagate through the shared clock; CPU-step failures also retain sound status.

The runner's opt-in `--with-sound` connects both serial directions to the main
UART and checks source, generated-code and asset audits before linking. Six
rejection tests cover untimed/unguarded/partial/stale code and path escape.
The Metal frontend and default runner leave this diagnostic disabled. A shared
machine completes ten million main instructions with 807,783 sound instructions
and 179 sound IRQs. The longer coupled run reaches main instruction 14,391,262
and sound instruction 1,259,939 before missing sound PC `0x600582`, dispatched
from `0x600778`. Sound cycles are 16,293,469 with 660 IRQ entries; shared clock
stops at 1,443,228,193 ns. Main UART is in mode `4e`, command `37`, with actual
TX edges. Evidence is under ignored `build/segarally-timed-v2/evidence/
sound-linked-600582`. Samples are bounded and drainable, but host playback is
not connected; undrained output drops and counts old frames.

The default-off 100-million run still exactly matches its prior checkpoint:
223,328,073 reference cycles, 4,585 IRQs, 513 frames and pixel hash
`c2b1e0b7602037ac`. CMake builds the shared generator and passes inherited
68000, timing, machine and sound scheduler tests. Next: compile the newly
observed sound entry, retain explicit timing limitations, and validate the
independent graphics lane's first native car capture through Metal.

## Verified callback and continuation candidates (2026-10-14)

The guarded native profile now contains 67,031 instructions and 46 compiled
objects, with no missing templates. The object callback table at `0x005ddfc0`
contributes 17 nonzero targets from 32 masked entries. An inherited-decoder
helper checks the mask, scaled load, saved object pointer, full widths of
intervening disjoint stores, reload and null guard. Null entries are excluded.
The previous seven-entry startup table still contributes three distinct targets.

The same helper derives ten adjacent continuations from explicit address loads
and register moves. It accepts a small decoded whitelist, rejects clobbers,
control transfers and known interior entries, and iterates only from the selected
seed profile. Every candidate retains a source hash and nonempty per-root seed
provenance. Unknown indirect side entries and exclusive target coverage are not
claimed. Live RAM instruction guards remain active. Six actual execution
frontiers are separately recorded; conditional paths use observed evidence,
without expanding the straight-line proof to unverified control flow.

Ten synthetic proof tests, seven existing table tests and eight bootstrap tests
pass. The proof suite also passes ASan/UBSan. Semantic mutation tests recompute
hashes, demonstrating rejection from operands/dataflow rather than hash alone.
Generation source-set SHA-256 is
`4eff7066f1a4fa54b2cb5a9cb8eafa51b91208728388a7a42fed678415ffe2c9`;
coverage report SHA-256 is
`e04965c6781f84931f58350b6ac69baf8314bfeb887254bd988be9e952de702b`.

The latest native run reaches 266,700,692 instructions / 589,330,734 reference
cycles and 1,355 rendered frames before missing `0x005af6f8`. The last capture
is a white transition after the title, with zero geometry frames. Evidence is
under ignored `build/segarally-timed-v3/evidence/native-005af6f8`. This is native
coverage progress, not a first-car or playable-game result. Next: audit the
new bounded dispatch table, continue toward 3D and compare its CPU/Metal frames.

## Independent title checkpoint and phone launch (2026-10-14)

Independent CPU/device instances now agree through 266,518,444 instructions,
3,573,076 ordered cycle-stamped writes, 588,916,392 reference cycles and
13,559 IRQ entries. The frozen v2 profile stops at the same missing-code
target `0x005caba0`; 1,354 rendered frames / 1,355 geometry parses and final
pixel hash `2e007e4918e0a776` match. All 23,556,655 physical UART edges match.
This extends the earlier 100-million parity checkpoint through the title
screen, with RX still idle and the sound CPU/chip unconnected. Shared device
implementations remain outside independent full-MAME/PCB validation.

Reproduce using `tests/test_segarally_boot_parity.py --build-dir
build/segarally-timed-v2 --instructions 300000000 --timeout 600 --tgp --io
--comm --control --video --screen --uart`. The runner now accepts an explicit
bounded timeout and up to one billion instructions for later checkpoints.
Machine/boot diagnostics also record current polygon count, frames containing
geometry and the first such rendered frame (one-based, zero if absent),
without changing execution or rendering. All-device strict compilation passes.
Nonempty geometry is not proof that the car is visible.

The installed native app launches and its retrieved log confirms scene startup
and `GPU driver=metal device=Apple A15 GPU`. A separate bounded GPU-capture
launch was denied because the phone had locked. No phone frame or gameplay
claim follows from initialization; there are no further retry loops. Local
device logs remain ignored. Continue native coverage and capture work on Mac
while the phone is unavailable.

## Metal frontend and first native iPhone installation (2026-10-14)

The standalone `platform/segarally` host uses shared Model2AMachine and the
unchanged Daytona GpuRenderer with pinned SDL3 3.4.16. macOS and signed arm64
iOS Release builds succeed. It requires Metal, provides touch/keyboard input,
stores EEPROM/backup images, clears touch on pause, restores the pacing origin
on foreground, and supports exact instruction/frame limits and GPU captures.
Controller handling remains future work. The earliest iOS log is available
as `Documents/segarally-native.log` through file sharing.

Two independent machine instances render frame 513 through GPU and CPU paths
at 99,846,823 instructions / 222,990,800 reference cycles, PC `0x005a37a8`.
All 496 by 384 RGB pixels match, with zero differing components. RGB SHA-256
is `1c05e1c4b0aa6db64ed1f4fbee89a57471d6706282b5f69f3e0a8004a1e264d3`.
This is the original startup warning, not a polygon/car scene. A separate
88-instruction budget test exits at exactly 88 instructions / 229 cycles,
PC `0x450`. Strict host compilation and seven asset-packaging tests pass.

Asset staging validates main/TGP/video audits, hashes, paths and current video
manifest before packaging. The first Xcode POST_BUILD path accidentally kept
a literal platform placeholder; fixing that path exposed an incremental-build
signature issue because copying after CodeSign changes the sealed bundle.
The final implementation stages verified assets as tracked Xcode resources
with a build dependency, preserving unchanged files. Final
`codesign --verify --deep --strict` passes. The app installs successfully on
the paired iPhone as `com.lgarciacarro.segarally.native` (Sega Rally Native),
separate from the prior prototype. Launch/capture verification is still pending.
Its frozen v2 generation contains 66,557 instructions; ongoing v3 discovery
does not modify those sources during platform compilation.

Next: verify phone startup, capture the first native 3D scene and compare its
Metal pixels with the CPU renderer. The new port is not yet verified playable.

## Native SCSP, DSP and physical audio link (2026-10-14)

The native SCSP implements 32 voices, sample looping/envelopes/LFOs, DSP,
masked registers, DMA, timers and MIDI against shared 512 KiB sound RAM.
It uses the common clock and an explicit pinned-reference random provider.
Stereo samples are drainable and bounded; overflow discards oldest frames
and counts them. IRQ callbacks retain clear/assert/hold rather than folding
all states into a boolean. The separate sound CPU probe still stops at its
original SCSP frontier; the new chip is not yet connected to the game.

Strict and ASan/UBSan unit runs pass 1,484 checks. Independent comparisons
against unmodified pinned `scsp.cpp`, `scspdsp.cpp` and `diserial.cpp` pass
44,160 checks, including 256 randomized DSP programs over 16 frames each,
32-voice PCM8/PCM16/noise, active full-chip DSP, all LFOs, sample buffers,
registers, timers and 256 physical MIDI frames. The oracle has its own
attosecond scheduler and does not call the native chip algorithms. Preparing
the six pinned source/header files in an ignored include directory plus empty
`emu.h`, `dirom.h`, `logmacro.h` allows `test_model2_scsp_reference.cpp` to
compile separately; exact source hashes are in `THIRD_PARTY.md` and the local
`build/validation/scsp-reference-audit.json`.

A separate integration fixture wires the actual native UART and SCSP serial
pins through one queue. It passes all 256 simultaneous bytes in each direction,
checks receive IRQs and rejects early completion; strict and ASan/UBSan runs
pass. No game response or sound CPU is mocked. Normal CMake targets cover the
chip and physical-link fixtures.

The first sample-period proposal (starting at 22,676 ns) was wrong: pinned
`disound.cpp` generates sample zero for any positive time. A subsequent
strictly-after-every-edge proposal also failed exact integral boundaries.
The implementation now starts at 1 ns and uses ceiling rational deadlines
thereafter, with zero-time update doing nothing. This proactive queue is not
the complete lazy MAME stream/timeslice/speaker pipeline. MVOL is applied to
each produced sample; downstream resampling and lazy gain routing are outside
comparison. Reference imperfect envelopes/noise/DSP, immediate DMA and missing
1Fs IRQ remain explicit. Cold monitor/DMA latches are zeroed in both fixtures
where the source constructor does not explicitly initialize them.

Next: establish sound 68000 cycle costs, bus wait/prefetch and interrupt timing,
then connect the generated sound program and audio output to the machine.

## Shared resumable machine (2026-10-14)

`Model2AMachine` now owns the CPU, bus, shared clock and optional devices.
The bounded runner is a thin wrapper around it; the platform host can use the
same implementation. It supports relative instruction budgets, run-until-frame,
input state, pre-start EEPROM/backup loading and persistent stopped frontiers.
No extra instruction executes when a caller resumes after missing code or
unsupported hardware. External GPU mode selects the existing Video contract.

Both the extracted runner and repeated frame stepping exactly reproduce the
frozen 100-million checkpoint: 223,328,073 reference cycles, 4,585 IRQ entries,
513 frames, PC `0x005a379c`, screen hash `c2b1e0b7602037ac` and 19,074 TGP
instructions. Strict all-device compilation passes. A synthetic lifecycle
fixture checks budget resumption, persistent stops, partial failing-instruction
cost, saved-state size validation and load-after-start rejection; direct and
CMake runs pass. The original independent parity harness remains separate.

Cloning pinned SDL3 made the inherited top-level build discover its windowed
launcher, which then required absent ImGui. `M2_BUILD_WINDOWED_APP` now gates
that inherited target, defaulting on; the headless verification build sets it
off. This keeps optional platform dependencies from blocking device checks.
Next: finish the dedicated Metal host and signed asset packaging, then verify
actual 3D scenes. Startup CPU/Metal tile equality is not polygon/race parity.

## Bounded table and title callback coverage (2026-10-14)

The seven-entry table at runtime `0x005af85c` has three distinct targets:
`0x005af878`, `0x005af88c`, `0x005af8a0`. A checked unsigned bound rejects
indices above six, followed by a four-byte scaled load and indirect branch
through the same register. The new manifest stores addresses/hashes only.
Generation independently checks operands, code/data spans, relocation aliases
and exact target contents. Seven tests include semantic mutations with
recomputed hashes; the eight bootstrap tests also pass.

Native execution then exposes real entries `0x005d0700`, `0x005caba0`,
`0x005e4550`, and `0x005e4920`. These remain observed seeds, distinct from
table candidates and prior label annotations. The latter callback was reached
after 266,519,130 instructions / 588,918,441 reference cycles. The latest
capture remains the original title screen at 1,354 rendered frames; it does
not establish attract or driving. The guarded native profile now contains
66,597 instructions / 46 objects, 402 unresolved indirect sites, 13 invalid
decode stops, no missing templates and no unmapped direct targets.

The frozen `build/segarally-timed-v2` profile has 66,557 instructions for
the first Metal/iPhone build. Further discovery uses fresh verified imports
in `build/segarally-timed-v3`, avoiding concurrent changes to compiled app
sources. The current v3 aggregate generated-source SHA-256 (sorted filename,
NUL, bytes) is `fcfd0d874697ebb1daa0e0743e5e85796cce5f7fb4d8f42ee642392d19461037`.
The original `build/segarally-timed` remains the independent 100-million
instruction baseline. Continue collecting actual missing entries without
patching game instructions, fabricated return values or unchecked table scans.

## Native sound CPU reset checkpoint (2026-10-14)

The shared 68000 generator now supports an explicit `--board model2a` map and
generation audit. The verified Rally program produces 740 native instructions,
two indirect sites and no missing templates. It executes 278,301 completed
instructions from PC `0x600100` / SSP `0x5000`, clears its own 512 KiB RAM,
then stops at PC `0x600184` on SCSP write `0x100400`, data `0x0300`, mask
`0xff00`. This is the expected unsupported-chip frontier, not working audio.
The reproducible command is `scripts/segarally_sound_recompile.py` after the
verified sound import; its report lives under ignored `sound/native/audit.json`.

The optional sound CPU address-space interface preserves the inherited default
map. Model 2A supplies exact big-endian RAM, program, sample and SCSP lanes,
including explicitly ordered high/low longword accesses. Reset copies only
16 bytes; copying the whole vector table would bypass the game's own setup.
Generated instructions guard all their live words, including writable RAM.
Unmapped devices remain typed frontiers and no sound replies are fabricated.

Validation passes 66 bus checks, 57 inherited 68000 checks and four generated
synthetic tests covering RAM mutation, reset size, byte lanes, budgets and
default-map compatibility. Bus and actual generated reset paths pass ASan/UBSan.
Repeated Rally generation is deterministic. Actual Daytona generation remains
byte-identical at 1,916 instructions, SHA-256
`a0b69c9763dbd5560b58247049a41d4300d9926e19bf20a731cc76a6c61f1042`.
This probe has no sound CPU clock, wait states or asynchronous IRQ delivery.
Next: implement and independently compare SCSP, then establish cycle-based
sound scheduling and connect its real serial pins to the main UART. Do not
reuse Daytona's Model 1 board or its empirical instruction-rate timing.

## Native UART and first visible startup screens (2026-10-14)

The timed i8251/uPD71051 path now executes the actual control recovery sequence
`00 00 00 40 4e 37`: idle sync programming, reset-to-mode, then 8N1 at x16.
Physical clock edges begin high at 1 us and low at 2 us, alternating thereafter
from the shared queue. Ready pins feed the existing IRQ controller; RX remains
idle because SCSP is not connected. No sound acknowledgment is manufactured.
Actual synchronous serial activity is fenced; async reference limitations are
documented in the source. Strict and sanitizer suites pass 49,152 transmit and
49,152 receive frames over 192 formats, plus reset, errors, pin/buffer behavior
and exact Model 2A bus lanes. Dedicated CMake tests pass.

Independent native/reference startup matches 100,000,000 instructions,
1,719,294 cycle-stamped writes, 223,328,073 reference cycles and 4,585 IRQ
entries. All 8,933,122 physical UART edges and output/ready events agree, as do
full chip/control/queue state and final video RAM/pixels. Both render 513
frames with screen hash `c2b1e0b7602037ac`. The capture is the game's Japanese
region startup warning, visually inspected. The main CPU has uploaded and
executed its real TGP program (19,074 native TGP instructions at this point).
This extends the earlier separate upload probe into actual main-game startup.

Longer native runs discover two actual indirect entries, now recorded in the
observed-seed manifest: runtime `0x005ab6a4` after 182,304,560 instructions,
then `0x005ae9c4` after 182,692,288. The separate ignored
`build/segarally-timed-v2` profile contains 66,537 instructions / 46 objects;
the old `build/segarally-timed` retains the 100-million parity baseline.
The new profile reaches the Sega Rally Championship 1995 logo, captured and
visually inspected, then stops at missing target `0x005af878` after
266,510,626 instructions / 588,905,944 cycles / 1,354 rendered frames.
That target comes from an executed, bounded seven-entry jump table at
`0x005af85c`, dispatched at `0x005af858`; table analysis is next. The longer
logo run is native evidence, not yet an independent CPU parity checkpoint.
Capture output remains ignored (`--capture-frame` writes `boot-frame.ppm`).

The SCSP sound importer verifies all five sound ROMs, including the supplied
`epr-17890.30` alias matching canonical `epr-17890a.30` by CRC32/SHA1. Portable
images use big-endian bus byte order. Reset copies exactly 16 ROM bytes to
512 KiB sound RAM, producing SSP `0x5000` and PC `0x600100`; the program itself
must install the remaining vectors. Ten importer tests pass. Sound CPU and
chip clocks are 11,289,600 and 22,579,200 Hz respectively. These images do not
make Daytona's Model 1 sound board compatible; native SCSP execution is still
missing. The existing 68000 generator needs Rally's actual ROM map and an
explicit memory adapter before reuse.

## Screen, timer and video startup checkpoint (2026-10-14)

The native core now reaches its sound-UART initialization at PC `0x005c4ce0`,
write16 zero to `0x01c80002`, after 14,386,245 instructions and 36,071,853
reference cycles (1,442,874,120 ns). The shared Video renderer has completed
82 startup frames; geometry parsing has begun 83 frames. The last screen hash
is `528ac52fdc1e6325`. The main CPU has not yet uploaded the TGP. This is a
bounded native startup checkpoint, not attract mode or gameplay. The UART is
the next actual access frontier; SCSP audio execution is also still absent.

The video adapter uses exact ranges, mirrors and byte lanes, with stable RAM
shared by the inherited Video renderer. All four tile aliases invalidate the
renderer; texture writes retain the reference's maskless halfword packing.
CRTC offsets, palette/translation notification, render-mode frame readback,
polygon count, Z clipping, texture/luma RAM and framebuffer storage are mapped.
Direct framebuffer test-mode presentation remains an explicit error. Nine
verified graphics inputs produce 32 MiB polygon, 16 MiB texture and 2 MiB
video-table images; padding and interleaving were independently reconstructed.
The video-table image remains unused analysis input. Import tests cover bad
size/checksums, duplicate names, aliases, lanes, overlap, bounds and output
confinement; all seven pass. No derived images are tracked.

One event queue now orders screen, timer and delayed-mask callbacks by deadline
and insertion. The host synchronizes it before every bus access and CPU
boundary, guards callback reentry, and defers CPU IRQ line handling to a
boundary. IO/EEPROM callbacks see the event cursor, not a later CPU timestamp.
Screen phase follows pinned MAME: first begin 17.384 ms, first end 19.024 ms,
frame period 17.384 ms, blank duration 1.640 ms. The begin latch precedes
conditional geometry parsing, vblank IRQ and comm tick. End renders before
incrementing the screen counter. These replace the previous frozen frame
counter, which held the core in its frame-bit wait at `0x005a37a8`.

Reference timer details are retained: full 32-bit countdowns, 40 ns ticks,
80 ns delayed enable, last-read latch merging, active-mask replacement,
maskless ACK, retained requests on disable and pending enable surviving soft
reset. Treating ACK as `data | ~mask` would incorrectly preserve other lanes.
The independent pinned-handler differential passes 1,777,563 comparisons over
250,000 operations. Clock tests cover 12,000 randomized queue operations,
50 exact frame phases, equal-deadline insertion/replacement, reset, lifetime,
overflow and reentry. Strict and sanitizer tests pass. CMake device tests and
the bus callback/deferred-IRQ integration fixture pass.

Before screen events were connected, independent native/reference engines
matched 4,000,000 instructions, 1,138,135 ordered cycle-stamped writes and
10,593,743 reference cycles with independent control/video state. That frozen
checkpoint does not prove asynchronous-screen parity. With screen events now
connected, independent engines also match the actual UART frontier above:
14,386,245 instructions, 1,164,002 ordered cycle-stamped writes, 36,071,853
cycles, 83 geometry parses and 82 rendered frames. CPU/control state, pending
clock tokens, queued IRQ edges and frame phases match after every step; full
video RAM and rendered pixels match at the frontier. No IRQ has been entered
yet in this startup segment. Full MAME/PCB/race parity remains unverified;
absent auxiliary ROMs still prevent a complete
reference-machine trace. The shared CPU reference renderer is used for this
diagnostic path; Metal integration follows actual native startup.

Do not re-propose a first vblank at 15.744 ms (visible-line duration), a
startup end at 1.640 ms, per-register time advancement, a synthetic ready UART,
or importing Daytona's game-specific sound board. All would change the
contracts established here.

## Timed native startup and communication registers (2026-10-14)

The generator emits pinned-reference instruction cycle costs before opcode
effects, after budget checks and RAM-code guards. Shared call/return services
charge 9/7 cycles, including interrupt entry/return. The CPU's elapsed counter
is separate from completed instructions and remains monotonic through reset
and IAC reinitialization. The independent interpreter retains its own fixed
deduction sites and supplies the same elapsed clock. At the reference 25 MHz,
EEPROM timestamps now advance by 40 ns per reference cycle. Unknown costs fail
generation; boot/parity scripts reject stale generation metadata lacking
`reference_cycle_accounting_version=1`.

All 66,523 instructions / 46 objects were regenerated in the ignored
`build/segarally-timed` directory. Native/reference startup with independent
TGP, I/O, EEPROM and communication instances agrees after 1,531,005 completed
instructions, 367,943 cycle-stamped writes and 2,893,143 reference cycles.
Both stop at PC `0x005c5698`, writing `0xffac` as 16 bits to horizontal video
sync register `0x01040000`. The cycle total includes the attempted frontier
instruction's cost. This is 115,725,720 ns on the diagnostic reference clock;
it is not a measured arcade startup duration. The main CPU still has not
uploaded the TGP at this frontier; its separate 108-instruction probe passes.

Connecting time changes the EEPROM path: the frozen checkpoint reached the
communication access after 1,334,542 instructions, whereas the timed run
reaches it after 1,530,994 instructions with the same 367,940 writes. Both
native and reference follow the longer path with equal cycle timestamps.
The earlier shorthand describing this communication access as a read was
incorrect; it is a byte write, now explicit in parity diagnostics.

Compiler timing metadata passes 660 comparisons over all 164 reference
opcodes. The generated template suite passes 14,496 state/cycle checks.
Generated MMIO, burst accesses, call/return spills, interrupt helpers,
reset/IAC continuity and overflow fixtures pass under ASan/UBSan. Eight import
and mapping tests pass. CMake device/timing targets also pass. These fixed
costs retain MAME's estimates and absent burst-stall continuation; full-machine
scheduler and PCB timing parity remain unverified.

Model2AComm reuses Daytona's unchanged CommBoard simulation with exact 16 KiB
shared RAM, its mirror, CN/FG byte lanes and reference readback. Permanently
closed transport endpoints preserve the reference's no-peer initialization;
a null transport would skip it. The adapter never opens sockets or fabricates
link-up. MAME's pinned header enables this simulation, but no Z80/HDLC firmware
is executed. Mask/mirror/reset tests, sanitizers, the inherited communication
tests and 819,250,000 no-peer comparisons against pinned source methods pass.
The bus marks shared communication RAM as BURST and keeps flags non-burst.

Next parallel work is the exact video memory/register adapter using the shared
Video renderer, the IRQ/four-timer controller with delayed mask updates, and
verified polygon/texture ROM import. These remain bounded bring-up steps;
full startup, attract mode and racing are not yet verified in the new project.

## Native I/O and EEPROM checkpoint (2026-10-14)

The 315-5649 register device, Rally cabinet wiring and 93C46 EEPROM now run as
native components through Model2ABus. The bus accepts multiple independent
device adapters so I/O and TGP coexist. Byte lanes, direction/latch behavior,
analog and counter auto-increment, EEPROM pin order and input mux follow the
pinned reference. Neutral controls are explicit editable input values. Drive
commands and lamps are latched; no missing drive CPU firmware is substituted.

With `--with-tgp --with-io`, native startup reaches PC `0x005abe00`, byte write
zero to communication CN mirror `0x01a14000`, after 1,334,542 instructions.
Two independently instantiated native/reference device sets agree after each
instruction on architectural state, I/O state, EEPROM image and 367,940
ordered writes. The reference CPU is the inherited interpreter over the same
native board implementation, not full MAME. This checkpoint freezes device
time at zero: it cannot establish EEPROM startup or busy-completion timing,
and its next task is per-opcode CPU reference cycles and board timing.

Chip tests sweep all 256 direction masks, analog selectors and 65,536 counter
values. Adapter tests exercise real masked bus accesses, unconnected byte
lanes, reset, cabinet inputs and a serial EEPROM read. EEPROM tests cover all
64 words, seven commands, disabled writes, partial aborts, edge cases, reset,
optional streaming wrap, exact busy boundaries and persistence. Strict and
ASan/UBSan runs pass. An independent ignored harness extracted hash-checked
MAME event/decode/read/write methods and passed 30,638,679 comparisons over
60,000 randomized transactions across both streaming modes.

Reference caveats remain explicit: the chip's serial status is a fixed MAME
shortcut; 93C46 programming begins at the final serial bit in MAME, while the
modern ST datasheet begins it on CS falling; WRAL ANDs existing cells; fixed
busy durations are reference estimates. Clock polling never advances time.

Clean pinned SoftFloat, ymfm and LZMA dependency repositories were locally
cloned from the Daytona checkout into ignored `extern/`, with editor Git
exclusions. CMake configured with Unix Makefiles because Ninja is unavailable.
Dedicated CMake targets cover the new devices. Next: connect the CPU cycle
clock, implement the communication register interface, and follow actual
startup into the remaining hardware without inventing ready responses.

## Guarded RAM code and native Rally TGP (2026-10-14)

The expanded generation profile compiles 66,523 native instructions into 46
objects, with zero missing templates. Its 403 prior-lift label candidates are
kept distinct from one observed indirect target (`0x005d0710`, canonical ROM
`0x31710`). This target was initially assumed present in the broader profile;
an explicit `has_code` audit and native run disproved that assumption, so its
actual execution evidence now seeds both profiles. There are still 400
unresolved indirect sites and 13 invalid decode stops; this is static coverage,
not full-game reachability. The six labels without matching source metadata
are excluded, not guessed.

The real reset copy is analyzed as a relocation, with each generated RAM
instruction checking its live first and optional second word. Relative branches
and loads keep their guest RAM semantics. A changed instruction throws before
execution. The stricter reachability mode also rejects a missing displacement
word rather than inventing zero; Daytona's existing callers keep their default
behavior. Eight bootstrap/mapping tests, independent synthetic relative-code
and mutation tests, and 14,496 native/reference opcode comparisons pass.
The inherited reachability test also passes all 19 compatibility checks.

The new bounded native/reference harness compares architectural registers,
FP/cache/control/IRQ state and every ordered memory write after each completed
instruction. With independently instantiated native TGP boards, it passed
1,183,027 instructions and 361,067 writes using the same bus implementation
on both sides (not independent hardware parity). The native boot
then stops at PC `0x005a0e34`, byte write `0x11` to the mapped I/O chip at
`0x01c00000`. The preceding legacy-DPRAM probe at `0x01c00202` is genuinely
unmapped on Model 2A; its reference zero/discard behavior is counted. Initial
buffer and backup SRAM now match pinned reset contracts. IRQ acknowledgments
and zero-mask reset writes are supported; a nonzero enable still stops until
asynchronous board timing is implemented.

Rally's actual TGP upload is 1,665 words at program offset `0x5ae94`, count at
`0x5c898`, CRC32 `072484b1`. Daytona's unchanged TGP generator produces native
code: 1,663 words use implemented instructions and 735 are reached by constant
flow. The startup probe uploads through Model2ABus and the inherited
TgpBoardModel/TgpBoard, sends the real first function-port command, and gets
response `8` after 108 native instructions, with output FIFO empty afterward.
This is not full command-stream parity. All table/data word lanes, hashes,
aliases, and padding were independently checked. Six synthetic importer tests
cover invalid hashes, alias conflicts, swapped contents, missing inputs and
duplicate names. `--with-tgp` links this code to the main runner; the current
main-core frontier precedes the upload, so it reports TGP booted=false there.

The prior prototype now defaults to the complete validated Daytona geometry
priority, culling, and palette contract. Sixteen matched real scenes keep the
roof decal stable (961 red pixels), window dark coverage at 976–981 pixels,
and identical sky regions. Header palette zero now yields a black shadow,
replacing the earlier guessed palette-256 tan patch. A raw-command comparison
of 141 objects / 3,370 face+attribute records has zero differences from
Daytona. The earlier claim of lost clouds was a mismatched-capture comparison
and is disproved. Priority/material/fillmap/Metal tests pass; the signed app
was installed on iPhone Lauren. Launch failed with iOSLocked, so on-phone play
of this correction is unverified. Generated apps and captures remain ignored.

Next lanes: exact 315-5649 I/O, 93C46 EEPROM, board timing, further observed
code targets, and full native startup. Installed MAME 0.289 cannot produce a
full reference trace without three absent required ROMs: drive `epr-17891.ic12`,
communication `epr-16726.bin`, billboard `epr-18022.ic2`. No local archive
contains them; they are fixed devices, not removable slots. The communication
program was initially mislabeled as SCSP BIOS; the actual SCSP program is the
present `epr-17890a.30`. No substitute ROMs or downloads were used.

## Sega Rally native bring-up (2026-10-14)

The next checkpoint follows the pinned reference's read-zero/discarded-write
contract for the observed unmapped boot gap at `0x00f80000`. The bus counts and
reports that compatibility behavior; adjacent gaps and actual unimplemented
devices remain explicit stops. PCB behavior is unverified. Tests cover the
zero value, discarded byte write, diagnostics, flags, and both gap boundaries.
Native reset now executes 1,182,186 instructions, clears work RAM, copies
program `0x1000..0x60fff` into RAM `0x005a0000..0x005fffff`, reinitializes the
CPU, and reaches an unresolved RAM code target at `0x005a2150`. Adding the
verified relocation to generation is next; aliasing that RAM as ROM would be
wrong. A one-million-instruction run ends inside the copy loop, so its limit
is not a complete boot or a hang.

The `sega-rally` branch preserves Daytona upstream history at `b33baeb28`.
The dedicated project is a research bring-up, not the older lifted-C phone
prototype in the sibling `segarally95-recomp` checkout. No publication remote
is configured. The first project checkpoint extracted the shared polygon
priority functions, documented the separate project, and excluded local ROMs,
captures, generated code, and apps from Git.

The verified Revision B (`srallycb`) manifest imports eight files from the
user's mixed `srallyc.zip`: 1 MiB of program in a 2 MiB zero-filled region and
12 MiB of data in a 36 MiB zero-filled region. The initial thirteen annotated
seeds produce 4,081 native i960 instructions in four compiled objects. There
are 71 unresolved indirect sites and ten invalid decode stops; boot-table
heuristics can seed data, so these figures do not establish complete coverage.

The generated core links with Daytona's Cpu/Lockstep services and the new
Model2ABus. It executes reset at `0x420` and stops after 88 instructions at
PC `0x450`, a byte read of `0x00f80000`. The bus currently implements only
verified memory regions and reports unsupported devices explicitly. The
bounded runner records this frontier and keeps `full_game_boot_verified=false`.
Generation and boot commands are in README.md.

Validation: bootstrap tests 5/5; generated floating-point and flushreg
templates versus the reference core, 10,344 checks; Model 2A bus tests,
including real Cpu unaligned accesses across a buffer mirror boundary. A
second import independently matched both ROM_LOAD32_WORD lanes and region
padding. The FP tests establish MAME host-double behavior only, not full
i960 extended-precision correctness. This diagnostic limitation is recorded
in the Sega Rally design and must be resolved before fidelity is claimed.

Next: establish the first boot-gap reference contract, expand code seeds from
verified addresses, and bring up actual Model 2A devices and the game's TGP
program. Attract/RAM parity, full-race replay, SCSP audio, and the new project's
SDL3/Metal phone integration remain incomplete.

What not to re-propose: Daytona's upper program-ROM alias is writable RAM on
Model 2A; the old lifted helper `0x310` is not the reset IP; the CGM asset at
`0x02879db0` is not the setup function at `0x00029eb0`; successful generation
is not a game boot. In the older renderer, splitting solids and textures by
camera depth caused flicker. An initial priority-only replacement stabilized
windows but hid a roof decal and introduced a tan ground patch. Full window,
Z-adjust, face, and material contracts need verification before installation.

The sections below are retained upstream Daytona history, not Sega Rally
completion claims.

## Scenery selection and polygon budget PR branch (2026-10-08)

Branch `scenery-budget` starts from upstream/main `82201ed`, separate from
the staged integration merge. Widescreen at the existing 16:10, 16:9 and
21:9 choices now appends nearby scenery cells omitted by the game's narrow
directional selection while retaining course exclusions and original order.
The default polygon allowance scales with view width and retains the higher
draw-distance allowance; the launcher offers Automatic and a fixed Custom
allowance. Native Default remains at 5,000. The per-board scenery state
prevents linked cabinets sharing a width or custom allowance.

The headless replay tools can exercise 32:9 scenery and budget behavior, while
the launcher remains capped at 21:9. The source commit's larger 32:9 capture
matrix is held for a later panorama PR because the sky boundary needs work.
The upstream frame pacing and mobile input loop were retained while adding
the budget call. The 32:9 headless replay is a development route, not a
launcher choice: a Deluxe '93 frame visibly shows the remaining vertical
sky-art boundary. The panorama PR should address that presentation and then
expose the ratio. Refresh this branch against upstream/main before opening
the PR if upstream has advanced.

Release tests in the isolated checkout: `scenery` and `app_config` CTests
pass. Fresh Deluxe '93 generated code built `m2run.exe` and `daytona.exe`.
Across 600 attract frames, native keeps the 5,000 budget and does not change
the original list; 32:9 uses 13,771 and expands the list on 416 frames. A
3,600-frame Deluxe '93 race uses 9,033 at 21:9 and 13,771 at 32:9, expanding
the list on 2,013 frames in both runs with no budget rejections. A 32:9
Custom 5,000 run retains the expanded list and fixes the allowance at 5,000.

Fresh Revision A generated code built `m2run.exe` and `daytona.exe`,
including all four new cost and budget-observer hooks. In a 3,600-frame
advanced-course run, native keeps 5,000 with no list change; 32:9 uses
13,771 and expands the list on
2,041 frames, raising observed peak object cost from 4,710 to 7,517. The
particular sampled frame still matches pixel-for-pixel, so it is not evidence
of a visible correction there. The original `race_to_end` missing-building
fixture does show the correction at frame 3,600: new selection keeps 25
cells versus 10 with old selection, and the two 32:9 captures differ in
20,483 of 524,544 RGB pixels. Both runs use the same 13,771 budget and have
matching CPU/TGP instruction totals and sound command count, isolating cell
selection as the cause. The local `roms/daytona93.zip` is a split set, so it
cannot be imported alone (it lacks the shared `mpr-16528.10`). The merged
`roms/daytona.zip` contains the complete Deluxe '93 set: the PR branch's
Deluxe '93 importer accepted it, and all ten imported images match the cache
used for recompilation byte-for-byte by SHA-256. Testers can select that
merged archive in the Deluxe '93 launcher. All caches, captures and NVRAM
remain ignored local files. Deluxe '93 `m2gpushot.exe` also built and completed a
3,600-frame Direct3D 12 headless 32:9 race capture: the output is 1366x384,
the allowance is 13,771 and the list expands on 2,013 frames, matching the
software replay's selection metrics. This is a GPU smoke test, not a proof of
software/GPU pixel identity or physical device behavior.

## Vita icon/LiveArea images, app name always "Daytona USA" (2026-10-12)

Images in `platform/vita/logo_livearea/` (icon0, bg, startup, template.xml; not
committed) go into the VPK when present, checked at configure time; otherwise the
Vita defaults. The home-screen name no longer depends on the renderer. Not yet
installed on a console (`platform/vita/HANDOFF.md`).

## Vita rebindable controls (2026-10-12)

Launcher Controls page: each game action (pedals, steering, shifts, start, coin,
views) rebinds to one pad button, swaps on conflict, saved in vita.cfg; sticks and
Select chords stay fixed. Effects volume default now 20. Host tests 35/35, Vita
compile checks pass, not tried on a console (`platform/vita/HANDOFF.md`).

## Vita music and effects volumes (2026-10-11)

The desktop launcher's music/effects balance on the Vita (Options, defaults 100/75
because the effects cover the music on the Vita's speakers); details in
`platform/vita/HANDOFF.md`. Not yet heard on a console.

## Shared runtime fast paths split from M2_DC_SPEED (2026-10-11)

`src/runtime/fast_paths.h`: M2_FAST_BUS (TGP ports before the page table,
remembered RAM pages), M2_FAST_TGP (FIFOs as a ring read directly by the TGP),
M2_FAST_CPU (register frames through work RAM; needs M2_FAST_GEN), M2_FAST_GEO
(block reads, culled polygons skip texture reads and lighting). M2_DC_SPEED turns
them all on (Dreamcast unchanged, not rebuilt here); video (16-bit PVR layers),
frame skip and M2_AL stay Dreamcast-only. The Vita turns the four on by default
(`build_vita.py --no-fast-paths` to compare). New for the Vita: the TGP FIFO
pointers are also set in the non-M2_DC_MEMORY constructor, and TgpBoard is not
copyable with M2_FAST_TGP. Parity: desktop m2run with and without all five flags,
race_basic and time_attack 6000 frames: same i960/TGP counts, interrupts, 120/120
frame dumps and (race_basic) WAV identical; ctest 18/18. Console (probe 1, LTO,
saturated race windows at the same 1,465 polygons/frame): 47.6 -> 50.4 emulated
fps; i960 code 7.02 -> 6.10 ms, TGP 3.62 -> 3.30 ms, core 1 np_s 7.5 -> 6.5 us per
polygon (`extern/perf/perf_probe_base.log` vs `perf_prob_modif_1.log`; the probe
clock read calibrated 0.705 vs 0.939 us, unexplained). Not yet measured without
probes. What not to re-propose for the Vita: `--fast-gen` (slower than
`--fast-inaccuracy`: per-instruction checks), fast_gen.py's rewrite_tgp (rejects
the --fast_inaccuracy TGP output; about 0.1 ms/frame to gain), M2_DC_SPIN_SKIP
(Vita race frames end in the idle loop at about 44k instructions, never at the
110k cap), the Dreamcast video paths (the Vita uses M2_VITA_RENDER_OPT's tile cache).

## Mobile upstream rebase and GPU startup diagnostics (2026-10-07)

Rebased mobile onto original alphanu1 upstream/main 0435a46; recovery branch
backup/mobile-before-gpu-20261007. Main/Vita/PSP refs are not modified.
The Xiaomi Mali-G715 report exits normally from SDL_main after GPU creation
fails, not an established native crash. API version alone cannot establish
support; no device feature dump or physical phone is available here.

Shared SDL_GPU creation now uses properties to opt out of unused shader clip
distances, indirect-draw first-instance and anisotropy. Audited game shaders,
samplers and pinned ImGui backend for usage. Depth clamping remains required:
the current pipelines disable depth clipping, so merely disabling that device
feature would be invalid. No shader fork or external dependency edits.
fail() now snapshots the SDL error, logs it and opens an SDL native message
box; dialog failure is logged too. Startup lists compiled GPU backends.
These frontend changes follow the design's Platform layer section.

Mock regression tests cover requested features, property/device failures,
cleanup and error-message preservation even when logging changes SDL's error.
Host app objects and mobile main syntax checks pass; GPU startup, pacing and
TestHold tests pass. Xiaomi runtime compatibility and dialog appearance are
not hardware-verified. Android build initially required matching ANDROID_HOME
and ANDROID_SDK_ROOT; mismatched paths are a local configuration issue.
Android assembleDebug succeeds with both SDK variables pointing to
/home/boucy/Android/Sdk. The arm64 APK passes ZIP integrity and apksigner
verification, contains libmain.so/libSDL3.so, and contains no ROM archives.
Touch-control and mobile ROM-path regression tests also pass.

## Original upstream and platform test-menu integration (2026-10-06)

Merged alphanu1/daytona-arcade-recomp main 1877da9 into the fork, not merely
the fork's origin/main. Recovery branch: backup/main-before-alphanu1-20261006.
Upstream adds the shared TestHold helper and launcher action. Mobile keeps
that shared action with wrapped help text on small displays; Vita exposes
the same one-shot action in its ImGui options, retains physical Test/Service
bindings and does not persist the request. Both ROM versions wait until
frame 240, hold Test for 173 game frames and release, per upstream tests.
Resetting an armed Vita game restarts the full hold; Reset Defaults cancels it.
Inputs and Platform layer sections govern this frontend-only integration.

All 15 targeted tests (13 Vita, pacing and TestHold) pass, as do standalone
touch controls and 13 Android-path plus two desktop ROM-file cases. Desktop
app objects build and mobile launcher/main syntax checks pass with M2_MOBILE.
Vita main_gpu.cpp cross-compiles for both the 1993 and Revision A targets.
Full ROM cross-rebuilding was stopped deliberately in favor of targeted
frontend compilation; no new VPK/APK/IPA or physical device validation is
claimed for this update. Vita and mobile branches are advanced to the
integrated main; PSP untouched.

## Vita and mobile integration into main (2026-10-06)

Rebased psvita-native-frontend and mobile onto fetched origin/main 91560cf.
Mobile's touch controls and iOS file picker were already ancestors of main;
its rebase therefore fast-forwarded without replaying commits. Vita replayed
13 commits, retaining the ImGui launcher, both ROM revisions and platform
settings. The only conflict was this handoff: both upstream licence notes
and platform history were retained. No implementation conflicts occurred.
Recovery refs: backup/vita-before-main-merge-20261006 and
backup/mobile-before-main-merge-20261006. PSP is untouched.

This merge integrates the rebased Vita work into main; mobile is already
included. Publishing uses an explicit lease on the old GitHub Vita tip,
with main and mobile updated by fast-forward in the same atomic push.
Device builds and hardware runtime testing are not repeated by this merge.

Validation: desktop app objects and the selected test targets build; all
13 Vita tests plus app_pacing pass. Standalone mobile touch controls and
Android/desktop ROM-file tests pass. The broad all-target build stopped at
test_fp with unresolved SoftFloat helper symbols in this empty-build-type
configuration; no floating-point implementation changes were made. Mobile
tests are standalone programs/scripts, not named CMake build targets.

## Vita main rebase and ImGui launcher (2026-10-06)

Rebased psvita-native-frontend onto fetched origin/main 50d6638. Recovery
branch backup/vita-before-imgui-20261006 retains the pre-rebase history.
Main and PSP branches are not modified by this work; no Vita push performed.

Replaced the GXM frontend's bitmap menu with pinned upstream Dear ImGui,
rendered through the existing vita2d context. All 28 Vita options, dual-ROM
launching, link settings, physical controls and saved settings remain.
Front touch operates rows, adjustment buttons and the scrolling settings
panel; D-pad selection scrolls into view. UI work runs only in the launcher,
pause menu and loading screen. No additional gameplay display buffering.
The adapter supports the uniform-tint primitives used here, not arbitrary
per-vertex colour gradients. Geometry fringe AA is disabled; font AA remains.

Rebase integration preserves the Vita native-sized backdrop stretch path,
widescreen CPU tile fallback and foreground caches, while adapting the HUD
polygon helper to upstream panel detection. Non-Vita external-renderer
margin handling is retained. Host desktop app compiles; all 13 Vita host
tests pass, including real ImGui draw generation against a mocked backend,
pool exhaustion and GPU shutdown synchronization. These checks do not prove
physical Vita rendering, touch behaviour or gameplay performance.
The Vita ImGui target disables its unused desktop shell-opening handler,
which otherwise links unavailable execvp/waitpid functions from VitaSDK.

Both VitaSDK GPU builds pass. Final dual-ROM package:
build/vita-enhancements/daytona_vita.vpk (01.24, Daytona Recomp ImGui).
ZIP integrity and ARM ELF checks pass; the bundled daytona.self SHA256
matches build/vita-revision-a/eboot.bin. Archive contains executables and
licenses only, no ROM archives. Physical Vita installation remains untested.


## Mobile touch controls and latest main (2026-10-06)

## Vita Revision A link and dual-ROM launcher (2026-10-03)

Rebased psvita-native-frontend with --rebase-merges onto fetched origin/main
9ad266b (three-computer link validation). Recovery branch:
backup/vita-before-link-20261003. Merge conflicts combined enhance and
comm_board in Vita runtime; preserved Vita wide margins and foreground
cache invalidation while accepting upstream scene detection. Main and PSP
branches are unchanged; nothing pushed.

User supplied roms/daytona.zip alongside daytona93.zip. Revision A import
passes CRC validation; its i960/TGP/sound code is separately generated under
build/revision-a-host, never committed. GXM build selects DAYTONA_VITA_ROMSET;
the daytona93 VPK can bundle Revision A's SELF as app0:daytona.self.
Options ROM selector + Start/Reset replaces the process using LoadExec.
Each executable uses its own ux0:data/<set>/<set>.zip, vita.cfg, EEPROM and
backup RAM; existing 1993 saves are not repurposed as Revision A saves.

Vita SceNet IPv4 transport implements the shared TCP ring protocol with
nonblocking connect/accept/read/write, partial-send retention, a 64 KiB
bounded queue, reconnect delay and two-second no-progress timeout. Network
modules/heap are owned only where initialized here. Options expose enable,
four next-IP octets, local/next ports and optional frame sync (off by default).
Reset applies changes; pause status shows local IP, RX/TX and cabinet ID.
1993 link requests are rejected explicitly; Revision A is required on peers.
Master/slave, unique car numbers and matching cabinet/region settings remain
the game's test-menu choices. Native audio now tolerates unsupported effects
as upstream does (linked vibrato omitted), while invalid data still faults.

The host integration uses the real Vita transport through a narrow POSIX
SceNet shim against desktop TcpLink: numbering and data both directions,
37-byte partial sends, stalled-peer loss, reconnect and queue overflow pass.
This is not evidence of Vita Wi-Fi, executable switching or a hardware race.
Those remain the next device checks. CPU500/core options, road renderer,
wide CPU tiles and removal of the buffer selector remain preserved.
Address/undefined sanitizer execution of the socket integration also passes.
Host suite after rebase: 24 tests pass, two optional Lua tests skip.
Both ARM release executables and SELF/VPK builds pass. Final dual package:
build/daytona-vita-dual-rom-link.vpk. Archive checks pass; bundled eboot.bin
and daytona.self match their respective build outputs. The archive contains
only executables, SFO and license documents, no ZIPs or extracted ROM assets.
Build directories: build/vita-enhancements (1993, existing generated input),
build/vita-revision-a (fresh Revision A generation). Both were incrementally
rebuilt after final frontend edits, then bundled in that order.

## Remove configurable Vita display buffering (2026-10-02)

User reports 30 FPS and requests removal of single/double/triple settings.
Removed the menu/config field, physical ring adapter and its obsolete tests.
Old gpu_buffers keys are ignored and disappear on the next settings save.
The same pinned libvita2d now builds without source injection, retaining
upstream's normal three-surface queue and display synchronization. This
removes custom buffering, not the framebuffer storage needed for scanout.

Inspection found no explicit 30 FPS cap: FrameClock retains fractional time
at native 57.524 Hz; the display callback waits one vblank, not two.
Do not claim the reported slowdown is proven caused by buffer count, or
force game logic to 60 Hz. Audio pacing, CPU/core options, renderer and
wide tile fallback are unchanged. Hardware speed remains unverified.
Package: build/daytona-vita-standard-presentation.vpk.
Validation: release cross-build and archive check pass; ELF has upstream
vita2d_swap_buffers and no custom daytona_vita2d symbols. Host CTest:
21 passes and two optional Lua skips. New one-step GXM clock regression
produces 5752 game steps over 6000 simulated 60 Hz display ticks (100 s).
Deleted adapter/test sources remain recoverable in Git history.

## Optional Vita CPU clock and fourth core (2026-10-02)

GXM options now include CPU 500 MHz and a persisted fourth_core switch.
Defaults remain 333 MHz and fourth core off. CPU requests are read back;
rejected/ineffective 500 MHz requests fall back to a 444 MHz request, with
actual frequency shown in options. An overclock plugin/profile may override
application requests; selecting 500 alone is not proof it is active.

Main probes USER_ALL | SYSTEM affinity and falls back to USER_ALL on failure.
Reference sound jobs and both audio callback implementations adopt the shared
mask once per change, with callback caches reset when reopening devices.
No kernel patch/dependency is installed. A working core-unlock plugin is
required. Existing threads can use the additional core; no game-loop
parallelization or audio clock/pacing change is introduced.

Host fallback tests and audio worker/lifecycle tests pass; complete CTest
suite: 22 passes, two optional Lua skips. Vita release cross-build and VPK
archive validation pass. Package: build/daytona-vita-500mhz-fourth-core.vpk.
Actual plugin acceptance, stability and performance require hardware testing.
Wide CPU tile fallback, GPU roads and double-buffer default are unchanged.
Main and PSP remain untouched; no push.

## Wide tile recovery after hardware slowdown (2026-10-02)

User reports moving tile composition onto GXM made the game slower. Disable
the explicit wide GPU tile capability in the Vita frontend: widescreen now
uses the existing CPU backdrop/foreground fallback. Original-aspect GPU tiles,
GPU 3D, the road subdivision change, audio, clocks, physical double-buffer
default and main3044f3b remain unchanged. No additional performance hypothesis
is presented as established; the earlier CPU-only timing excluded GPU cost.

This isolates wide tile composition from the road change, which also can add
vertex work. If this build remains slow, measure/subtract that separately;
do not advertise moving work to GXM as automatically faster. Hardware result
for this comparison build remains pending. Main and PSP untouched; no push.
Package:build/daytona-vita-wide-tile-recovery.vpk.
Validation: VitaSDK build and21 CTest passes (2 optional Lua skips). The
fallback composition and mode transitions are covered by renderer tests.

## Rebase onto GitHub main and GPU tile/road follow-up (2026-10-02)

Fetched origin/main3044f3b and rebased psvita-native-frontend with merge history
retained. Backup:backup/vita-before-main-20261002 at20becae. A preliminary flat
rebase was aborted before continuing with rebase-merges to preserve Vita merge
content. Resolved Video API conflicts by keeping desktop snapshots/instance
tracking and preserving Vita widescreen, plus the renamed HUD copy destination
parameter. Main and PSP branches are untouched; no remote push. The obsolete
Vita frame-skip helper/docs from an old merge were not reinstated; replacement
docs describe the physical GPU buffers (double default).

New main renders tiles on GPU except for per-item HUD relocation, which still
uses CPU foreground composition. Vita now matches that split: GXM composes
centred foreground as well as backdrop, skips CPU pixel composition/uploads,
and retains dirty state for transitions back to the relocated HUD fallback.
CPU tile decode/cache work remains. Main's SDL_GPU shaders are not Vita GXM
binaries; they are retained for desktop, not falsely advertised as a direct
Vita shader port. Audio timing, game cadence and double buffering unchanged.

Main's per-fragment reciprocal-depth UV correction confirms the difference
from Vita's affine tessellation. Add an edge-midpoint texture-error criterion
alongside the existing depth/span minimum subdivision. A synthetic shallow
road triangle goes from46.545 to0.925texels midpoint error, at subdivision8
instead of1. Initial test wrongly expected0.5 at that cap; corrected to check
the measured sub-texel result. The0.5target is not a guaranteed bound, especially
at the eight-way cap or when pool pressure lowers subdivision. This can add
vertex work. No homogeneous matrix or untested shader replacement is used.

Tests exercise scroll-dirty GPU foreground skipping, fallback transitions,
HUD placement, source UV/stretch separation and25440 tessellation cases.
Hardware road appearance, GPU timing and overall speed remain unverified.

Validation: full host build,21 CTest passes with2 optional Lua skips, and
ASan/UBSan renderer contracts pass. The first dirty-transition test omitted
tile_memory_w under write tracking; it now uses the real board notification.
Desktop SDL_GPU Vulkan smoke tests complete120 and1200frames at16:9; the latter
with HUD-edge option enabled. This is not Vita rendering or full-race parity.
VitaSDK build and archive checks pass. Package:
build/daytona-vita-main-gpu-tiles.vpk
SHA256:50dde138308c20680cf6b5a4f9a8ba92abc5c84cb4aa9fb8a4104c90e5a3728b.
Frontend options, controls, audio and physical buffer adapter compare unchanged
against the pre-rebase backup. Source tree is ready for device testing; no push.

## Widescreen GPU backdrop optimization (2026-10-01)

Hardware feedback: all physical buffer choices still slower in widescreen.
Buffer count was not a cure for the extra rendering work. The wide path still
CPU-composed scrolling backgrounds, unlike original aspect's GXM tiles.

Add an explicit GPU-background capability enabled only by Vita's GPU frontend.
Wide screen_update skips CPU background composition/copies; GXM uses the
existing System24 tile upload and composition around unchanged 3D polygons.
Only background destination x changes for stretch, never UVs or 3D projection.
CPU foreground and per-item HUD relocation remain intact. Non-stretched side
margins clear to palette pen0 as in the original GPU path. The old CPU backdrop
path remains when capability is off; normal desktop/PSP behavior is unchanged.
Double GPU buffers remain default; no frame skipping, audio or clock changes.

A synthetic host test (1000 scrolling updates,margin93,empty geometry,constant
character pixels,changing hscroll) measured526.78ms before and238.81ms after
for CPU screen_update, about55percent less. Commands/source are under ignored
build/vita-wide-bench-new.cpp and /tmp/vita-wide-bench.cpp. This excludes GPU
cost and does not establish hardware FPS, visual fidelity or a full-speed game.
Prior homogeneous-WVP failure is not reintroduced: no draw_polygons changes,
shader changes, custom matrix, or HUD algorithm changes.

Host renderer contracts cover margins59/93/200, centred/stretch coordinates,
unchanged UVs and HUD pixels, no CPU backdrop generation, and one CPU layer
draw instead of two. Full host build and21 CTest passes (2 optional Lua skips),
ASan/UBSan renderer contracts, VitaSDK build and VPK archive checks pass.
Package:build/daytona-vita-wide-gpu-background.vpk
SHA256:400df3856a523876c0686cd47a8aac79c7c555b3b0e3d247760fb8e5ed36cb1e.
Hardware rendering and audio checks remain necessary. Main/PSP untouched;
no remote push.


## Vita physical GPU display buffers (2026-10-01)

User requested actual Vita GPU buffering separate from main's software draw
mode, with double as default. Options now persists gpu_buffers1..3; default
and Reset Defaults select2. Old draw_mode remains ignored. No frame skipping,
clock catch-up, audio backend, or polygon-renderer changes are reintroduced.

The pinned public MIT libvita2d source is built privately in the build tree.
display_buffers.inc changes the active GXM surface ring, not board cadence.
Double/triple retain normal display-queue sync; single finishes GPU work and
directly presents the sole surface without queuing identical old/new sync
objects. It may tear during rendering and is not promised faster. Switching
drains GPU and display work, copies the latest frame to surface0 when needed,
waits for scanout, then resets front/back indices. All three allocations stay
alive to allow switching; this does not reclaim framebuffer RAM.

Source archive hash verified. Initial build caught upstream JPEG ceil missing
math.h with the current compiler; target-only forced include resolves it.
All seven pinned shaders' extracted binaries match the installed package
exactly (object metadata hashes differ). No shader compiler used.
The design and dependency records now describe physical buffers rather than
the withdrawn frame-skip options; upstream MIT license is packaged.

Validation: adapter tests cover all mode transitions, VSync on/off, invalid
counts, uninitialized/in-scene/system-app rejection and GPU-before-display
ordering. ASan/UBSan pass;21 CTest passes,2 optional Lua skips. VitaSDK cross-build,
linked buffer-control symbols and VPK archive checks pass. Package:
build/daytona-vita-gpu-buffers.vpk. Hardware switching,
tearing, frame pacing and audio remain unverified. Main/PSP branches untouched.


## Vita pre-draw-mode pacing recovery (2026-10-01)

Hardware feedback after9790e57: game and audio still lag since draw modes.
The synthetic debt test did not establish the cause of native audio stutter.
Back out the Vita draw-mode UI, config loading and presentation gate, and the
follow-up retained-debt clock. main_gpu.cpp is identical to6a6fdc8 apart from
the PACING RECOVERY menu title; controls and input tests match that baseline.
This restores the previous VSync-paced presentation on every frontend loop,
not just when a board frame is due. Saved draw_mode values are ignored and
removed on the next settings save; other preferences are preserved.
Shared main frame-skip support remains merged but defaults to zero and is
not enabled by Vita. Native audio remains device-clocked per the design
document Audio section; neither audio backend nor GPU geometry was changed.
Do not re-propose frame skipping as physical single/double buffering.

Validation: source comparison to6a6fdc8,20 CTest passes,2 optional Lua skips,
VitaSDK cross-build and VPK archive validation. Installable recovery package:
build/daytona-vita-pacing-recovery.vpk. Hardware playback remains unverified;
this is a controlled rollback, not a claim of a measured hardware speedup.
Main and PSP branches untouched. No remote push.


## Vita audio pacing follow-up (2026-10-01)

Hardware report: audio stutters with draw-mode selection. A deterministic
37-second alternating 35 ms / 2 ms host-timing test exposes the one-step
clock discarding whole-frame debt:1114 board/audio steps instead of2128.
Vita GPU frontend now retains at most four frames of debt, still executing
only one board frame per loop. Cheap skipped frames can recover that debt;
no multi-step live-geometry submission or renderer changes are introduced.
Pause/reset clears debt; sustained overload remains bounded. Other frontends
keep the existing FrameClock policy. Audio dispatch stays outside draw gating.

The design document Audio section requires device-clocked native playback.
That backend already has it and is unchanged. This finding explains reference
sample starvation and late main-board commands, not proven native callback
underruns. Do not claim this reproduces or resolves every hardware crackle;
if native music still stutters, capture callback timing and active engine on
the device next rather than increasing latency or changing audio fidelity.

Validation: regression recovers2128 steps, bounded overload/reset tests pass;
20 CTest passes,2 optional Lua skips; public VitaSDK cross-build passes.
Package:build/daytona-vita-audio-pacing.vpk. Hardware listening remains pending.
Main and PSP branches unchanged; no remote push.


## Vita draw modes from updated main (2026-10-01)

Merged origin/main931504e05f4cd471a207c78b275f48e6ce77cc4c into the Vita branch.
Upstream calls frame-skip presets Double Buffered / Single Buffered / Every
Third Frame. They mean draw every1/2/3board frames, NOT physical buffer counts.
The earlier uncommitted buffer-library drafts were moved to ignored
build/paused-buffer-drafts; they are not compiled or shipped. The installed
libvita2d, framebuffer ring and VSync remain unchanged.

Vita Options exposes those three modes, persists draw_mode0..2 (default0),
clamps loaded values and applies on resume. GPU submission/clear/swap is gated
by the same pre-step board frame counter used by M2Board::vblank_end.
No stale live geometry is redrawn on skipped frames: retain the last displayed
frame instead. The clock is capped to one board step per frontend iteration.
Board execution, input and reference/native audio dispatch stay outside the
presentation gate. Menu drawing remains available; zero-step iterations avoid
duplicate scene submission and retain the existing idle delay. SIM FPS label
makes clear the counter measures simulation, not presentation cadence.
The polygon renderer and perspective vertices are unchanged from Wide2.

Validation:20CTest passes,2optional Lua skips. Draw cadence tests sweep invalid
and valid modes over600board frames (600/300/200draws). Public VitaSDK build
and VPK archive checks pass. Three6000frame race_basic headless replays show
identical196665345i960,223429779TGP,12050interrupts,3636sound-command bytes,
and7843660868000instructions. Host times35.18/19.45/14.24seconds are not Vita
FPS. Last-picture hashes differ as expected because skipped modes retain an
earlier frame:ad67233983ea8808 /04b9cd7b8fe2cdda /0504a052c927e9ff.

Artifact: build/daytona-vita-draw-modes.vpk
SHA256c02ffa1702a682062c991cbf569dabafcbe8bd22d055540254fd6281fc8833b1.
Menu title DRAW MODES. LoggingOFF. No hardware result yet.
Main and PSP branches untouched; only the local Vita branch is updated.
Next: compare simulation pace, audio and displayed motion across presets on
real Vita. Lower draw frequency can reduce render cost but also makes motion
less smooth; do not advertise these presets as actual buffer-count changes.


## Wide2: merge GitHub main and Vita options (2026-10-01)

Rebased mobile again onto origin/main 10c85cb before completing touch input.
Kept upstream pacing.cpp and Pacer alongside mobile controller startup, iOS
Files import and screen fixes. Backup: backup/mobile-before-main-touch-20261006;
the touch-work stash remains as an additional recovery copy. No branch pushed.

Shared mobile-only touch overlay: analogue horizontal steering, gas/brake,
sequential gears, four views, coin/start and launcher menu. Raw SDL fingers
capture controls independently (device and finger IDs), not ImGui's single
mouse pointer. Short taps survive until a game frame; focus loss, cancellation,
menu and safe-area changes release input. Existing physical controls merge
with touch through Controls; desktop receives zero touch input by default.
This follows the design's Platform layer & build boundary; runtime/shaders
and upstream pacing remain unchanged. See mobile handoff for validation.

## Mobile rebase onto remote main (2026-10-06)

Rebased mobile onto origin/main 6817bff, preserving Android document imports,
iOS Files import, screen sizing and unsigned packaging. CMake conflict
resolution keeps upstream link/force-feedback sources and Windows socket
libraries alongside mobile SDL targets. Generated-source paths combine
M2_GEN_ROOT with upstream M2_ROMSET. Mobile startup gamepad enumeration now
uses the upstream Devices owner. Initialize the link address before Android's
stale-URI early return. The previous mobile tip is retained at
backup/mobile-before-main-20261006. Main and the other platform branches are
unchanged. Existing IPA files predate this rebase and must not be presented as
rebuilt from it.
Validation: desktop daytona_app compilation passes; 13 Android-path import
tests and 2 desktop-path tests pass with the host SDL shim. Remote main is an
ancestor of the rebased tip. iOS/Android device builds are not rerun here.

## iOS launcher and Files picker (2026-10-06)

Replaced the unsupported SDL iOS dialog with a UIKit document import delegate,
keeping a separate private copy for each selection before the existing ROM
validation. Enabled modern full-screen launch sizing, safe-area placement,
logical-point ImGui styling, text wrapping, a visible scrollbar and blank-space
drag scrolling. Picker errors are shown beside the ROM field. Changes follow
the design's thin platform layer; runtime and shaders are unchanged.

Device Release IPA and simulator build succeed. The simulator cannot provide
visual verification: SDL_CreateGPUDevice reports that its device does not meet
SDL_GPU Metal hardware requirements; the resulting black screen is not UI
validation. Native Files selection and layout need another physical-device
test. See platform/mobile/HANDOFF.md. Do not claim successful picker import or
screen fit based on compilation alone.

## Mobile iOS packaging (2026-10-06)

The iOS build script supports `UNSIGNED=1` for AltStore: disable Xcode
signing, stage the device app under `Payload/daytona.app`, and produce
`build/ios/Daytona-unsigned.ipa`. The bundle template now explicitly supplies
CFBundleExecutable and the APPL package type. Custom build and generated-source
paths are normalized before packaging; argument handling remains compatible
with macOS Bash 3.2. No ROM archives or generated sources are committed.
This follows the design document's Platform layer & build and ROM handling
sections: the shared runtime/renderer is unchanged. See the mobile handoff
for build validation and device-test status.

## Recovery 1 after hardware regression (2026-10-01)

IMG_2856 shows the c3ec891 package losing most textured scene geometry: road
and car surfaces missing, while tiles and some solid/checker geometry remain.
This overrides the host-test success recorded below. The exact GXM failure
mechanism has not been isolated; the host uniform/projection model did not
execute the installed shader and was insufficient to validate the change.

Restored gpu_fast.cpp, perspective_vertices.h, video.cpp/h and corresponding
renderer tests byte-for-byte to1ca8cb3 (git diff --exit-code verified).
Both homogeneous-WVP and wide GPU-tile changes are withdrawn, rather than
shipping another unverified matrix guess. Steering curves, their controls
tests, saved settings and existing Test/Service bindings remain.
Menu identifies DAYTONA RECOMP - RECOVERY 1. Main and PSP untouched.

Public VitaSDK cross-build and VPK archive checks pass. Input tests pass;
restored renderer ASan/UBSan contracts pass, including painter order, checker
parity, wrap/mirror addressing, allocation ownership and shutdown.
Artifact: build/daytona-vita-recovery-1.vpk
SHA256825368c373f9c52dd00329ff16fc9d9e08aed68c0dfa614efe3eed584693e0a5.
Logging remains off. No hardware recovery result has been observed yet.

Road wobble and wide-mode cost remain as in the earlier working renderer.
Next: verify Recovery 1 on hardware before attempting isolated optimisations.
Do not redistribute daytona-vita-perspective-curves.vpk as a working fix;
its host math checks and replay hash did not demonstrate real GXM correctness.


## Vita perspective, widescreen cost and steering curves (2026-10-01)

Work remains on psvita-native-frontend; main and PSP branches untouched.
The Rendering/Enhancements design governs: presentation changes only, native
board timing unchanged, original aspect and linear steering remain defaults.

Road texture warp: previous affine subdivision used1/4/16/64 times as many
triangles depending on depth/span, then reduced precision under pool pressure.
Replaced it with homogeneous positions and the existing libvita2d WVP shader:
clip.w=depth, clip.z=0.5*depth, ordinary UVs. This gives perspective interpolation
with three vertices per source triangle. Painter order, stencil clips, tint,
solid/checker paths and fence ownership are preserved. Matrix changes are scoped
to textured batches and restored on all exits. No custom shader/compiler.
The exported libvita2d matrix is an internal ABI dependency documented with
upstream source revision and installed archive hash in platform/vita/THIRD_PARTY.md.
Recheck it when updating that library. Physical GPU output is not yet verified.

Widescreen had forced every tile layer through CPU composition and full uploads.
GXM now composes background tiles at the selected aspect, and foreground tiles
when centred. Sky margin colour is sampled through the same tile rectangles,
preserving split/window/scroll semantics. Active edge HUD alone uses shared CPU
grouping; unchanged source pixels reuse the grouped layer/upload even when
background scroll dirties drawing state. Pixel comparison, not a hash; extra
HUD source snapshot is about0.74MiB. Further distance still adds geometry.

Options adds persistent Linear/Soft/Extra Soft steering (linear, signed square,
cubic) after deadzone and before inversion. D-pad and full lock are unchanged.
Existing Test/Service mappings remain. Diagnostic logging staysOFF.

Validation:
- Host build and20CTest passes;2optional Lua tests skipped.
-5151 perspective sample comparisons and matrix projection/depth checks.
- Actual renderer submits3vertices for a depth100:1, wide triangle; no subdivision.
- All256stick positions across curves, monotonicity, endpoints/invert/D-pad tests.
- Final standalone renderer contracts pass, including eight sky comparisons
  (four split modes x normal/line scroll) against CPU top-left composition,
  unchanged-HUD upload reuse and matrix restoration.
- ASan/UBSan renderer contracts pass including sky comparison; final later test
  fixture enlarges the long-depth triangle, with the ordinary contracts rerun.
-6000frame16:9/edge-HUD/default-distance host replay:196665345i960,
  223429779TGP,3636sound-command bytes, hash9047513777edfaae, unchanged.
  42.27seconds on this host is not a Vita performance measurement.
- Public VitaSDK cross-build and VPK archive validation pass. Package:
  build/daytona-vita-perspective-curves.vpk
  SHA25666f209118967cb3c0beb55453d54a0f4dfb5b88c1bfda1b09bd683e2656aee87.
  Previous package retained at
  build/vita-enhancements/daytona_vita-before-perspective.vpk.

What not to re-propose: more affine subdivision to hide road warp costs both CPU
and vertex pool and remains approximate. Do not restore blanket CPU wide mode.
The former extreme-UV test rejected a finite polygon only because CPU u*q
overflowed; homogeneous submission keeps it finite, and its expectation was
updated. Host Ninja reported a truncated log and rebuilt fully on a subsequent
invocation; builds were sequential in each directory and completed successfully.
Next: confirm road lines, HUD/sky/clip transitions and frame pacing on real Vita.
No device FPS, crash-free-runtime or visually-fixed claim from these host tests.


## Vita GXM presentation options and cabinet binds (2026-10-01)

Switched to psvita-native-frontend and fast-forwarded to origin/bc02bb0 first.
No PSP files or commits were brought across; main and PSP branches untouched.
Upstream already contained desktop widescreen, per-item race-HUD gating and
draw-distance hooks, but the GXM frontend had none of their options connected.

Added persistent Aspect0..3, HUD edges and Draw Distance-2..2 settings, live
application on resume, scrolling16-row options and unchanged defaults.
Original GXM System24 fast path remains. Wide modes use shared CPU tile/HUD
composition, widened geometry clip planes and matching GXM projection/clip/HUD
offsets. Layer textures reserve896x384 for up to21:9;12MiB layer arena raises
totalGPU reservation30->32MiB. Wider/further options may cost FPS; no physical
performance claim.21:9 fits with vertical letterboxing.

Select+Triangle maps Test and Select+Square Service, consuming coin/view inputs.
Plain Select coin now triggers on release so staggered chords do not insert
coins. Menu latch suppresses a coin on resume; Start+Select remains frontend menu.
Controls regression covers both chords, staggered press and release.

Found existing generated C++ lacked hook_draw_list. Regenerated privately into
build/vita-enhancements-input/gen with seeds/daytona93_hooks.txt; onlychunk012
differs. Vita build links enhance.cpp and uses that generated tree. No generated
game code/assets are committed.

Validation complete: full host build,20CTest passes (2optional Lua skipped),
11build-helper tests and final GPU lifetime/layout/HUD ASan/UBSan pass.
Default and Furthest6000frame host16:9/HUD-edge replays complete:
default196665345i960 instructions, hash9047513777edfaae; furthest202533889,
hashc45f82273dcf08c6. Both223429779TGP instructions and3636sound-command bytes.
These are host replay results, not Vita speed or physical visual validation.

The first cross-build used old hookless generated code and was discarded.
An interrupted retry initially overlapped; both owned build trees' processes
were stopped, then a single build was resumed and completed. A host replay
link first missed the ymfm include path; corrected before the recorded runs.
The build helper now rejects GPU packages with a missing generated hook.

VPK: build/vita-enhancements/daytona_vita.vpk, archive integrity verified.
SHA256:95523616c6226f77293c8f71122f483aa1bbb9bc9973cc2e017483d9ba14da42.
ELF contains hook_draw_list and shared hud_polygon_offset; frontend objects
are newer than the final settings/control/source edits. Logging remains off.
Evidence: build/vita-enhancements-{package,final-tests,asan-final,
race-default,race-furthest}.log. Physical Vita testing remains required,
especially FPS at higher scenery levels and HUD appearance.
Changes are local on psvita-native-frontend; no push to main or PSP.

## Current state

**Force feedback: centring driver override, Linux (issue #16).** A PS2
Driving Force on Linux feels crashes but no centring on bends: the kernel's
Logitech driver offers, as far as known (hid-lg4ff from memory, not checked
here), constant force and autocentre but no spring, so the board's centring
spring is never played (autocentre is off: the game centres the wheel).
rt::DriveBoard::motor(x): the force the board's own closed loop drives for a
wheel at x (springs towards the centre over 8% beyond a 2%/6% dead zone,
uncentring away, the constant force as is, resistance nothing). Controls tab,
Linux only: "Centring driver override" (ffb_centring_override, off); on, the
constant force carries motor(wheel) from the game's own steer value
(io().inputs.steer) and the spring effect carries nothing. main passes it on
Linux only. Also: test_app_controls counted every joystick, so a real wheel
plugged in failed it (#16); it counts on top of what is there. Tests pass;
not tried on a wheel without a spring.

**Hide mouse cursor in game (issue #18).** Launcher, Game tab, next to
Fullscreen: "Hide mouse cursor in game" (hide_cursor, saved, off by default).
One rule in the main loop, every pass: hidden when the option is on, a game
exists and the launcher is closed; shown otherwise, so Esc (or a fault that
opens the launcher) shows it and Resume or Start hides it again, without
touching each place in_launcher changes. While hidden, ImGui's
NoMouseCursorChange is set: its SDL backend shows the cursor on every frame it
draws, and mobile draws its touch controls during play. Tried in the desktop
app (2026-10-07): hidden in play, shown on Esc, hidden again on Resume.

**Hold Test button (the test menu without F2).** rt::TestHold
(src/runtime/test_hold.h): holds the cabinet's Test switch (in0 0x04, active
low) for 173 frames (3 s), from game frame 240 at the earliest, then reports
it is done. Launcher, Game tab, next to Start: "Hold Test button"
(Config::hold_test, not saved; it clears itself when the hold ends); F2 and
any Test binding still work. For mobile, which has no F2 key. Measured first
(m2run, screen hash at frame 2,400): the game wants a fresh press from about
frame 200 after power-on; a press already down then is ignored even if held
to frame 287; a 3 s hold from frame 200, 240, 600 or 1,200 gives the same
screen as a 10-frame press at 1,200 (the test menu; the menu's
Test-selects-an-item does not fire). Revision A the same. test_test_hold. Tried in the desktop app
(2026-10-06): the test menu opens and the box clears.
Left for the Vita frontends' developer: GPU25 has no Test item; the plain
frontend's TEST SWITCH presses for one frame only, which can be missed; both
can use rt::TestHold. Dreamcast has no way into the test menu yet.

**Licence: BSD-3-Clause (LICENSE).** "Copyright (c) 2026, Ben Templeman and
contributors". The same licence as the MAME code transplanted into the
runtime; every linked component is compatible (BSD, zlib, MIT, public
domain). rules.md rule 9, THIRD_PARTY.md and the README say so; the open
decision is gone. GPL code (the MiSTer core, Supermodel) stays read-only.
Docs also brought up to date: the mobile notes folded into Current state,
PR #8's CI, branches now on main, #6 (Android and iOS, with touch, merged),
and the README's platform table: Android, iOS and PS Vita tested on devices
and working (2026-10-06), Dreamcast in Flycast.

**Force feedback: confirmed, enhancements documented (issue #9).** The
reporter confirmed on a Simagic Alpha Mini: no shaking, centring works, no
cut-outs. Documented in docs/issues.md, not started: a safeguard against
force feedback cutting out (SDL's DirectInput update reacquires a lost device
and retries, but its effects stay stopped: restart any effect
SDL_GetHapticEffectStatus reports stopped, about once a second; a theory from
SDL's code, seen in another recomp, not in Daytona), and settings asked for
after FFBPlugin: strength up to 200%, centring spring, resistance, a damper,
minimum force, with defaults that keep today's force feedback.

**Android and iOS (PR #14, platform/mobile).** The desktop runtime and its
SDL3/SDL_GPU renderer packaged for Android (arm64, SDLActivity, the game as
libmain.so) and iOS, from game code generated on a desktop (M2_GEN_ROOT). A
touch overlay on mobile only: analogue horizontal steering, gas and brake,
sequential gears, the four views, coin and start, and the launcher menu.
Fingers are tracked by device and finger ID (not ImGui's one pointer), short
taps last until a game frame, and focus loss, cancellation, the menu and
safe-area changes release them; touch merges with physical controls through
Controls, and the desktop gets no touch input. Android reads the ROM document
through SDL; iOS imports a private copy of each Files selection (a UIKit
document picker), the launcher fits the safe area and scrolls. UNSIGNED=1
platform/mobile/ios/build.sh makes build/ios/Daytona-unsigned.ipa for AltStore.
Runtime and shaders unchanged (the design's Platform layer & build). Checked:
Android assembleDebug and iOS device Release builds, plist lint, the unsigned
IPA; the ROM import and touch unit tests. Tested on devices (2026-10-06):
Android, iOS and PS Vita all play with no issues reported. Not recorded yet:
performance figures. (The iOS simulator cannot be used: it stops at
SDL_CreateGPUDevice, no SDL_GPU Metal there.) Found on an Android device: a
build that saved the picker's content:// URI in the config, reopened at the
next start, crashed on opening; a saved content:// is now cleared and the
archive asked for again (a Browse result is staged into app-private storage
first). If it still crashes at start, take a logcat before changing the import
again. Details: platform/mobile/HANDOFF.md.

**Windows setup: pin Clang discovery to VS 2022.** Both setup scripts now
select the documented VS 2022 toolchain when newer versions are installed
alongside it. CMake receives the matching generator and installation path;
mismatched cached configuration is cleared, preserving generated sources.

Checked: three setup regression tests pass. Revision A built with VS 2022
and Clang 19.1.5; 12 CTest tests passed, two optional Lua tests skipped.
CI now runs the setup tests and builds on branch pushes. Remote CI passed
(Build and Vita compile check; merged as PR #8).

**Frame pacing and fullscreen mode (issue #7).** src/app/pacing.h: Pacing
(Clock, Display, Vrr) chosen from the display's refresh and three settings,
all off (pace_smooth, pace_sync_display, pace_vrr), and Pacer, the per-pass
frame count. Clock is the old loop moved over unchanged (test: identical
frame counts over 20,000 random passes). Display: a frame every N refreshes
(Smooth: a multiple of 57.52 Hz within 1%; Sync: the first division of the
refresh at or under 61 Hz, if 56 or over: 60 on 60/120/180/240 Hz, nothing on
144/165), capped by the wall clock (vsync forced off) and restarted after a
stall. Vrr: one frame per pass, SDL_DelayPrecise to a 1/57.52 s deadline,
vsync kept (not IMMEDIATE as first planned: VRR with vsync and a frame cap
does not tear). Reference audio's stream ratio is scaled by game_hz/57.52 so
Sync to display does not overrun the 240 ms queue; native audio keeps its
device clock. The refresh is read every pass, so pacing follows the window
between displays (seen: 60 and 144 Hz). Fullscreen mode (fullscreen_mode,
"WxH@Hz"): the display's modes in the launcher, applied with
SDL_GetClosestFullscreenDisplayMode/SDL_SetWindowFullscreenMode. Measured
over 15 s in the game: default 57.9, Sync on 60 Hz 59.8, VRR 57.53 frames/s.
Design doc's open question on pacing answered. Not tried on a VRR display;
not listened to with Sync on.

**Legacy Logitech wheel support (issue #4).** The original Driving Force
(046d:c294) is listed but sends nothing through SDL 3.4.16's HIDAPI lg4ff
driver, which reads c294 reports only when they are exactly 27 bytes; the
reporter confirmed it works with SDL_JOYSTICK_HIDAPI_LG4FF=0. A setting, not
a change for everyone (SDL's driver serves the wheels it was written for):
Controls tab, "Legacy Logitech wheel support (Restart Required)",
legacy_logitech_wheels (on by default on Linux, whose kernel driver gives
Logitech wheels force feedback; off on macOS, where SDL's lg4ff haptics are
the only force feedback for them). On, main sets SDL_HINT_JOYSTICK_HIDAPI_LG4FF to
"0" before SDL_Init (SDL reads it when it finds devices; a change while
running is not guaranteed to hand the wheel back to evdev) and logs it.
Hidden on Windows, where SDL leaves lg4ff off already (hid.dll). The wheel's
GUID differs between the drivers, so it is bound again after switching.
Checked: a throwaway --profile with the setting on logs it, runs and saves it;
all CTest tests pass. Not tried with a Logitech wheel here.

**Audio tab: music and effects volumes (issue #10).** Launcher: a new Audio
tab with Volume, Mute, Music, Effects (music_volume, effects_volume: 1 and 1
by default) and the Native audio switch, moved from the Game tab. Reference
audio: at each MultiPCM key-on, SoundBoard::voice_channel reads the owning
channel from the driver's voice pools in 68000 RAM (0xf01500 and 0xf01618, 28
records of 10 bytes; byte 0 nonzero in use, bit 3 the chip; byte 1 the slot
code; byte 3 the channel; byte 6 0xff on the engine layers' reserved records,
skipped: they carry chip 0's bit with chip 1's fixed slot codes, which gave
1,597 false double matches in a race before they were skipped). Music is
channels 0-9 and 15 (NativeSoundSequencer::music_channel, the driver's "stop
music" set); a key-on with no record (the engine's fixed slots 18-27 on chip
1) is an effect. MultiPcm sums music and effect slots apart and scales them
before its 16-bit clamp; at 1 and 1 the sums are added as before. Native
audio: VoiceEvent::music, NativeSampleMixer::set_volumes per voice, set from
the audio callback through atomics.
Checked: class against the native sequencer's channel, note for note
(scratch program on the oracle's pairing): daytona93 race 4,899/4,899, attract
2,274/2,274; daytona race 4,892/4,892, attract 2,095/2,095; one voice record
per key-on. race_basic reference WAV byte-identical before and after (20 MB);
screen hash ad67233983ea8808 and instruction counts unchanged. Music alone +
effects alone = the full mix except 0.097% of samples, where a chip clips in
the full mix. Race RMS: effects 0.195, music 0.059 (10 dB). New tests:
multipcm_volumes, native_sample_mixer's volumes case; all CTest tests pass.
Not listened to on this machine.

**Force feedback: Daytona's drive board commands (issue #9).** The command
meanings taken from Supermodel's later drive boards were wrong for Daytona:
the game's centring spring (0x39-0x3C, on all race) was played as a 60 ms
sine, so wheels shook constantly with no centre pull, and pads buzzed. Checked
against the drive board's own program, EPR-16488A: disassembled in MAME
0.289, then run with Lua taps on its I/O (commands fed on port $27, wheel
positions on its ADC at $80, motor power and direction read from $46).
rt::DriveBoard now follows it: motor on/off (0x0-), one effect at a time with
strength in the low 3 bits (0x2- resistance, 0x30-37 centring spring, 0x38-3F
the same with a dead zone, 0x4- uncentring, 0x5-/0x6- constant force), no
force for 0x1- and the x8-xF halves, queries (0x8- up) changing nothing; no
vibration (the board has none). app::ForceFeedback: spring (negative for
uncentring), friction and constant force; pads rumble only for the constant
force and uncentring; a wheel whose haptics fail to open gets nothing instead
of SDL's DirectInput rumble (a sine through the wheel) and is retried every
3 s; failed effect updates are retried; pausing keeps the board's state
(ffb.reset() on a new game). Launcher: "Log force feedback" (ffb_log, off).
Checked: test_app_controls and all CTest tests pass. Not checked on a real
wheel; which of 0x5-/0x6- is left on a cabinet is not known (Invert force).
Details and the command table: docs/issues.md.

**Open issues reviewed (docs/issues.md).** Each open GitHub issue checked
against the code: #4 (Driving Force on Linux: SDL 3.4.16's lg4ff driver drops
input reports that are not 27 bytes; ask for a run with
SDL_JOYSTICK_HIDAPI_LG4FF=0), #7 (frame pacing; four display settings
planned, all off, so the default stays native speed, 57.52 frames/s), #9
(force feedback; the drive board's commands checked against its program),
#10 (reference audio's mix matches MAME; native audio's master gain 1.95 and
missing sample envelopes) and #6.

**Dreamcast port (in progress; on main since PR #11).** Now about 32 frames/s in
Flycast (reported 2026-10-06; the work behind it is not on main yet, so the
measurements below stop at the older figures). platform/dreamcast: a
KallistiOS build run by platform/dreamcast/build_dreamcast.py after the
desktop build, from the `daytona` (Revision A) set only, like platform/vita.
Runtime changes are behind M2_DC_MEMORY (ROM read through a page cache the
frontend supplies, texture and frame buffer RAM supplied, a two-level page
table, no GPU-layer copies, lazy rasterizer buffers, reused Lockstep
callback slots, geo_test's cursor moved on without its loop), M2_DC_SPIN_SKIP
(the game's frame-wait loop at 0x1394 skipped by whole passes to the next
lockstep event: 79% of the i960's instructions in a race) and M2_DC_SPEED
(the same output, faster: tile rows with nothing to draw skipped, geometrizer
object data parsed only for frames that are shown, frame skip 3 allowed, tile
layers composed in the PVR's 16-bit formats);
the desktop defines none of them. Checked after the changes: build-daytona m2run
race_basic, screen hash 9427a612c5cb7511, 909,312,001 i960 and 195,261,176
TGP instructions, as before. In Flycast a whole recorded race (race_basic) runs,
drawn by the PVR with textures and both tile layers, in lockstep with the
desktop at all 100 checkpoints; 25 frames/s with every 2nd frame drawn, the default, with sound (175.9 s for the race; 29 frames/s, 207 s, with every 4th; 1,414 s at the start); with the controller it runs until Flycast is closed. The geometrizer's transforms by the SH-4's ftrv (M2_DC_NATIVE_GEO, picture only; --no-native-geo turns it off): race 240.8 -> 237.0 s in Flycast, lockstep exact. Tile layers composed again only on the lines that changed (M2_DC_SPEED, Video::compose16; dcmemcheck's layer hashes identical): 237.0 -> 228.5 s. The race's scenery layer (pixmap layer 2) drawn by the PVR from a video RAM texture at its scroll instead of composed every frame (M2_DC_SPEED; dcmemcheck rebuilds and compares it): 228.5 -> 218.2 s. The geometrizer's ROM reads in blocks (GeoPtr::read, M2_DC_SPEED; dcmemcheck identical): 218.2 -> 212.8 s. Two remembered ROM pages per region and an FPU finiteness test in the renderer: 212.8 -> 205.3 s. The i960's and the TGP's instruction counts kept in registers by fast_gen.py (dcfastcheck identical every frame): 205.3 -> 195.1 s. Renderer radix sort and colour cache: 195.1 -> 193.4 s. No library memcpy/memset for kept polygons and register frames (M2_DC_SPEED): 193.4 -> 187.5 s. Tile block compare without memcmp: 187.5 -> 186.5 s. Culled polygons skip their texture reads and lighting (M2_DC_SPEED): 186.5 -> 180.1 s. Two-level entry switch and binary chunk search in the i960 code's dispatch (fast_gen.py): 180.1 -> 176.9 s. The runtime reads count + Lockstep::pending during a call, so the generated code stores its count once instead of adding it (M2_DC_SPEED): 176.6 -> 175.9 s, 0.21 MB of RAM back. The play build prints no TRACE lines and reports every 1,800 frames (the serial port is slow on a console). Found for the desktop too, not changed
there: Lockstep::calls_ never shrinks (a callback every 1,024 instructions;
estimated 270 KB a second on the desktop). KOS 2.2.1 in extern/kos-dc
(git-ignored): DreamSDK R4's installed KOS master stops every C++ program
using libstdc++'s exceptions at startup. Details, measurements and failures:
platform/dreamcast/HANDOFF.md.

**Vita build: the ROM set.** The 1994 set's M2_ROMSET broke the Vita compile
check (its CMake builds the runtime itself, without the define);
platform/vita/CMakeLists.txt defines M2_ROMSET="daytona93", and
rom_import.h falls back to daytona93 when a build does not say. Checked on
the link-play branch's CI: Vita compile check passes (with link play's
comm_board added to the Vita runtime there).

**Link play (on main).** Revision A's communication board
(837-10537), from MAME's m2comm simulation: src/runtime/comm_board.{h,cpp}
(the protocol: shared RAM set-up at cn_w, the master's 0xff/0xfe numbering
tokens round the ring, every frame each cabinet's 0xe00-byte block from
shared 0x2000 to the next, landing at 0x21c0, and the master's 0xfc vsync;
frame sync optional, off by default as in MAME). M2Board::set_link attaches
it to a LinkTransport; with none (the default, and daytona93) the comm
registers stay plain, so nothing changes. src/app/link_socket.cpp: TCP, this
cabinet listens, connects to the next (retried until it answers), non-
blocking reads, whole-frame sends with a 2 s limit, POSIX or Winsock. App:
launcher Game tab "Link play" (on/off, port, next host:port, frame sync,
status line); daytona --profile NAME for a second cabinet on one computer;
m2run --link-listen/--link-next/--link-sync and --save-nvram. Master and
slave are the game's own settings (test mode > GAME SYSTEM: LINK ID, CAR
NUMBER; factory: MASTER, car 1, twin). Checked: tests/test_comm_board.cpp
(two boards in memory: numbering, data both ways, frames in pieces, loss);
two headless m2run cabinets on localhost (master: factory EEPROM; slave: one
set to LINK ID SLAVE, CAR NUMBER 2 through test mode by script): the
linked attract (通信システム 2人まで対戦できます), both through the linked
course and transmission select, a race with POSITION /2 on both, red car
(car 1) and blue car (car 2); "cabinet 1 of 2" / "2 of 2"; the link lost
when the other cabinet quits. Not checked: two computers, Wi-Fi, frame
sync, more than three cabinets.
First two-computer try (Mac 192.168.1.52 slave, port 15112, next
192.168.1.2:15113; the other the master): NETWORK CHECKING, because the
other computer was not listening on 15113 (connection refused: its port was
still the default 15112). The launcher's waiting status now says which half
of the ring is missing (next cabinet reached or not; a cabinet connected to
this port or not), and the help says every computer can use the same port.
Then linked on two computers once the cabinets' settings matched (the game
cancels the link, "CANCELLED", when they differ: one was DELUXE/USA, the
other TWIN/JPN). Native audio then stopped the game: linked play's music
turns on the MultiPCM LFO (MIDI controller 0x01, value 1, channels 0 and 5;
found with the new m2run --native-audio-check on a headless linked pair,
frames ~1,510), which the native mixer does not have, and the app treated
any unsupported effect as a fault. Now it plays without the vibrato and
says so once in the log; invalid data and callback failures still stop it.
Open: the LFO in the native mixer (the driver's controller 0x01 handler to
MultiPCM registers 6/7, MAME's multipcm LFO).
Tested by the user: three computers linked (Windows, macOS and Linux), all
played well.

**The seed_scan seeds checked against MAME (daytona93).** A local MAME
build (scripts/build_mame.sh) and scripts/m2_check.sh on every scenario: the
11 in scripts/inputs and two new ones, attract_long (9,000 frames of
attract) and race_to_end (race_basic played on to 20,000 frames: out of
time, game over and the screens after). All match MAME in every check:
native i960 code (every device event and interrupt), the native
geometrizer, the CPU 3D layer, the composed screen, the recompiled TGP and
the sound 68000. race_to_end alone: 2.16 billion i960 instructions, 19,997
frames, 20.2 million polygons, 597 million TGP and 261 million 68000
instructions identical. Coverage (MAME's indirect-branch log against the
seed list): the 11 old scenarios reach all 334 harvest seeds and none of the
248 seed_scan seeds (they are the harvest's own scenarios); attract_long and
race_to_end reach 81 of them (10 of 19 game modes, 13 of 29 task states, 3 of
85 jump-table entries, 5 of 16 lda tables, 48 of 92 ROM-record handlers, 2 of
7 data-ROM pointers), now checked. The other 167 need states no scenario
reaches yet (other endings, name entry, link play). Found on the way:
- An Apple silicon MAME differed from ours in the geometrizer's last bit
  (attract frame 173): clang fuses a*b+c into FMA on arm64 by default;
  build_mame.sh now builds it with -ffp-contract=off, like our code.
- build_mame.sh picks a Python whose XML parser loads (Homebrew's Python
  3.14 here had a pyexpat built against a newer libexpat: "No parsers
  found"); setup.sh --build-mame on macOS installs sdl3 and pkgconf (MAME's
  macOS front end is SDL 3, found through pkg-config), not sdl2.
- The first m2_check run of race_to_end stopped at 6,897 frames (cause not
  found: MAME's output goes to /dev/null there); run again by hand, MAME
  recorded all 19,997 frames and every check matched.

**Setup builds the 1994 set too.** scripts/setup.py builds every set it
finds in roms/: daytona93 into build/ (as before), daytona (Revision A) into
build-daytona/ (configured as the main build, with -DM2_ROMSET), and prints
how to start each. The rejection and no-ROM messages name both sets.
getting-started.md and the README describe both, including Revision A's
first run (a single cabinet set in test mode). Checked: setup.py from a
configured tree with both sets in roms/: both games built, tests pass.

**Widescreen: scene or 2D screen, by window and coverage.** The margins'
fill (stretched or sky behind a 3D scene, each row's edge colours on a 2D
screen) was chosen by "the 3D covers half the screen". Attract and race
camera shots that look at a lot of sky cover 36-49% (measured: daytona93
attract frames 720-780, 6720-6860, 7020-7140, 8320-8460; Revision A the
same kind), so those frames smeared their sky's edge colours sideways and
then snapped to stretched when the camera moved. Video::scene(): one window
and at least 15% coverage. Measured over attract and race_basic on both
sets: daytona93's car and circuit select draw their 3D in 2-3 windows,
Revision A's select screens have no 3D, titles none; every one-window frame
with 15-49% is a scene. Checked: daytona93 attract frame 760 now stretched,
circuit select still edge colours (16:10, stretch on).

**Wheels, pedals and force feedback.** Controls gained a third binding
column, Wheel / joystick: every SDL joystick is opened (app::Devices), and
an action binds to an axis, button or hat direction of a device by GUID, so a
wheel, pedals and a shifter can be separate devices. An axis is calibrated
when bound: the capture records where it rested and how far it was moved
before being let go (pedals resting at either end or short of full range; a
900-degree wheel's chosen lock). Saved as `<action>.joy=` lines, with
`joy_deadzone`. Force feedback: the game writes the drive board's command to
I/O board dual-port RAM byte 0x11 (found by logging its writes: 44 in a
race, every type), which IoBoard queues; rt::DriveBoard decodes them (the
meanings below were wrong for Daytona and are replaced: see "Force feedback:
Daytona's drive board commands" above. Then: the
command set of Sega's later drive boards, as Supermodel documents it:
0x1- centring, 0x2- friction, 0x3- vibration, 0x5-/0x6- pull right/left, 0xc-
reset; 0x0-/0x4- sequences and 0x7- not modelled) and app::ForceFeedback
plays them on the steering device: SDL haptics (spring, friction, sine,
constant force on the steering axis; only changed levels sent) or rumble.
Launcher: Force feedback strength (70% default, Off) and Invert force; m2run
prints the commands by type. Checked: tests/test_app_controls.cpp with SDL
virtual joysticks (a wheel and pedals resting at +32767; config round trip;
ADC values; a wheel paddle; unplugging; drive command decoding; rumble and
its scaling and stop). Not checked: a real wheel or force feedback (none here; user
testing): the launcher marks the wheel column, its dead zone and force
feedback Experimental; the direction of the pull may need Invert force.

**The 1994 set (daytona, Revision A) builds and runs, for comparison.**
CMake `M2_ROMSET` (daytona93, the default, or daytona), one set per build
directory: `scripts/recompile.py --set daytona --build-dir build-daytona`
with the set at roms/daytona.7z. The importer has both sets' MAME tables
(Revision A: other program, sound program, two main data ROMs mirrored from
0x800000, two polygon and two texture ROMs; the TGP program is the same
2,024 words, at main_data 0x800020). Saves are per set (pref folder
daytona-recomp/<set>). Seeds: seeds/daytona.txt, the daytona93 seeds carried
over by code (new scripts/seed_map.py: 469 of 582, 175 of them by
alignment where the revision inserted code), then seed_scan.py --set
daytona (239 more; 38,001 instructions reachable). Revision A does some
float maths on the i960's FPU instead of the TGP: m2recomp gained addr, subr
and modi (MAME's semantics; register forms only; daytona93's generated code
unchanged). Hooks: seeds/daytona_hooks.txt, draw distance at 0x17508
(daytona93's draw-list routine moved by 0x490: the same masked instructions,
RAM and boot-time budget); checked: -2 and +2 change the frames as on
daytona93. Its default settings are a linked twin cabinet, which waits on the
link at the settings screen: set a single cabinet in test mode (F2) once;
m2run and m2gpushot `--nvram DIR` load the app's saved EEPROM and backup RAM
(tools/common/nvram.h). Checked with that: 12,000 attract frames, and every
input script (races on all three courses, steering, test mode screens)
runs with no missing code; race_basic plays through circuit select,
transmission select, the rolling start and the race, with drive board
commands. Native audio works on Revision A too: its sound program has the
native sequencer's tables from 0x505a on 0x48 bytes later, their pointers
with them (sequence banks and engine sound tables unmoved); the sequencer
detects the layout by those tables' first pointers. scripts/
test_native_sound_oracle.sh takes the set from the ROM folder's name and
M2_NVRAM=DIR: Revision A race, 6,000 frames: 4,892 of 4,893 notes the same
as the reference audio (the extra one at the run's last instant), the same
two pitch differences as daytona93, RMS ratio 1.02, no invalid data.
Why: the arcade's wheels sit inside the wheel arches with a gap; ours (and
MAME's) poke out over the wings. The car code and data are the same in both
revisions (wheel table at 0x234af4 / 0x230d54: ±0.525, 0.32, 1.4125/-1.4;
body and wheel models on shared polygon ROMs), and Revision A here frames
the arcade footage's bridge shot (frames 8680-8780) almost exactly and still
shows the tyres over the wings: not a revision difference. In attract the
car is moved by the course-following routine (0xca40), so body roll is 0
and pitch small; the full physics (0xf31c) does not run. Open: what on the
board differs (the geometrizer port, the TGP).

**Super sampling (hardware renderer).** Launcher > Game > Super sampling:
Off, 2x, 3x, 4x (`supersampling=` in launcher.ini; m2gpushot
`--scale N`). GpuRenderer::render draws into a target `scale` times the
frame: vertices stay in original pixels (the viewport scales them), clip
rectangles and the depth buffer scale, the tile shaders map target pixels
back to original ones (tiledata[2]; tile pixels repeated), and textures
take a finer mip level, log2(scale) levels (texlod + 128 log2(scale); the
rasterizer picks levels from z, not screen size). The checker pattern is
per target pixel. main draws it into its own texture with mip levels and
shows the level nearest the window's size, so a frame bigger than the
window is averaged down (supersampling). 1x: byte-identical to before (104
race frames, 16:9). race_basic 16:9 headless (Metal): 455 / 389 / 295 /
265 frames/s at 1x / 2x / 3x / 4x. Checked: a 3x race frame (no cracks,
HUD in place) and the app at 3x (700 frames).

**Tilemaps, step 2: drawn on the GPU (hardware renderer).** m2.hlsl
ps_tiles_back / ps_tiles_front compose the System 24 layers per pixel with
Video::draw's rules (window masks, per-line scroll, the split modes, the
back pass's opaque 3 and 2), then the widescreen margins as fill_margins
(edge colours, the sky's colour, or stretched). Inputs: the decoded pixmaps
(a u16 per pixel, pen and category; uploaded by the span of tile rows
changed since the last upload, by the tile generations; never with
SDL's cycle flag, which would drop the rows not sent), and a per-frame
snapshot Video takes at screen_update of tile RAM 0x4000-0x6fff and the
4,096 pens. Video's external-3D `cpu_layers` is now `desktop`: the CPU only
estimates the 3D coverage (margin fill) and, with the HUD at the edges in a
race, still draws the front layers and moves the HUD blobs (uploaded as a
texture then). Checked against the previous build (CPU-drawn layers):
byte-identical frames, 0 pixels differ, over race_basic at 4:3, 16:9,
16:9 stretched and 21:9 with the HUD at the edges, the advanced and expert
courses, test mode and test drive (622 frames; the races use split modes
and per-line scroll). race_basic 4:3: 400 -> 526 frames/s; game 1.82 ->
1.03 ms, renderer CPU 0.53 -> 0.33 ms, GPU 0.16 -> 0.54 ms. Tested by the
user on macOS (Metal), Windows and Linux.

**Tilemaps, step 1: decode only changed tiles (both renderers).** Measured
first (m2gpushot --bench now reads Video's own timers): of the hardware
frame's 2.2 ms game time, the CPU tilemaps took 1.27 ms: decoding the four
512x512 layers 0.55 ms (all 16,384 tiles, every frame), drawing them with the
scroll/split/mask rules 0.63 ms, composing 0.09 ms. Video::decode_layers
(desktop; the Vita path keeps its own cache) re-decodes only tiles whose
value or character changed, comparing character RAM (256-byte pages, then
32-byte characters) only on frames the game wrote it. Same pixmaps: race,
time attack, test mode and attract screen hashes unchanged. Decoding 0.55 ->
0.02 ms; hardware 343 -> 409 frames/s, software race 189 -> 204. Next: the
layers drawn on the GPU (0.65 ms drawing + 0.09 composing).

**Vulkan application name (MangoHud showed "SDL").** SDL's Vulkan backend
hard-codes VkApplicationInfo: no application name, engine "SDLGPU", which
overlays such as MangoHud show instead of the API; SDL has no property to
change it. `patches/sdl3/0001-vulkan-application-name.patch` (applied by
setup.py's new apply_patches, shared with MAME's patches; already-applied
patches are skipped; fetch forces the checkout if a patched file would block
a new pin) reports SDL_SetAppMetadata's name ("Daytona USA", set by main)
and no engine name. Checked: setup re-applies it after a revert and skips it
when present; macOS builds and runs. The MangoHud result itself is untested
here (no MangoHud on macOS).

**Windows: the game's messages.** daytona is a WIN32 (GUI) program, so on
Windows its output went nowhere and a command window returned at once; a
tester could not see which renderer ran. It now attaches to the parent
console when there is one, else writes daytona.log in the pref folder, and
prints `daytona: renderer hardware (GPU)` / `software (CPU)` whenever the
active renderer changes. Checked on macOS (both lines); the Windows branch is
compiled by CI only.

**Hardware renderer speed (`m2gpushot --bench`).** race_basic, 6,000 frames,
headless on this Mac (Metal): software 189 frames/s (5.3 ms a frame) at 4:3
and 188 at 16:9; hardware 324 (3.1 ms) and 347. Hardware frame at 4:3: game
2.20 ms (logic, geometrizer, CPU tilemap layers), renderer on the CPU 0.72 ms
(vertices, uploads, a 4 MB texture RAM compare, the colour table), waiting
for the GPU 0.17 ms. From the draw-mode figures (every third frame drawn:
442 frames/s), logic is about 0.75 ms, the CPU tilemap layers about 1.45 ms
and the CPU 3D rasterizer about 3 ms a frame. Next costs, in order: tilemaps
on the GPU; texture RAM tracked by writes instead of compared.
Done: M2Board::tex_write counts writes (VideoMem::tex_generation) and the
renderer uploads texture RAM only when the count moves; the 4 MB compare and
shadow copy are gone. Renderer CPU 0.72 -> 0.54 ms, 324 -> 346 frames/s at
4:3; GPU frames byte-identical to before (8 of 8 sampled).

**Hardware renderer, widescreen and a mip-level fix.** Hardware mode keeps
widescreen: Video's external-3D mode with CPU layers no longer drops the
margin (only the Vita path does); both layers are width() wide, the
backdrop's margins filled as in software mode with the 3D coverage taken
from Raster::coverage_estimate (the polygons on an 8x8-pixel grid; no CPU 3D
layer exists here) and the front layer's HUD moved to the edges; the GPU
projects with the margin, widens full-width windows into it, and moves the
condition panel's quads by Video::gpu_hud_shift (same box and z as the
software path). Fix found while comparing: the rasterizer's max mip level is
30 - countl_zero(min(w, h)) = log2(min) - 1; stage 2 used log2(min), so a
fading circuit-select map (texlod -321, mml 1132) took level 7 not 6 and
came out coloured instead of grey. Measured after (race_basic, Metal,
m2gpushot vs m2run, every 650 frames to 5200): 89.7-100% identical,
94.4-100% within 8 levels; the rest are rounding on high-contrast textures
(road lines, rock), where a one-step texel coordinate difference flips the
blend. 16:9 with HUD at the edges: race frames 94.8-98.3% identical.

**Hardware renderer, stage 2 (textures).** ps_poly is a port of the
rasterizer's draw_tex_span and fetch_bilinear_texel in integer arithmetic:
the 4-bit sheets with their 2048x1024-as-1024x2048 mapping, bilinear 8-bit
blending (LERP), wrap/mirror/edge rules, mip levels by fast_log2 (the same
128-entry table) and texlod, the microtexture, the translucency flag and
test, the luma RAM and the colour translation. Data: three read-only storage
buffers (texture RAM, both sheets, uploaded only when it changes; luma RAM;
colour translation with the rasterizer's gamma applied on the CPU); per
polygon texture state as flat integers decoded as render_one does; 1/z, u/z,
v/z interpolated noperspective. Measured (race_basic, Metal, m2gpushot vs
m2run): frame 1500 99.9% of pixels identical; race frames 3000 and 4500
95.5% and 95.1% identical, 98.4% and 98.8% within 8 levels. The differences
are scattered over textured surfaces (far road, rock face), not edges:
float differences flipping mip-level and texel thresholds. The rasterizer
accumulates 1/z, u/z, v/z per pixel along each span; the GPU evaluates each
pixel's directly, and Metal compiles with fast math. Exactness is stage 4.
build_shaders.py pulls the x86-64 Ubuntu image explicitly (a cached arm64
one failed with "exec format error").

**Hardware renderer, stage 1 (geometry).** Launcher > Game > Renderer:
Software (exact; default) or Hardware (Experimental). `src/app/gpu/`:
`m2.hlsl` (one source) -> `scripts/build_shaders.py` (DXC v1.9.2609 to
SPIR-V and DXIL, SPIRV-Cross to MSL; Docker when the tools are not
installed) -> `shaders_gen.h` (committed; builds need no shader tools).
GpuRenderer draws the 3D in the rasterizer's order (window, then z, newest
first), projected as model2_3d_project, each polygon a fan from vertex 0,
clipped to its window by scissor; a depth buffer with depth = draw order
and LESS reproduces "first polygon to fill a pixel wins" (the rasterizer's
fill buffer). Colour: the solid renderer's palette/luma/gamma; textures are
stage 2 (the Vita GPU path is a reference only: a tester saw small road
geometry/orientation errors there). Video's external-3D mode gained
`cpu_layers` (the CPU still draws the tilemap layers for it; the Vita path
does not). No widescreen in hardware mode yet. `m2gpushot` renders frames
offscreen through it for comparison with m2run's (tools/common/
input_script.h shared). Checked on Metal: the game runs (710 frames in
15 s), and a race frame's geometry, HUD and backdrop line up with the
software renderer's. Not yet run on Vulkan or Direct3D 12.

**Draw mode (frame skip).** Measured first: Daytona runs the board in 60 Hz
mode and the geometrizer starts a new frame every vblank (3,000 of 3,000
race frames drew a new 3D picture), i.e. double buffered. Launcher > Game >
Draw mode: Double buffered (every frame, the default), Single buffered
(every 2nd), Every third frame (every 3rd); `m2run --frame-skip 0|1|2`.
M2Board::vblank_end skips screen_update on the frames between (3D raster,
tilemaps, composition), keeping the last picture; the geometrizer still
parses every frame (the game reads its polygon count). Measured race_basic:
identical i960 (196,665,345), TGP (223,429,779) instruction, interrupt
(12,050) and sound byte (3,636) counts in all three; 190, 332, 442 frames/s
headless on this Mac. Default hash unchanged.

**Skip launcher.** Launcher > Game > "Skip launcher" (saved): start-up goes
straight into the game, as `--autostart` does, when the ROM set checks out;
otherwise the launcher shows with the reason. Esc still opens it. Checked:
with it set, `daytona` started the game at once (377 frames in 8 s).

**"Stretch tile background" stretches, it does not extend.** In a race, with
the option on, the backdrop as drawn for the 496 columns is scaled across
the whole width (linear blend per row); the tester wanted no repeat at all.
Video::draw_ext (drawing the tiles past the screen edge) is removed. Earlier:

**Widescreen sky: plain by default, "Stretch tile background" to extend.**
A tester still saw a seam in the margins with the tile backdrop drawn out.
Measured: the race sky is tilemap layer 2, one layer in normal scroll mode
(not a split pair: the split-mode alternation added to draw_ext changed 0
pixels there), and its hscroll sweeps the whole 0..511 range over a lap
(152 values), so the original 4:3 screen passes the picture's join too;
only ~79% of rows match across it. So the margins default to the sky's
plain colour behind 3D, and launcher > Enhancements > "Stretch tile
background (Experimental)" (`m2run --stretch-backdrop`) draws the tiles out.
2D screens keep each row's edge colours either way. Split pairs in draw_ext
now alternate A/B every 512 columns (a 1024-pixel panorama), as the
hardware's layout implies; not exercised by Daytona's race sky.

**Launcher labels.** Options that need it say so: "Graphics API (Restart
Required)"; "Native audio (Experimental, Reset Required)" (it applies when a
game starts or is reset, not on an app restart); "HUD at the screen edges
(Experimental)". Everything else applies straight away.

**Draw distance (enhancement, off by default).** Launcher slider (Shortest,
Shorter, Default, Further, Furthest = -2..+2), `m2run --draw-distance N`.
Found by tracing, not by guessing: the geometrizer's master z clip is unused
(0xff); object commands in the display list are written by the TGP, which
reads model lists from its own ROM (only 8 of ~3,000 i960 FIFO words per
frame match model addresses). The i960 picks what to draw:
- Scenery by course cell: a 16x16 grid (cell = x + 16 y; offsets table
  0x17136 = dx + 16 dy). 0x16f74..0x17070 takes the 5x5 around the car's cell
  (r8) in nearest-first order (0x17104; 0x1711d on two courses), kept where
  two visibility masks allow (r13 from 0x171f8, r9 from 0x1727c), into the
  draw list (count 0x5016c0, cells 0x5016c1.., 63 bytes before 0x501700) and
  a near list of the 10 nearest (0x501600) plus bitmaps 0x501500/20/40.
  Measured over a race: 8-13 cells listed, always reaching radius 2.
- 0x17828 draws every object of each listed cell (table 0x501420) until a
  per-frame polygon budget runs out: 0x5010e8 accumulates each object's cost,
  0x5010f4 is the limit (5000, set once at 0x1210), checked at 0x17a78 and
  0x1786c. Extra cells appended to the list drew nothing until the budget
  was raised: it, not distance, is what stopped them.
- The road: a 14-section window (0x13f5c: 5 back via +0x8c, 14 forward via
  +0x88, list at +0x5c of its struct; ordered pair checks via table 0x13ec4
  and TGP maths at 0x14180). Also game logic (car/section interaction);
  not changed.
Mechanism: `m2recomp --hooks FILE` ("ADDRESS name": the generated code calls
rt::hook_<name>(c) before that instruction), `seeds/daytona93_hooks.txt`,
`src/runtime/enhance.{h,cpp}`. hook_draw_list at 0x17078: -1 keeps the
game's list within one cell, -2 the car's cell only; +1 lists all 5x5, +2
all 7x7 (inside the grid), and raises the budget to 10000 / 15000 (restored
to 5000 back at default). Only the draw list and the budget change; the near
list and bitmaps the game logic reads do not. Measured (race_basic, m2run on
this Mac): -2 242 frames/s, -1 198, default 193, +1 190, +2 185; default's
screen hash unchanged (ad67233983ea8808). +2 adds visible scenery (frame
2600: 1,560 pixels, a tree line behind the billboard); most frames differ
by tens of pixels, because the road, not the scenery, is the horizon.

**Setup's "ROM set rejected" was shown for any recompile failure.** A
Windows tester got it with a zip that imports on macOS and Linux. The
reader is portable C++ (binary I/O, fixed-width fields, its own inflate
and CRC), so the likelier cause is a later step failing on Windows (the
game's generated code has never been compiled there) under the wrong
message. `recompile.py` now exits 3 only when `m2import` rejects the set;
any other failure gets "ROM accepted, the build failed, see the errors
above". The tester's next run confirmed it: the ROM was accepted and the
generated code compiled (`m2run`, `m2native` built); the final build step
still failed. Likely cause, not yet confirmed on Windows: `daytona` is a
WIN32 (GUI) executable and `main.cpp` did not include `SDL3/SDL_main.h`,
so nothing provided `WinMain`. Now included (no effect on macOS/Linux).
The tester's next output showed the real failure: MSVC building SDL itself,
`yuv_rgb_internal.h` C2099 "initializer is not a constant". Our directory-
wide `/fp:strict` reached SDL, and under it MSVC will not fold C float
constants in static initializers. SDL now gets an empty COMPILE_OPTIONS
(ours keep /fp:strict). CI never built SDL (it was only added with
generated game code); SDL, ImGui and the app objects (`daytona_app`) now
build whenever extern/sdl3 exists, so CI compiles them on every OS; only
linking `daytona` still needs a ROM set. CI then compiled SDL, ImGui and
`daytona_app` under MSVC and clang-cl (including the yuv_rgb file that
failed). The tester's PC had built with MSVC: setup.ps1's Visual Studio
Installer `modify` ran unelevated and, it seems, failed quietly. setup.ps1
now runs it elevated, checks the Clang toolset with vswhere afterwards,
stops with instructions if it is missing, and sets M2_COMPILER=clang
(`--msvc` for MSVC). Untested on a real PC; CI only checks it parses
(GitHub's Windows runners have no winget). README's Setup and
docs/getting-started.md now cover Clang on Windows and `--msvc`, updating,
a clean rebuild, the "build failed" message, and the widescreen options.

**Widescreen (enhancement, off by default).** Launcher > Enhancements >
Widescreen: Original (4:3), 16:10 (614x384), 16:9 (682x384), 21:9
(896x384); `m2run --aspect 16:9` for headless dumps. More of the scene at
the sides, same focal length, nothing stretched: `Geo::set_wide_margin`
opens a full-width viewport's left/right clip planes by the margin,
`Raster` draws into a wider layer (stride >= 496 + 2 x margin) with the
clip widened for full-width viewports, `Video` composes at `width()` with
the tilemaps centred and each back-layer row carried into the margins (the
sky's colour, not the backdrop pen). With it off every path is the old one:
all 11 scenarios give their previous screen hashes. With 16:9 all 11 run to
the end. Sampled 21:9 race frames show no obvious edge pop-in yet; not
checked frame by frame.
"HUD at the screen edges" (with widescreen; `m2run --hud-edges`): two
groups in 496-wide coordinates move out by the margin: lap and lap times
(x < 125, y < 130) left; position, condition panel and course map
(x >= 352, y < 300) right. Decided per item: the front layers' pixels are
grouped into blobs (pixels within 4 of each other join) and a blob moves
only if it lies wholly inside a group, so a banner crossing a group stays
whole and centred. The first version decided per area (a band at the cut
had to be empty): during the rolling start the banner's letters passed the
cut, the decision flipped frame to frame and the HUD jumped back and forth.
Measured after, frames 2585-3200 every 5th: the course map is at the edge
in all but the first (HUD not yet drawn). "40TH/40" reaches x 367. The
condition panel's box and car are polygons in the main 3D window at sort z
0x600 (scenery there is above 18000); overlay polygons (z <= 0x0fff) inside
a group move with it, but only while the race HUD is on screen: a tester
saw the car's door come off in an attract close-up, because the car's own
near polygons (and the ranking screen's 2D markers) sat in the HUD's
right-hand area and were moved. Now nothing moves unless the frame has the
condition panel's box (one checker-shaded overlay polygon, texheader
0x8000, sort z <= 0x0fff, ~77x82 at x 385..462, y 67..149;
Raster::race_hud_visible). Measured after: 240 attract frames at 16:9
identical with the option on and off; the race HUD still moves, and the
map stays at the edge through the rolling start. A tester still saw 3D
moved in play: the rule was any polygon at sort z <= 0x0fff inside the right
group. Now only the condition panel's own quads move: polygons at exactly
the box's z inside the box's outline (Raster::find_race_hud records both).
Measured: race at 16:10, option on vs off, 0 of 120 frames differ outside
the HUD areas.
Side margins: in a 3D scene (the 3D layer covers >= 50% of the screen;
measured races 69-100%, select screens ~23%) the back layers are drawn
margin to margin by Video::draw_ext, draw()'s rules pixel by pixel for any
screen column: scroll, per-line scroll, the split modes that put layers
L and L^1 side by side, priority, window masks. Checked: its visible columns
equal draw()'s on every frame of a race (M2_CHECK_DRAW_EXT=1 prints any
difference; none). Earlier tries, all wrong in play: one plain sky colour;
each row's edge carried out (smeared the clouds); copying columns mod 512
(a tester saw the backdrop duplicated with a seam: a column off the screen
can belong to the other layer of a split pair); a "joins up across the
wrap" test to tell sky from menus (failed on the race sky, median 79% of
rows). On 2D screens (car, circuit select) each row carries its own edge
colours: their art covers only the 496 columns, and drawing further shows
leftover tiles as stripes. Before that the margins were the sky's plain colour (the back
layers' top-left pixel); carrying each row's edge out smeared the sky
picture's clouds and mountains. Off: all scenario hashes unchanged; 21:9
with it on: all scenarios run to the end. rules.md now lets enhancements change game logic.
Measured for draw distance: Daytona leaves the master z clip at 0xff
(0 polygons culled by distance over a 6,000-frame race); backface 4.2M,
behind the camera 1.1M, off-screen 1.4M. The limit is in the game's code.

**Windows: Clang by default; MSVC fixed.** MSVC failed on SoftFloat: the
CMake used SoftFloat's `build/Linux-x86_64-GCC/platform.h` everywhere, whose
`opts-GCC.h` needs `__int128`, `__builtin_clz` and GNU inline (C4235, C4013).
MSVC and clang-cl now get `cmake/softfloat-portable/platform.h` (no
`INLINE_LEVEL`, no builtins, no 128-bit type) and `__declspec(thread)` /
`thread_local` for its globals; `-DM2_SOFTFLOAT_PORTABLE=ON` forces that
header anywhere. Measured here with it forced: `test_fp` 0 mismatches,
race_basic, time_attack and course_expert screen hashes identical to the
GCC-header build. `setup.ps1` installs (or adds to an existing Visual
Studio) the Clang tools; `setup.py` builds with the ClangCL toolset when
present, MSVC otherwise (`M2_COMPILER=clang|msvc` forces one), and
reconfigures a build directory set up for the other.
`.github/workflows/build.yml` builds and tests both, without a ROM set,
alongside macOS and Linux (GCC, Clang).
Not yet run on a Windows PC here; a tester reports Clang + Ninja builds.
First CI run: SoftFloat compiled under both; both then stopped on
`tests/test_vita_gpu_memory.cpp`, `alignas(262144)` (C2345; clang-cl: 8192
bytes at most on Windows). That buffer is now aligned at run time, and the
nine Vita host tests are opt-in (`-DM2_VITA_TESTS=ON`, 21 tests) instead of
part of every desktop build (12 tests).

**PS Vita frontend merged (PR #3, `c3007c6`).** Desktop unchanged by it,
measured: all 11 scripted scenarios and 3,000 frames of attract give the
same instruction counts and screen hashes as `398eb4b`; 21/21 tests pass.
Native audio is opt-in (launcher checkbox). Its notes live in
`platform/vita/` (HANDOFF, THIRD_PARTY for SDL2); the Vita build itself is
untested here (needs VitaSDK).

**Setup for players.** `docs/getting-started.md` walks from nothing to
playing on each OS, with troubleshooting for what went wrong in practice:
`./setup` for `./setup.sh`, the ROM set under another name (setup silently
built tools only), a different Daytona set (`daytona`, program ROMs
`epr-16722a`/`16723a`: `m2import: missing epr-16530a.12`), Start disabled.
`scripts/setup.py` now says so itself: with no `roms/daytona93.*` it names
any archives in `roms/`; a rejected set gets an explanation instead of a
traceback; on success it prints the command that starts the game. README's
Setup links to the guide.

**macOS (Apple clang, arm64, Metal) builds and plays.** First run on a Mac
with a real `daytona93` set turned up:

- `scripts/recompile.py` built only the check tools after generating code,
  never `daytona` or `m2run`, so a fresh `./setup.sh` left no game. It now
  builds everything.
- The launcher saved the ROM path as typed (`roms/daytona93.7z`), so the
  check failed and Start stayed disabled when run from another directory.
  It now stores the absolute path. A saved GPU choice the host lacks
  (Vulkan on a Mac without MoltenVK) exited at `SDL_CreateGPUDevice`; it now
  falls back to automatic.
- The seeds were short. The windowed game stopped at `0x1d8c`, then longer
  runs at `0x2266f8`, `0x5788`, `0x225028`, `0x223078`, `0x2265c4`: code the
  game reaches only through pointers in states the harvest never visited.
  `scripts/seed_scan.py` finds them statically (see Findings) and added 248
  seeds (334 -> 582): 24,504 -> 34,497 reachable instructions, recompile
  still all native.
- Time attack stopped in the geometrizer: texture-point/header reads and a
  polygon-RAM walk run one past the end of their memory. `GeoPtr`/`GeoPtr16`
  now wrap (see Findings).

Measured after, `m2run`: attract 40,000 frames (11.6 min of game time),
all 11 scripted scenarios in `scripts/inputs/` (races, courses, time attack,
test menus) run to the end, all native; race_basic screen hash unchanged
(`ad67233983ea8808`) by the wrap. `daytona` on Metal: 60 s, 3,396 frames,
no stop. **Not verified against MAME**: the newly seeded code has not been
lockstepped (it only runs in states the scenarios reach, and the harvest
did not), and the wrap has no MAME counterpart to compare with.

**Sound.** The Model 1 sound board runs natively: its 68000 program
(`epr-16489`/`16490`) is statically recompiled (`src/m68k` decoder,
`tools/m2sndrecomp`, runtime context `src/runtime/snd_cpu.h`), with the
YM3438 (ymfm, BSD-3) and both MultiPCMs (MAME's, transplanted into
`src/runtime/multipcm.cpp`) as native code on `snd::SoundBoard`
(`src/runtime/sound_board.cpp`). All 1,916 reachable instructions decode
exactly as MAME's 68000 disassembler prints them; the driver has no
indirect jumps. Lockstep against MAME's 68000 (`tools/m2sndcheck`, MAME
patch 0003, `M2TRACE_SNDLOG`): attract 15.7M instructions and race 77.9M
instructions, 3,648 interrupts, 5.16M device accesses, all identical.
No clock: the driver polls YM timer B (868 Hz tick) in its main loop and
takes the UART's RxRDY on IPL 2, so time is counted in 68000 instructions
(752,000 per second, MAME's rate on this program) and events land on it:
command bytes one line-time apart (31.25 kbit/s), YM timer expiries in
exact YM clocks. `GameLoop` advances the board one frame per video frame
and hands it that frame's UART bytes. `daytona` plays it through two SDL
audio streams (YM at 55.6 kHz, MultiPCMs at 44.6 kHz) with a small speed
trim holding 60 ms of queue; volume and mute in the launcher. `m2run --wav
FILE` writes it headless. Against MAME's own audio (`-wavwrite`) for 26 s
of attract: the same 48 command bytes, per-second loudness within a few
percent, a constant ~125 ms offset (when the i960 sends the first
commands), no tempo drift. Race: 3,636 bytes natively vs MAME's 3,648.
Checked here with SDL's disk audio driver (no sound card in the
container): continuous output from the windowed game.

**Launcher.** `daytona` opens a Dear ImGui launcher in its window
(`src/app/launcher.cpp`): ROM browse (SDL3's native file dialog: Windows,
macOS; xdg-desktop-portal or zenity on Linux; typed path as fallback) with
per-file verification, graphics API and fullscreen; a Controls tab binding
every arcade control to a key and a gamepad button or axis half (press to
bind), analogue triggers for the pedals and a stick for steering, live
meters, dead zone, invert. Settings save to `launcher.ini` in the SDL pref
path as they change. The ROM set is loaded natively from the zip
(`src/runtime/zip.cpp`, own inflate; `rom_import.cpp`, the importer's table):
images byte-identical to `scripts/m2import.py`'s, in 0.8 s. 7z ROM sets too (`src/runtime/archive.cpp`: the
7-Zip LZMA SDK's public-domain decoder; solid LZMA2, LZMA, PPMd checked
byte-identical to the zip import, 0.9 s for 7-Zip's default). The build's
importer is now the same C++ code (`tools/m2import`, used by
`scripts/recompile.py`), so a .7z-only user can build. Esc in game
returns to the launcher (Resume, Reset, Quit). Verified here under Xvfb on
Vulkan: verification, the Controls tab, a key rebind saved to the ini, Start
from the zip, pause, and the zenity file dialog. Resolution and upscaling
options are to come.

**The game is playable in a window.** `daytona` (`src/app/main.cpp`): SDL
3.4.16 (built from source, static) with SDL_GPU: Vulkan or Direct3D 12 on
Windows, Vulkan on Linux, Metal on macOS (`--gpu` to choose). Each composed
frame is uploaded to a GPU texture and blitted, letterboxed 4:3, onto the
swapchain; the game advances at the board's 57.52 frames/s and is presented
at the display's rate. Keyboard and gamepad map to the I/O board; settings
EEPROM and backup RAM persist in the SDL pref path. Verified here on Vulkan
(Mesa lavapipe under Xvfb): the attract demo in the window. Direct3D 12 and
Metal not run yet (no Windows or Mac here). The 3D layer is still drawn by
the CPU reference rasterizer; the GPU rasterizer is a later step.
`rt::GameLoop` (`src/runtime/game_loop.cpp`) holds the frame pacing that
`m2run` and `daytona` share.

**The game runs on its own.** `m2run` runs the recompiled game on the
native board runtime (`src/runtime/m2_board.cpp`) with no trace and no
MAME: boot, the settings screen, then the attract demo in full 3D.
Headless for now (frames dumped every N; `scripts/rgb2png.py`); 600 frames
in 5.7 s including the CPU rasterizer (106 frames/s), 23 M i960 and 12 M TGP
instructions, all native.

How it runs, with no clock:
- Frame pacing is the game's: vblank starts when the game waits in its
  wait-for-vblank loops (0x12b0-0x12bb, 0x12f0-0x12ff: spinning on the
  frame counter at 0x00500000); a CPU-bound frame (the boot texture upload
  at 0x1388) gets vblank after one frame's worth of work (110k
  instructions, as MAME measures 25 MHz / 57.52 Hz). Vblank ends when the
  handler has returned to the wait loop. A windowed build waits for vsync
  there; frame rate is the only limit.
- The TGP runs before the i960 reads buffer RAM: the game sends the TGP a
  command, writes -1 to the mailbox at 0x0091fff0 and spins until the TGP
  writes 0 (0x1166c). Without this the game waits forever.
- I/O board: the dual-port RAM mailbox protocol, native (command 1: latch
  inputs into bytes 0-10; 3: load the 128-byte settings EEPROM into
  0x100-0x17f; 2: store it). Inputs take the scripts/inputs format
  (`m2run ... --inputs FILE`).
- Sound UART: TxRDY is immediate, so the IRQ3 handler drains its queue at
  once; the sound board receives the bytes at the line rate (Sound, above).

With scripted inputs (`--inputs scripts/inputs/race_basic.txt`) the whole
game flow runs standalone: coin-up, Circuit Select, car select, the race's
rolling start (3,000 frames in 23 s headless: 80 M i960 and 62 M TGP
instructions, 381 bytes sent to the sound board).

**The whole screen now renders natively, identical to MAME.** `src/runtime/video.cpp`
adds the segaic24 tilemap chip (four 64x64-tile layers, per-line scroll,
special window modes, 8-pixel window masks), the tilemap palette pens (as
MAME computes them at palette write time, refreshed each frame once a scroll
colour is written), the CRTC offsets, and MAME's composition order (2D back,
3D, 2D front). It runs at the i960 instruction count of each MAME screen
update (vblank end; patch 0002 `su`/`scr` lines) and is held to a hash of
MAME's composed screen: **596 of 596 attract frames and 5,996 of 5,996 race
frames identical**, 3D layer 5,587 of 5,587. Dumped frames (boot settings
screen, attract with HUD) are byte-identical to MAME's.

Everything on screen now comes from native code: the recompiled i960 and
TGP programs, and native C++ for the fixed-function chips (geometrizer,
rasterizer, tilemaps). Still replayed from MAME's trace in the harness: the
sound board (UART bytes) and the I/O board (dual-port RAM). Design change:
the sound 68000 will be statically recompiled, not interpreted.

**M3 started: the 3D layer renders natively, pixel-identical to MAME.**
`src/runtime/raster.cpp` is a CPU reference rasterizer (MAME's Model 2
renderer and the poly.h triangle/polygon setup, transplanted). In
`m2native`, at each vblank it draws the previous display list from our own
video memories (palette, colour translation, luma, texture RAM, all written
by the recompiled i960) and is held to a hash of MAME's 3D layer (patch 0002
`fb` lines): **423 of 423 rendered attract frames and 5,587 of 5,587 race
frames identical**, and a dumped frame is byte-identical to MAME's. A
mutant (one gamma entry off by one) fails every frame. Frame dumps:
`M2NATIVE_FBDUMP_DIR`/`M2NATIVE_FBDUMP_EVERY` (ours),
`M2TRACE_FBDUMP_DIR`/`M2TRACE_FBDUMP_EVERY` (MAME); `scripts/rgb2png.py`
converts either (game output: keep under traces/). Finding on the way: the
original Model 2's texture RAM keeps only 16 bits of each 32-bit write,
packing two writes per stored dword (MAME tex0_w/tex1_w); the bus now does
the same. CRTC offsets are still taken from MAME's log (they come from the
segaic24 tilemap chip, next).

**M2 geometry, native and matching MAME.** The whole geometry path now runs
natively inside `m2native`: recompiled i960, recompiled TGP, buffer RAM, and
the geometrizer (`src/runtime/geo.cpp`, MAME's HLE transplanted; the
original Model 2 geometrizer's DSP code is undumped). At each MAME vblank
(same i960 instruction count) the geometrizer walks our buffer RAM and is
held to MAME's log (patch 0002, `M2TRACE_GEOLOG`): every word it hands the
rasterizer and every polygon kept after culling and clipping, vertices bit
for bit.

| scenario | frames | rasterizer words | polygons kept | result |
| --- | --- | --- | --- | --- |
| attract | 597 | 11,954,823 | 403,475 | identical |
| race_steer_left | 5,997 | 151,874,210 | 6,658,999 | identical |
| time_attack | 5,997 | 136,744,927 | 5,937,107 | identical |

A mutant (luma off by one when it is exactly 100, in all four parsers)
diverges at frame 173, rasterizer word 385. A one-ulp change to a vertex
cannot show: the geometrizer hands the rasterizer 24-bit floats (MAME's
`f2u(x) >> 8`), which drop the low 8 bits. `scripts/m2_check.sh SCENARIO`
runs all of it (the race's geometrizer log is 2.8 GB of text; it is deleted
after the check unless M2_CHECK_KEEP is set).

**M2 started: the TGP program is statically recompiled and matches MAME.**
The TGP runs a 2,024-word program the i960 uploads from the data ROM;
`m2tgprecomp` turns it into native C++ (MAME's MB86233 semantics inlined per
instruction from `src/runtime/tgp.h`; no interpreter, no hand-written HLE),
and `m2tgpcheck` replays MAME's TGP-side log (patch 0002) through it:

| scenario | TGP instructions | input words | output words | banked accesses |
| --- | --- | --- | --- | --- |
| attract, 600 frames | 12,390,181 | 1,293,703 | 398,072 | 7,789 |
| race_steer_left | 230,600,970 | 21,489,825 | 6,727,552 | 3,233,496 |
| time_attack | 97,745,484 | 8,089,105 | 3,291,602 | 2,025,772 |
| test_tgp | 32,597,984 | 3,300,128 | 1,083,012 | 7,789 |

All identical to MAME; in attract every register was also checked after
every instruction (`M2TRACE_TGPPC`). A mutant (fml result off by one ulp when
A = 1.0) diverges at TGP instruction 2,339. Native speed ~500 M TGP
instructions/s (race: 0.48 s for 231 M).

**Native i960 + native TGP together** (`m2native` now models the geometry
ports, TGP FIFOs and buffer RAM instead of replaying them; the TGP runs on
demand, clockless, until its input FIFO is empty):

| scenario | i960 instructions | TGP instructions | TGP output words | buffer RAM hash |
| --- | --- | --- | --- | --- |
| attract | 70,926,456 | 12,390,181 | 398,072 identical | identical at 1,153 of 1,153 samples |
| race_steer_left | 643,000,979 | 230,600,970 | 6,727,552 identical | identical at 11,951 of 11,951 |
| time_attack | 663,975,129 | 97,745,385 | 3,291,602 identical | identical at 11,951 of 11,951 |

Buffer RAM (128 KB) is built natively from its three writers: the i960, the
geometrizer command port (0x800000) and the TGP's banked writes. The only
reads that differ from MAME (103 in attract, 64,956 of 5,451,850 in the race)
are all the i960 polling the TGP's mailbox, the last three dwords of buffer
RAM (0x91fff0-0x91fff8, TGP bank offsets 0x7ffc-0x7ffe): our TGP has already
finished when the i960 looks; MAME's, paced by cycle estimates, has not. A
timing artefact like the UART one, so lockstep uses MAME's value there and
counts it. Finding on the way: races read and write the TGP FIFOs 16 bits at
a time (MAME's 32-bit handlers see the whole dword, other lanes zero).

**M1 met with native code**: `m2recomp` statically recompiles 23,262
instructions (seeds: boot record + `seeds/daytona93.txt`) to portable C++,
and `m2native` runs them with no interpreter and no fallback. It matches
MAME for all 600 attract frames: 70,926,456 instructions, every one native,
1,153 samples, 3,315,201 device events, 627 interrupts; 1.3 s (55 M
instructions/s with lockstep checks on every instruction).
`scripts/recompile.sh` generates and builds it (into git-ignored
build/gen); `scripts/m2_check.sh` traces MAME and runs both harnesses.
An address with no recompiled code is a hard error naming it.

Beyond attract, the same native code matches MAME through seven scripted
scenarios (coin up, selects, races, test mode), every instruction, device
access and interrupt, including the sound-UART interrupts that land mid-code
during races:

| scenario | frames | instructions | device events | interrupts |
| --- | --- | --- | --- | --- |
| race_steer_left | 6,000 | 643,000,979 | 39,750,589 | 11,443 |
| course_advanced | 6,000 | 640,678,236 | 42,356,938 | 12,475 |
| course_expert | 6,000 | 641,018,396 | 42,010,671 | 12,419 |
| time_attack | 6,000 | 663,975,129 | 20,342,792 | 11,506 |
| test_mode | 4,000 | 439,587,066 | 6,954,614 | 4,051 |
| test_tgp | 3,500 | 385,086,539 | 6,708,822 | 3,578 |
| test_memory | 3,500 | 385,423,629 | 6,748,842 | 3,553 |

`scripts/m2_check.sh SCENARIO` reruns one (inputs from
`scripts/inputs/SCENARIO.txt`); each takes ~7 min of MAME plus ~15 s native.

The reference interpreter (`src/refcore`, MAME's executor) is a test
oracle only: `m2replay` uses it; the game build will never link it.

Earlier, **M1 reference milestone**: `m2replay` runs Daytona's own code through
the runtime (MAME's i960 semantics, transplanted) with devices replayed from a
MAME trace, and matches MAME for all 600 attract frames: 70,926,456
instructions, 1,153 samples (RAM hashes + registers), 3,315,201 device
events, 627 interrupts; 2.0 s.

M0 tooling runs against real MAME with the real game (`daytona93`, user's ROM
set, git-ignored `roms/` in this cloud container, never committed). Model-2-only
MAME with the harvest patch builds here (`scripts/build_mame.sh`, ~50 min cold,
15 s incremental); `scripts/run_trace.sh` runs it headless.
Built and tested here: i960 decoder + `i960dis`, trace library + `tracediff`,
and the MAME plugin `tools/mame-plugins/m2trace` (trace recorder, input
recorder/replayer). The plugin is tested only against a mock of MAME's Lua API.

Build: `./setup.sh` (Linux, macOS) or `setup.ps1` (Windows) installs the
toolchain, fetches the pinned dependencies, builds, recompiles the game if
`roms/daytona93.zip` is there, and runs the tests (see README.md). Verified
end to end on Linux with GCC and Clang; macOS and Windows (MSVC) not yet run.
The Lua tests need `pip install lupa` (Lua 5.4) and skip without it.

Running the plugin (user's machine, with their ROM set):

    M2TRACE_OUT=traces/attract.m2tr M2TRACE_FRAMES=600 \
    mame daytona -plugin m2trace -pluginspath "<mame>/plugins;<repo>/tools/mame-plugins"

`M2TRACE_RECORD_INPUT` / `M2TRACE_REPLAY_INPUT` record and replay inputs;
`docs/trace-format.md` has the rest.

## Complete

- Design document.
- Project rules (`rules.md`) and `.gitignore`.
- M0 step 1: "from memory" figures checked against MAME `sega/model2.cpp` at
  `dddd73680656e355bb2b5beecab1167c9f07bf81` and the Model 2 MiSTer core at
  `591e148e87d27e03d50cbf7318bf0b1d1328c4bf`. Design doc corrected in place
  (Target hardware summary, TGP HLE, Floating point, Audio, Open questions).
- M0 i960 disassembler: `src/i960` (decoder + MAME-syntax formatter),
  `tools/i960dis` (linear sweep; `--interleave` joins the ROM_LOAD32_WORD pair),
  `tests/test_decode` (66 hand-encoded checks), `tests/mame_oracle`
  (differential against MAME's own `i960dis.cpp`, compiled unmodified).
- Ghidra SLEIGH cross-check (`tests/ghidra_oracle.py`, third-party module
  mumbel/ghidra_i960 via pypcode, `scripts/fetch_ghidra_i960.sh`).
- `src/i960/reach` (recursive descent + boot-record seeds), `i960dis --follow`,
  `tests/test_reach` (19 checks).
- M0 trace format, `tracediff`, MAME plugin and input recorder
  (`docs/trace-format.md`, `src/trace`, `tools/tracediff`,
  `tools/mame-plugins/m2trace`), with `tests/test_trace` (30 checks),
  `tests/lua_core_test.py` (19) and `tests/lua_plugin_mock_test.py` (17).

## Next, in order

1. Scenarios that reach the other 167 seed_scan seeds (other endings, name
   entry, link play), checked with scripts/m2_check.sh like attract_long and
   race_to_end.
2. Hardware renderer: exact pixels against the CPU reference (stage 4).

## Open decisions

- FP oracle for lockstep: MAME disagrees with the hardware model on cvtri
  ties (see Findings). When a replay diverges there, the trace diff will show
  MAME's value; the recompiled build follows the model (rules: PCB > MAME).
  Settling which the PCB does needs a hardware measurement.
- `addc` carry. MAME never sets it. Recompile to the silicon and flag the diff
  when it fires (the MiSTer core made the same call, its study §2.3).

## Findings

Confirmed from MAME `model2.cpp` (all MAME figures, not PCB measurements):

| Item | Was (from memory) | MAME |
| --- | --- | --- |
| i960KB clock | ~25 MHz | 25 MHz (`50_MHz_XTAL / 2`) |
| TGP | MB86234 + microcode ROM | one MB86234 at 50 MHz; program uploaded by the i960, 2,024 words from the data ROM; run LLE |
| Resolution | ~496x384 | 496x384 active, 656x424 total, 16 MHz pixel clock |
| Refresh | ~57.5 Hz | 57.524 Hz; line rate 24.39 kHz (MAME: "TODO: from System 24") |
| Sound | 68000 + 2x SCSP, ~11 MHz | **Model 1 sound board**: 68000 @ 10 MHz + YM3438 + 2x MultiPCM |
| Main to sound | command latch | i8251 UART at 31.25 kbit/s; IRQ3 handler is the transmit loop |
| I/O | direct ADCs | Model 1 I/O board, own Z80 @ 4 MHz, via MB8421 dual-port RAM |
| IRQ order | unknown | bit 0 vblank -> IRQ0, bits 2-5 timers -> IRQ2, bit 10 UART -> IRQ3; ICR 0f0e0d0c (measured): all priority 1 |

i960 decoder (MAME `i960.cpp` / `i960dis.cpp` at `dddd7368`):

- MAME's executor implements 62 non-REG and 102 REG opcodes; its disassembler
  knows many more (whole-family table). The recompiler accepts only the
  executor's set.
- Executor and disassembler disagree on three encodings; decoder follows the
  executor and flags them: CTRL/COBR bits 1:0 (executor adds them to the
  target), MEMB bits 6:5 (executor ignores), MEMB scale > 4 (executor shifts).
- MAME's disassembler table has `ldtime` at 0x671, shadowed by `ediv`; it can
  never print. `movre` appears at 0x6e1 (undocumented, executed) and 0x6e9.
- Differential test, random + every opcode byte x 65,536 tails: 71,108,864
  words, 0 mismatches, 8.0 s on 4 threads (this container). Mutation check:
  three deliberate faults (MEMB scale bound, COBR displacement mask, one REG
  mnemonic) gave 79,619 / 2,220,826 / 9,837 mismatches, so the test can fail.
- Exhaustive run, `mame_oracle --exhaustive`: all 4,294,967,296 first words
  (second word random per word), 0 mismatches, 596 s on 4 threads (this
  container). Text and reported length both compared.

Trace tooling:

- Every signal the design doc asks for except indirect branch targets is
  reachable from MAME's Lua API alone (write/read taps, `read_range`,
  `state[]`), so the plugin needs no MAME patch yet.
- MAME's end-of-frame notifier is not on a guest instruction boundary, so it
  cannot be a lockstep sample point; the default is the vblank-ack store.
  Unverified for Daytona until the first trace.
- Mock-driven plugin test: record -> replay reproduces the trace exactly; a
  replay value applied one frame late is caught at the right frame; a memory
  change is reported as a region hash at the right epoch.
- The Lua encoder and the C++ writer produce byte-identical traces for the
  same content; the Lua hash equals an independent Python FNV over 8 sizes
  either side of the 1 KiB unpack block.
- Hashing reads 1.4 MiB per sample in Lua; cost unmeasured until a real run.
  If it is too slow, hash fewer regions per sample, not a weaker hash.

First real MAME runs (`daytona93`, MAME `dddd7368` + harvest patch, 600
frames of attract, headless, empty NVRAM each run):

- Plugin loads and every tap fires: 3,567,333 events in 600 frames. 68 s per
  run with tracing on this container (MAME reports 14.9% speed).
- Found and fixed: `screen.frame_number` is a method at this MAME, not the
  property the Lua reference documents; and MAME silently drops errors raised
  in tap callbacks, so the first run had 0 samples and no message. Samples are
  now pcall-wrapped and failures reported.
- **MAME is deterministic for Daytona**: two independent runs gave
  byte-identical traces (60,942,182 bytes, 1,154 epochs, 3,567,333 events)
  and identical branch harvests.
- vblank-ack sampling works, but the handler writes the ack (`fffffffe`) twice
  back to back: steady state is exactly 2 samples per frame, the second epoch
  holding only the second ack. Deterministic, so lockstep is fine; every other
  sample is redundant (cost, not correctness).
- ICR = `0f0e0d0c`, set once (synmov at 0x00000a40): IRQ0-3 -> vectors
  0x0c-0x0f, **all priority 1**. Taken in attract: vector 0x0c (vblank) 576x,
  handler 0x0e00; vector 0x0f (sound UART) 51x, handler 0x0f50. Timers
  (IRQ2) never fire; final enable mask = vblank only.
- Geometrizer program port (0x00804000) **is** written: 411,757 writes, from
  epoch 89. TGP FIFO: 773,279 writes, 697,078 reads.
- Harvest: 16 `bx` sites / 49 targets, 2 `callx` sites / 56 targets; no
  `balx`, no `calls`.
- Static reach with the 107 harvested targets as seeds: 89 -> 13,081
  instructions, 0 stops on non-executable opcodes, **0 quirk encodings in
  reachable code**, 31 indirect sites (18 exercised by attract).

Scripted gameplay in real MAME (`scripts/inputs/race_basic.txt`: 3 coins,
start, confirm selects, hold accelerator; 6,000 frames, no steering):

- Replay self-check passes in real MAME: "replay matched the recording for
  5997 frames", in two independent runs; the two branch harvests are identical.
- First attempt stayed in attract: default settings take 3 coins per credit
  (screen showed CREDIT 1/3). With 3 coins, snapshots show car select, then
  the Beginner course, lap 2 of 8 by frame 6,000.
- Full run 103 s emulated at ~14.5% speed, with or without tracing (MAME's
  software 3D dominates; the Lua taps cost ~nothing).
- Race harvest: 20 bx sites / 53 targets, 6 callx sites / 113 targets.
  Interrupts over 6,000 frames: vblank 5,975, sound UART 4,820, timers never.
- Attract + race seeds (168): static reach 21,850 instructions (attract alone
  13,081), 26 of 35 static indirect sites exercised, still 0 non-executable
  opcodes and 0 quirk encodings reached.

Harvest over 15 scripted runs (attract, races, manual gearbox, time attack,
test mode; all replays self-checked "matched"):

| runs | merged seeds | static reach | indirect sites hit / found |
| attract | 107 | 13,081 | 18 / 31 |
| + race | 168 | 21,850 | 26 / 35 |
| + manual, time attack, test mode | 319 | 23,138 | 46 / 67 |
| + round 3 | 333 | 23,258 | 47 / 68 |

- Still 0 non-executable opcodes and 0 quirk encodings in reachable code.
- FP in reachable code: 108 instructions, only `cvtri` 39, `cmpr` 37, `cvtir`
  22, `scaler` 8, `cvtzri` 2. No FP arithmetic, transcendentals or extended
  forms: the FP oracle question shrinks to five operations.
- Reached and confirmed by snapshot: Beginner race (auto and manual with
  shifting), time attack (start + accelerator at car select), test mode menu
  and sound test.
- Not reached: Advanced/Expert courses (circuit select stays on Beginner with
  steering pulses held 30 frames at 0xe0 from frame 1560 and from 1450), TGP
  and memory test items (7 red presses from frame 1500 land on SOUND TEST
  twice, deterministically). Cause unknown; not guessed further.
- Circuit and car select confirm on an accelerator press ("step to choose");
  holding the accelerator from the start picks the defaults.

FP, step 1 (`src/i960/fp`, `tests/test_fp`, `tests/fp_vs_mame`):

- Operand forms measured: all 108 reachable FP instructions use g/l registers
  (single precision in and out); none uses fp0-fp3 or FP literals. AC =
  `3f001000` in all 1,153 attract samples: round to nearest, exceptions masked.
- Reference = SoftFloat 3e extF80; native fast paths = host float, round to
  nearest. Exhaustive proof: cvtri, cvtzri, cvtir over every 2^32 input, scaler
  over every 2^32 single at n = 0, 1, -1, 127, 128, -126, -127, -149, -150, 254,
  -300; 0 mismatches, ~5 min on 4 cores. Plus 36 hand-computed IEEE values,
  specials cross-product, 2^26 random each. A MAME-style fast cvtri
  (`std::round`) fails with 130,905 mismatches (quick run), so the proof bites.
- Bug found on the way: SoftFloat's `extFloat80_t` field order depends on
  `LITTLEENDIAN`, defined only in its private platform.h; C++ callers saw the
  other order and every result was wrong. Now a public definition.
- MAME vs model, every input (`fp_vs_mame`, ~15 min on 4 cores):
  | op | inputs | disagree (MAME on x86-64) | cause |
  | cvtri | 4,294,967,296 | 8,388,608 | every exact .5 tie: MAME `round()` away from zero, IEEE to even |
  | cvtzri | 4,294,967,296 | 0 | |
  | cvtir | 4,294,967,296 | 0 | |
  | cmpr | 268,435,456 pairs | 0 | |
  | scaler | 77,309,411,328 (18 exponents) | 14 | 0 x 2^n (n >= 1024), inf x 2^n (n <= -1075): MAME pow() gives NaN |
  MAME built for ARM64 also differs on 830,472,191 cvtzri and 830,472,191 +
  8,388,608 cvtri inputs (NaN, out of range): its C casts are UB and
  saturate, so MAME's own result is host-dependent there.
- Practical risk for lockstep: a Daytona cvtri on an exact .5 value. The
  trace diff will show it as a register/RAM divergence right after a cvtri.
- MAME never sets FP exception flags in AC; the model reports them. Where the
  i960 records them and whether Daytona reads them is unconfirmed.

M1 groundwork:

- UART-interrupt lockstep: option (a) chosen (safe points in the shipped
  build; a test harness replays MAME's delivery points).
- Delivery points are keyed by MAME's completed-instruction count (patch):
  a stalled FIFO op counts once. Attract, 600 frames: 70,926,456
  instructions, 1,882 interrupt events (1,254 line changes, 622 immediate
  takes, 5 pending-table takes). Two runs: identical IRQ logs and traces.
- Stalled accesses: MAME's `i960_stall()` rewinds IP to PIP, so the plugin
  marks an access with ip == pip as stalled (new record types 0x12/0x13);
  comparisons drop them by default.
- Read taps now cover every non-RAM range the i960 reads (irq, timers, geo,
  copro status to 0x3f, comm, renderer), so the harness can answer them all.
- `scripts/m2import.py` builds program.bin and main_data.bin from the user's
  zip (CRC-checked, MAME's layout) into git-ignored build/rom_cache.

M1 reference core (`src/runtime`, `tools/m2replay`, test only):

- Semantics: MAME's `i960.cpp` transplanted (BSD-3, notice kept); runs at
  36 M instructions/s interpreted.
- Bugs found on the way to MATCH, in order: (1) the bus treated
  read-only-tapped ranges as write-checked; (2) **MAME's ldl/ldt/ldq and
  stores advance the address only on regions flagged BURST** (RAM/ROM, geo
  program port, TGP function port, comm); elsewhere they repeat the address
  (FIFO pops). The bus now carries MAME's BURST flags per region; (3) **the
  plugin's region hash was wrong**: `read_range(first, last, 32)` steps one
  *byte* at a time, so it hashed a dword at every byte address. Fixed with
  step 4; MAME-vs-MAME comparisons had still passed because it was
  deterministic; (4) buffer RAM is written by the geometrizer data port and
  by the TGP (`copro_tgp_memory_w`), not only the i960, so in M1 it is
  treated as a device: the i960's accesses are recorded and replayed, and
  its hash is left to M2; (5) the plugin's own `read_range` fired the
  buffer-RAM read taps while hashing; taps are now suppressed while sampling.
- A per-instruction log on both sides (`M2TRACE_PCLOG`, `M2REPLAY_PCLOG`:
  count, PIP, AC, register-file hash) located bug (2) at instruction 109.
- Mutation check: addo off by one when src1 == 1 (fired 129 times) diverges
  at epoch 3; one program-ROM byte flipped (copied to RAM at boot) diverges at
  epoch 0. Three earlier mutants never fired and so proved nothing either way
  (operand 12345, base 0x00500000, a data-ROM byte attract never reads).

Real program image (`daytona93`, epr-16530a/16531a, counts only):

- Linear sweep of the 256 KiB image: 55,098 lines, 16,280 undecodable words,
  7,356 flagged quirks, 759 non-executable opcodes. Mostly data decoded as
  code; not meaningful as code statistics.
- Recursive descent from the boot record (`i960dis --follow`, with the
  0x00220000 mirror): reset IP 0x860, 0 interrupt handlers (the PRCB's table
  is in RAM), 4 system procedures; 89 reachable instructions, 1 indirect site,
  0 quirks. The reset path ends in `b .` idle loops. Static analysis cannot
  get past boot without harvested targets.
- Mainline Ghidra has never shipped an i960 module (checked HEAD `8e9a8e7a`,
  the last 40 release tags, and full history). The user pointed to the
  third-party mumbel/ghidra_i960 (Apache-2.0), now used.

Ghidra SLEIGH cross-check (mumbel/ghidra_i960 `727ef787` via pypcode 3.3.3):

- All 23,258 reachable Daytona instructions: 0 differences (validity,
  mnemonic, length, target, operands).
- 1,000,000 random words, 1.5 s per 100k: after normalising syntax (MAME omits
  a x1 index scale, prints negative displacements unsigned, prints mode-5 as an
  absolute address; Ghidra prints `disp (ip)`), 72 differences in 5 groups,
  all known: MAME's disassembler names the integer src1 of cvtir/cvtilr/
  scaler/scalerl as an FP register (executor `get_1_ri` and SLEIGH read an
  integer); and SLEIGH decodes `movre` only at 0x6e1, MAME also at 0x6e9.
- The cross-check found four more encoding classes MAME treats specially;
  now decoder quirks: `sfr` (s1/s2, COBR bit 0: Cx special-function
  registers, ignored by MAME), `literaldst` (literal destination: MAME
  fatalerror, so no longer executable), `fpliteral` (FP literal other than
  fp0-3/+0.0/+1.0: MAME reads 0.0), `testfields` (test* with non-zero unused
  fields). 0 of any quirk in reachable code.
- Mutation check: making MEMB mode 7 read a displacement gives 2,871
  differences, exit 1.

Also found:

- A separate geometrizer (0x00800000 / 0x00804000) walks the display list in
  buffer RAM at vblank. It is not the TGP. MAME's is HLE in host `float`.
- The SCSP figures in the MiSTer core's design study are for 2A-CRX in general;
  its README confirms Daytona's working sound is 68000 + FM + MultiPCM. Both
  sources agree with MAME.
- MAME `subc` carry was fixed upstream since MAME 0.289; `addc` was not
  (operands still `uint32_t` before widening, `i960.cpp` ~line 1359).
- The design doc's "15 kHz capture" was wrong for this timing; changed to
  24 kHz medium resolution, pending PCB confirmation.
- macOS (Apple clang, arm64) failed to link `test_fp`/`fp_vs_mame`: C++ sees
  SoftFloat's globals as `extern thread_local` and calls a TLS wrapper
  function that the C (`_Thread_local`) definitions never emit. Linux accepts
  it, which is why it went unnoticed. C++ now gets `__thread` (GCC/Clang) and
  MSVC keeps `thread_local`. Checked: macOS `./setup.sh` builds with 5/5 tests
  passing; Ubuntu 24.04 arm64 GCC 13 and Clang 18 build `test_fp` with 0
  mismatches. Windows is untested, but MSVC's definition did not change.
- The harvest's seeds covered only the states the scripted MAME runs
  visited. The game reaches code four other ways, all now scanned by
  `scripts/seed_scan.py` to a fixed point (recompiling each round):
  - game-mode table at `0x18cc`, 31 entries, `ld 0x18cc[g0*4]; callx`
    at `0x18bc`, index the mode byte at `0x5010a0` (harvest reached 9);
  - task state chains: each state stores the next state's address with
    `lda` (`0x1dd4`: `lda 0x2266f8,r5; st r5,0xc(r3)`; `0x2266f8` itself
    stores `0x226764` at `0xc(g13)`);
  - jump tables read as `ld T[r*4]` then `bx` (`0x5808`, masked to 4) or
    `lda T[r*4]` then `ld`, `callx` (`0x225010`);
  - handler addresses in ROM data: 24-byte object records in the program
    ROM (`0x2375bc`: `0x223078` then floats) and tables in the main data
    ROM (`0x28660cc` -> `0x2265c4`, a bare `ret`).
  A candidate counts only if `i960dis` decodes it to a `ret` or branch with
  no `?`, `!noexec` or `!quirk` first. That rejected float constants that
  `lda` loads (`0x50d0` = 1.0f, `0xcdd8`). From the committed seeds the scan
  finds 275 entry points in one round and nothing in the next; 248 of them
  are the added seeds, the other 27 fall inside code those reach.
- Geometrizer reads past the end of memory (time attack, frame < 6,000):
  texture point/header data (`GeoPtr16`, 4-8 words from a masked start) and
  a polygon RAM walk (`GeoPtr`, index 32,768 of 32,768). MAME's raw
  pointers read whatever follows the array there. The display list is not
  ours at fault: time attack matched MAME to the word, buffer RAM included.
  Now wrapped to the memory size, as a hardware address counter would;
  unconfirmed on the PCB.

## What not to re-propose

- SCSP for Daytona. It is the Model 1 sound board (MAME `model2o` config and
  the MiSTer core's working sound on hardware).
- Five TGPs. One device; "5x" was a board-level package count.
- MAME's frame notifier as the lockstep sample point (see Trace tooling).
- `read_range(a, b, 32)` without a step of 4 (reads every byte address).
- A mutation test whose mutant is not shown to fire.
- Using MAME's disassembler as the decode authority. It is the text oracle
  only; semantics come from the executor.
- `THREAD_LOCAL=thread_local` for C++ users of SoftFloat (breaks the macOS
  link; see Findings).
- Seeding one "no recompiled code at X" at a time. Each state stores the
  next, so the game stops again one state on; run `scripts/seed_scan.py`.
- A function-start test (previous word is `ret` or `b`) on its own for code
  pointers: it rejected `0x2266f8`, whose previous word is the second word
  of an 8-byte instruction. `seed_scan.py` uses it only for the data ROM.
- Every aligned word of the 32 MB data ROM as a candidate without that test:
  5,497 candidates, mostly chance matches in graphics data.
- A TGP microcode ROM dump. The program is uploaded at boot from the game's
  data ROM; dump TGP program RAM only after the upload, or it is zeros.

M1 native (`tools/m2recomp`, `tools/m2native`, `src/runtime/lockstep`):

- One label per instruction; operands, branch targets and FP fast paths
  resolved at recompile time; `goto` for direct transfers, a dispatch switch
  for indirect ones. Unknown instructions or FP operand forms stop the
  recompile (exit 1), so nothing is left to run at runtime.
- Lockstep (`src/runtime/lockstep`) applies MAME's interrupt lines and takes
  at the same completed-instruction counts; shared by both harnesses.
- **IAC 0x93 (reinitialise) is an indirect transfer**: boot reinitialises to
  0x924 through `synmovq`. The first native run stopped there (no code); the
  harvest patch now logs IAC targets and the seeds include it.
- Mutation check: the generator's addo template off by one when src1 == 1
  diverges at epoch 3.
