# Handoff

## Native driving critical-path review (2026-10-26)

This checkpoint is an engineering review and documentation correction only.
No game behavior, executable, performance or playability changed. The latest
code checkpoint is 6f321d6. Native builds 019, 020 and 021 have the identical
ELF identity 2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158.
The application never calls the recovered vehicle functions; its linker map
discards them. Green component tests, cross-links and source-boundary audits
have not advanced the running car. Reporting successive isolated recoveries
as if a playable update were about to be connected was misleading.

The missing work is more than wiring. The main caller's central
0x3521c..0x371a4 span is 8,072 bytes, containing unrecovered calculations as
well as calls to recovered leaves. The actual motion/orientation/position
updates are within it at 0x36cc4..0x36ddc. The earlier flag/input/segment
wrapper, later fold/publication wrapper and callees also require integration.
The dependency map now divides the caller into six concrete regions; these
sizes exclude callees and are not a completion percentage. Helper37870 still
needs earlier filtering/input/event effects to produce its recovered tail's
inputs. Neither repeated main-contact steps nor captured tail operands close
the next-frame state.

The current live comparisons reseed individual sampled regions from original
state. They establish component behavior, not a self-contained sequence of car
updates. The next acceptance unit is the complete main-player update, starting
from one validated initial state and immutable course/tuning assets, advancing
its own persistent state under controls, and comparing state/effects after
each consecutive update. Original expected outputs are comparison data only.
Focused component checks remain useful for finding the first difference; new
leaf counts or standalone hardware fixtures are not integration milestones.

Rendering has an independent obstacle: StudyScene reapplies captured
object-to-reference-view matrices and recomputes scene center/radius. Repeatedly
translating a lone model and fitting it again would hide the translation.
Keep this static study unchanged and develop a separate persistent world/car
render path with verified transforms. A bounded native course/tuning loader,
actual pedal/gear consumers, initialized persistent state and verified update
cadence are also absent from the application. Reuse native GS/clipping/pad
services; do not relabel captured view matrices as world transforms, remove
linker garbage collection to fake integration, or animate a substitute model
and call it recovered gameplay.

The native rewrite guide now makes complete consecutive updates, then their
input-to-render application connection, the next milestone. Public PS2 status
leads with the static preview and distinguishes compiled sources from called
gameplay. The reusable-engine guide's stale source count is removed. These
changes follow Platform layer & build's existing native PS2 exception; no
arcade runtime or generic physics fallback is introduced. Local links and
diff checks are sufficient for this documentation-only change; existing
runtime evidence is not relabeled as a new executable validation.

## Native segment selection and composed contact step (2026-10-26)

The preceding ad0dab3 checkpoint is published; PS2 and source-export CI passed,
as did Metal and both Vulkan workflows. Its broader desktop Build matrix was
still running at the last observation. This batch recovers full course-segment
selection and the final geometry/publication region of the main balance helper,
and connects candidate preparation to the complete four-point contact pass.
The governing design remains Platform layer & build's native PS2 exception.
No arcade processor, mapped bus or device runtime enters the native target.
The full vehicle callback and continuous native driving loop are still missing;
the static preview and gameplay-performance status have not changed.

The new resolve_main_contact_step starts at the original candidate prefix and
ends after its immediately following contact call returns. It derives bases,
candidate triples and the three response motion components from the newly
prepared vehicle. All state commits together, including the prefix, four
contacts and feedback. Capture 013's independently validated streams join by
object/frame/instruction interval and complete saved caller words, with only
the call-written return pointer changed. Call-ID coincidence is not a join
proof. All 26 steps / 104 contacts / nine active responses / 1,196 state words
match, fingerprint 9a47fb54b88fea84. Evidence:
build/ps2-native-main-contact-step-live-001/result.json.
The final marker is derived from observed prefix exit plus the established
callee-preservation contract, not a separately observed post-contact marker;
the unused initial scalar is zero because the callee resets it. All captured
prefixes still have zero pending correction, so their active-consumption path
is covered synthetically, not by these live samples. The checker calculates
its intermediate contacts itself and links no original execution engine.

Sixteen authored square controls independently check both prefix branches,
signs, hits/misses and feedback choices. Four rejection paths include rollback
after the fourth contact fails, preserving the consumed prefix and earlier
contacts. Fingerprint 76ae45f11adbe279 is identical on host and corrected EE
fixture. The first EE test failed because its decimal 0.1f expectation compiled
to 3dcccccc; the native result was the verified original 3dcccccd. Disassembly
shows the differing bits in integer constant stores before runtime arithmetic.
The fixture now uses explicit expected bits for both signs. No production
arithmetic changed. Failed ee-001 evidence is retained; the passing report is
build/ps2-native-main-contact-step-ee-002/runtime-validation.json. The final
ASan/UBSan evidence is main-contact-step-sanitize-002/validation.json under build/.

Segment selection recovers the complete 0x2b0b0 helper and 0x2aed0 predicate,
including actual geometry command 5c in its offline oracle. It searches in the
original alternating order, consumes only visited quad vertices, retains signed
zero edge behavior and publishes both full selected index and wrapped signed16
current/progression values. A no-match result has all three values -1. The typed
API accepts explicit bounded immutable assets, not mapped pointers. All 3,634
synthetic original cases and 26 captured pairs match: 3,660 cases / 1,549 hits /
2,443,240 CPU instructions / 784,700 geometry instructions, fingerprint
af7d5f3820082612. Final proof:
build/ps2-native-segment-selection-live-002/result.json.
Twenty-five captured calls select immediately and one selects the next candidate;
miss, wrap and large-index coverage is synthetic. The 69,170-case host corpus
additionally covers every signed16 progression encoding at count one; that larger
host set is not claimed as original-execution coverage. Four parser tests,
ASan/UBSan, 55 atomic rejection checks and two search-order/read-domain controls
pass. Review removed an overbroad promise about differently typed overlapping
input/output storage; supported storage does not overlap.

The main balance geometry operation covers the complete region 0x382f8..0x38658,
not the whole 0x37870 helper. It preserves two recovered polynomial rotations,
ordered transforms/additions, separately rounded binary64 balance terms, both
in/out triples and selected-index publication. It requires no external table
asset. All 20,480 synthetic and 26 captured evaluations match original CPU and
geometry execution: 3,177,900 CPU instructions / 13,801,262 geometry instructions,
fingerprint 12f85315dea7bb1c. Source-bound proof:
build/ps2-native-main-balance-geometry-live-002/result.json. Four portable tests,
ASan/UBSan and 84 atomic rejection checks pass. Earlier parts of the full helper
still contain filtering, input/power and sound/event dependencies; recovering
this tail does not establish those missing effects.

Capture 013 adds only a separate main-motion stream. All 2,850 enabled/disabled
checkpoints match; all ten older streams and code coverage are byte-identical
to 012. It records 601 segment pairs and 601 tail pairs, with 26 sampled pairs
of each in 104 new events. Total stream bytes: 10,036,377, within the 16 MiB
bound. Validation SHA256 is
4e3209462468247f583609fd4898e2bb6634357f3fa83562a58d62ad74dd8b97;
new stream SHA256 is
e2db2d94769f5da7c0c0dd0b00e8ede09319fdc208e0d4b3edc14b521ae609b0.
The 120-source capture closure was verified before later inventory integration;
those sealed historical reports are not silently relabelled as newer builds.
Original images, captures, disassembly and generated oracle code remain private.

All three final standalone fixtures execute in Play 0.72 HLE with exact markers,
empty guest stderr and two window captures each. Segment: 3,634 cases /
1c1c549c187f8285, 55 rejections and two controls. Balance geometry: 1,504 cases /
3e2a507aab74e9d4, 84 rejections and two controls. Joined contact step: 16 controls /
76ae45f11adbe279 and four atomic rejections. Each corresponding ee directory has
a source/build/ELF/log-bound runtime-validation.json. The roughly 20-second
observation windows are not execution timings; physical PS2 testing is pending.

Native 021 passes the fixed 84-file source boundary, dependency directions and
forbidden-symbol audit, with all recorded project sources current. The static
ELF remains 2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes / 593,080 resident bytes; the still-unconnected helpers are
removed by the linker. The 21-file shared engine identity remains
8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
CI cross-links all three new fixtures. Independent production, original-oracle,
joined-parser and domain reviews found no remaining blocker. All 245 portable
PS2 tests pass in 59.702 seconds; log:
build/ps2/validation/main-motion-tests-001.log. Exact staged-tree and archive
source gates remain required before publication.

Next: recover the intervening main-callback calculations and earlier balance
helper effects, compare a complete car update against the original, then connect
continuous native driving. Do not substitute captured motion or generic physics
for those missing operations. Full game rendering/audio, course behavior and
physical-console validation remain separate work.

## Native main vehicle operations (2026-10-26)

The preceding ac7f576 checkpoint is published with all five workflows green.
This batch recovers four more typed value operations from the main vehicle
path: candidate preparation, the complete bounded contact-slot helper, paired
balance and parameter selection. The governing design is Platform layer &
build's native PS2 exception. Original execution remains an offline comparison;
no arcade processor, mapped bus or device runtime enters the native target.
The full main callback and continuous driving loop remain unconnected. The
static preview is unchanged, and there is no new gameplay frame-rate result.

Candidate preparation covers the complete region 0x34ff0..0x35218, including
the pending-middle clamp, recovered metric, conditional state update and both
candidate triples. The native contract accepts bounded finite values and the
metric's zero-or-normal squared-sum domain. Of 17,044 synthetic cases, 16,315
are accepted original comparisons and 729 exercise explicit atomic rejection.
The original comparison executes 1,293,072 CPU and 690,109 geometry instructions,
fingerprint 966c10f581152bec. All 26 captured prefixes match as well: 17,070
combined cases / 16,341 accepted / 1,294,632 CPU / 690,733 geometry instructions,
fingerprint 426c711e238b610d. Evidence:
build/ps2-native-main-candidate-live-001/result.json.
All captured prefixes have zero pending deltas. The conditional consuming and
clamping paths are covered by synthetic original execution, not these live
samples. Twenty-two captured markers are already 15 on entry; counting final
marker 15 does not count taken branches. The original scaling constant is the
exact binary32 value 3ecccccc represented in binary64, not rounded binary64 0.4.

The 0x37200 helper updates one of four contact slots, packed types, accumulated
type mask, histories and response. Separate binary32/binary64 rounding and
table-based normalization are retained without a processor abstraction. All
25,852 synthetic cases match original execution, including 7,248 active calls,
fingerprint 121b47e419609ea7. All 104 captured calls match too: 99 active and
five inactive. Combined: 25,956 cases / 7,347 active / 2,393,780 CPU instructions /
504,364 geometry instructions, fingerprint 57a543d2f4626b3f. Final evidence:
build/ps2-native-main-slot-live-002/result.json. Its earlier report preserves a
real rejected-domain finding: four observed vectors have zero XZ and nonzero Y,
so the metric gate can pass while XZ normalization receives a zero square sum.
The original interpolation expression still runs, with row 4096 and exponent
adjustment 63. The corrected native domain includes that case and underflow to
zero, with 64 added signed-zero/underflow controls. Do not substitute a generic
zero-normal shortcut or claim coverage of unsupported nonzero tiny sums.
The private table decoder also needed a corrected assumption: stored slope
words become magnitudes at the pinned interpolation port. Correcting that
asset conversion did not change the native arithmetic contract.

Inside the larger 0x37870 helper, paired balance preserves the two binary64
rounding boundaries between binary32 sums/differences and the final store.
Zero rounded sums yield positive-zero balance; no epsilon is introduced. All 101,130
synthetic and 26 captured cases match: 4,345,340 original instructions,
fingerprint 611fdba5efd25614. Parameter selection copies 13 raw words from the
record chosen by the highest set bit 9..1, defaulting to index zero. All 6,144
synthetic and 26 captured cases match: 99,140 instructions, fingerprint
6767e7e5a9bd359d. Synthetic tests cover independent and aliased source records;
all live selections choose index two. Neither proof recovers the complete
helper. Current shared-reader-bound evidence:
build/ps2-native-paired-balance-live-002/result.json.

Capture 012 adds independent vehicle and cadence streams, with parent/child
frame proofs, exact operand spans and all paired boundaries. All 2,850
enabled/disabled checkpoints match. It observes 601 complete main callbacks,
2,404 slot pairs and 601 prefix/balance/selection pairs; 26 sampled callbacks
produce 468 detailed events and 104 slot pairs. All eight previous streams and
code coverage remain byte-identical to 011. Total stream size is 9,863,066 bytes,
within the 16 MiB bound. Validation SHA256 is
26fa5c2cd5865204a0489daea81d528456fed276896556b2f5e531fc5d453358;
vehicle stream SHA256 is
aac6be0db399dc96f820d8bbb5b921c1f7221e30dd4d9d382e7e6c7a379b22e1.
Its historical source closure was verified before later build-inventory edits;
the sealed capture is not claimed to have run with that newer builder. The
shared reader also checks restored parent PFP after each of the 130 sampled
child returns. Original images, captures and generated comparison code stay
private under ignored build/.

All three standalone EE fixtures execute in Play 0.72 HLE with exact completion
markers, empty guest stderr and source/build/ELF/log bindings. Candidate: 1,684
cases / 1,641 accepted / d782474a4f893dc8 and 75 rejection checks. Slot: 2,876
cases / 947 active / 32ef92cd4f0d4583, 316 rejection checks and two known controls.
Paired balance: 2,154 cases / 1fd698b609c8f960 and 24 rejection checks; parameter
selection: 3,072 cases / cf0278eb2ab1a5a1. Reports are the corresponding
build/ps2-native-{main-candidate,main-contact-slot,paired-balance}-ee-001/runtime-validation.json
files. The roughly 20-second observation windows are not execution timings.
The candidate window-image attempts failed; both later fixtures produced two
captures. Numerical guest logs establish completion in all three. Host and
ASan/UBSan checks pass; physical PS2 behavior remains unverified.

Native 020 passes the 78-file boundary, dependency-direction and forbidden-symbol
audits, with all recorded source hashes current. The static ELF remains
2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes / 593,080 resident bytes, because the unconnected operations
are removed by the linker. The unchanged 21-file shared engine identity is
8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
CI now cross-links these three fixtures. Independent arithmetic, oracle,
capture-parser and restored-frame-proof reviews found no remaining blocker.
The final portable PS2 suite passes all 225 tests in 46.339 seconds; log:
build/ps2/validation/main-vehicle-tests-001.log. Current source bindings for all
six live/runtime reports are checked in main-vehicle-evidence-001.json under
that validation directory. Exact staged-tree and source-archive gates are
required before publication.

The dependency map corrects an earlier research assumption: helper 0x34ce0
ends at 0x34e3c, after the return at 0x34e38. Extending to 0x34f40 included data.
It schedules counters, feedback and events; it is not a generic force routine.
Both vector pointers of the complete 0x37870 helper are in/out, including its
final geometry-result store through the first pointer. Neither observation
authorizes replacing the unrecovered effects with generic physics.

Next: recover the intervening main-callback operations and remaining 0x37870
effects, compose a whole-car update against paired observations, then connect
the continuous native loop. Only then measure EE/VU/GS bottlenecks and gameplay
frame rates. Original textured rendering, game audio integration, full-course
behavior and physical PS2 validation remain separate work.

## Native four-point candidate contact pass (2026-10-25)

The preceding 7ec4e8b checkpoint is published with all five workflows green.
This batch composes orientation construction, four course searches, interpreted
contacts, per-contact force and final response aggregation as native game-value
operations. It follows the governing design's Platform layer & build exception;
no arcade CPU, generated game instructions, mapped bus or device runtime enters
the native PS2 target. The complete main vehicle callback and driving loop are
still unconnected. This is a recovery checkpoint, not a playable PS2 release.

The complete candidate pass matches all 26 captured original calls, 104 contact
results and nine active aggregate updates. Every pass compares all 12 transformed
offset words and 33 final state/contact words: 312 offsets and 858 final words
in total, fingerprint 2e948c3490a06a63. The comparison decodes the owner's verified
course/normal/local-point assets, starts from the original candidate pose and
prior contacts, and computes searches itself. Captured interpreted contacts are
expected outputs, not supplied native inputs. The checker links no original CPU
or generated firmware. Its final source-bound report is
build/ps2-native-candidate-contact-live-001/result.json. The recorded replay
window is not full-course coverage or independent PCB certification.

The component proofs remain separately scoped. Contact-point preparation uses
the actual float-angle polynomial, not an external sine table. It matches 49,295
synthetic evaluations / 49,287 unique inputs and 46,675,549 firmware instructions,
fingerprint f05d0d03253121d8. With all 26 live preparations: 49,321 evaluations /
49,313 unique / 46,700,153 instructions / a0a0b262f4a83792. Matrix restoration,
zero/subnormal rounding, three alias modes and 114 rejections are checked.
The final report is build/ps2-native-contact-point-live-001/result.json.
The earlier assumption that these rotations needed external table ports was
wrong: commands 2a/29/2b use initialized polynomial helpers 1e6/1e8; raw-angle
commands 32/33/34 belong to a different interface. Null-table original execution
and explicit rejection of external accesses establish that distinction.

Per-contact force preserves both rounded crosses, normalization, separately
rounded dot/square-root/products, mixed-precision gain and maximum scalar
selection. Equal scalars select the new gap. The native contract accepts bounded
finite operands and a nonnegative rounded dot; it does not claim the original
raw-pointer alias path. Synthetic evaluation covers 18,432 cases, of which
18,360 are accepted original comparisons and 72 are explicit negative-dot native
rejections. All 104 live pairs match, 13 active and 91 inactive. Combined:
18,536 evaluations / 18,464 accepted / 2,003,904 CPU instructions, fingerprint
b7152b8cdc529ab3. Evidence: build/ps2-native-contact-response-live-001/result.json.

The post-query composition oracle executes the original response/aggregate
control flow but deliberately bypasses only the checked query/adapter calls.
Its supplied contacts are that narrower API's inputs. All 4,224 synthetic and
26 live passes match, including the 13 output state words, with 2,128,380 CPU
instructions and 615,149 geometry instructions, fingerprint 6b4f4fd1f1c199bf.
Evidence: build/ps2-native-contact-pass-response-live-002/result.json. The full
candidate comparison above separately closes preparation/search/interpretation.

Capture 011 adds response_pre after the program's own status-byte read, avoiding
a duplicate instruction event at the query-return boundary. It also observes the
12 owner-supplied local-point words at entry/prequery. Saved parent, index/frame,
slot/scratch pointers and complete paired response counts are checked, without
extra FIFO/device reads. All 2,850 enabled/disabled checkpoints match. All six
older streams and main cadence remain byte-identical to 010. It observes 601
complete main passes and 2,404 response pairs; 26 passes / 1,098 events / 104
response pairs receive detailed snapshots. Validation SHA256 is
b2c5d4b0f8655e306bf7f05dfba1ca088e788a00a6efcaf0f9d1e4710e4f5359;
main stream SHA256 is
a6dc646caf1569f4bb71a5bddfb89d9f6a92cd069a271abba6610de8fbb71600.
Shared readers preserve older-capture compatibility. Fresh cross/metric proofs
against 011 are rounded-cross-live-004 and scalar-metric-live-002; both retain
their prior fingerprints and all 36 live matches.
The changed shared result reader also has a fresh proof against 011:
build/ps2-native-contact-result-live-004/result.json passes all 138 live replies
and retains fingerprint c898451d51b72992.

All four new EE fixtures execute in Play 0.72 HLE with exact completion markers,
empty guest stderr and current source/build/ELF/log bindings in their runtime
validation reports. Force: 1,200 cases / 1,128 accepted / cd1b0a7946e70d88 and
33 rejection checks. Point preparation: 3,279 / ef519d24a3f602fa, three aliases
and 114 rejections. Post-query response: 640 / 3a490cc705f0b6f2 plus inactive
preservation and late atomic rejection. Complete candidate pass: 768 cases /
512 active / 256 misses / b8ed3cad65bf6be4, a hand-calculated geometry case and
six atomic rejection paths. Review strengthened its fixture with a successful
fourth-contact control followed by an invalid fourth divisor, proving rollback
of the first three new slots as well as the car state. Final candidate EE
report is build/ps2-native-candidate-contact-ee-003/runtime-validation.json;
the other three are their corresponding ee-001/runtime-validation.json reports.
Host and ASan/UBSan checks pass. Observation windows near 20 seconds are not
execution timings. All final window-image attempts failed; numerical guest logs
provide the evidence. No physical-console or gameplay-performance claim follows.

Native 019 passes the fixed 70-file source boundary, dependency directions and
forbidden-symbol audit. All recorded sources match the compiled inputs. The
static diagnostic ELF remains 2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes / 593,080 resident bytes: the unconnected helpers are removed
by the linker. The unchanged 21-file shared engine identity is
8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
CI cross-links all four new fixtures. Independent arithmetic, observer, parser,
source-dependency and composition reviews found no remaining blocker.
The final portable PS2 suite passes all 202 tests in 37.194 seconds; log:
build/ps2/validation/candidate-contact-tests-001.log. Exact staged-tree and
source-archive gates are required before publication of this checkpoint.

Next in order: recover the surrounding main vehicle callback and candidate
integration, validate paired whole-car updates, then connect a continuous native
driving loop. Only that loop can establish meaningful EE/VU/GS bottlenecks and
native gameplay frame rates. Original textured rendering, full course behavior,
game audio integration and physical PS2 validation remain separate work.

## Native contact normalization, scalar metric and aggregate updates (2026-10-25)

The preceding b844fe4 checkpoint is published with all five workflows green:
PS2, the desktop Build matrix/source export, Metal and both Vulkan workflows.
This batch adds three independently written typed components under the game
source role. No arcade processor, mapped bus, geometry state machine or device
runtime enters the native PS2 target. Driving remains unconnected and physical
console validation remains pending.

Contact-direction normalization recovers the region 348ec..34968 with separate
binary32 products, ordered sums, an exactly rounded square root and divisions.
The bounded integer implementation preserves gradual underflow and the original
tiny-value fallback. Finite input magnitudes are limited to 2^20; output aliasing
and atomic rejection are explicit. Portable and original comparisons pass
59,912 synthetic evaluations / 59,896 unique triples, including 5,419 fallback
branches and 1,416,212 original instructions, fingerprint 53b71c272e0a45ea.
ASan/UBSan and 48 rejection checks pass. All 13 captured pairs also match:
59,925 combined evaluations / 59,909 unique triples / 1,416,524 instructions,
fingerprint 82716f3cde064327. Final source-bound evidence is
build/ps2-native-contact-normalization-live-003/result.json. The first oracle
assumed three reads on both branches; the original normal branch rereads YZ,
so its checked count is five rather than three. Correcting this harness did
not change production arithmetic. Exact instruction counts are 24 and 20.

The recovered scalar metric implements the verified command 58 / primitive 1ab
sequence with initialized constants and no external table access. It preserves
the pinned reference's unusual first correction: a unit difference returns
1.0183569, not 1.0. This is not a claim about independent PCB timing or a generic
Euclidean distance. Positive subnormal squared sums are explicitly rejected;
zero and normal sums are accepted within the bounded coordinate domain.
The synthetic comparison covers 156,644 inputs, with 146,736 accepted native
results. It executes 148,411 original commands / 6,996,272 instructions; 8,233
coordinate rejections skip firmware and 1,675 original executions establish
the rejected subnormal-sum path. Six coordinate aliases and 96 rejection checks
pass. Capture 010's 36 interleaved-input calls all match: 156,680 combined
evaluations / 146,772 accepted / 148,447 original calls / 6,997,470 instructions,
fingerprint 0b7c806697edb751. The final report is
build/ps2-native-scalar-metric-live-001/result.json. Original firmware and
all recorded values remain private under ignored build/.

Contact aggregation exposes signed scalar accumulation and the already-active
final state update. Its explicit inputs include response triples, previous
motion/candidates, divisors, base components and the feedback gate. Preserved
fields retain arbitrary bits; numerical rejection leaves the whole state
unchanged. Enabled feedback truncates exact scaled products with checked int32
bounds. The scalar-step and final-update regions pass 25,728 and 18,400 synthetic
cases respectively, fingerprint 390cbdf6408065cd; ASan/UBSan and 33 rejection
checks pass. The oracle's initial store model incorrectly expected an extra
absolute-value write when the first feedback product truncates to zero while the second is
negative; the original omits that write. Only the expected trace changed.
All 36 live scalar steps and nine active final updates also match: 25,764
combined scalar steps / 18,409 updates / 1,272,452 original CPU instructions,
fingerprint 57b055a27cdfbbf8. Final source-bound evidence is
build/ps2-native-contact-aggregate-live-002/result.json.

Observer 010 adds paired normalization, metric, signed-step and final-update
boundaries. The metric input shares the existing cross output boundary and its
result shares the step input, avoiding duplicate instruction-count events.
Saved caller identity, frame/index/pointer checks and bounded ordinary RAM reads
remain enforced. No extra FIFO/device reads are introduced. All 2,850 observer
enabled/disabled checkpoints match; all six older streams and main cadence
remain byte-identical to 009. There are 26 detailed main passes / 994 events,
36 scalar/step pairs, 13 normalization pairs and nine active aggregate pairs.
Validation SHA256: 3565df75062bf756e90842e0bc5eb24b3ba54acb51c3fb064d75774cdaf16f73.
The main stream SHA256 is
c9fd5cfa9b1dcf704edad9f1f32288535ddd80849369b7c261272da85ab9ad29.
These are bounded host-reference comparisons, not independent arcade-board
or full-course fidelity certification.

All three standalone EE fixtures execute in Play 0.72 HLE with exact PASS
markers, empty guest stderr and source/build/ELF/log identities bound in their
build/ps2-native-*-ee-001/runtime-validation.json reports. Normalization passes
4,064 cases / hash 2f11e06fc7af1915 and 48 rejections; its ELF is
153922383664734e9b37e4fd6a653edb655f90f75d2ae8a6f6214b5077223fd8,
1,227,036 file bytes / 181,088 resident bytes. Scalar metric passes 11,972 cases /
11,234 accepted / hash 46407e33dcf00f1b and 96 rejections; its ELF is
b989dc8897c59852908e0aa26f67d88fc1fedff258b3bd7766c82c47af7812b1,
1,228,276 file bytes / 182,368 resident bytes. Aggregation passes 2,240 scalar
steps and 912 updates / hash 63248df7338a0797 plus 33 rejections; its ELF is
cf4761829bbbfbc11ff553720ba5e2d3b801b43b29114dd62e0e4a93c88eb2c6,
1,232,752 file bytes / 186,336 resident bytes. Observation windows are 20.064,
20.175 and 20.109 seconds respectively, not measured execution time. Both
aggregate window-image attempts failed; its guest completion evidence passed.

Native 018 passes its 62-file fixed source boundary, dependency-direction and
forbidden-symbol audits. Recorded sources match the compiled inputs. Its
static diagnostic ELF remains 2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes / 593,080 resident bytes: disconnected helpers are removed
by the linker, so this is not a new gameplay result. The shared engine retains
its 21-file identity 8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
CI now cross-links all three new synthetic EE fixtures.

All final oracle reports rehash their current source dependencies. The shared
reader change also has fresh earlier proofs against capture 010:
build/ps2-native-rounded-cross-live-003/result.json retains all 36 live matches
and fingerprint 61cedd9373797928; build/ps2-native-contact-result-live-003/result.json
retains all 138 live replies and fingerprint c898451d51b72992. Independent reviews
found no remaining arithmetic or observer blocker.
The final portable PS2 suite passes all 175 tests in 29.922 seconds; its log is
build/ps2/validation/contact-aggregate-tests-002.log. The exact staged tree and
source archive must pass the publication gate before this checkpoint is pushed.

Remaining work is the main pass's orientation-transform construction and
intervening per-contact force expressions, followed by complete pass and
main-car integration against paired driving states. Typed leaf recovery alone
does not establish a playable native driving loop or console performance.

## Native contact transforms, reply interpretation and main-pass capture (2026-10-24)

The preceding e6957c4 checkpoint is published and all triggered workflows now
pass, including the full desktop Build matrix, PS2, source export, Metal and
both Vulkan workflows. This batch continues the typed native-runtime recovery;
no generated arcade processor code or device model enters the PS2 application.
The native driving loop remains unconnected and physical PS2 testing is pending.

Rounded cross product now has explicit three-component inputs, separately
rounded products/subtractions, a finite +/-2^20 input domain, gradual underflow,
signed-zero preservation, atomic rejection and three alias modes. Its original
firmware comparison passes 110,048 synthetic evaluations / 110,032 unique inputs
and 2,641,152 instructions, fingerprint 428bbf5526cd241b. The independently written
bounded integer arithmetic matches the portable float reference and passes
ASan/UBSan. The standalone EE fixture executed in Play 0.72 HLE: 6,208 cases,
fingerprint 408f01a4e8d06ea5, 180 atomic-rejection checks and a fused-arithmetic
discriminator. ELF 96251ba4dd8fa8a00a8dc46746ece7fe665d2400e9d5ee3a47bd8f843f50361d
has 1,226,020 file bytes / 180,192 resident bytes; its 20.18-second observation is
not measured completion time. Empty guest stderr and current source/build/ELF
identities are bound in build/ps2-native-rounded-cross-ee-001/runtime-validation.json.

Rounded affine transformation accepts explicit matrix columns, translation and
point values. Every scalar must be finite within +/-2^20. Translation is the
first accumulator value; each product and subsequent sum rounds separately.
There is no geometry store or matrix stack. Five alias modes and 720 atomic
rejection evaluations pass. The portable reference and original command 48
match 84,273 evaluations / 2,696,736 instructions, fingerprint 7005cd920cda465e.
All 32,768 zero-sign combinations, selected boundaries and 50,000 random inputs
are covered; the full binary32 input domain is not exhausted. ASan/UBSan passes.
The standalone EE fixture passes 4,065 cases, fingerprint eb07dfee53139c87, plus
order/fusion guards and rejections. Its executed ELF is
93035e5363e73de1cfae54d4988fd5d485aa83752d2f5bea4bccd422d048619b,
1,226,380 file bytes / 180,576 resident bytes; Play observed 20.181 seconds with
empty stderr. Source-bound proof is under
build/ps2-native-rounded-affine-ee-001/runtime-validation.json. The valid
original-oracle report is build/ps2-native-rounded-affine-oracle-002/result.json.
Attempt 001 stopped one instruction late because the generated hook runs after
an instruction, causing a rejected extra FIFO read. The oracle now stops after
the return at 46b and checks its destination 46c; production arithmetic did not
change for this harness fix. Independent arithmetic/oracle review found no blocker.

A separate main-contact observer stream now covers entry, complete transformed
points, all four query/reply/result sequences, response vectors, aggregate cross
operands/results and the actual saved caller-frame return. It reads only bounded
ordinary RAM/current registers and never adds FIFO or device reads. Existing
alternate streams retain their behavior. Saved frames work through interrupts;
all incomplete or mismatched phases reject capture publication. Independent
review and synthetic malformed/cached/spilled-frame tests passed.

Capture 008 passed all 2,850 enabled/disabled frame checkpoints and recorded 601
complete main passes and 2,404 complete query/result adapters. It sampled 17 passes
but none of 36 crosses: cadence located those operations in frames 2571..2579.
The strict cross consumer correctly rejected 008 as empty. Capture 009 adds only
that observed nine-frame sampling window; it also passes all 2,850 checkpoints,
with 26 detailed passes / 878 events and all 36 crosses sampled. These comparisons
are bounded host-reference observer checks, not independent PCB fidelity.
Original firmware, generated code, all captures and executable artifacts stay
private under ignored build/. The public tests use original synthetic inputs.

All 36 sampled cross operations match the native function, isolated original
firmware and captured three-word outputs. The combined comparison passes
110,084 evaluations / 110,068 unique inputs, with 2,642,016 original instructions
and fingerprint 61cedd9373797928. All 26 sampled parent passes are complete;
the 36 operations cover the nine active frames. Their inputs contain no
subnormals, so that coverage remains synthetic. The larger response is unconnected.

The contact-result EE fixture also passes: 2,440 cases with fingerprint
82d33d3b52e3a7d4, 34 atomic-rejection checks and three interpolation guards.
Its private normal table is original synthetic test data, not the game's table.
Executed ELF 0ba20976d1f98c579f3acce88cd49163efd8297e05536bf9f18ea1a24c2f2028
has 1,238,196 file bytes / 186,720 resident bytes. Play's 20.122-second observation
produced exact PASS lines and empty guest stderr. Both window-image attempts
failed; no image capture or measured completion-time claim follows. The
source/build/ELF/guest-log identities are bound in
build/ps2-native-contact-result-ee-001/runtime-validation.json.

Native017 integrates the three additional source/header pairs in the game role
and passes its 56-file dependency-direction and forbidden-symbol audits. All
recorded project inputs match the final compiled sources. The diagnostic still
has ELF 2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes / 593,080 resident bytes: these disconnected leaves are
linker-discarded, so this is not a new gameplay runtime result. The 21-file
shared engine identity remains
8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
CI cross-links all three new standalone fixtures using synthetic data only.

The typed contact-result adapter implements status-only misses, plane gap and
normal interpolation, and edge determinant/division with raw direction copying.
Only arithmetic inputs are bounded; unused raw payloads and miss preservation
retain all bits. Negative divisors are admitted. The plane normal reads an
explicit immutable 8,192-pair asset (64 KiB), with exact original table indexing
and slope-sign decoding performed offline. A production asset-loading path is
still pending. Native code has no emulated ports, registers or original table.

Final source-bound comparison passes 40,530 cases, including all 138 captured
results (104 main and 34 alternate): 125 live plane and 13 live edge paths,
with miss behavior covered synthetically. All five output words match native,
original CPU/normal instructions and captured outputs. The combined totals are
2,505,270 CPU instructions and 341,350 normalization instructions, fingerprint
c898451d51b72992. Evidence is build/ps2-native-contact-result-live-002/result.json.
Its initial live parser assumed the earlier globals window and a serial query
loop; actual main captures have a narrow selector block and pipelined queries.
The parser now accepts exactly one verified selector representation, preserves
the observed sequence and rejects ambiguous data. Review also added exact
parent/child point, slot, frame, saved-return and rounding-mode checks. Seven
new arithmetic/capture tests and independent reviews found no remaining blocker.

The shared selector-helper change required a fresh cross-product proof:
build/ps2-native-rounded-cross-live-002/result.json binds the final helper and
retains the same 36 live matches and combined fingerprint. The final portable
PS2 suite passes all 155 tests in 30.219 seconds; its complete output is
build/ps2/validation/contact-response-tests-001.log. All three new EE fixtures
and the native target bind the unchanged compiled sources. Full per-contact
response, matrix construction and aggregate updates remain the next recovery
work; these proofs do not connect native driving or certify a complete race.

The next command 0x58 study corrects one dependency label: primitive 0x1ab uses
firmware-initialized internal constants, with no external normal-table access.
Its exact pipeline arithmetic remains under study. A generic square root must
not replace the pinned reference behavior without a separate bounded proof;
no command 0x58 native implementation or reference-runtime change is included.

## Main-car publication and candidate-contact boundary (2026-10-24)

Checkpoint 0fae93e is published. Its PS2 portable/cross-build jobs, exact source
export gate, Metal and both Vulkan workflows passed; the wider desktop Build
matrix was still running at this follow-up's preparation. No Vita workflow was
triggered for that checkpoint. Work continued directly with the distinct main
car's publication helper instead of substituting the earlier flag-merging helper.

The new make_main_car_pose accepts MainCarPoseState and four explicit external
words and returns the existing RenderPose value type. It copies all 22 words,
including flags and the opaque index, without modifying inputs or consulting a
previous destination. The native interface has no mapped addresses, guest
registers or bus behavior. Return-by-value gives an independent snapshot;
arbitrary overlapping guest memory and observable store order are outside its
contract. Coordinate units, physics and game-loop behavior remain unclaimed.

The verified helper is ROM [0x34078,0x34130), 42 original instructions. The
private original oracle confines fetch/data access, checks all 23 ordered reads
and 22 ordered writes, unchanged source/external memory and surrounding guards,
and varies source/destination word alignment. All 77,208 distinct inputs match:
65,536 source low-halfword flag patterns with varied upper bits/old destination,
1,408 walking-bit cases, 264 selected raw encodings and 10,000 random states.
This is not an exhaustive full-word cross-product. The result fingerprint is
b74eb51b13296a1c over 3,242,736 instructions, 1,775,784 reads and 1,698,576 writes.
The final source-bound report is build/ps2-native-main-car-pose-oracle-001/result.json.
Portable mathematical/value tests and full ASan/UBSan checks pass; independent
production/oracle review found no blocker.

The actual EE fixture passes 2,184 cases and 48,048 words, fingerprint
d2fae2297e475353. Its ELF is
1511a352ad80d6d98d86429565f5a46c8513867705de497c44ec6304f4f0f705,
1,224,904 file bytes and 179,040 resident bytes. Play 0.72 HLE produced the exact
PASS with empty guest stderr in a bounded 15.115-second observation, then was
stopped and reaped. The source/build/ELF/log identities are bound under
build/ps2-native-main-car-pose-ee-001/runtime-validation.json. This is not measured
fixture completion time, physical-console evidence or a live paired helper capture.

Native016 passes its 50-file dependency/symbol audit and binds current sources.
It retains the executed static-study ELF hash
2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158;
the new helper remains disconnected and linker-discarded in that diagnostic.
Shared engine identity remains unchanged. The helper has a source-only CI fixture.
The final portable suite passes all 140 PS2 tests in 28.926 seconds, recorded in
build/ps2/validation/main-car-publication-tests-001.log.

The scoped main_contact_pass_recovery.md study identifies ROM 0x34380..0x34cd8
as a four-point candidate-contact pass (2,392 bytes, 496 static sites), not a
small result adapter. It records exact state accesses, query sequencing and
unrecovered transforms/reply interpretation/response aggregation. The complete
fourth point is available at the pre-call boundary 0x346d8; an earlier proposed
capture boundary preceded its final store and was corrected in the metadata.
Point slots are not labeled as wheels and no generic suspension is substituted.
Firmware command 0x5b's separately rounded cross product is the smallest next
candidate; its native implementation/differential is still pending. The larger
normalization/square-root path depends on original tables and needs its own
bounded contract. A future observer must capture complete main-pass inputs,
interpreted replies and outputs before claiming its behavior recovered.


## Native steering, orientation and course-contact recovery (2026-10-24)

The preceding checkpoint e7b58839 is published; all six triggered workflows,
including general Build, PS2, Vita, Metal and both Vulkan workflows, passed.
This checkpoint continues the native-runtime exception: typed game behavior,
separate private original-code comparisons, no arcade processor/device runtime
in the native application. The reusable engine's hardware sources are unchanged.
The diagnostic remains a static geometry study; no playable driving loop is
connected and no physical-console or gameplay speed claim follows.

Five additional typed components are integrated: alternate orientation,
projected contact, complete course-polygon search, cabinet byte conversion and
its stateful steering filter. All game semantics stay in the game source role;
reference firmware, generated code, address-based adapters and captured data
remain offline. Exact contracts and bounds are documented beside the components.

Alternate orientation now preserves the actual candidate-fold/wrapped-difference
path with its separately rounded binary64 smoothing and middle-channel step.
It has no pi/8 final clamp. Native integer arithmetic preserves signed zero and
gradual underflow independently of the EE FPU. Synthetic plus live comparison
passes 93,977 evaluations / 93,937 unique inputs, fingerprint f1d52dd19971b0f0.
All 34 live states are within the explicit domain; 26 middle-channel snaps and
8 steps occur, but candidate wraps are covered synthetically, not by this replay.
The EE probe passed 3,728 cases, 3,638 accepted, fingerprint f9e5c92050970893,
plus aliases and 60 atomic rejections. ELF ca3b1a51d0fa86f80b03db850ae21624a614382a735267024dd416ae2241562b;
1,229,060 file bytes, 182,896 resident bytes. The 15.066-second Play observation
and empty guest stderr are bound in its private runtime-validation.json.

Projected contact preserves each rounded difference/product/cross and the
original all-same-sign-bit acceptance rule, including signed-zero behavior.
Its domain is three/four XZ vertices and a point bounded by +/-2^20. It does not
repair winding, choose the nearest surface or implement collision response.
100,774 cases match the original firmware (5,791,705 instructions), fingerprint
128f72100657ddf0. Host mathematical and sanitizer comparisons pass. EE execution
passed 4,870 cases (402152f180506e07) and 64 atomic rejections; ELF
c499c0944e9a1ac557b865111661f16b0c59bc2b213cb97c71381e7d75879b2f,
1,226,272 file bytes, 180,576 resident bytes, empty stderr in a 15.073-second
observation. These windows are not measured completion times.

The complete typed course search tries current, next, previous and returns the
first projected match, copying all 16 geometry/coefficients words and attributes.
A valid miss clears the result; invalid encountered input leaves output untouched.
Its immutable spans have explicit segment/count/coordinate bounds and contain no
command transport, mapped pointers or persistent device result cache. The CPU
adapter's subsequent height/normal/status interpretation remains separate work.
Synthetic original firmware comparison passes 12,323 searches, eight retained
result slots and 3,550,622 reference instructions, fingerprint ccd788d0a091b5f8.
This includes all 32 attribute bits, all bits together and 255-polygon late-hit
and miss cases. Final source-bound report: build/ps2-native-contact-search-005/result.json.
Initial harness pointer assertions confused the saved pair order and omitted
the original 0xff800000 sign extension; actual instruction/pointer observation
corrected the offline harness without changing native code. The earlier data
study likewise corrected relative versus absolute subordinate asset offsets.
The observed query argument is a result-slot selector, not a collision mask.

All 34 live queries match selected native polygons and every one of their 578
reply words against firmware and captured results. All 227 visited candidate
predicates agree too. The native search EE fixture passed 192 selection cases
(37f1a2cc4dd9eb9b) and ten boundary checks; ELF
cb0ba641fe5e52fa1f0ae4387663f21bc93dcedc9f80e625130d40bb55172dfe,
1,228,244 file bytes, 182,112 resident bytes, empty guest stderr during a
15.090-second Play observation. Independent production review found no blocker.
Capture-reader regression tests found a cross-stream chronology gap between
query completion and contact entry; the final reader rejects it and all six
new parser/provenance tests pass. The final source-bound live report is
build/ps2-native-contact-query-live-004/result.json, with result fingerprint
efc0379c98c1055f and predicate fingerprint f8630eb1a6dc71b0. The separate native
host/sanitizer fixture passes 12,288 selection cases and 12 boundary checks.

Cabinet byte mapping passes exhaustive original comparison for all 256 inputs
(7,263 instructions), fingerprint 466a329bdf779c3a, and all 32 sampled live calls.
The EE fixture reproduces the same fingerprint; ELF
5b1ffd567726386eb29f574ae268252d6c95bb7ceea1c7afda9520fd3b965d7a,
1,222,476 file bytes, 177,120 resident bytes, empty stderr in 12.038 seconds.
One live leaf call includes 74 extra interrupt instructions (101 total); the
initial reader rejected that valid span. The corrected reader records a bounded
interrupt-inclusive span separately from the isolated 27..30-instruction cost.
The stateful filter separately matches 68,160 evaluations / 50,001 unique inputs,
including 18,432 sequence updates and 32 live pairs; fingerprint 359512f016c2d54d.
The live pairs include actual subnormal decay histories. Initial EE compilation
found a fixture portability issue: unsigned int and uint32_t have different
underlying types on this SDK. That is a test-state declaration, not native math.
An explicit uint32_t declaration fixes it. EE002 passes 7,168 evaluations,
2,048 sequence updates (335d3a428df4150d), two aliases and 2,560 rejections.
Its source-bound runtime record verifies ELF
a8fc42139c59289663608713e70fc982a9120b4725dcf6531e80aa44045cce10,
1,226,732 file bytes, 180,712 resident bytes and empty guest stderr during a
20.243-second Play observation. Independent capture-reader review found a
missing caller-frame proof/count check in the first filter adapter; the final
reader validates all 16 saved words, RIP/frame/cache identity and completion
counts. Final source-bound synthetic/live reports are under
build/ps2-native-steering-filter-oracle-final-001 and
build/ps2-native-steering-filter-live-final-001, with unchanged comparison hashes.

Capture 007 passes all 2,850 enabled/disabled frame checkpoints with unchanged
source/executable/input identities. It completes 1,202 alternate/contact/orientation
calls, 34 detailed groups, 5,409 callback observations, 733 steering-leaf calls
and 601 complete filter calls (32 sampled pairs for each steering stream).
Failed captures 005/006 remain failed: an interrupt changed the child-frame r2
that the observer incorrectly treated as a permanent return address. Actual RET
restores the saved caller frame's RIP. The observer now validates the matching
register-cache frame or exactly 16 whitelisted saved RAM words and derives the
return from that frame. At call 1106/frame 2752, raw r2 is 0x5c9cc0 while actual
caller RIP is 0x5cd8a8. Pairing, completion checks and saved-frame proof remain
strict; the observer does not modify game state or read FIFO/device addresses.
Ten portable observer tests and eight real-observer sanitizer scenarios pass.

The callback evidence also corrects the development priority. Object 0x500000
runs the separate ROM 0x34f40 callback once per observed frame (601 calls);
objects 0x500100..0x500700 run the other dispatcher. All 34 detailed snapshots
retain the main pointer/callback identity. The cabinet filter returns into that
main callback, and its result matches main-object +0x74 in 26 snapshots across
13 matching sampled frames. This is an input association, not an inference
from allocation order. The main routine is 8,884 bytes / 1,713 static instruction
sites and has its own dependencies and distinct pose-publication contract.
See platform/ps2/native/main_vehicle_recovery.md. Recovering the alternate alone
cannot make the controlled car native. Full main update, collision interpretation,
forces, game geometry/camera, audio events and native-loop connection remain.

The final native015 target compiles the five new components and passes its
48-file dependency-direction and forbidden-symbol audits. Every recorded source
hash still matches. Its 2,274,240-byte ELF remains byte-identical to the executed
native011 static study (2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158),
with 593,080 resident bytes before dynamic allocations: unused recovered leaves
are removed by the linker. Their actual EE evidence comes from standalone
fixtures, not the unchanged diagnostic. The 21-input shared engine manifest
remains 8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2,
matching the independent engine-demo source-bound smoke. CI cross-links all
five new source-only native fixtures alongside existing checks. README and
native/engine guides retain the explicit unconnected-gameplay limit.

The final portable PS2 suite passes all 138 tests in 28.377 seconds; log:
build/ps2/validation/native-car-contracts-tests-001.log. All five new native
fixtures executed successfully in bounded Play runs, which were stopped and
reaped. Independent arithmetic, capture-reader and architecture reviews are
complete. Original firmware/program/data, generated references, screenshots,
captures and compiled binaries remain excluded from the source checkpoint.


## Native driving contracts and actual caller recovery (2026-10-24)

Checkpoint 48294f1 is published and all five triggered workflows passed,
including the previously pending general Build. The next work follows the
governing native-runtime exception: typed game behavior and separate offline
original-program evidence, with no arcade processor/device runtime in the
native target. These changes do not connect a playable race loop.

Two additional game components are independently recovered. The planar
direction blend preserves the original rounded dot, upper-capped weight and
separately rounded interpolation for component pairs in [-1,1]. Original
bounded integer dyadic arithmetic avoids EE floating behavior, preserves signed
zero/subnormals and allows output aliasing; outside-domain inputs reject without
mutation. It is neither normalization nor a conventional clamped interpolation.
The private original-instruction comparison passes 305,824 cases, 303,888 unique
quadruples and 5,607,871 instructions, fingerprint 07c477eae4944c20. The first
oracle attempt omitted RAM burst semantics for a four-word store and repeatedly
wrote one address; correcting the adapter resolved that test-harness defect.

The contact-neighborhood leaf selects cyclic previous/current/next indices for
explicit counts 1..65,535 and signed-halfword inputs. This is query preparation,
not intersection, height, normals or forces. Its private full-query differential
passes 524,288 cases and 10,529,461 instructions, fingerprint 874c2bf9c8bbc6d6.
The prior map incorrectly called global 0x214354 a pointer: it is a selector into
eight inspected 12-byte descriptors. Actual command-7 firmware queries confirm
counts 300 for selectors 0..4 and 10 for 5..7, each with one coprocessor-data word
read; 216 reference instructions total, fingerprint 4669617b8d3deeef. No descriptor
or firmware bytes belong in Git. Final private count report is
build/ps2-native-contact-count-final-001/result.json.

Both leaves passed actual EE execution in Play 0.72 HLE. Direction: 6,032 probe
cases (dd9701872b4776e2) and 36 invalid-input cases, two aliases each; ELF
5e33018d3b606d200a13e1ca1147d9f891a69411441e8239c5dff0d1254b1794,
1,224,704 file bytes, 179,040 load-segment memory bytes. Contact: 1,179,642 checks
(219618ade1210792); ELF
c8dc243bdac15cf98b3287ea20af63cb7e5703abe9a5f0aedf595154f0cd0e48,
1,222,516 file bytes, 177,000 load-segment memory bytes. Private runtime reports
bind frozen sources/headers, build reports, ELF and guest logs under
build/ps2-native-direction-ee-001 and build/ps2-native-contact-ee-001. Both guest
stderr logs are empty. The contact fixture produced no result in its first
12.077-second observation; a 45.199-second run obtained PASS. Direction's
observation lasted 12.033 seconds. These are bounded emulator windows, not
measured fixture completion times or physical-console performance.

The new host-only observer uses the real nineteen-event Lancia cabinet replay,
the verified Revision B program/asset pack and the existing exact instruction
boundary. Reads are bounded to ordinary RAM, never device/FIFO reads. A separate
host executable records input events, per-frame checkpoints and private images;
neither its observer nor converted data is linked into the native executable.

The first capture failed with zero primary-integrator entries despite visible
race countdown and driving. The second observed 545,813,594 instruction boundaries
but zero entries at both the original-ROM and relocated primary addresses. This
was a sampling-scope error, not proof the game lacked driving work. Coverage 003
established the branch: object+0xe8=0 calls 0x2cce0; 1 calls 0x2e650; 2 uses an
inline path; other values take another branch. Across completed frames 2200..2800,
the alternate and shared velocity/blend each ran 1,202 times; the primary and its
orientation tail ran zero times. All 2,850 enabled/disabled frame checkpoints
matched. Capture 003 correctly remains failed as a primary-call capture despite
that observer comparison. Do not label branch values as car type or player/AI
without additional evidence.

The alternate's first return is 0x2eee4. Initial analysis accidentally extended
past it into the next function and misattributed a later orientation region;
the corrected routine spans 0x2e650..0x2eee7. Its actual orientation region
0x2ecb4..0x2edff folds a stack candidate triple, then uses wrapped differences,
binary64 smoothing and a separate middle-channel step. It does not have the
primary's pi/8 bounds. The middle-step constant was loosely compared with
pi/180 but is not equal to it; no units claim follows. Exact identities/math
are in platform/ps2/native/velocity_step_recovery.md. The existing finalize_orientation
must not be substituted. A new primary-tail capture adapter was tested on
synthetic records, then retained only in ignored
build/ps2-native-orientation-capture-study-001 because this replay does not
exercise its contract. It is not shipped or counted as live validation.

Query observation also needed correction: a fixed return site cannot pair
queries from nested helpers or the alternate caller. The observer now follows
each actual saved return PC/frame pointer. Native contact/direction comparators
require complete event sequences and the successful observer-comparison report
with exact capture hashes before accepting live evidence. A diagnostic capture
without its completion marker or successful parity report is not valid input.

Capture 004 passed the complete 2,850-frame enabled/disabled comparison. Its
private validation.json and cadence-summary.json bind all original/generated
inputs, executable, checkpoints and captures. The 1,202 completed alternate
calls belong to two objects, 0x500100 and 0x500200, each once per completed frame
2200..2800 with selector 1. This is not two simulation substeps or a 120 Hz claim.
Five other observed objects use selector 3; player/AI ownership remains unknown.
The 32 detailed calls produce 288 events and no primary-orientation events.
object_words is a 0x400-byte raw RAM window, not a proven object size; addresses
are 0x100 apart. Optional out-of-range dependency blocks are omitted. These
snapshots are not a complete integrator input schema.

All 32 live blend groups (29 distinct inputs) agree bit-for-bit with the native
component and isolated original execution. Every sampled component is in the
supported domain and arithmetic control uses nearest rounding. The combined
synthetic/live comparison executes 305,856 cases and 5,608,447 original
instructions, fingerprint 75876bd959ec4992. All 32 live contact queries also match,
fingerprint f04c0719a03c245f: selector 0, count 300, signed indices 12..39, return PC 0x5cd898
in the alternate. Reports are build/ps2-native-direction-live-001/result.json and
build/ps2-native-contact-live-001/result.json. Full caller domains, independent
PCB fidelity, collision/intersection arithmetic and game integration remain
unproven. Public reproduction instructions are in platform/ps2/tools/driving_capture.md.

Both new modules are explicit game-role sources, separate from the shared
engine. The native build retains its no-arcade-runtime boundary. Original EE
fixture manifests are tracked, with CI configured to cross-link them; generated
code, captured data/images and compiled artifacts remain private. Next work
follows the actual alternate integrator: recover its geometry-query dependencies
and contact results, identify the player object's inputs, then establish the alternate
orientation arithmetic before connecting the real native driving loop.

Final validation passed all 109 tests in tests/test_ps2*.py in 26.012 seconds.
The native013 full build succeeded: ELF SHA-256
2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes and 593,080 load-segment memory bytes. The ELF is
byte-identical to native011/012 and therefore to the already executed image.
The unused new leaves are removed by linker garbage collection; this build
does not establish new gameplay execution. The private identity report is
build/ps2-native-013/runtime-validation.json. Independent documentation review
found a stale latest-build paragraph referring to native010; it now binds the
current native013 evidence. Final reviews found no remaining blockers.
Publication requires the exact source-tree/archive check; remote
CI has not yet run for this change.

## Reusable native PS2 engine boundary (2026-10-24)

Checkpoint 9de64d72 is published; all six remote workflows passed, including
the general compiler matrix, source gate, PS2, Metal, both Vulkan checks and
Vita. Its final native car image and GS framebuffer capture remain private.

The PS2 service layer is now explicitly intended for additional games, with
Rally as one consumer. The governing Platform layer & build exception and
new docs/ps2-engine-architecture.md record this decision, service ownership,
current limits and the sequence for further extraction. Existing source paths
are retained; the build now assigns reviewed engine, game, study and application
roles and checks dependency direction before linker garbage collection can
hide an unused import. The original engine-demo application may use only its
own role and the engine. Offline recovery and captured-reference-view mesh
semantics remain outside that engine.

Both applications record the same complete engine source/header/IOP identity.
CI checks both targets and compares that shared manifest. This is source
identity, not a claim of identical compiler settings, objects or runtime cost.
Direct external SDK calls remain permitted in application startup; this gate
is not a general isolation/security proof or an enforced lifecycle facade.

Independent review found a real first-pass boundary hole: resolving a declared
engine source symlink to an already allowed game source could erase its role.
Two regression cases reproduced acceptance in direct validation and native
smoke. Declared inventory identities must be checked before resolution, not
only the resolved dependency union. This finding is distinct from the already
covered engine-header-to-game-header indirection case.

Further review reproduced a second hole involving a symlink followed by `..`:
lexical normalization before physical resolution can check a different header
from the compiler. The gate now compares original spelling resolution against
the normalized declared identity. Fifteen focused engine-boundary tests cover
the accepted inventories and rejected directions/redirects, including unused
units and headers. Do not replace these checks with a union of resolved files.

The original sample constructs its own animated PSMT4 checker cube, uses the
bounded clipper, VU0 final XYZ and direct GS Z16S, and opens the shared pad/PCM
services. It imports no Rally leaves, captured meshes or external assets.
Left stick X/Left/Right rotate, Square toggles automatic rotation, L1/R1 change
distance, Cross supplies a quiet original tone, and Select+Start toggles the
information page. Animation time is a sample policy, not recovered game timing.
PadSnapshot's physical axis is now left_x; cabinet steering stays in its adapter.
The field rename preserves byte layout and input behavior. Mocked pad tests
pass and the original portable bringup smoke keeps hash 7fc55e0bfbc7af31.

The preliminary engine-demo-001 build succeeded before the renderer resource
fix: ELF 550d90be1b7bf4e935f61a50e1f26bc6a3086952000f0ff159c4e229ae0ee3c9,
2,077,636 file bytes and 460,456 resident bytes before dynamic allocations. It
was not executed and must not be cited as the final resource-contract build.

The resource review identified two concrete reuse hazards. Texture cache keys
retain address/dimensions/generation even after caller storage is freed; a new
asset at the same address and generation can select stale VRAM. Explicit
invalidation is needed before reusing that identity. A second Display could
also reset the process-wide VRAM allocator under an active owner; initialization
and failure cleanup need a single-owner contract. These are resource-lifetime
fixes, not a new scene graph or general allocator framework.

Both resource fixes are implemented. Display initialization is caller-serialized,
idempotent after success and limited to one GS/GIF/VRAM owner. Failed setup frees
CPU buffers and releases the lease for retry. Fatal reverse-transfer retirement
quarantines subsequent Display instances; that fatal branch was reviewed, not
fault-injected. invalidate_textures() clears index-cache identity before storage
reuse; copied-content CLUT entries remain valid. Normal generation updates keep
working, and the frame rendering path is unchanged.

The original resource fixture uses link-time SDK wrappers only in its private
test link, with no production injection hooks. Play 0.72 HLE/Vulkan completed
464 checks, 24 injected initialization failures and 320 exact GS pixel samples,
fingerprint a6f9c6418b875dc3. Failures cover packet/staging allocation, GIF setup,
every VRAM allocation class and video-mode setup, followed by retry and owner
replacement. The first build attempt was sandbox-denied; the approved second
build succeeded and ran. Later helper review found that post-link source or
manifest drift could leave a success status in a failing report. The helper now
records build_failed; regression tests exercise both drift cases and success.
The final resource003 rebuild is byte-identical to executed resource002:
49e3c25755910cd71c6254e51c413af86d2299b421d3a580c29299ebf0fbfc9b.

Final engine-demo003 ELF:
c1b90b77cd2a3288cb9a1bf1c2bbafcdd556fd43c5304caf0bf17ac29c0564e9,
2,084,180 file bytes and 467,888 load-segment memory bytes. It is byte-identical
to executed demo002. The 12.237-second engine-demo-001 run reached logged frame
660 with 12 triangles and initialized audio; later sampled frames reused cached
texture/palettes without upload. Its actual window capture shows the original
checker cube. No interactive pad or speaker test was performed.

Final native012 ELF:
2338119372579ee7012ec95fcd43f5210d8382cb112d6f4dbce9d3fc8f050158,
2,274,240 file bytes and 593,080 load-segment memory bytes. It is byte-identical
to executed native011. The 10.212-second engine-native-car-001 run submitted
1,040 static car triangles through logged frame 480 with initialized audio.
Both applications have the identical 21-input shared engine manifest:
8bfba3e1904f90f61cec9230f3ea3f2a66013a37a6a7257d8dbe9055be1094e2.
Resident figures exclude dynamic allocations; emulator cadence is not game or
physical-console performance. All runs were bounded, stopped and reaped, with
empty guest stderr and successful window captures.

The scene005 fixture rebuilt with the final driver is byte-identical to executed
scene004 (90e24fe6616a41d773444a57460cd21de5a947c9efbd4203218d05ae06ebffc3).
The engine-car-regression-001 run completed in 10.263 seconds and read back the
entire 640x448 RGB car frame. Its 860,175-byte PPM is byte-identical to the prior
published scene003 result, SHA256:
800d51d7ec0d6f9fb425a253cfac61a40e156a7e569705ab4c42ac0ebacbe0ce.
Private runtime-validation.json files in final demo003, native012, resource003
and scene005 directories bind final source/build inputs, exact executed ELF
identity, emulator reports, logs and captures. No game capture is exported.

The final portable PS2 suite passed 94 tests in 23.439 seconds; private log:
build/ps2/validation/engine-reuse-tests-002.log. The earlier 90-test suite passed
before the second redirect case and three late-drift orchestration tests were
added; the 94-test result supersedes it. CI now cross-builds the second app,
compares both engine manifests and cross-builds the resource fixture. These
checks establish engine reuse and regression evidence, not finished Rally
gameplay, texture recovery, VU1 production integration or console validation.

Known architectural limits remain explicit: logical 496x384 viewport, partial
physical controller state, 44.1 kHz float input with 48 kHz output, blocking SDK
waits, and startup-dependent IOP reset ownership. The current study's RLYMESH
and captured object_to_reference_view matrices are not renamed into a generic
world-space asset format. Centralized reset/session ownership, configurable
viewport, full controls and native-rate transport require separate validated
changes. Do not add an ECS/plugin/task framework without a real consumer need.


## Native PS2 model-space geometry study (2026-10-24)

Checkpoint 1a426ba is published on port/ps2. All five triggered workflows
passed, including every desktop compiler and source gate, PS2, Metal and both
Vulkan workflows; Vita was correctly excluded by path filtering.

The next native milestone connects an owner-provided static mesh to the EE
transform/clip path and direct GIF/GS renderer. It remains a geometry study:
flat diagnostic colors and lighting, fitted/orbit camera, no recovered game
camera, textures, collision, driving, race progression or game audio. Both VUs
are inactive for these solid triangles. The original checkerboard mode keeps
its validated VU0 final-XYZ conversion. The native fixed inventory, transitive
include checks and ELF symbol audit still exclude the arcade runtime.

An opt-in host-only geometry hook captures object-local source words before
transform/focus/culling, preserving each captured affine transform separately.
A bounded independent decoder reconstructs hidden connectors, all link modes
and face topology. The exporter re-derives its records from raw words, verifies
ROM-backed streams against the owner's polygon image and writes the explicit
version-one mesh format. The complete 1,634-frame capture run matches every
capture-disabled reference checkpoint. Private final capture evidence is
build/ps2/native-mesh-final-001/capture-validation.json; workflow and limitations
are in platform/ps2/tools/mesh_capture.md. No capture or mesh belongs in Git.

The capture contains 56 objects (52 ROM-backed, four RAM-backed), 7,494 raw
points and 5,816 source triangles, with no direct/empty exclusions. Ordinals
1..6 form a recognizable body/interior/four-wheel subset: 1,302 vertices,
1,040 triangles, six objects and 32,808 bytes. SHA256:
1def378046fcc070e7fd6e11b7a888b3b342df7eb9d019e50f5ad68eba547d73.
A host software preview was shown explicitly as a host preview, not a PS2
screenshot or proof of the exact car identity. Whole-capture export correctly
rejects object42's out-of-domain background coordinates; do not present this
selected subset as complete course recovery.

The transactional loader caps vertices/faces/objects, exact payload size,
indices, partitions, CRC and numeric encodings before publication. Its 4,527
host/sanitizer checks include independent serialized fixtures, all truncations,
corruption and allocation failure. Scene preparation separately validates typed
meshes, transforms captured view-space bounds, fits a diagnostic camera and
clips into at most 16,384 triangles; capacity failure does not truncate or
replace the previous scene. Host projection and failure checks plus sanitizer
runs pass. New geometry output includes normalized reverse depth.

GS manual v6 printed p26 rejects CT32+Z16; the correct 16-bit pairing is
PSMCT32+PSMZ16S. The implementation uses this pair, ZTE=1, and masked ALWAYS
for painter/debug work. A depth scene clears to zero then uses GEQUAL. VRAM
allocation is 3,424,256 bytes at NTSC448 and 3,833,856 at PAL512. The exact
integer depth quantizer passed 206,609 host/EE checks with fingerprint
ce7160f9d89dd495. Depth001 executed all visual phases, but all sixteen macOS
window captures failed, so these logs alone establish no GS pixel result.

Review caught a status-caption bug before the car run: the persistent scene
caption could cover an error page. present_status now bypasses decoration.
Native007 was built but never run; native008 includes this correction. The
same correction leaves depth002 byte-identical to the executed depth001 ELF
because its unused status function is removed by the linker.

Native008 ELF SHA256:
4410bb1563dabb27dad013ecf89ae17bbd9623f9479f9b500799add64f8c63c7.
It is 2,272,456 file bytes with 592,048 load-segment bytes before dynamic
allocations; all 34 project input hashes and the symbol audit pass. In Play
0.72 HLE/Vulkan it loaded and submitted all 1,040 car triangles through frame
1,080 in a bounded 20.198-second run; guest stderr was empty. Scene persistent
storage is 1,324,224 bytes, excluding retained mesh and other services. Initial
preparation took 52,295,680 emulated bus ticks; repeated static submissions
must not be described as game performance or physical console speed.

The same ELF passed the no-asset bootstrap regression for 17.135 seconds,
through frame960, with two triangles, initialized audio and zero sampled
ring-empty/late-service events. Both owned processes were reaped. All three
window captures failed with 'could not create image from window'. Exact frozen
source, ELF, asset and run bindings are in build/ps2-native-008/runtime-validation.json.
The private study mesh was restored to Play's host:native-scene.bin afterwards.

All 74 portable PS2 tests passed in 23.728 seconds before the final display-only
status correction. Independent final review found no loader/scene, offline-hook
or source-boundary blocker. Physical PS2, interactive pad/speaker behavior and
full-game fidelity are still unverified. The next work is actual GS pixel
inspection, original material/texture recovery, and typed collision/camera/car
contracts before connecting a native race loop. Do not re-propose captured
frame playback or substitute generic physics as recovered gameplay.

Final validation supersedes the earlier capture limitation. Native009 produced
a successful window capture visibly showing the car body/wheels and study
caption in Play, in a 10.062-second run with empty stderr. Native010 binds the
final 34 input hashes and reproduces that exact ELF, SHA256
1464d12f8ca51588dfbcd4fd5e6c636e69b84927b1b40481a46bcc8106a5fb8f;
2,273,340 file bytes, 592,304 resident bytes before dynamic allocation. Evidence:
build/ps2-native-010/runtime-validation.json. The emulator's visible 59f/s
counter is repeated static rendering, not measured native gameplay throughput.

The optional Display readback uses bounded GS local-to-host/VIF1 reverse DMA,
FINISH fencing, cache synchronization, exact completion and empty FIFO checks.
It returns pixels only on success. A timeout stops owned DMA and quarantines
the reverse bus; it never flips direction with unread data or permits another
draw. If DMA cannot stop, its buffers remain allocated until restart. Independent
protocol review caught and corrected the initially proposed unconditional
BUSDIR restoration: stopping DMA does not imply the GS FIFO has drained.

The first two transfer attempts correctly rejected D_CTRL=0; the other captured
settings were D_PCR=4, D_ENABLER=0 and D_STAT=0x00200004. SDK channel initialization
does not set DMAE, and Play0.72 resets it to zero while allowing draw transfers.
The standalone fixtures now explicitly set that bit, preserving other settings;
the production readback gate remains unchanged. The new wrapper's first
sandboxed attempts lacked Docker socket access; approved retries succeeded.

The final depth fixture passed all 206,609 quantization checks and 512 exact RGB
samples with zero mismatches, fingerprint d1d172cf244cce83. Solid/textured
regions passed opposite draw orders, the diagnostic page, far-only blue after
clearing, and reversed draw orders returning near red. Five invalid readback
requests preserve output. Executed ELF SHA256:
415047e1bbd1eafeceb1e0f76af9ffa0afc3b155763ec7b8ae9d2ebf39706264.
Evidence: build/ps2-native-depth-wrapper-004/runtime-validation.json. This is
sampled original-region validation in Play, not whole-image/game fidelity.

The separate scene fixture successfully read a complete 640x448 GS framebuffer
in 640x16 strips and wrote a private PPM, with the same mesh CRC/counts. It ran
for 12.081 seconds with empty stderr; the car is visible in both raw GS output
and the corresponding window capture. Executed ELF SHA256:
d99a1246e53a81310335fbcfb4be422b65debf9d12b9ef47ee89a99acb060ffa.
Exact file/run/input bindings are in
build/ps2-native-scene-wrapper-003/runtime-validation.json. This diagnostic
image precedes TV filtering and is not published. The existing output remains
in Play's host directory; the fixture deliberately refuses to overwrite it.

build_native_depth_fixture.py provides fixed depth/scene inventories, confined
new output directories, pinned tools and input/manifest drift checks. Both
modes cross-link locally and are now cross-built by PS2 CI. Portable frame,
geometry and scene tests were rerun successfully after the readback API change.
README and PS2 entrypoints now distinguish the static study from gameplay.

## PS2 transfer completion, geometry and orientation (2026-10-24)

Checkpoint b6f0064 is published on port/ps2; all six remote workflows passed,
including every desktop compiler, PS2 native/audio/vector cross-builds, Metal,
Vulkan and Vita. The push response eventually timed out after publication;
GitHub's ref and a fresh fetch independently confirmed the exact commit, and
the local remote-tracking branch is synchronized. Its exact tree/archive
passed build/ps2/validation/manual-study-source-001.json.

The subsequent long VU1 timeout is now isolated to Play 0.72's final-qword
VIF/DMA resume path. A stalled command can remain in the prefetched final
qword after QWC reaches zero and STR clears; later DMA resume is gated by STR
and CountTicks does not drain the retained VIF buffer. Appending one optional
qword of VIF NOPs after the unchanged FLUSHE/MARK allows the same microcode,
workload and timeout to finish. This is an emulator transport diagnostic,
not a requirement of PS2 hardware. The no-padding failure remains preserved.
Primary-source functions and exact hashes are recorded in
build/ps2-vif-tail-study/source-findings.json. No emulator implementation was
modified or imported.

The padded fixture passes 300 VU1 submissions, 700,416 complete-memory word
comparisons and 12,400 independent VU0 fixed-coordinate checks. Fingerprints
are 03ca33026fb14bc9 (VU1) and a7a419b86e81da5f (VU0). Its ELF SHA256 is
c8b537fac69c78b5adb2042278b6688d306f1738b5d308d9ac978a1c7353cb48.
Eight samples retain a pending MARK before and after VU0 work. This proves
work before pipeline completion, not simultaneous instruction issue or speed.
Play's CFC2 VPU-STAT case explicitly returns architectural zero, so busy-state
observation is separately INCONCLUSIVE. The 1,794,216 requested repeated
vertex evaluations are neither unique cases nor measured throughput; checking
final outputs can miss transient errors overwritten by later repetitions.
Evidence: build/ps2-dual-vu-resume-001/runtime-validation.json. The default
baseline also reran successfully with its unchanged ELF and fingerprint;
see build/ps2-vu1-resume-baseline-001/runtime-validation.json. Select both
--dual-unit and --trailing-nop-qword to reproduce the passing combined test.

A fourth typed native component recovers the integrator's final orientation
tail at ROM 0x2e560..0x2e638. It folds all three bounded channels, then clamps
only first/third to signed round32(pi/8), with the observed strict binary64
threshold. Integer bit operations preserve exact final values and atomic
rejection. Pitch/yaw/roll names, axis conventions and caller bounds/cadence
remain unproven; it stays disconnected from the diagnostic loop.
The private original-tail comparison passed 294,300 evaluations (294,260
unique triples), 882,900 channel operations and 25,111,815 original instructions;
fingerprint 40f5b5992d4371cc. Host tests and sanitizers pass 321,331 checks.
A standalone EE fixture passed 27,031 checks in Play during a 17.084-second
run, ELF cc3fbf750b7876d10caf39017d75842bd78a29fc10cd99901ab54a61312bd21b.
Evidence: build/ps2-native-orientation-validation-001/result.json.

The new original native geometry reference clips seven homogeneous planes,
interpolates raw texel UV before reciprocal depth and emits ordered native
triangles through fixed scratch/output staging. Invalid data, insufficient
capacity and numerical failures leave caller storage unchanged. This explicit
platform convention is not a recovered game camera/projection contract.
Host tests and ASan/UBSan pass 280,831 checks, including 20,000 triangles
against an independent exact rational barycentric oracle. The EE fixture
passes 322 selected checks with the matching fingerprint fe8dc06b88de7a24;
ELF b74b7e906f5d7c45377795f259a46b380b28c9160ade6d43de3658c1fbbd171b.
Disassembly confirms software-double helpers and a 2,304-byte own stack frame
before callees. Its cost is unmeasured; it remains outside the active renderer.
Evidence: build/ps2/validation/native-geometry-001.json; contract and VU output
budget: docs/ps2-native-geometry.md. Do not fit 96 input vertices plus worst-case
clipping expansion into a 96-output-vertex bank: it holds 32 output triangles,
or only four input triangles at the eight-fan worst case.

The final local PS2 suite passes all 68 tests in 22.174 seconds. Both new
modules are in the native source inventory; main remains unchanged. The
inventory-only driver update reproduces the executed padded VU ELF byte for
byte in build/ps2-dual-vu-inventory-002, so no additional numerical run was
needed for that artifact. The initial sandboxed attempt lacked Docker socket
access; its failed output remains preserved separately.

The native-006 bootstrap cross-links with 30 verified project input hashes
and a passing forbidden-runtime symbol audit. ELF SHA256:
52ab97be39d4897c48e637f25a4deb780c183b613628514c56516cf497deeb18;
2,024,916 file bytes, 450,088 resident bytes before dynamic allocations.
It ran for 17.249 seconds in Play, with 19 samples through frame 960: two
diagnostic triangles, initialized audio and zero sampled ring-empty/late-service
events; guest stderr was empty. The owned process was reaped. macOS window
capture failed with 'could not create image from window'; do not claim a new
screenshot. The last successful native-004 capture was shown with that label.
Visual/gameplay content remains unchanged: diagnostic page and original test
geometry, no car/course or connected native race loop. Exact runtime evidence
is build/ps2-native-006/runtime-validation.json. Physical PS2, interactive
controller behavior and audible output remain separate checks.

## PS2 manual study, native arithmetic and service fixes (2026-10-23)

The completed native foundation was pushed as 69b4726 after the exact tree and
source archive passed the export gate. The requested hardware collection is
cloned at build/ps2/references/PS2-Programming-Docs, revision
b13fb347ceebd3f71a0ce978adb15b60c64ed6ec. All 18 repository files match the
local manifest: 16 PDFs, the pad specification and README (17,971,685 bytes;
2,470 PDF pages). docs/ps2-architecture-study.md records the reading map,
specific references and intended measurements. The manuals and searchable
extracts remain local, under the explicit named-collection study exception.

The car integrator lead at ROM 0x2cce0 spans 6,508 bytes and 1,299 decoded
instructions. Its 133 TGP stores and 52 reads are static sites, not per-update
counts. Descriptor/tuning inputs, contact-query semantics, units, cadence and
mixed-precision arithmetic remain unresolved. The new dependency map records
these gaps instead of labeling a generic vehicle simulation as recovered code.

Its one-step angle-folding helper is now independently implemented for finite
binary32 inputs bounded by round32(2*pi). Accepted values within ±pi preserve
their original bits; the outer interval receives one exact correction with
explicit ties-to-even rounding. Larger/nonfinite values reject unchanged.
This uses integer arithmetic only, with no guest CPU/FPU/device runtime.
The actual integrator caller bounds remain unproven, so it stays disconnected.
All 16,844,326 private original-program comparisons match (328,030,729
instructions; fingerprint 256956588cf6bc50), including every nontrivial folded
input of both signs. Portable/sanitizer tests pass. A standalone EE fixture
passed 36,262 checks in Play; object disassembly contains no COP1 arithmetic.
Evidence: build/ps2-native-angle-validation-001/result.json.

The manual study exposed a real SPU2 gain error: PCM input volume uses Q15,
whereas master direct volume uses Q14. BVOL is corrected from 0x3fff to
0x7fff; master remains 0x3fff. The IOP server now counts delivered completions
that find the prior half unserviced, separately from empty-ring silence. A
completion-generation guard also prevents stale semaphore wakeups from
dequeueing new PCM into an already refilled half. Protocol version 2 uses the
existing reserved reply word and rejects older modules. Actual-server mock
tests cover gain registers, callback ordering, stale wakes, wraparound,
channel layout, restart and all initialization cleanup failures; sanitizers
pass. Counters cannot detect a suppressed interrupt or prove audible quality.
Synchronous SIF RPC can still wait indefinitely if the IOP itself stops.

The pad adapter now distinguishes an accepted mode request from completed
configuration. It waits for completion, retries eventual failures, resets
pending state on disconnect, and retries missing mode metadata while allowing
valid digital input. Real adapter/mocked-SDK tests and sanitizers pass.
The native diagnostic page shows ring-empty silence and DMA late service as
separate values. Physical controller/audio behavior remains unverified.

The frozen native-004 build includes all three recovered modules and these
platform fixes. Its SHA256 is
8e089ac653569c56606ea66b453ad57a4ea6193050798e16866339b0a144f84a;
file size is 2,021,548 bytes, resident load segments occupy 445,992 bytes, and
the heap/stack ceiling is 32,059,864 bytes excluding dynamic allocation. All
26 source/header hashes and the forbidden-runtime symbol audit pass. The
game systems are still not connected to its diagnostic loop.

Native-004 executed for 17 seconds in Play through sampled frame 960, with
empty guest stderr, zero ring-empty silence and zero delivered late-service
events at all 19 sampled checkpoints. The original nonzero audio fixture also
submitted and consumed all 48,001 frames through the updated IOP module;
that fixture intentionally ran out of input and recorded 56,959 silent frames.
Measured speaker amplitude remains unverified. Runtime evidence is bound in
build/ps2-native-004/runtime-validation.json and
build/ps2/validation/audio-spu2-002.json. A subsequent status-only pad fix
labels SDK EXECCMD negotiation correctly and has its own mocked-state check.
The resulting native-005 ELF cross-links with all 26 input hashes and the
symbol audit verified: SHA256
b6495ccdc3498c1d948d3103e900c1c7d2feee725c09e5e42be7a52ad7cec5ce,
2,024,916 file bytes, 450,088 resident bytes and 32,055,768 bytes of heap/stack
ceiling before dynamic allocation. It has not been executed. Do not attach
native-004 runtime evidence to this later artifact.

Additional findings: the EE standard library correctly reports a non-IEEE
FPU, so the angle helper asserts the required storage properties instead of
is_iec559. Do not relax its arithmetic proof or replace it with native float
subtraction. DMA completion alone is not a VU/GS completion fence. The first
isolated VU1 experiment checks exact matrix cases and whole-memory bank guards
before any PATH1 drawing or production integration.

The original VU1 experiment subsequently passed 168 microprogram runs,
13,608 vertices and 430,080 complete-memory checks in Play, fingerprint
232e0bdb6fed9a5f. Its original sources are promoted under platform/ps2/tests/vu1
with a pinned standalone assembler/build helper and six output/source-boundary
tests. The promoted ELF is byte-identical to the executed experiment,
8e26bd695aae5729727236cbd05ac47d001f709c7d766da17837e37135948366.
It proves selected dyadic transforms/projection/packing and bank ownership,
including alternate-bank transfer overlap; it performs no XGKICK drawing and
does not enable VU1 in the game. Play 0.72 lacks TOP/TOPS MMIO read cases;
the fixture verifies XTOP values actually stored by VU1 and all memory instead.
Do not reinterpret the initial unsupported-read assertion as a geometry error.
The CP0 Count register increments every CPU clock, as the EE core manual
states on pages 70/72; do not use the initially assumed half-clock conversion.

The foundation's general desktop CI exposed a recording compatibility bug:
inserting sound_buffer_frames before instruction_boundary_hook shifted the
existing ten-boolean aggregate initializer. All four desktop jobs failed the
same two recording tests with a noncanonical manifest. The capacity member
now follows the original fields, with a compile-time compatibility check;
v1 decoding also assigns named flags. The recorder rejects nondefault capacity
and external image providers, which v1 cannot represent or identify. Default
bundle/reference checks pass (181 and 23 checks); an isolated M2_PAGED_ROM
variant passes 185 checks including provider rejection before access.
Evidence: build/ps2/validation/recording-ci-regression-001.json. Fresh remote
CI remains necessary. Existing full-core artifacts retain their historical
source identities; generated source caches and the separate native target
are unaffected by this header correction.

PS2 CI now cross-links the original dual-VU fixture with the pinned host
toolchain in addition to its existing portable, audio and native-bootstrap
checks. A cross-build does not replace the separately recorded execution.
The local PS2 host suite passes all 64 tests in 21.626 seconds; its log
is build/ps2/validation/manual-study-tests-002.log. The VU numerical cases use
clip W values of 1, 2 and 4; general perspective rounding is not established.
The helper subsequently gained an explicit dual-unit opt-in check; all seven
helper tests pass with that addition.

The first combined VU test preserved 12,400 VU0 components and 692,224 VU1
memory words across baseline/cooperation checks, but observed no outstanding
activity at VU0 entry. Play 0.72 executes up to 100 quotas of 5,000 VU
instructions synchronously at launch. A bounded repeated entry was added
only as a diagnostic: a two-iteration control passes, while the 1,024-iteration
control times out before any VU0 conversion call. Its MARK is 106 rather than
107, VIF status 0xc4 and command 0x14000025. Review found no demonstrated bank
or repeat-loop defect; the cause remains unresolved and is not labeled an
emulator bug. Raw busy snapshots are not established physical-idle evidence.
The default fixture runs the validated baseline; --dual-unit selects the
unresolved test without changing its timeout. Do not claim cooperation,
simultaneous hardware execution or performance gains. Repeated outputs may
hide transient errors, and vertex totals describe requested work only.
The current default baseline was independently rebuilt and executed with
168 submissions, 430,080 memory comparisons and the unchanged fingerprint
232e0bdb6fed9a5f. Its ELF is
build/ps2-vu1-baseline-final-001/vu1-transform.elf, SHA256
d185235be35d70a5611409c8614bd1ca18739636fa2209996bb1e9b5306c78d6.
Next: resolve the isolated long-entry failure and validate general projection,
clipping and bounded PATH1 submission before production VU1 integration;
continue car/collision contract recovery independently.

## Native PS2 rewrite boundary and hardware foundations (2026-10-23)

The current direction is a complete native PS2 reimplementation. The previous
translated machine remains a separate reference target, not the architecture
of the new game. This is an explicit PS2 exception in the design document.
`native-port` must not link generated i960/TGP/68000 code, interpreters, Model 2
buses/devices/scheduling, SCSP/DSP or the arcade frame adapter. Keep future
recovery tools and asset conversion offline. Do not turn the reference probe
back into the main port or substitute generic invented driving physics.

The native target has a fixed source inventory, transitive include checks,
forbidden-symbol checks and a source-drift guard after linking. It rejects
core manifests, generated/build source, symlink escapes and opaque include
macros. The independent renderer accepts native vertices/materials and packed
four-bit textures. It shares only direct PS2 platform facilities: GS/GIF,
validated final XYZ packing on VU0, controller state and original IOP PCM
transport. `pad_state.h` separates physical input from cabinet/runtime types.
CI now cross-links both the native bootstrap and the original audio fixture.

The bootstrap displays original diagnostic geometry and a readable 40-column,
23-line architecture page. Select + Start toggles the page and consumes both
buttons until both are released. The left stick moves diagnostic geometry;
Cross gates an original quiet 440 Hz tone. Audio refill follows the 48 kHz IOP
queue, not display cadence: the maximum 1,412 input frames fit the 2,048-frame
buffer. This is not a playable game or a game-performance benchmark.

The frozen native-003 ELF is
c8e4ae54b132df5ab75ab0dc103ee131a6de4592328c8c76c669ceac31d45eb6.
Its file occupies 2,020,964 bytes, PT_LOAD residency is 445,480 bytes, and
32,060,376 bytes remain above the load high-water mark for heap/stack. Dynamic
allocations are not included. All 24 project source/header hashes match its
build report. Both native-002 and the final native-003 ran for 17 seconds in Play
0.72: readable page, two submitted triangles, PCM transport initialized and
zero sampled underrun frames through frame 960 in the final run. Its 2,457,600-tick steady
diagnostic loops equal one modeled 60 Hz display interval; this says nothing
about completed-game speed or physical-console performance. Captures and logs
are in build/ps2/emulator/native-bootstrap-{002,003}-*.

Two independently authored native leaves now use typed value objects:

- `race_progress` decrements a positive signed countdown and increments two
  wrapping counters plus selected entries in two 64-entry banks. Invalid
  native indices reject atomically. All 32,768 private original-program cases
  match, covering 735,250 instructions and 163,840 ordered writes; fingerprint
  5acab57d56227b63. Actual lap meaning and call cadence remain unproven.
- `car_pose` publishes twenty raw payload words, an opaque progress word and
  the observed flag merge into a distinct render pose. All 65,536 flag-byte
  pairs with randomized payloads match 3,670,016 original instructions and
  1,572,864 writes; fingerprint f77a27e7080bcf13. This does not establish axis
  conventions, numeric interpretation, geometry transforms or car physics.

Both compare complete surrounding memory against the owner's verified
Revision B program, SHA256 bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439.
Portable tests and sanitizers pass. The native target compiles the modules but
does not invent a gameplay caller; unused functions may be linker-eliminated.
The host oracle/interpreter stays outside that target. Contracts, original
function hashes, offsets and reproducible commands live beside the native
sources. Private evidence is under build/ps2-native-{progress,pose}-oracle-001.
Both standalone native EE fixtures also executed in Play: 121 progress
boundary/rejection checks plus repeated updates, and all 65,536 pose flag
pairs. ELF/source/log identities are bound by
build/ps2-native-leaves-ee-validation-001/result.json. These are emulated-EE
tests, not physical-console or connected-gameplay validation.

Next in order: recover and compare car integration, collision/course data and
camera math; convert private assets into native layouts; connect a validated
race loop, opponents, menus and native sound events. Candidate function leads
are car update 0x2bcf0, yaw 0x2f3c0, motion 0x304f0, integration 0x2cce0,
contact 0x2f8a0, velocity 0x30e00, camera 0x1fdc0 and spline pose 0x422b0.
These are leads, not recovered contracts. Earlier lifted null guards and
nonfinite workarounds are not original-program ground truth. Complete-course
replays and physical PS2 tests remain necessary. VU1 is inactive; VU0 currently
packs final coordinates only, with no TGP/transform offload claim.

### Reference probe work preserved from before the native pivot

The direct GS path replaces shaded RGBA texture uploads with indexed PSMT4
textures and independent CLUTs in the same 512 KiB cache budget. A captured
Lancia scene at frame 1634 drops texture traffic from 84,713,472 to 560,128
bytes plus 7,680 cold palette bytes; frame 1357 drops 93,134,848 to 1,016,832
bytes plus 8,768 palette bytes. Both preserve all 2,073,288 viewport pixels
of the earlier native GS renderer. Emulator render measurements improve by
8.95 and 6.48 times respectively; these are neither full-game speed nor
physical-hardware measurements. The nearest GS result is still different
from the CPU reference: MAE 12.2414 and 7.5407 out of 255 respectively.
Evidence: build/ps2-gs-indexed-001/real-scene-validation.json.

Strip uploads reduce staging from 768 KiB to 64 KiB. Compact layers reuse two
existing video buffers; lazy CPU raster saves 1,310,720 bytes until needed.
Fallback composes captured layers with the reference's cached geometry frame
instead of rereading live tile state. All 1,634 continuous host frame records
match the existing reference CSV, SHA256
e4ac3d14e3849d118febc2eb85e4062c3741022fd8e02da7a4f2bca48c74f9cd.
Scalar and VU0 final packing match 295,872 tested components and both captured
real-scene images exactly. Three native/debug/reference/status/fallback cycles
also preserve all viewport pixels and caches; evidence is under
build/ps2-native-transitions-002. DMA ownership is explicit; timeout/recovery
and useful preparation/transfer overlap remain unimplemented.

An original bounded IOP RPC server sends PCM through open SDK libsd to SPU2.
It has a 16 KiB queue and 4 KiB DMA buffer, zero-fills underruns, and resamples
44.1 kHz input to 48 kHz using integer phase. No audsrv code is copied. The
separate reference target can now enable paged 68000/SCSP sound. A 16.528-second
original UART replay matches all 728,884 stereo frames with zero drops;
22,784 scalar samples are nonzero. The ROM-free SPU2 test consumed 48,001
nonzero frames, but also recorded 56,959 underrun silence frames. The native
rewrite reuses transport only, not that arcade sound implementation.

The frozen combined reference/audio ELF is
26920bb516df0728c71c8b2b1666fc4edeb482c5d954a289879a56925ff727d4,
with 17,764,288 resident bytes and a 14,741,568-byte heap/stack ceiling. Its
50-second Play run matched all seven host checkpoint fields at frames 1–3,
but frame 3 took about 4,178 ms and the audio queue mostly delivered silence.
Known allocation was 11,954,632 bytes in a 12,259,392-byte arena, not a peak.
This established integration, not acceptable gameplay/audio speed. Later
debug-only reference-main edits are not represented by that frozen ELF.

The reference board clock now caches the earliest event deadline while
preserving callback order, equality, UINT64_MAX, reentry and exception rules.
Boundary tests, 12,000 random operations, sanitizers and all 1,634 host records
match. The no-event EE microbenchmark improves about 2.5–2.9 times; no broader
game speed claim follows. Further scheduler optimization stopped at the
native-rewrite pivot.

### Findings not to re-propose

- Tiny overlays are illegible on the PS2 output. Keep the large optional page
  and printable logs; turning it off must refresh even during a black boot.
- Keying textures by shading uploads tens of megabytes per scene. Keep index
  identity separate from CLUT identity. The small fixture uses 512 index plus
  128 palette bytes: 640 bytes, not the initially reported 576.
- Play's macOS OpenGL backend did not implement the needed destination-alpha
  behavior. Vulkan restores the expected checker result with the same ELF;
  this is an emulator backend, not a Vulkan dependency in the PS2 program.
- Play may capture the previous submission. Present the final scene again
  before comparing it; the final transition regression then matches exactly.
- Compact fallback initially differed at host frame 325 because it recomposed
  live tile state and discarded cached geometry. Reusing the saved layers and
  once-per-geometry raster fixes that specific divergence and the full replay.
- Add GS origin before VU conversion to fixed point; adding it after truncation
  is wrong for negative fractions. Do not infer IEEE arithmetic parity.
- A benchmark callback captured too much state and measured allocation cost.
  Only the corrected no-event result is used; the revised recurring-event
  fixture is compiled but not claimed executed.
- A per-display fixed sample count drifts across PAL/NTSC. Queue-based refill
  fixes diagnostic pacing, but long render stalls can still underrun. RPC
  queue-full waits are bounded; a completely stuck synchronous SIF RPC is not.
- Source identities taken only before compilation can become stale during
  parallel edits. The build now rejects input drift after linking.
- The supplied PS2 Linux index led to the public Sony-authored programming
  guide and Toshiba TX79 manual. The latter describes related C790 hardware,
  not an interchangeable EE: its cache/FPU properties must not become PS2
  constants. Public Linux development advice supports explicit DMA ownership
  and concurrent VU work, not a Linux runtime dependency. A subsequent explicit
  instruction requested all manuals from the named collection. The local-study
  exception is recorded in rules.md; all 16 PDFs, controller spec and README
  (17,971,685 bytes; 2,470 PDF pages) are downloaded and searchable locally.
  The repository clone is under ignored build/ps2/references and excluded from
  VS Code repository discovery. Manuals and extracts are not redistributed.
  References and limits are in the hardware notes.

Final local checks pass: 56 PS2 tests, 13 source-release-gate tests, portable
smoke fingerprint 7fc55e0bfbc7af31 and the native source-boundary smoke. The
native smoke checks dependencies only; it explicitly does not execute the ELF.

All ROMs, generated game C++, private oracle data, captures, IRX/ELF files and
build reports remain ignored local artifacts. No PS2 game release is ready.

## PlayStation 2 architecture and game-core bring-up (2026-10-23)

The experimental `port/ps2` branch adds an open-PS2SDK host with direct GS/GIF
DMA presentation, double buffering, cabinet input mapping, embedded open IOP
controller/storage modules and a bounded read-only game pack. It follows the
design's Platform layer & build and Reference & validation contracts. The
fixed-function GS renderer is a documented exception to the desktop shader
path. VU geometry, native textured game rendering, SPU2 sound, real-console
validation and playable speed remain unfinished. Desktop releases are unchanged.

The ROM-free hardware diagnostic cross-links for the EE and boots in Play!
0.72 with synthetic geometry explicitly labeled as such. The actual 82-source
game-core target links the generated i960/TGP programs and shared Model 2A
devices, without a stub game loop. It currently uses the CPU reference
renderer, disables sound and shows observed startup/error checkpoints. Play!
now displays the game's own SOUND INITIALIZE screen. The EE checkpoint
fields at frames 1, 2, 3 and 60 exactly match the host reference. Only these
four sampled checkpoints were compared; this does not establish a playable
race. The CPU reference path advances at roughly 0.33 game frames per second
in Play!, independently of its display refresh counter. Local test tools,
captures, private packs and ELFs remain under ignored build directories.
No PS2 player release is published.

The pinned arm64 PS2dev image is
6b7c60ca61f40905e0aa22c0939707d91b6df2d5d8995f45c803c14031f06015,
with GCC 15.2.0; the amd64 image is separately pinned for source-only CI.
The build driver records source identities, toolchain, ELF load segments and
limitations. Per-source transitive include fingerprints avoid both stale
objects and scanning all old worktrees. SDK source/license provenance is in
THIRD_PARTY.md; no proprietary SDK or external BIOS is used.

Eight verified images occupy 107,741,184 bytes. The private page-deduplicated
pack occupies 29,491,200 bytes and reconstructs all eight SHA256-identical
images. The game probe uses a 512 KiB ROM cache; program/data mirrors, ignored
ROM writes, short-image padding, TGP reads and geometry checks retain their
resident semantics. `M2_PAGED_ROM` does not enable Dreamcast timing/checksum
shortcuts. CPU-only Video no longer allocates external-renderer layer copies.
The optional `M2_COMPACT_TILEMAP` uses four decoded rows instead of full
tilemap planes; it must keep the exact reference pixels and allocate full
planes when switching to an external renderer.

The current core ELF SHA256 is
c6976f7f59b6b851bd59b180a2d5d263c39acf6eb446c8cf65a8052e2b95b81e.
Loaded segments occupy 16,786,584 bytes, leaving 15,719,272 bytes above the
SDK's 1 MiB load origin for heap and stack. Known persistent allocations are
at least 13,246,204 bytes, with a 262,144-byte constructor table copy. Actual
EE allocator checkpoints at machine construction and completed frames 1/60
report 13,263,920 allocated bytes in a 13,577,064-byte arena. These are not
runtime peaks. Lazy GPU scratch saves 1,564,672 bytes on CPU-only hosts;
compact tile storage saves another 3,844,080 bytes. Native GS status sprites
remove the old 761,856-byte temporary diagnostic image. Busiest-scene memory,
SDK/stack use and FIFO growth still need measurement.

The original canonical generation correctly failed the changed runtime
boundary-source contract. Fresh generation in build/ps2/native passes the
audit; all 56 generated i960/TGP source files are byte-identical to the old
generation. No old audit was rewritten. A 1,634-frame real-game host run with
paged images and a 512 KiB cache matches resident instruction/cycle counts,
board time, PC, registers and screen hashes on every frame. This is host
equivalence, not a full-race or PS2 floating-point certification. Evidence,
exact EE object sizes and conservative heap budgets are in
build/ps2/validation. Build and portable checks are in the PS2 workflow.
The sparse EE checkpoint comparator records input hashes, fails malformed,
missing-reference, duplicate, out-of-order or mismatched rows, and writes a
new partial-evidence report without overwriting earlier observations. The
core-final-001 report matches the final ELF run's four recorded frames;
its 250-second capture shows the actual game boot text. Guest stderr is empty,
and core-final-001-runtime.json binds the ELF, logs and private capture hashes.
Final controller-fixed host-source and memory evidence is recorded separately
in comparison-controller-final-1634.json and
ee-memory-budget-controller-final.json. Previous ELF reports are preserved.

Review fixes preserve loader-provided host modules by probing host transport
before the optional IOP reset. The TGP table's exact 262,144-byte size is
checked before allocation, preventing oversized malformed packs from
consuming EE memory. Pack eviction tests exceed the actual cache capacity.
The build driver's default output now receives the same resolved-path check
as explicit output, rejecting symlink escapes. Both CI trigger filters cover
all bring-up sources and transitive project headers, including Model2Frame
and Trace. Dedicated regressions cover these review findings.
The pad adapter closes its opened IOP port before releasing its DMA buffer,
cannot be copied or moved, and retries rejected analog-mode requests. Mocked
SDK tests exercise successful/failed initialization, disconnect/reconnect,
digital pads, analog retry and exception unwinding; ASan/UBSan also pass.
Physical controller behavior remains untested.
Final local checks pass: 35 PS2 pack/core/build/checkpoint/pad tests, 13 source
release-gate tests, the 64-case compact compositor comparison and the
144-case Model2Frame fingerprint f4d290e762fc3523. The portable bring-up
smoke fingerprint remains 7fc55e0bfbc7af31. These checks do not replace
destination-console tests.

Findings not to re-propose: an ELF fitting below 32 MiB does not prove its
heap fits. The first executable used 16,777,496 loaded bytes and could not fit
the desktop-style runtime. Global `-fno-inline` made the measured executable
114,048 bytes larger; representative `-Oz` and `-fno-jump-tables` trials gave
identical objects to `-Os`. None is accepted as a speed improvement. EE
newlib's int32_t is long, requiring explicit raster template types and printf
casts; `usleep` is unavailable and SDK `DelayThread` is used. Play! guest
stdout is disabled by default: enable iop.fileio.stdlogging in its local
portable config. Also disable ui.pausewhenfocuslost for unattended tests.
Earlier apparent pack/frame stalls were confounded by focus pauses, sparse
logging and a one-submission delay in static GS status presentation; those
observations did not prove a pack fault or exhausted memory. Some PNG
previews appeared black despite identical decoded status pixels. Use pixel
comparison, guest logs and timestamps before diagnosing a crash. Continuous
game-frame uploads now display actual game pixels in the same emulator.

Next in order: extend observed boot into gameplay; profile EE execution and
memory peaks; implement ordered native textured GS rendering and validated
VU geometry batches; add IOP/SPU2 audio; validate physical console storage,
controller, full races and restarts. VU offload and a player release are not
claimed by the current boot result.

## Local generated-output cleanup (2026-10-22)

The requested space recovery removed 14,996 generated files occupying
23,270,944,768 allocated bytes (21.67 GiB). The build directory is now 5.8 GiB;
the volume reports about 31 GiB available. Removed outputs comprise 1,383 old
compiled binaries, 4,083 compiler artifacts, 6,076 raw captures/caches, 2,884
marked generated C++ files and 570 game-image copies whose SHA256 matched
preserved canonical files. No source checkout or Git store was removed.

Main already matched the published d496852 commit before cleanup. All 89
repository/worktree HEADs, Git-store paths, status outputs and staged/unstaged
diff hashes match the pre-cleanup inventory. The 77 working copies with
unfinished edits remain local; those changes have not been merged or published.
All 184 untracked regular files and 1,407 pre-hashed ignored source candidates
retain their hashes. Two additional source candidates retain their recorded
type/size; three symlinks retain their type/length. Those five entries had no
pre-cleanup content/target hash, so no stronger comparison is claimed.

Current public Preview 4 and private Mac/iPhone/Windows/Linux installers,
original ROM archives, saves, canonical native generation and the current Mac
player/assets were excluded from deletion. The installed Mac app is untouched.
PNG witnesses, replay inputs, manifests, logs and numeric summaries remain.
Earlier entries describing old executables or raw video/audio captures as
retained are historical: the deletion manifest now determines their local
availability. Historical build caches need removed outputs regenerated;
deleted duplicate images can be restored from the recorded identical file.

The ignored audit directory is build/maintenance/disk-cleanup-2026-10-22. It
contains the reviewed file-level plan, hashes and canonical replacement paths,
deletion log, space measurements and post-cleanup source verification. This is
maintenance only; physical iPhone testing and the remaining game-fidelity
work are still pending.

## Signed iPhone touch preview and Xcode ownership fix (2026-10-18)

The complete arm64/iOS 15 app now compiles, links and is development-signed
locally as version 0.1.0/build 5. Its private IPA is 28,381,881 bytes, SHA256
c25f9e2b533d07937da471c81d2c7a7923b7697c2070d6389a0f688ed42f8546.
All 25 archive files match the signed app, including eight verified images,
icons, profile and dependency notices. `codesign --verify --deep --strict`
passes. The existing identity/profile covers the paired iPhone and same bundle;
no account, certificate or provisioning changes were made. The existing signing
certificate expires October 27, 2026. Private metadata stays ignored.

The first install attempt failed before installation: CoreDevice 4000,
connection reset by peer. No physical launch, graphics, touch or sound pass is
claimed. Reconnect/unlock before retrying. No IPA was uploaded to GitHub or
Apple, and older signed packages remain unchanged. Package and checksums are
in build/installers/iphone-touch-preview-001; the build-result.json binds final
source inputs, executable, archive and validation limits. Device output is in
build/ios-player-integration-001/install.json.

The full Xcode build exposed an ownership issue hidden by standalone compiler
checks: COMPILE_LANGUAGE:OBJCXX target options did not emit -fobjc-arc for the
mixed-language ImGui library. The pinned backend warned that bridge-retained
and bridge-transfer casts had no effect. The host's ARC setting was already
correct; segarally_imgui now sets XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=YES too.
The final object/response files confirm ARC and the incremental build passes
without those warnings. The iOS bootstrap also guards SDL_MAIN_HANDLED because
Xcode supplies it already. Existing filesystem u8path deprecations in unchanged
host code and SDL empty-object archive warnings remain. Initial product/object
identities are preserved in the worktree's build/iphone-before-imgui-arc-001.

A redirected configure command and a separate cosmetic preview executable
waited at execution approval and were cancelled without starting. Retrying the
explicit CMake command without redirection used the existing scoped approval.
The older mac-candidate-006 inputs then failed the correct boundary-source
check: tools/m2recomp/main.cpp was stale. The current preserved generation at
build/player-defaults-6c22e24-001/native passes the boundary and sound contracts.
78 required files (140,302,767 bytes) were copied with identical hashes into the
isolated checkout, retaining audited relative paths. No audit was rewritten or
weakened. The previous copied inputs and failures remain retained.

The final checkbox color change compiled in the full iPhone build; its separate
Mac cosmetic capture did not execute. Functional layout evidence remains the
704/1,136-check runs before that color-only delta. The earlier strict iOS API
checks and touch tests keep their stated scope. All five GitHub workflows pass
for published UI commit 54dbb3c: Build, Metal, Vulkan CRT, Vulkan Model2/WSI and
Vita. Its exact source tree/archive gate passed at 144ff622, with local proof
in build/ios-source-gate-001. This follow-up records the real Xcode fix and
package outcome; the final source tree is gated separately before publication.

## iPhone direct Metal and touch interface (2026-10-18)

The Apple source build now defaults to direct Metal on iOS as well as macOS.
The shared presenter guards CAMetalLayer's desktop display-sync property;
Model 2, CRT, generated shaders and ImGui Metal rendering are reused. SDL still
owns window, input and audio services. The iOS build script exposes
`--graphics metal` (default) and `--graphics sdl` for comparison and records the
selected backend. Its 12 command-plan regression tests now run in GitHub CI.
No native CPU, board, sound or game-timing implementation
changed. This follows the inherited design's Platform layer & build contract;
its Reference & validation requirements remain open.

The original touch overlay uses graphite panels, ivory labels, orange feedback,
ribbed pedals, analog steering travel, shift arrows and compact top controls.
Its hitboxes and input ownership remain unchanged. Touch-visible hosts use a
safe-area-aware Pit Stop Settings layout, 45-point normal framed controls,
large presets and a fixed resume footer. Display tuning is collapsed initially;
content scrolls separately from the footer. Desktop settings keep their prior
layout. This is a custom rendered interface, without VoiceOver support. The
scrollbar is 28 points wide; no universal 44-point accessibility claim is made.

Validation so far: 12 iOS pipeline tests and 511 touch checks pass. Five shared
Objective-C++ units compile strictly for arm64/iOS 15 with the iOS 27 SDK and
no diagnostics. A ROM-free direct Metal overlay harness renders idle/active
states at 844x390 and 568x320 logical points while checking unchanged input
state. A separate production Settings harness passes 704 and 1,136 assertions
at those sizes, including actual SDL touch-mouse events, persisted CRT preset
changes, tab switching, scrollbar drag to the final binding row and resume.
Normal tabs and footer are 45 points tall. Those are macOS Metal UI checks;
physical notch placement, multitouch, game pause, audio, pacing and lifecycle
are separate iPhone checks. A final checkbox color correction is presentation
only and does not change the tested layout or interactions.

Ignored evidence is in build/ios-ui-review-001, build/ios-player-integration-001
and build/worktrees/ios-touch-design-001/build/touch-proof-001. iOS API/build
records are under build/worktrees/ios-direct-metal-001/build. The first iPhone
configure failed because sandboxed Xcode compiler probes could not access
DerivedData/CoreSimulator caches; that failure remains in iphone-001. The
explicit toolchain configure retries in a new iphone-002 directory. The completed
compiler/SDL probes then exposed the asset stager's correct refusal of native
inputs outside the isolated checkout; copying the required existing inputs into
that checkout preserves its containment rule without regenerating game code. An initial
UI harness assertion was too strict about transient horizontal content extent;
the stable layout has no active horizontal scrolling. Its failed capture and
later successful proofs are both retained.

README, architecture, renderer and package references now link the dedicated
iPhone build/interface guide. Public desktop Preview 4 artifacts remain
unchanged; this work does not publish a public iOS IPA or TestFlight build.
The project's own UI does not license SEGA's game content or translated game
instructions. Commercial distribution remains a separate rights decision.

The initial GitHub audit found main at 40d9568 with four passing workflows,
Preview 4's seven assets matching their frozen sizes/digests, and the reviewed
documentation synchronized. The stale macOS-only About description was updated
to mention Mac/Windows/Linux, direct Metal/Vulkan and experimental iPhone work;
its homepage now points to Preview 4 and platform topics are current.

## Local build-output cleanup (2026-10-18)

The requested cleanup reclaimed 11.86 GiB from old compiler objects/dependency
files, copied game images verified against preserved canonical assets, extracted
stages backed by exact archives, duplicate archives and disposable Wine system
binaries. Current public/private package identities, source edits, repository
heads and all 83 worktrees were preserved; 75 worktrees contained changes.
No whole worktree or source checkout was removed. Logs, saves, trace evidence,
canonical game inputs and current release packages remain local.

Audit records are under build/maintenance/disk-cleanup-2026-10-18. Historical
build caches may need their removed derived inputs regenerated/restored. For
current iPhone UI validation, four exact image files (90,177,536 bytes) needed
by preserved Mac CMake caches were restored from current player assets; their
identities are recorded in
build/ios-player-integration-001/restored-current-build-inputs.json. No generated
source or game assets were added to Git. Docker images were left unchanged
because the apparent older-image sizes were almost entirely shared layers.

## Linux Wayland player publication (2026-10-17)

Player Preview 4 is published on LaurentGarcia/sega-rally-recomp as
v0.1.0-player.4, release 416015569, targeting 53ec202. Its seven uploaded assets
are the Mac DMG, Windows ZIP, Linux tar.gz, three platform manifests and
SHA256SUMS. All seven remote sizes and GitHub SHA256 digests match the frozen
local allowlist; the tag points at the expected commit and release notes match.
The target's four triggered CI workflows pass: Build 38983515035, native Metal
38983514843, Vulkan CRT 38983514927 and Vulkan Model2/WSI 38983514915, including
the separate native Wayland job. The source archive gate passes the exact
53ec202 tree (8911ddc9e5bb43350a777b95234eb317df5b96f0).

The new Linux import archive is 8,698,088 bytes, SHA256
`ef14d4e84938f34afab416271b4e01a7fc9d0a35eafd6ed994aa7567772b3862`.
It retains the normal Wayland-enabled executable from 81f8288, SHA256
`a621cd2ba1309b270248cbc72fe74f80adda08c4f078b39ca26b7cbcdbc9b915`.
The 101 recorded project compiler inputs match that commit; 57 generated inputs
are identified by hash only. The archive includes 16 files and two directories,
with all bytes, hashes and modes independently checked and no symlinks, ROM
assets, generated game source or lifecycle wrapper. Its external manifest is
7,911 bytes, SHA256
`0735056bcc53f2d6c136ef50f0639c7f84f051cc8f3688f6af264caae412e6b9`.

All 24 pinned protocol XML identities and 23 exact copyright blocks are checked.
The initial independent notice audit expected the later THIRD_PARTY.md snapshot;
the frozen archive actually retains 81f8288's file plus the complete separate
SDL-Wayland-protocols.txt notices. The manifest records that distinction from
repository-only notice metadata at 53ec202. The archive was not rewritten.
An earlier opaque Python packager stalled on a Docker escalation and created no
archive; its script is preserved. The completed packager separates ordinary file
work from narrowly scoped explicit Docker checks and preserves all older assets.

A fresh extraction, with no source/build directories mounted, imports all eight
expected game images and completes 2,800 frames through direct Vulkan under
headless Weston/CPU lavapipe. Execution, native PCM, final raw image, saves and
observations match the earlier normal run; CRT is on, exit/saves are clean and
validation reports zero errors. The quality checker correctly retains
completed_with_findings: 3,111 dummy-device shortfall requests, zero chip/host
drops. This is not a physical GPU/controller/speaker or performance pass.

Final packaging evidence is under
build/worktrees/rc-platform-readiness-001/build/linux-wayland-packaging-002:
final-result.json SHA256
`73005170833e8da1b348c4a1b011fe569ae23cfc6f3ce7c6505ac36da1ab94d5`;
relocated-001/evidence/result.json SHA256
`89bbb003269f7653ea3795571e3e1b97f982fb51b00ffe89abbce67d73e86aff`.
Publication inventory, independent audit and remote proof are in
build/publication/native-player-004. Mac/Windows binaries and manifests remain
byte-identical to Preview 3, preserving their distinct compiled-source identities.
The earlier Linux personal archive remains local and X11-only. No personal
package or iOS build was uploaded. README and current platform/status references
now point at Preview 4; historical Preview 3 evidence remains intact.

Next RC work remains NORMAL Championship/Lakeside completion, reproducing the
intermittent long host-audio stalls, physical Windows/Linux device testing and
independent full-race MAME/PCB comparisons. The passing SHORT Practice CPU
lifecycle and recorded cross-API scene findings do not close those requirements.
SDL window/input/audio replacement and public iPhone distribution remain separate.

## Linux package notice inventory (2026-10-16)

The Wayland-enabled SDL library compiles generated interfaces for all 24 pinned
protocol XMLs. The package must retain their per-file notices, not only SDL's
zlib license: 23 files contain copyright blocks, including the distinct
text-input-unstable-v3 terms; pointer-gestures-unstable-v1 contains no such block.
THIRD_PARTY.md now records the exact SDL pin, generator inputs and separate
notice inventory. The new import-only package is being checked with those
notices included; no new release is claimed by this documentation commit.
The host-provided Wayland/xkbcommon libraries are not copied into the package.

## Full SHORT Practice lifecycle i960 comparison (2026-10-16)

The bounded raw native/i960-reference run completes all 22,000 frames and
4,208,428,228 instructions without a mismatch: 134,770,594 ordered writes,
9,561,241,000 reference cycles, 233,617 IRQ entries, terminal PC 005dde3c.
Every opcode compares architectural CPU state, ordered cycle-stamped writes,
interrupts and separately instantiated board state; final video RAM/pixels
also agree. All 1,901 replay events apply with no pending events. The Practice
start and finish callbacks execute successfully, followed by results, score
entry, attract and a new Championship start.

This raw harness has sound off, UART RX idle and no Machine boundary callback.
It reuses CPU context/helpers and shared board/TGP implementations. Independent
TGP/sound, full MAME-machine/PCB and extF80 hardware parity are not established.
A separate offline audit correlates all twelve pins and instruction/cycle
counts at all 1,901 event cuts with the sound-on Metal recording, fresh all-FF
seeds, terminal frame/counts/PC and final BGRA hash 90fb8f11f726352f. It does not
consume Machine resume cuts or compare every intermediate CPU register to that
recording. Raw final saves are not exported; prior save comparisons are separate.

All 207 frozen reference inputs rehash, also independently checked at integration.
The guest takes 2,304.586 monotonic seconds under its original 3,600-second cap;
host suspension extends wall age, so neither is a gameplay speed measurement.
The temporary PID-bound idle-sleep assertion exits normally with the guest.
No CPU/runtime/generator source changed during the run. Documentation is added
to docs/course-regression.md and summarized in the current status reference.
Review also corrects the earlier PCM wording: the stream contains nonzero
audio, not nonzero samples at every frame; startup silence is present.

Evidence root: build/worktrees/rc-courses-001/build/course-coverage/independent-i960.
attempt-001/result.json SHA256
`a22c98f231562d9c550c37945d50386420cbb1cd6339bbce1b9991bc8c2623fb`;
full-review.json SHA256
`22434fe6f5a9984419b4102771eeff6674bf5abc04ef7a0dba4a4acdab20d2cc`;
attempt-001/guest/stdout.txt SHA256
`d3184d5bbe8cebd45746631d22fbd2a3df8ae210464412088e1d748f9b10dcef`.
NORMAL Championship/Lakeside completion and the other fidelity gates remain open.

## Actual Wayland player and restoration (2026-10-16)

A separate Linux player build from 81f8288 enables Wayland alongside X11 with
libdecor disabled. An isolated 258 MiB copy preserves the original build cache;
all 57 generated-game objects are reused unchanged. The normal executable is
SHA256 `a621cd2ba1309b270248cbc72fe74f80adda08c4f078b39ca26b7cbcdbc9b915`.
Only original wrappers in a separate diagnostic executable automate hide/show
and report presentation outcomes; that automation is absent from the player.

Three 2,800-frame runs (SDL Vulkan, direct Vulkan, diagnostic direct Vulkan)
match native PCM, final saves, raw image, observations and terminal execution:
536,636,736 instructions, 1,216,921,001 cycles, PC 005d07d8. The diagnostic
records 22 real hidden offscreen updates and 2,415 successful restored presents;
all execution payload rows match after excluding executable-context identity.
It presents 2,778 frames, uses two swapchains and retains four Model 2 plus one
CRT pipeline. Both direct runs enable synchronization validation, with no errors.

Software lavapipe/headless Weston/dummy audio produce 3,252/3,151/3,071 device
input shortfall requests, respectively, with zero chip/host drops. Quality
reports correctly remain completed_with_findings. These are gameplay/window
comparisons, not physical device playback, controller, GPU or MAME/PCB passes.
The first outer runner looked for redirected lifecycle logs in stderr.txt and
failed after all three games completed. That failure is retained. A separate
audit reads host.log and rehashes the existing captures without rerunning them.

Report: build/worktrees/rc-platform-readiness-001/build/wayland-player-001/execution/audited-result.json,
SHA256 `c4aa727ca7aac6f36379f1e8d03ed5fa631d875d3aedc3ee5415255c48ce8a1d`.
The build report SHA256 is
`2773e0d130ba3d2aca27021d65c3e599a761537ec2f6e2b94b2e4331ed4c9562`.
Its 99-file source inventory is explicitly a subset of linked dependencies;
package verification separately covers the linked digest/recording libraries.
Original build objects, source inputs, game data and existing archives remain
unchanged. docs/native-renderers.md records the commands' scope and findings.

## Bounded Mountain follow-up and hosted Wayland check (2026-10-16)

One input-only NORMAL Championship trial uses the exact best Desert/Forest
prefix, steering gain 1.5, lookahead three and takeover at frame 11,987. All
2,399 guest-state/pin checkpoints through frame 11,985 match. It visibly retires
fourth at frame 16,800, reaching Mountain segment 287 versus the previous
lookahead-four best's 293/third. Desert 0:59.60 and Forest 1:20.85 are unchanged.
It stops normally at the explicit 26,000-frame/10B-instruction/600-second bounds:
4,835,942,698 instructions, 11,299,641,004 cycles, 144.24 seconds. Exit zero at
that bound is not finish evidence. No further sweep or runtime change follows.

The final audit verifies 108 frozen inputs and 146 artifacts at
build/worktrees/rc-raster-001/build/course-coverage-001/championship-mountain-look3-001/final-audit.json,
SHA256 `cbd096426de4dd5db33680dfc4db9081c60309d0eb1b3064c9ade822f9d3cda9`.
The guest result hash is
`087c8b4220efa0d123713c74449f757a4b55034f1d073124f63dcc94ed979909`.
docs/course-regression.md retains this negative result alongside the best path.

After explicitly installing EGL development files, the hosted native Wayland
job passes at f1c42e0 (run 38981705741, job 117243826616). Its sibling X11
presentation and Model 2 jobs also pass. The earlier rejected disabled-Wayland
build remains in CI history; no assertion or backend requirement was relaxed.

## Wayland CI dependency and current status correction (2026-10-16)

The first hosted Wayland job (run 38981338922, job 117242648859) fails before
WSI execution: pkg-config cannot find egl, so pinned SDL silently disables
Wayland despite the requested CMake option. The generated-header assertion
correctly rejects that binary. The local container already had EGL development
files. The CI job now explicitly installs libegl-dev, and the renderer guide
records this SDL window-backend dependency. No graphics code, fixture checks
or passing local evidence is changed. The failed CI log is retained at
build/rc-wayland-source-002/ci-wayland-004.log; earlier empty download attempts
also remain. All other completed renderer jobs passed at 81f8288.

The documentation index now links the course replay and desktop player checker.
The current status page records the new Practice lifecycle and Wayland checks,
the exact-scene pixel attribution and the long-attract audio failure/repeat.
README wording distinguishes the earlier tested buffer remedy from the later
intermittent host stalls. These are current-source developments; Preview 3
release binaries are unchanged and no full RC/fidelity pass is claimed.

## Vulkan restore and native Wayland regression (2026-10-16)

The original native Wayland fixture stalled after hiding and restoring a
window at unchanged dimensions. Bounded phase tracing locates the block in
vkQueuePresentKHR: event pumping, the frame-slot fence, image acquisition,
cached-frame encoding and queue submission all return. The presenter now
marks a hidden/minimized/zero-size swapchain dirty; on restoration, the existing
drain/device-idle retirement recreates presentation state even at the same
extent. Game image resources, shaders, pipelines and FIFO mode are unchanged.

The new ROM-free fixture runs under private headless Weston with pinned SDL,
Wayland enabled, X11 and SDL GPU disabled, CPU Vulkan and synchronization
validation. It covers pause/cache, hidden offscreen update, same-size restore,
three resizes and overlay submissions. Final uninstrumented run passes 110
checks and 68 presentations, with five swapchains/four lifecycle idle waits,
persistent four Model 2 plus one CRT pipeline, and zero validation errors.
Both integration and independent review verify all 18 measured source hashes;
all 23 artifacts also rehash. Captures are raw offscreen images. This is not
actual-game, compositor-pixel, physical GPU/input/audio or display certification.

Evidence root: build/worktrees/rc-platform-readiness-001/build.
wayland-check-003/result.json SHA256
`03d3050ae638b7db836f60dc79e91c9b907f2546a9608d53a11cdae21c61f6f0`
preserves the phase-traced timeout. Earlier timeout runs 001/002 and their
measured source/executable versions remain intact. Instrumented fix run 004
passes; final run wayland-check-005/result.json SHA256
`79b14cec55927bed33c0e96e7026d21f6677d5cfdd9df2d34eadda8a4ae8183e`
binds the exact uninstrumented production fix. Timeout cleanup is bounded and
stops both the fixture and its private compositor.

A separate pinned-SDL Wayland CI job and opt-in standalone CMake target are
added. docs/native-renderers.md gives the command and limits. Preview 3's
published Linux player remains X11-only; source fixture success does not
retroactively add Wayland support to that binary. The existing X11 regression
also passes: 101 presentations, six swapchains/five lifecycle idle waits,
persistent four plus one pipelines, zero validation errors. All nine raw and
displayed images equal the prior presentation-008 baseline. Its report is
x11-regression-001/result.json under the evidence root, SHA256
`e68b6fba08b55087a89a6549f19b2b8289d95d89d96432e988ddd431fdbb0f13`.
The exact staged source/archive boundary check passes before publication.

## Exact-scene Metal and Vulkan attribution (2026-10-16)

An unchanged production frame at 2,600 completed frames reproduces the existing
951-pixel default Metal/Vulkan difference under isolated offscreen rendering.
All 8,526 projected vertices and CPU images match across hosts; the export also
preserves actual execution records, PCM and final saves. Executable/recording
identity hashes differ because export instrumentation changes the executable.
Of the 951 pixels, 940 retain polygon ownership but differ in integer UV and
sampled texel. The other 11 change surviving polygon and all involve cutouts.
This narrows investigation to GPU vertex/interpolation and texture sampling;
vertex arithmetic versus interpolation, and coverage versus cutout thresholds,
remain unresolved. No production shader/default change is justified yet.

Unmodified Accurate-textures output agrees across APIs and with CPU for this
one frame. Eight instrumented Accurate draws are excluded: optimization dropped
unused resources and compacted Metal's edge-buffer binding. Default diagnostic
packets never access that buffer, with all edge metadata verified zero. The
invalid draws and initial analysis are retained. This is neither temporal
flicker coverage nor independent MAME/PCB or performance certification.
docs/native-renderers.md records the detailed counts and interpretation.

The report rehashes 170 sources and 262 artifacts at
build/worktrees/rc-raster-001/build/cross-api-001/result.json, SHA256
`a525657551cde68020edf78d0fee7e24ab1ee3d52a2c7b481130844801ff1d9e`.
Only documentation is integrated; game snapshots, diagnostic shaders and
captures remain ignored local evidence. Next is a bounded common-shader
investigation of the GPU arithmetic/interpolation stage, without rebaselining
away the discrepancy or claiming an entire race from one scene.

## Reusable Practice finish and Championship restart (2026-10-16)

The frozen config/segarally_short_mountain_finish_restart.inputs now covers
SHORT Practice Mountain through Stage Clear, results, name entry, Top 8,
attract return, then two new credits/start and a visibly moving Championship
Desert race. This is Practice completion followed by a Championship start;
it is not Championship completion. The operator SHORT setting remains active.
docs/course-regression.md gives the explicit 22,000-frame invocation with new
isolated saves, startup settings, observed frame landmarks and evidence hashes.
Only original cabinet inputs are added; no game data/captures or generated code.

The CPU prefix preserves 602 prior instruction/cycle checkpoints and 99 shared
numbered images. The released signed personal Mac app then completes all 22,000
frames, 4,208,428,228 instructions and 9,561,241,000 reference cycles, ending at
005dde3c. All 1,901 input events apply with no takeover. All 4,401 CPU frame cuts,
final saves and final Accurate-textures raw image match. The 16,866,030-frame
stereo PCM stream has finite samples and contains nonzero audio alongside
startup silence. This hidden, unthrottled run opens no playback
device and provides no presentation-pacing, speaker or independent MAME/PCB pass.
The complete recording audit verifies 44,014 resume rows, pin values, continuity
and the completion seal. Integration verifies the frozen replay/report hashes.

Evidence under build/worktrees/rc-courses-001/build/course-coverage:
cpu-coverage-review.json SHA256
`4fabc182e58a35ead7e4e60441b18e84bbacac378113388b6dbd2a20dc7c9926`;
metal-short-restart-003/result.json SHA256
`c5ff8bc0fa5b64d7e97855b366377c2f1cd7edc1e7cfeed306993a29da60c153`;
recording-review.json SHA256
`66067af8322f5cb20d06a2f8ae496c7aac76023aa0501b79ccaffb82c8adcf0f`.
The tracked replay SHA256 is
`94a36fd909030d7330bdbe16fbfb1d33e189b7d2d11641c7b6b8329d51510b2e`.
The first Metal attempt stopped in the sandbox before guest execution. The
second redundantly enabled the CPU reference renderer on every frame and was
stopped at 20,222 frames after 858 seconds; its bound assertion failed. Both
are retained. The successful third attempt compares against the already frozen
CPU run and does not duplicate its expensive renderer during Metal execution.

NORMAL coverage remains open: best Championship inputs finish Desert 0:59.60
and Forest 1:20.85, but retire on Mountain segment 293/300. Practice Lakeside
finishes lap one in 1:19.75, then retires on lap two. Stronger steering helped;
broader anticipation, braking and predictive input searches did not establish
full completion. These attempts did not identify a missing native callback;
they do not establish a game defect or exclude one. No timer, physics,
instruction or clock patch was introduced.

## Extended host timing and RC checks (2026-10-16)

Reference & validation and Platform layer & build govern this work. The actual
signed personal Mac player was exercised for 18,000 paced visible attract/title
frames with fresh saves, default CRT/sound, native PCM, logical-device output and
a complete input recording. It completed 3,492,142,456 instructions, 7,822,841,002
reference cycles and all 18,000 presentations without a guest frontier. However,
the delivery gate failed: 20 device input-shortfall requests, 20,854 host-dropped
frames, no chip drops. Producer gaps of 161–305 ms cluster around board time
109–122 seconds; measured machine steps/presentation do not explain those gaps.
This is a new retained failure, not a reason to erase preceding shorter passes.

Evidence is build/validation/rc-attract-audio-001/capture-002. Artifact review
verifies all 36,013 recorded PCM intervals, the completion seal and unchanged
packaged inputs: capture-002-review.json, SHA256
`657ce1f5d93b7178455dd0c65c66d18f6ebe3bb477c45fccb136797608ca7698`.
The first sandboxed capture aborted before host logging and remains capture-001;
the authorized visible retry is capture-002. Logical output precedes CoreAudio
hardware output; these counters neither identify the stall cause nor certify
what a listener heard. Parallel jobs were present, but are not established as
the cause.

Opt-in audio diagnostics now separately time input/event handling, PCM capture,
audio submission, recording, diagnostic writes and requested/actual sleep with
wake overshoot. Existing CPU/presentation timing remains. These measurements
leave the board clock, queue policy, native PCM and normal diagnostic-off path
unchanged. A fresh complete Mac host builds at build/rc-audio-host-001; all 13
frontend tests pass. The first CTest invocation found seven excluded fixture
executables unbuilt, not failing assertions; its log is preserved before the
explicit fixture build and passing rerun.

The separate instrumented repeat completes the same 18,000 paced, visible frames
with zero chip/host drops and zero device shortfall requests. All 13,799,492
native PCM frames, final raw image, initial/final saves, per-resume observations
and recorded execution payload match capture-002 exactly. Maximum producer gap
is 35.300166 ms; measured wake overshoot is at most 1.015672 ms. Result:
build/validation/rc-attract-audio-001/instrumented-001/result.json, SHA256
`6edf93bb5c7c8a29bb0a2a6ebd37d4c60262f1a440d8dde71436bd7263e84778`.
The earlier long stalls did not recur, so instrumentation is validated but an
audio-stall fix is not established. Do not relabel this repeat as a controlled
load/performance comparison or physical listening test.

The new scripts/segarally_pc_check.py runs the released Windows/Linux player
without compiling it, using isolated saves and optional owned-ROM import. It
collects paced SDL/direct Vulkan output, input recording, native/logical PCM,
seals, final image/state and environment evidence. The optional hardware check
requires a matching loader-reported integrated/discrete device; software,
virtual and unknown devices are distinguished, and a separate bounded child
protects the tool against a hung loader probe. Manual input and speaker results
remain explicit operator observations. Nine focused integrity/classification
tests pass locally and are wired into source-boundary CI. Usage is documented
in docs/segarally-pc-check.md and linked from release-platforms.md.

The actual released Linux player completes a 120-frame SDL/native plumbing run
in software Vulkan with dummy audio. Both runs match terminal execution, PCM,
saves, observations and final raw pixels, and all recording seals verify. The
checker correctly returns completed_with_findings: startup sound is silent and
the slow diagnostic environment produces 52/84 device shortfall requests. This
is not a physical-PC or playback-quality pass. Report:
build/worktrees/rc-platform-readiness-001/build/pc-smoke-001/result.json, SHA256
`d1fb31ab764d77f1367ceff2cd609b0e8d320bf33a6c22787d0ed520ad5505e2`.
Integration review re-parses both captured runs and matches the runner identity.
An earlier xvfb-run wrapper stalled before game execution under translation;
its process was stopped and retained before direct bounded Xvfb startup worked.

The hardware audit still finds double-precision i960 FP registers and generated
arithmetic; existing extF80 reference helpers cover only a restricted operand
subset. A complete architectural FP migration is actual development, not a
completed fidelity gate. Pinned MAME SCSP/device behavior also retains documented
limitations. An Intel public manual mirror returned HTTP 403, so no new CPU
semantics were derived from its unseen contents. A search result for a mirrored
Sega development manual was excluded under the no-internal-documentation rule;
it was not opened or used for implementation. Do not substitute new speculative
hardware semantics, queue inflation or clock changes for a measured correction.

Next: retain these diagnostics for the intermittent long host stall; complete
NORMAL Championship/Practice finish coverage; resolve the measured cross-API
default-image discrepancy; run the new player checks on physical Windows/Linux
machines. Native Wayland and complete native host replacement remain separate
work. Preview 3 release assets are unchanged by this source/tooling update.

## Desktop player packages and CI (2026-10-16)

Player Preview 3 is published on the personal LaurentGarcia/sega-rally-recomp
repository as v0.1.0-player.3, targeting 1a59cea. Its seven assets are the
Mac/Windows/Linux ROM-import binaries, three manifests and SHA256SUMS.
The Mac DMG and manifest are byte-identical to Preview 2 and retain their
tested 8360ea9 identity; the new Windows/Linux packages bind to 1a59cea.
Public upload sizes/digests and the remote tag match the final audited staging.
Publication proof: build/publication/native-player-003-published.json.
Final assets/notes are at build/publication/native-player-003-final;
personal Windows/Linux archives are at
build/installers/native-player-preview-003-final and were not uploaded.
The Mac personal DMG remains in mac-direct-metal-personal-001. No iOS build
or TestFlight submission is added by this release. README/platform guides
now link the real three-platform binaries and explain each launcher.

Independent archive/source/dependency review passes 1,547 checks, report
build/worktrees/native-vulkan-frontend-ci-001/build/release-package-independent-audit-001/result.json,
SHA256 `c601f576faa34616d59197f9b76d9c5e941ba72dd0800f1367db6cac7b3be6b1`.
The final Linux README-only delta and final public/private staging pass
335 further checks at release-package-independent-audit-002/result.json
in the same build directory, SHA256
`a5e5c46407558c3ff37cbe281767161eefc1e6cf0e06e057b6151af9725e7bc7`.
These supplement the gameplay evidence; native Windows GPU/audio/controller
testing, physical Linux GPU/speakers and full-machine/PCB fidelity remain open.

The direct Vulkan integration is committed and pushed as
`1a59ceafc6ff6870d06db723a1e6cf035bbebe57`. All five GitHub workflows pass:
Build 38939712027, Metal 38939711941, Vulkan CRT 38939711991,
Vulkan Model2/presentation 38939711929 and Vita 38939711949. The Build matrix
includes Linux GCC/Clang full direct Vulkan frontend links, Windows MSVC/Clang,
Mac direct Metal and the source boundary. Thirteen source-boundary regressions
pass; the exact Git archive gate is
build/publication/native-vulkan-player-source-gate-001.json, SHA256
`8a421e3835f278204b7225fe07c1a1858a5dbc228ab665c95cce317b2eb533d6`.

The actual Windows game cross-build compiles all 57 generated game/TGP/sound
units with direct Vulkan and the SDL comparison path. Its AMD64 PE is
27,802,900 bytes, SHA256
`03f87d793d9a9004ec49589d8ca1698ff19ca810d9d1129261cf51f88b5842f7`.
Compiler dependencies bind to the committed source. Runtime checks identify
the three bundled MinGW runtime DLLs and the GPU driver's external vulkan-1.dll;
the import library comes from the pinned official Vulkan-Loader definition,
not Wine. Wine startup, help, importer fixtures and owned-ZIP import pass,
including all eight image identities. This does not test Windows rendering,
physical audio, controllers or a complete race. Reports under
build/worktrees/native-vulkan-windows-001/build/ are:
windows-vulkan-003/result.json SHA256
`09bcc7ed13313f46ca4777297a1fea7dab24f0fca6f08f3eebb89b79d6014409`,
windows-runtime-001/result.json SHA256
`0b6c18f5e69ac6e9b600b1f764da5aa1b277d2b2e6fd62a2cef5d280096312db`,
and windows-wine-002/result.json SHA256
`6390c3865fc9ef237e4f7d63daa020c87a83be279308be66ae0652ac85da6098`.
The import ZIP is 17,114,899 bytes/SHA256
`edc84f1ab5b31efe6162fc289fabde6817ea507a9072394f6b367f5893806052`;
the private ZIP is 33,669,831 bytes/SHA256
`fac3761c7ac3b5c090c5834701413ab4b1f4b091f43f4819c649b1861c48b1d0`.

The extracted Linux import archive verifies the owned ZIP, then runs 2,600
visible direct Vulkan frames without source/build directory mounts. Final
image and audio reproduce the measured game run, with CRT enabled and zero
validation errors. The separate private archive starts for 120 frames using
bundled data without import. The exact measured ELF is preserved, SHA256
`a331c400cf019d4e73110cfb2de82668fde5466cb42239b2e9e5a7a932952d66`.
Outer integrity evidence under the native-vulkan-playable-001 worktree is
build/linux-packages-1a59cea-001/relocated-check-001/result.json, SHA256
`84fbacdb9fabe0b2da31250c35a9e96416670ee99472be3467c4295624dc1e3f`.

Packaging findings: generated Windows inputs must sit beneath the ignored
build root for staging; they were mounted there without editing generated code.
Legacy SDL_GPU_D3D12/SDL_GPU_VULKAN cache flags are unused by pinned SDL;
actual compiled backends, rather than those flags, establish capability.
The first Wine local-socket operation and Linux container wrapper were blocked
by the sandbox before their checks ran; authorized retries preserve those
failures and complete validation. Final Linux documentation review finds
SDL_WAYLAND=OFF and SDL_X11=ON: this binary requires X11 or XWayland, not native
Wayland. A new archive revision corrects the README while preserving the
measured executable/assets and preceding archives.

The corrected Linux archives are under
build/worktrees/native-vulkan-playable-001/build/linux-packages-1a59cea-002/:
import 8,586,382 bytes/SHA256
`3168a945b6caadb0749acb8954e04886f8bb1671b8f00f9d945468a7cdec9ad9`;
private 25,097,897 bytes/SHA256
`f679d994b53ca828bcced96d209c2539df9703393c0e7098d2a80e8530cb3211`.
Only README text differs from the relocated tested package. The new result
rehashes all archive entries, 99 source dependencies and 57 generated inputs,
and explicitly binds unchanged executable/assets to the earlier run:
result.json SHA256
`894caa626c516f7757d69a5be71a16a3e8e261cab7f84a04e802486888c911ad`.

## Direct Vulkan game, presentation and Settings (2026-10-16)

Platform layer & build and Reference & validation govern this integration.
Windows/Linux builds can enable SEGARALLY_NATIVE_VULKAN and select
--native-vulkan; the SDL GPU default/comparison path remains available in the
same executable. --vulkan-validation requires the Khronos layer. Metal defaults
and the shared game/audio/input/save/recording loop are preserved. SDL still
provides windows, controls and audio; this is not complete SDL removal.

VulkanModel2 now uses persistent pipelines and three fenced asynchronous
slots with owned snapshots and reusable buffers/images. Callers may append
commands before submission. Diagnostic render/readback uses this same encoder.
VulkanPresenter owns FIFO presentation, image-indexed completion semaphores,
cached frames, native-aspect/integer scaling, hidden updates, resize/minimize,
raw capture and the common CRT shader. Original-display scaling uses
vkCmdBlitImage, matching pinned SDL's primitive. Device/format/blit support is
checked before use. Errors are latched; device loss exits rather than silently
falling back. Device-loss injection/recovery is not claimed tested.

Settings uses the unchanged pinned ImGui Vulkan renderer, with its own
descriptor/font resources and buffers covering both swapchain images and the
three command slots. Incompatible count/pass changes drain and rebuild;
steady UI frames do not wait for the whole device. The shared Controls,
preferences, input isolation and foreign-context fix remain in one UI body.
Teardown drains old submissions before releasing UI and presentation resources.

The asynchronous offscreen regression repeats all 38 images / 17,547,264 pixels
exactly against SDL Vulkan. Three deliberately pending submissions preserve
571,392 pixels after CPU inputs are mutated/destroyed. Report
build/worktrees/native-vulkan-playable-001/build/async-regression-002/result.json
has SHA256 `6d898d9832a24321f68de2fb267784cd05148392ece3496ec7a325cff926521e`.
The final exact-source presentation-008/result.json in that build directory
has SHA256 `86e12afeb0b393ffe74c41c1f63ee163bb1209dfd76c0c981a04e435cd18335c`:
101 actual X11 presentations, four swapchains, three resize idle waits,
four Model2/one CRT pipelines, 28 buffers including diagnostic capture,
six images and zero validation errors. Steady frames do not recreate
pipelines, allocate renderer resources or wait for device idle.

Four independent SDL/direct window comparisons cover 497x385 and 317x245,
nearest and linear: 538,020 pixels exactly equal. vulkan-blit-001/result.json
has SHA256 `ff3ccdfb97a94c7ad72a3273a78ad7e5aa64bbf39f3c53f90703980777de0962`.
The Settings fixture passes 304 checks including actual mouse CRT toggles,
preferences, binding isolation, foreign contexts, 24 queued frames, resize,
CRT/plain-blit overlays and native-to-SDL-to-native switching. Report
build/worktrees/native-vulkan-display-001/build/panel-vulkan-003/result.json
has SHA256 `4cf43770dd0f9a56089d54c0458ec922ff582088ddaac75308c3488107b496a8`.
All use Linux X11/lavapipe with zero Vulkan validation errors. They do not
establish vendor GPU performance or physical Windows playback.

The actual Linux game passes three visible 2,600-frame arms: SDL Vulkan,
direct Vulkan and direct Accurate textures. All end at 500,035,460 instructions,
1,130,001,001 reference cycles and PC 0x005e6964. All WAVs (1,993,322 stereo
frames), final saves, inputs, observation and complete resume files match.
Each direct arm presents 2,600 frames with one swapchain, four Model2/one CRT
pipelines and zero validation errors. Default final pixels equal SDL Vulkan;
Accurate final pixels equal the same-machine CPU reference. These unthrottled
runs drain/capture audio without physical playback. player-parity-001/result.json
under the Vulkan worktree build has SHA256
`69d6b203c566c9bf61d24416eed287c855be4c76a396d9b74b86f37cc5303859`.
Its final-audit.json rehashes 227 inputs and 49 artifacts, SHA256
`995b115c9103bfb1e644e001ea732ef042fca0d3b26b534bf29a61968c370a1f`.
It retains a separate existing cross-API discrepancy: default SDL Vulkan/Linux
and SDL Metal/Mac images differ at 951/190,464 pixels despite matching CPU
execution. Same-backend equivalence is not cross-API or arcade certification.

Failure history: initial WSI validation exposed missing barriers for reused
offscreen/acquired images; explicit dependencies fixed them. A deliberately
closed queue gate stalled lavapipe cold allocations; prewarming three slots
and diagnostic readbacks before closing the gate produces the intended
three-pending-frame ownership test without driver allocation serialization.
The UI fixture first captured during an asynchronous SDL/X11 resize with
inconsistent dimensions; waiting for the requested size fixed the fixture.
The initial plain path reused a zero-strength CRT shader; review required the
actual SDL blit primitive and exact odd/downscaled window comparison instead.
Those changes are all included in the final measured source, without shader
forks, game-clock changes or rewritten capture baselines.

The regular Linux GCC/Clang CI now additionally compiles/links the entire
direct Vulkan host and ImGui path using ROM-free link symbols. Eleven script
regressions pass. Local Linux GNU 13.3 full frontend and fourteen host CTests
pass; report build/worktrees/native-vulkan-frontend-ci-001/build/frontend-ci-validation.json
has SHA256 `b74914ffb9eb9aa7330fcc18c6c82d9ef62639b086d36513e04e5a059c501b0c`.
Root Mac compilation with the combined source also passes fourteen host tests
at build/rally-native-mac-frontend-001. Separate Vulkan CI executes invented
offscreen/async and window fixtures with lavapipe. Windows cross compilation
and actual player packaging proceed separately; physical Windows GPU testing
and complete-race/independent MAME/PCB checks remain open.

Final independent review passes 1,029 checks: every recorded input/artifact
rehashes, all 170 source files match root, and complete recordings/PCM/saves
and both final image comparisons are independently reread. The sole mapped
root input difference is the documented platform README update. No blocking
flag, lifecycle or queued-UI reuse defect is found. Report
build/worktrees/native-vulkan-frontend-ci-001/build/final-game-independent-audit-001/result.json
has SHA256 `c2077303418c6f736377b25715f97bd6944e9583e82426c6eaed1c392c4cd380`.

## Direct Metal Mac packages and preview publication (2026-10-16)

The measured 8360ea9 player is packaged in both editions. Personal DMG:
build/installers/mac-direct-metal-personal-001,
32,140,096 bytes/SHA256
`dedd27a907c14c6d43c0c6427c2a7105e929e55afb41ffb915bf006090d6779b`.
ROM-import DMG: build/installers/mac-direct-metal-import-001,
12,953,680 bytes/SHA256
`2d1c4d4703904ecc9584f452df23404673139a1a21e0b271d8e007c1c1c41b9d`.
Independent package audit passes 182 checks: read-only mounted bytes match
manifests/staging, strict signatures verify, notices/icon match, the personal
edition contains exactly eight verified images, and the import edition contains
no separate ROM/data files. Signed executables differ from the measured input
only in signature blobs and three signature-size metadata fields. Both retain
the honest compiled-game-instructions notice and ad-hoc/not-notarized scope.
Report build/worktrees/native-metal-presenter-001/build/mac-package-independent-audit-001/result.json
has SHA256 `416f2030f490c90370c84f35fc88d022a97dad1493b62416c12a31d6ac38da0c`.

All five GitHub workflows pass for 8360ea9, including the complete platform
matrix and the Mac direct frontend feature assertion. The source/export gate
passes at build/publication/native-metal-player-source-gate-001.json. The
ROM-import DMG, sanitized player manifest and checksums are published as
v0.1.0-player.2 on the personal LaurentGarcia/sega-rally-recomp repository.
The tag targets 8360ea9 explicitly; personal data is not uploaded. Publication
inputs remain at build/publication/native-metal-player-002.

The first packaged launch failed to read the external Documents-directory
selection replay and exited before SDL/game initialization. Its log is retained
at build/validation/native-metal-player-001/manual-001/host.log; the specific
access failure is not diagnosed as a game defect. A launch without that
external replay uses bundled data and fresh temporary saves at
/private/tmp/sega-rally-metal-play-8360ea9-001. It runs windowed with direct Metal,
sound, instruction-boundary timing and CRT, beyond 5,340 frames without a
logged frontier. No controller was connected. It subsequently closes cleanly
at 11,062 game frames / 11,060 presentations / two skipped drawables, with
8,480,974 native stereo PCM frames and zero logged chip/host drops or device
input shortfalls. The completed log/saves are preserved at
build/validation/native-metal-player-001/manual-002. This telemetry is not
a completed-race or physical sound certification.

Next: finish the parallel Vulkan actual-game/Settings integration and CI,
while retaining the tested SDL Windows package and replacing remaining SDL
platform services separately. Full-course and independent arcade fidelity
requirements remain open.

## Direct Metal game and Settings integration (2026-10-16)

The Platform layer & build and Reference & validation contracts govern this
integration. Mac builds now select direct Metal for the game, CRT and Settings
by default. `--sdl-gpu` retains the same-executable comparison path;
`--native-metal` selects the direct path explicitly. CMake's
SEGARALLY_NATIVE_METAL defaults ON only for macOS and can be disabled. iOS and
non-Apple players keep their existing backend. SDL remains responsible for
window/input/audio services; this is not complete SDL removal.

MetalPresenter owns the device, ordered queue, CAMetalLayer and cached 496x384
game target, with two pending submissions at most and explicit normal-exit
error checks. It supports CRT, original nearest/linear output, integer scaling,
resize, hidden offscreen updates and paused Accurate-textures redraws. Settings
uses the unchanged pinned ImGui Metal backend through a small typed bridge.
The existing event/controls/pause/audio/save loop and generated shaders remain
unchanged. Capture drains commands and reads pre-CRT pixels. Teardown drains
before destroying Settings, the Metal view or the window.

The Release game at build/native-metal-player-001 uses the existing audited
generation in build/player-defaults-6c22e24-001/native. Its executable SHA256 is
`edad3e83bb9d0fe54f0a461d577d6d6d3ca405a56c5cb7d3074c72eb45cfb8c5`.
All thirteen dedicated host CTests pass. First CTest invocation had seven
Not Run entries because excluded fixture executables had not been built;
building their explicit targets then rerunning all thirteen passes. The
original log is retained at
build/validation/native-metal-player-001/ctest-unbuilt-fixtures-001.log.

Three actual game arms run 2,600 frames with the same fixed Lancia driving
inputs and fresh saves: SDL Metal, default direct Metal and direct Accurate
textures. All end at 500,035,460 instructions / 1,130,001,001 reference cycles /
PC 0x005e6964 without fault. Native PCM, exact input/resume payload, observation
records and final saves match. Direct final raw image equals SDL; Accurate
final image equals the same-machine CPU renderer. Hidden direct runs report
2,600 game updates and 2,600 skipped drawables. Metal validation reports no
errors. Report build/validation/native-metal-player-001/parity-001/result.json
has SHA256 `6e1b4eb1db017aa611d38cdc275e5b0c7faf5364c60e2d370cf8177c58134eb2`.
Only final images are compared; this is neither full-race nor MAME/PCB proof.

A separate visible paced run uses --player with no explicit sound/CRT/backend
flags. Fresh defaults enable all three. It presents 2,100 of 2,100 game frames,
produces 1,610,005 native stereo PCM frames, and reports zero chip/host drops,
device input shortfall requests or requested extra input bytes. Runtime is
38.825 seconds including startup, not a comparative performance benchmark.
Application output capture is not a physical speaker fidelity measurement.
Report build/validation/native-metal-player-001/paced-001/result.json has SHA256
`1c8bbd5418956cc1627ed18cc25cb6fb0e9f709ddda1fa1a02e9e23c6a524002`.
Both wrappers hash source, executable and data before/after execution.

The actual window presenter fixture passes 55 checks: 20 presentations, four
game updates, two hidden updates, Settings/CRT/filter/integer scale/resize,
paused texture changes, capture and drained teardown. Four captures equal
preserved SDL fixture pixels, with clean Metal validation. Report
build/worktrees/native-metal-presenter-001/build/presenter-002/result.json has
SHA256 `ef3f550583154e0f0b7da4f0bc8f5f052edff58e95246c2a1b737367af4277d9`.
Its initial compile failed when the new SDK deprecated a pinned ImGui symbol
under Werror; matching the app's macOS 13 deployment target fixes the fixture
without changing upstream code. Read-only integration review finds no blocking
input, resource lifetime or cleanup regression. Regular frontend CI already
compiles this real Mac path using ROM-free symbols; it now asserts the Metal
cache option and records renderer coverage. All eight script checks pass.
All five workflows for the preceding 0a1de61 checkpoint are green.

The separate Settings fixture initially crashed with a foreign ImGui context:
CreateContext restores the preceding context, so the shared panel initialized
its renderer on that foreign context. The normal single-context game path did
not expose this. Explicitly selecting the newly created panel context fixes
the defect. The real GPU fixture then passes 91 checks, including actual mouse
Original/Arcade CRT toggles, preferences, remapping isolation, foreign-context
restoration and native-to-SDL-to-native rendering in one executable. EEPROM
and backup sentinel bytes remain unchanged. Report
build/worktrees/native-metal-display-001/build/panel-metal-003/result.json has
SHA256 `c45f836571d2d869b46b1bd6fc7915f6e39a240b4507967fba907d1314675546`.
The failing binary and crash report are retained; debugger approval had delayed
investigation, and the existing system crash report supplied the necessary
stack without a debugger. The final player is rebuilt with this one-line fix;
its executable SHA256 is
`788e2548af403a54882adcc319d67de5f0a497aa1490128d3e654969a95f3604`.

The final executable repeats all three 2,600-frame game comparisons successfully;
parity-002/result.json under the same validation directory has SHA256
`a818a9a2bec7669bd4aa47f5cf4b04c72fb61a1a741d34686b9493927ef21c45`.
The reusable presenter runner now accepts --panel for the focused Settings
fixture. Both modes pass with the fixed UI and Metal validation: panel-runner-001
has 91 checks/report SHA256
`c3485dc46f7499c5c13041b105de0d7da0f7da07450b32bccff1ed56e42e6b3f`;
presenter-003 has 55 checks/report SHA256
`f7f267cdfae8a6faf25fa76c6e3b54ef913429ef2269b4642121cc919e7a650a`.
Both are under build/worktrees/native-metal-presenter-001/build, with source,
dependency and output identities preserved.
The final paced-002 run likewise presents 2,100/2,100 frames with the same PCM
count and zero reported audio drops/shortfalls. Its duration is 37.408 seconds
including startup; result SHA256 is
`8960c5e39a94243b0138ccd335afab32cd52958ee1eb98039c32688f8a2ef35c`.
Independent audit passes 863 checks, rehashing each final run's 219 inputs and
all 72 artifacts, parsing the WAVs/PPMs and comparing the recordings byte for
byte. The paced output contains 1,605,632 finite stereo frames; native WAV
peak/RMS agree with telemetry. All final fixture source/dependency hashes also
match root. Report actual-game-independent-audit-002/result.json in the same
presenter worktree has SHA256
`64473e657b654eb4c6bdf18b975baadcf34bc7559695244205f442cb48563759`.

Next: package and launch this measured Mac player, finish direct Vulkan game
and Settings integration, then replace remaining SDL platform services with
equivalent lifecycle handling. Full-course completion and independent
MAME/PCB/precision evidence remain separate requirements. Private scenes,
captures, generated code and personal game-data packages stay outside Git.

## Direct Model 2 renderers and Windows audio confirmation (2026-10-16)

Direct Metal/Vulkan development is now active under the inherited design's
Platform layer & build and Reference & validation sections. The former now
explicitly records the Rally migration from SDL, preserving the one-shader-path
and shared machine/frame/resource contracts. The playable apps still use SDL.
No game logic, generated shader or existing player executable changed here.
Implementation, commands and remaining integration are in
docs/native-renderers.md. Native Apple window prototypes remain separate,
untracked work; no clear-color host is represented as a playable native app.

The direct Metal Model2 backend encodes into caller-owned retained command
buffers and targets. It draws background tiles, ordered polygons and foreground
or CPU-composed HUD from the unchanged Model2Frame/resources and generated MSL.
It copies per-frame resources; tests retain two unsubmitted draws, mutate Video,
submit in reverse order and verify both images. Depth clamp mirrors pinned SDL,
and input device/target/dimensions/palette checks reject invalid calls. Existing
untracked direct Metal CRT sources and tests are integrated unchanged after
actual GPU verification. Neither native binary links SDL.

On Apple M5 with Metal API Validation, 38 Model2 images match SDL Metal exactly:
17,547,264 pixels, zero differences. Twenty-six invented images exercise tile,
texture, cutout, order, scissor, Accurate-textures, scale and widened CPU-front
paths; twelve draw three private recorded scenes at scales 1/2 with the polygon
scanline option off/on. Native-scale recorded OFF also matches preserved SDL;
ON matches preserved CPU pixels, and composition hashes pass. The native
fixture passes 440 checks including snapshot ownership. Report
build/worktrees/native-metal-model2-002/build/model2-metal-004/result.json has
SHA256 `7014ba379e1d72f560366b9cef7c962bceefcf967f9524dd6e3ce8e9aeb5b8bd`.
CRT passes sixteen exact comparisons and 204 checks; report crt-metal-002 in
the same build directory has SHA256
`309051d8443e56b2b45c26b14c3845e10425d7de4132e3285594ef101b47c691`.
Both runs have clean validation output. Independent integration review verifies
all root source bytes against the tested hashes, rehashes both reports' inputs
and artifacts, and confirms the queued-frame test outlives its CPU storage.
These are offscreen correctness checks,
not player performance or complete-game evidence.

Direct Vulkan consumes the same packet/resource packing and generated SPIR-V.
It checks Vulkan 1.1/depth clamp, format support and physical dimension limits.
The synchronous implementation snapshots resources, creates pipelines per
frame, submits, waits and reads back BGRA. Native linkage has no SDL. Thirty-eight
images match the separate pinned SDL Vulkan binary exactly, also 17,547,264
pixels/zero differences, with Khronos synchronization validation enabled and
zero errors/VUIDs. Execution used lavapipe/Mesa 25.2.8, loader/layers 1.3.275
in a Linux x86-64 container on this Mac. The report at
build/worktrees/native-vulkan-model2/build/lavapipe-model2-003/result.json has
SHA256 `c5c63e9d42d50cefde2b93ff204f22c3606006724d0d03ba37b0647bf5e62900`.
Independent review rehashed all 21 measured inputs, both executables, eight
patch payloads and all 38 image pairs. Software-driver equivalence is not
vendor GPU coverage or an arcade-fidelity certificate. A wrapper watchdog
bounds failed test processes; synchronous device-idle cleanup is not a
production latency guarantee.

Dedicated Vulkan CI executes the 26 invented scenes with lavapipe and
synchronization validation. Dedicated Metal CI builds a static native library
and links both fixture clients; it explicitly makes no hosted GPU claim.
The latter's seven translation units compile/link locally with C++20, strict
floating point and Objective-C ARC, and both clients have no SDL symbols or
linkage. Local report under the Vulkan worktree's build/metal-ci-review-001 has
SHA256 `f3ead3e5b38a1fb7cc798f0ac477140c763b09b6d21c9932942bbfc76c7ac888`.
The common recorded-scene reader contains no captured data; actual scenes,
images, generated code, executables and test logs remain ignored/private.

Retained failures and corrections: review found temporary polygon vectors
whose lifetime ended before Video rendered them, and synthetic palette bytes
without palette_w notifications, leaving tile pens black. The shared fixture
now owns polygon storage and initializes/asserts colored pens. Vulkan rejected
zero sizes but originally accepted positive dimensions inconsistent with Video;
it now rejects those and checks device limits, with positive mismatch tests.
The first Vulkan build hit a GCC -Werror fixture-parentheses issue; the second
run asserted colored pens before screen_update resolved the dirty palette.
Both fixture errors were corrected before the passing run. The first Metal
compile hit an existing unused-private-field warning, suppressed only for the
fixture build. Initial sandboxed GPU runs could not access displays; unchanged
elevated runs passed. The final Metal wrapper rejects dirty pinned SDL source
and terminates child process groups on timeout. No gameplay fix was inferred
from these harness failures. No capture baseline or shader was rewritten.

The requested Windows SDL build was already current with gameplay source at
6df4433: its older package source differs only in later docs/Apple packaging.
Fresh independent audit build/validation/windows-sdl-readiness-002/result.json
SHA256 `026c257e4630278f69a95b8574f638264b50fa6614910e7fee64d3c2e8bec019`
verifies manifests, PE architecture/dependencies and notices. The first audit
incorrectly expected a newer THIRD_PARTY table in the frozen ZIP; the second
checks the actual complete bundled notices. No rebuild was necessary. Both
ZIPs are collected unchanged at build/installers/windows-sdl-preview-001 with
README, manifest and checksums: import 17,000,917 bytes/SHA256
`065e1ab8bddf7ffebbb32c56a0e75e6b1de83d9c3d12a8307d45c1ccfa39e6c0`,
personal 33,555,527 bytes/SHA256
`a36b707c45cfb0b38b04cbb695c6f5d230e9a80614a217ad43e4b29877253231`.
The owner reported audio working on a PC after initially suspecting silence.
No audio code or package change was made. This is qualitative device playback
confirmation; no hardware details, GPU capture or full-race recording was
provided. Personal game-data packages remain local, with no new binary upload.

Next: integrate asynchronous resource reuse and presentation, then native host
Settings/input/audio lifecycle; validate actual game replay and physical
devices before switching a player backend. Keep full-course completion,
independent MAME/PCB comparison and precision gaps separate. Existing build CI
and native Vulkan CRT CI are green at 6df4433; the new workflows must pass on
the published integration commit. All thirteen source-boundary regression
checks pass; the exact committed tree still receives its publication gate.

## Apple icon packages and recording fixture correction (2026-10-15)

The bbfdb31 Mac configure/build succeeds; the bare game executable is unchanged
at SHA256 `596e06fb98ca2c93e424aca938bdff7e863af0af5c9b3c859046d3102add04a9`.
The first host run passed twelve of thirteen checks. The recording launcher's
separate subprocess fixture returned empty output for the new sips metadata
query, so the valid-recording case failed before signing. Its fixture now
supplies a synthetic ICNS and asserts icon bytes/plist at the mock signing call.
No production behavior changed. The repeated full thirteen checks pass in 1.38 s.

Both new Mac icon DMGs are built locally from bbfdb31; ad-hoc signing, strict
signature verification and disk-image checks pass. The import edition is 12,816,659 bytes, SHA256
`8909619e36283989280574bee288877afc4ba63e03c8eb2f6cba0e4668bc2211`;
the personal edition is 32,018,708 bytes, SHA256
`f01d7bf1967f1b31519a12d8bbf3f62b403b853aaaeb2e02d90d844e770b733c`.
Outputs are build/installers/mac-player-{import,personal}-icon-001. Both contain
the same ICNS, SHA256
`aced86cc20f59720323e85a7681b6f3d6d73c67f663bccc5479e317074b6fb50`.
Independent audit passes: every file matches its manifest, icon hashes are
covered by CodeResources before signing, both signatures verify, and the
personal/import editions contain exactly eight/no asset images respectively.
All non-signature executable sections match the reviewed bare executable.
Report build/validation/mac-icon-package-independent-001/result.json SHA256 is
`5de3436fa1964fba243dd10fccc186475a08f7d19cdeea5a75c0732d362e562e`.
These are local packages; existing release assets remain unchanged. No gameplay
or fidelity change is claimed.

The bbfdb31 iPhone version 0.1.0/build 2 archive and local debugging export pass.
The new IPA is 27,769,177 bytes, SHA256
`ede1db2a9262c7b874da4cdfa5a390ab08abc6b4984a99219f51ee662999b1ee`.
Compiled Assets.car and merged iPhone/iPad icon metadata verify in both archive
and exported app. The catalog includes phone, tablet and 1024-pixel marketing
icons with no alpha. Strict signatures, eight unchanged game images, eight
notice files and all 209 reused native inputs verify. The old IPA is unchanged.
Independent report under build/worktrees/ios-personal-signed/build/
personal-icon-001/independent-review/result.json has SHA256
`2a7833a48fcaea13b12b37d816fedc6605ce6ae1620fc227c56e8f21403c6c69`.
The actual certificate still expires 2026-10-27 18:31:31 UTC; the development
profile still covers only two devices. No install, physical test, provisioning
change or upload was performed. This is not a TestFlight distribution build.

All three new Apple packages are collected as verified hardlinks under
build/installers/apple-icon-preview-001 with README, manifest and checksums.
Source and icon integration are pushed; source gates pass at bbfdb31 and
c814c8b. The latter's hosted boundary job passes; compiler CI is still running
at this checkpoint. Further work is physical iPhone testing and, separately,
distribution rights, personal App Store Connect setup and submission readiness.

## Original app icon and local iPhone distribution preparation (2026-10-15)

The shared original 1024-square RGB icon is assets/branding/app-icon.png,
SHA256 `021c23ada71d5fb229384d4555c4a541e2449106314b40c418f70271a0896226`.
It is pinned by exact path and bytes in the source-release gate. No arbitrary
PNG exception was added; thirteen boundary tests pass. Mac launcher/player
packaging derives ICNS and sets bundle metadata before signing. iOS CMake
derives an iPhone/iPad AppIcon catalog for Xcode to compile before signing.
All derived assets stay in ignored build output. The platform/build section
of the inherited design governs this packaging-only change.

The reviewed six-file wiring patch is app-icon-wiring-002, SHA256
`a4b8b08b15e7a1adbae7b4a54290d81df6e9499e7091f2278582cccbe7467716`.
Actual ICNS conversion, iOS 15 actool compilation and 21 existing launcher
checks pass in the isolated wiring checkout. The first sandboxed iconutil
attempt failed; the same conversion succeeds with system access. The initial
001 custom PNG decoder proposal was discarded in favor of sips metadata checks
and standard Apple conversions. Host test count remains thirteen. Actual Mac
packages and a signed iOS archive must now be rebuilt from this commit; no
previously signed package is edited in place and game-source binding stays strict.

The requested friends route is TestFlight, but the owner has not obtained
distribution authorization for the game content. Work remains local: no upload,
rights declaration, account changes or invitations. The local-only TestFlight
export plan is build/icon-design-001/testflight-local-plan.json. Existing signing
has only a development identity; no matching app distribution profile is
established. Personal App Store Connect access/app record, beta metadata,
required privacy/export declarations and physical device testing remain pending.
The current iOS build has no native Files ROM chooser; removing bundled assets
would not remove the translated game instructions from its executable.

The actual development signing certificate expires 2026-10-27 18:31:31 UTC,
earlier than the profile's 2027-10-14 date. The certificate is the same leaf
embedded in the app and profile; evidence is
build/validation/ios-signing-expiry-001/result.json, SHA256
`210658bdcca4229e9ab8b0078a7e144df54366960227cec9f7a74bfd279cfef3`.
The profile covers only two devices. Documentation and the private collection's
README now distinguish these dates; the previous profile-only statement did
not establish app usability through 2027. Existing package bytes remain intact.

## Four personal platform packages collected (2026-10-15)

The Mac DMG, signed iOS IPA, Linux archive and Windows ZIP with personal game
data are collected at build/installers/personal-player-preview-001, with a
manifest, checksums and installation/validation limits. These are hardlinks to
the exact independently reviewed artifacts; all four hashes match. No personal
package was uploaded. New Windows/Linux import packages also remain local while
the owner considers distribution. The existing Mac player release stays public.
SEGA credit is published in the README and both releases; credit does not grant
permission to distribute game content. No post was made on social media.

Linux002 import/personal archives are 8,578,159 and 25,091,934 bytes. A freshly
extracted public package passes help and owned-ZIP import without source/build
mounts: 23 files and eight fixed image identities. Relocation report SHA256 is
`cdc9ae6e2e6ead9c2fe66eecd5f6df17602178f60d2bf84a283f7e023a776e15`.
The conservative preview target is Ubuntu 24.04/glibc 2.39 with GCC 13 runtime,
Vulkan and desktop/audio libraries. No software Vulkan or Xvfb was installed;
desktop rendering, physical audio and gameplay remain untested.

Windows001 import/personal ZIPs are 17,000,917 and 33,555,527 bytes. The exact
AMD64 PE and three bundled DLLs pass Wine CPU003 help, 4,260 importer checks and
actual owned-ZIP import with all eight image sizes/hashes exact. CPU001 omitted
the fixture argument; CPU002 hit Wine Z: host-path reparse points. Both failures
are preserved. CPU003 uses fresh ordinary C: storage; no runtime guard changed.
The final independent audit SHA256 is
`2b78328ce2042caf004dda60b41bf917c771c8a629f32994ea2d2e03c9d918d7`.
Native Windows rendering, physical audio, controller behavior and gameplay are
unvalidated. THIRD_PARTY records exact GCC/MinGW package versions, reference
commits, DLL hashes and runtime-exception/MinGW notices. The initially missing
MinGW copyright notice was captured before final packaging.

Aggregate desktop evidence is build/validation/desktop-game-binaries-001/
completion.json, SHA256
`881d71c4362a3b924d0744ec6aab7cdf08856a2c33c3a2643446b98306ff0e62`.
Original and copied 209-file native generation remain unchanged. Actual desktop
game source is 058d4ab; iOS remains the separately signed 64aca2d build. No new
game code or renderer changes were made. Public-only desktop archives/manifests
and checksums are staged under build/release-binaries/desktop-preview-001.
Native Windows/Linux/device gameplay validation and distribution decisions are
the next steps; these packages do not establish full arcade fidelity.

## Player publication and original game credit (2026-10-15)

Player Preview 1 is published at v0.1.0-player.1 under the personal account.
GitHub reports the exact local SHA256 for all three assets: the real 9,839,642-byte
Mac DMG, player manifest and checksum list. Metadata is preserved at
build/release-binaries/player-preview-001/published-release.json. A fresh release
listing contains only the source prerelease and this player preview; the two
withdrawn compiler-bootstrap releases remain deleted.

The README now prominently credits SEGA and its original development team,
states this project's unofficial status and keeps the source license separate
from game content. Matching credits are published in both existing release
descriptions. Credits do not change the private/public asset boundary.

The personal Mac DMG and development-signed iOS IPA have been independently
rehashed and their eight bundled images rechecked. Report SHA256
`3bb45727bdffbec054424b4e577d87a94aa203102c7322891248443bd35c1c12`
is at build/validation/personal-binaries-001/result.json. No phone install or
physical test was performed. The 058d4ab hosted build run 38823583736 passes all
six jobs. The actual x86-64 Linux game now compiles/links and passes CPU-only
owned-ZIP import with all eight image identities exact. Linux packages and
Windows game compilation remain in progress; GPU/audio execution is unvalidated.

## Actual Mac players and signed personal iOS binary (2026-10-15)

Mac game source 058d4ab builds for macOS 13/arm64 and passes all thirteen host
tests. Actual CPU-only `--rom --import-only` succeeds with PATH limited to
/usr/bin:/bin. The prebuilt public package excludes all asset files and uses
only system frameworks/libraries. Signed executable SHA-256 is
`80481d6fd11de6773c854107895bd08111890a5509fe1ea55e9f8238d6328db2`.
The public DMG is 9,839,642 bytes, SHA-256
`d86c7e4d584ea4a897e2e157ae4086e8ab93b50df9fc58c3f5b40e8f0a53da10`.
The matching private personal DMG is 28,995,081 bytes, SHA-256
`b3c5d8d2e2893ab77419157cbf584403a0caba72eace848439fba6f76921e268`.
Both are under build/installers/mac-player-{import,personal}-001. Packaging
revision is bf547ba; the game-source revision remains independently bound.

The exact asset-free public app imports the owned ZIP into a new cache and
completes a neutral 2,100-frame paced run in 39.817 seconds including startup.
No explicit sound/hook/CRT flags are used. All three activate; native WAV,
final image, EEPROM and backup are byte-identical to the 6c22e24 baseline.
CoreAudio postmix is nonzero and all chip/host drops and device demands are
zero. App and ZIP are unchanged. Review SHA-256
`898736feb9418043def203ddefdbc3743d6c8f2d9cb4af9af17e4354e744b503`
at build/validation/public-player-import-audio-001/review.json. This does not
prove native picker interaction, speaker audibility or macOS 13 execution.
The first actual Mac player release is prepared at v0.1.0-player.1.

The personal iOS archive/export at source 64aca2d succeeds with the existing
personal development identity and wildcard profile, without provisioning
changes, install, launch or upload. Its 24,341,339-byte IPA SHA-256 is
`a108448667ffcce4e2db26e2cc1dbe1fb8e661ee8127e604b5737594532224c1`,
under build/worktrees/ios-personal-signed/build/personal-device-001/export.
Strict signature, eight images/notices and all 209 reused native files verify.
The profile covers two devices and expires 2027-10-14. This is a private
ROM-inclusive IPA, not TestFlight or an arbitrary-device distribution.
Repaired compiler CI at 64aca2d passes all six jobs. Actual Linux and Windows
game binary builds remain in progress locally; no generated inputs go to CI.

## Player package source binding (2026-10-15)

Mac packaging now requires the reviewed game build revision as well as the
input executable hash, rejects intervening game-source changes and records
game/build versus packaging revisions separately. The app declares macOS 13,
matching the current game's explicit deployment target. This avoids assigning
a later packaging-only commit to an older executable. Actual Mac compilation
and all platform packaging are still running; no new game release is claimed.

## Prebuilt player import and two package editions (2026-10-15)

The native C++ importer verifies 23 required ZIP members by size, CRC32 and
SHA1, constructs eight fixed SHA256-verified images, and atomically commits a
new local asset directory. It never compiles, downloads or uploads. The actual
owned ZIP produces eight byte-identical known-good images in under one second
on this Mac. Import checks pass 4,276 cases; 4,263 ROM-free cases pass ASan/UBSan.
Peer review caught a linked assets.json acceptance gap, corrected before freeze.
The inherited deflate reader is not claimed fully format-strict; the fixed
member and final-image identities remain mandatory. No ROM/image bytes enter Git.

The player accepts `--rom` and CPU-only `--import-only`, or uses the native file
picker when no bundled/imported data exists. Imports use separate unique cache
folders with a bounded relative pointer; existing game saves remain intact.
Twenty sanitizer-backed startup shim checks cover callback lifetime after quit,
cancellation, retry, cache reuse, invalid pointer paths and save preservation.
Actual picker interaction and integrated game startup still require validation.

The Mac player packager now has import and private personal editions. Both
contain the real precompiled executable; only the personal edition copies the
eight verified game-data files. A peer review required fixed asset identities,
manifest link rejection and copied-executable hash checking; all three are now
included. The new digest target retains unchanged public-domain SHA1/SHA256
sources even with recording disabled. Initial notice tests expected three source
notices; the correct five-file expectation now passes. The source-only boundary
remains; the requested prebuilt-executable exception is explicit in rules/design.
Actual Mac packages, signed personal iOS output and Windows/Linux game builds
remain in progress at this checkpoint.

## Desktop frontend compiler checks repaired (2026-10-15)

Linux GCC and Clang compiled the real host and passed its thirteen tests.
Windows MSVC and Clang also passed all thirteen tests, then the report step
failed because Visual Studio omits compiler paths from CMakeCache.txt. The
reporter now reads generated compiler metadata and verifies actual paths,
versions, generator and toolset; five focused identity checks pass. Older Mac
CI libc++ lacks floating from_chars, unlike the local Xcode toolchain. The
already validated iOS C-locale decimal parser now covers all Apple builds.
The existing 250 preference and 8,217 grammar/round-trip checks pass locally.
Hosted confirmation follows this change; no game/device performance is implied.

## Compiler-bootstrap releases withdrawn (2026-10-15)

The requested deliverable is now explicitly a prebuilt game app, with local
ROM import and no installed Python, CMake or Xcode for the player. The previous
83 KiB builder download did not meet that requirement. Both public releases
`v0.1.0-mac-builder.1` and `.2`, including their six download assets, have been
deleted as requested. A fresh API listing confirms only the source prerelease
remains. Source tags/history and local build evidence remain available.
README download links no longer point at those withdrawn releases, and the
old bootstrap workflow is manual-only. Windows/Linux bootstrap work is archived
locally rather than published. Runtime ZIP verification/import and actual game
binary packaging now take priority over renderer prototypes.

## Mac preview 2 published and opened for testing (2026-10-15)

Tag `v0.1.0-mac-builder.2` is public at a18ed52 under the personal account.
GitHub Actions run 38821643272 builds the empty DMG; downloaded source hashes,
DiskImages checksum and all three uploaded release-asset digests match. DMG
SHA-256: `ff1f98afed13346ff3ed96507e72b65ce45f5b958d2fd1762f20b8afac854c75`.
The public builder pins the validated 6c22e24 game source. The personal package
remains local at `build/installers/mac-6c22e24-002/`, DMG SHA-256
`4586decff59192d9c7566b265d6526c71f386f9bea054723c1ce4a884c828109`.
Its signature, all eight assets/notices and unchanged copied app bytes pass.
Attempt 001 failed because sandboxed DiskImages could not configure a device;
the separate authorized packaging attempt passes. Both records remain intact.

The exact validated game app is open in a window with a new player save folder
under `build/player-defaults-6c22e24-001/`. Its live startup log confirms Metal,
sound playback, enabled CRT and actual presentation-pipeline creation. Existing
player saves are untouched. Physical audibility still needs the user's feedback;
the digital capture checks above do not establish the original silence cause.
Avoid competing local GPU/audio validation while this interactive session runs.

Parallel work remains isolated: Windows/Linux public builders, desktop frontend
CI and direct Vulkan scene rendering. Frozen native Metal and Apple host patches
remain local candidates pending integration; they do not change this player.

## Mac player defaults and builder preview 2 (2026-10-15)

The exact clean 6c22e24 Mac app completes all ten local pipeline stages and
twelve host CTests. A fresh-save, neutral, paced 2,100-frame run using `--player`
without explicit sound, instruction-hook or CRT switches confirms hook=1,
playback=1, effects=1 and actual CRT pipeline creation. Native PCM has 1,676,601
nonzero scalar samples; CoreAudio logical-device postmix has 1,672,673. Both are
44,100 Hz stereo, with no chip/host drops or device shortfall requests. The native
PCM is byte-identical to the previous app's explicit-sound check. Evidence is
`build/validation/player-defaults-6c22e24-001/run-001/result.json`, SHA-256
`aa1f145e06b1bf2a298d9e36d221722ad98675cd1116ad4d0c5d89ce912de2a3`.
This proves startup configuration and digital output, not physical audibility
or the cause of the reported silent session. A separate read-only CoreAudio
query reports unmuted output at 0.625 volume; no system settings were changed.
Fresh arcade boot has about 16.27 seconds of native silence before attract audio.

The empty Mac builder now pins that verified game source and launches with
`--player`; the developer Mac launcher passes the same flag so save/log arguments
do not suppress interactive defaults. Existing display preferences still win.
Preview 1 imports require a new import to receive the new executable. The 21
launcher, 20 coordinator and three package retry tests pass. One initial launch
command assertion still expected the old argument list; it was updated to
require the player switch. New empty DMG CI and release publication follow.

Native Vulkan CRT CI is now green at run 38820833067 on 6c22e24. Discovering the
actual `lvp_icd.json` resolves the earlier instance-creation failure. GCC 13 and
Mesa llvmpipe execute both common-shader cases: 24,576 C++ and 24,576 independent
Python channel checks pass. This is a software-driver offscreen check, not
Windows/game/hardware performance validation.

## Shared frame packets, portable host and player startup (2026-10-15)

The dedicated host now selects Metal on Apple and Vulkan on Windows/Linux.
Platform CMake enables Objective-C/frameworks only on Apple and uses compiler-
appropriate strict FP flags. Actual executable identity and atomic save
replacement handle Windows/Linux explicitly. A separate ROM-free link fixture
compiles the real frontend/runtime/UI/sound/recording ABI and exits before
SDL/game startup; CI never packages it as a playable app. Integrated Mac build
and all thirteen host CTests pass. Windows/Linux execution awaits the new CI
stage. The local desktop pipeline can plan/build/package personal inputs, but
its complete Windows/Linux game generation run is not yet established.

Projection, priority, polygon edges and scissor packets move into SDL-free
`Model2Frame`. Shared resource helpers preserve gamma, color-translation and
tile packing. The projection body is unchanged from 9c969c1 apart from class
and rectangle names. Both old-body and new builds pass 144 invented cases with
packet fingerprint `f4d290e762fc3523`. Initial fixture use of a temporary polygon
vector failed: Video retains its reference. Keeping the vector alive corrects
the fixture; production geometry is unchanged. This prepares direct backends
without duplicating Model 2 behavior.

The new touch overlay shares the existing lazy Settings context. Its stable
finger ownership, steering, safe-area layout and pause/focus suppression pass
511 pure checks and the earlier sanitizer run. The unsigned iOS 15+ sound/hook
app builds with these controls and eight verified assets/notices. The original
float from_chars availability failure is fixed by a strict locale-independent
iOS fallback: 8,217 new parser checks pass plus the existing 250-case checks.
An initial stream fallback mishandled hex/subnormals and was replaced before
the successful build. Signing and physical phone testing remain pending.

Player startup (no arguments or explicit `--player`) now enables compiled sound,
exact execution and CRT on fresh preferences. Saved display choices still load;
explicit diagnostics retain opt-in defaults, `--original-display` bypasses
effects and `--no-sound` disables sound. The first public builder still pins
9c969c1 and must be advanced after a verified new game build. The reported silent
personal app is not yet diagnosed: its exact unchanged signed binary produced
nonzero native and logical-device PCM during a 2,100-frame paced isolated check,
with peaks 0.04023/0.04020. This does not establish audibility on the user's output.

The empty Mac installer is public at tag `v0.1.0-mac-builder.1`, source34aa798.
Its CI DMG SHA-256 is
`ab7ea01170e70ee576070d6c73fb8f237861595eae5bb0785a7ee1f74537b0bb`;
remote digests match the downloaded/verified file and manifest. The first direct
Vulkan CI compiles/links but fails before device creation because the workflow
guessed the lavapipe manifest filename. The corrected job discovers and records
the actually installed ICD; shader/native code is unchanged, execution pending.

## Direct Vulkan validation target (2026-10-15)

A standalone SDL-free Vulkan fixture creates native device resources, descriptors,
render pass and pipeline from the unchanged production CRT SPIR-V. Two invented
image cases check identity and phosphor-mask output over 24,576 channels; a
Python reader independently checks the saved pixels. The native source passes
strict syntax compilation. There is no local Vulkan loader, so actual linking,
shader execution and readback await the new Linux CI job using Mesa lavapipe.
That software-driver result will establish functional API/shader behavior only,
not hardware performance, Windows support, a playable game or a complete renderer.
The runner retains failure logs and kills/reaps timed-out process trees.

## First importer completion and SDL-free direction (2026-10-15)

The real empty Mac builder completes all ten pinned pipeline stages in its new
`importer-e2e-002/20261015-154033-p63p5zup` session, including eleven host CTests,
packaging, signature and independent receipt/asset verification. It emits
`completed.json`; no GUI, game launch or upload occurred. The earlier DNS failure
remains separate. This validates the coordinator-driven first import on this
Mac, not first-run GUI interaction or other machines.

The first Mac builder CI run compiles/signs the app and creates its DMG, then
DiskImages reports `Resource temporarily unavailable` during immediate verify.
Packaging now retains each verify log and retries only that exact transient
condition, at most three times. Checksum failures and exhausted retries still
fail; three focused tests cover those outcomes. CI confirmation remains next.

The latest direction replaces the previously proposed permanent SDL GPU layer
with an SDL-free long-term runtime. Direct Metal and Vulkan prototypes and native
Apple host work proceed separately; the playable SDL build remains a regression
baseline during migration. Native window/input/audio replacements are required
too, and no speedup is inferred merely from removing an abstraction. Common
geometry/shader rules and the original fidelity gates remain requirements.

## Local Mac builder and four-platform distribution (2026-10-15)

The requested long-term graphics contract is Metal on Mac/iPhone and Vulkan on
Windows/Linux through the existing SDL3 GPU abstraction, with common renderer
and shader sources. This resumes phone development and desktop portability;
the design scope and release-platform matrix now say so. Full-machine/race/PCB
criteria remain unchanged. Root CI's green tools/tests matrix does not prove
that the native game host compiles or renders on Windows/Linux.

The personal Apple Silicon game DMG is preserved locally under
`build/installers/mac-2d101a4-001/`, SHA-256
`53f23cbc7770b63dd512ad346824e2669c3630467b5db1ab5966044701043658`.
It contains personal game data, is ad-hoc signed, not notarized, and is not a
public release asset. Source prerelease v0.1.0-rc.1 is already published at
`9c969c140a6f3d050395abda6fab8a29ff0bcf9e`; its three uploaded assets are
the source archive, manifest and checksums, not four game binaries.

A separate SwiftUI Mac local builder contains no game payload. It snapshots a
selected ZIP, downloads the exact public source revision above and the three
pinned dependencies, runs the existing local pipeline, then verifies completed
app bytes/signature before permitting launch. Each attempt has its own folder,
logs and saves; cancellation reaps build subprocesses. Twenty focused tests
pass, including failed-completion, receipt tampering and cancellation cases.
Packaging compiles/signs the original UI, creates an Applications-alias DMG and
records every bundle byte. CI now produces that empty arm64 DMG as an artifact.
This is an ad-hoc signed macOS 13+ builder requiring local build tools, not a
notarized ready-to-play app. GUI interaction remains untested.

The first real importer attempt failed at GitHub DNS inside the sandbox; its
separate failed folder is preserved. The authorized second attempt is running
under `build/worktrees/mac-importer/build/importer-e2e-002/`. It has downloaded
the pinned source/dependencies and completed generation/configuration, and is
compiling the native host. Record its final result before publishing the empty
DMG. The native iOS build and Windows/Linux host portability are separate work
in progress; no installable phone/Windows/Linux binary is claimed here.

## CRT stage gallery (2026-10-15)

The requested On the stages gallery now uses the same real production Metal CRT
pass as the hero. Forest, Mountain and the SHORT Practice Stage Clear each run
through the existing 20-draw fixture and pass. Their original native captures
remain linked for comparison. All settings, native frame identities and new
1152x896 PNG hashes are recorded in `docs/images/README.md`. The exact-hash
source boundary now contains eight reviewed documentation images. No runtime,
shader, build or game-generation input changed; final Mac build/CI validation at
`2d101a43a439091c4ee842fa036b6b7a1eab42b6` remains the code checkpoint.

The next requested deliverables are local installable Mac packaging and an
iPhone frontend with improved touch controls. They are separate from the public
source release. Public binary distribution must not silently include the local
ROM-bearing app; distribution/signing scope is being established. Keep the
original public source/release work progressing while preparing those targets.

## Portable CI fixtures and final release rebuild (2026-10-15)

The first public CI run passes macOS and the source boundary but exposes three
portable-test faults on Windows/Linux. Indexed dispatch emits an always-false
unsigned `page < 0u` guard when the first generated page is zero; GCC rejects
that under the fixture's warning-as-error contract. Generation now omits only
that mathematically redundant comparison for page zero. Populated graph checks
exercise exact membership, unaligned addresses, the high page and UINT32_MAX.
The initial new assertion also ran against the intentionally empty graph; that
local fixture error is preserved, corrected and all five boundary tests pass.

The timeout mock now creates the POSIX-only `os.killpg` attribute in its Windows
fixture. Production process handling is unchanged. The standalone runtime-timing
fixture needs GNU-style flags, so MSVC/clang-cl CMake builds select the same
standalone `c++` driver used by the other generated-code Python fixtures. Native
MSVC/clang-cl project builds and C++ tests retain their normal compiler; Linux
and Mac retain the configured GNU-style driver. No tests or warnings are skipped.
All eight Mac pipeline tests and the native/reference runtime-timing fixture
also pass locally. Actual Windows/GCC confirmation requires the next CI run.

A clean build at public source c3ffbb3 already completed generation, all eleven
Mac CTests and strict signed packaging; its initial sandbox process-list denial
was preserved and only packaging/signature were repeated with access. Combined
report: `build/publication/clean-build-package-001/combined-result.json`, SHA-256
`b3288b7bb507c63cefb61e0e4f28a161d10d02d59b0e16e666e2cf2095a20670`.
Because the generator now changes, build the final committed release source in
a new clean checkout and retain both results. Final source archives include only
reviewed source and the five exact-hash documentation images; no personal app,
ROM, firmware, generated game code or raw capture bundle is published.

## Actual Metal CRT presentation in README (2026-10-15)

The original screenshots correctly preserved native pixels before presentation,
so they did not show the requested curvature or scanlines. The README hero now
uses the actual production CRT renderer on Metal, through the existing saved-
image fixture. The same preserved Desert input is displayed as an original/CRT
comparison; the three earlier gameplay images remain clearly labeled. This is
an offscreen presentation capture of an existing frame, not a new live race.

The pinned existing CRT fixture completes its exact 20 draws and checks all
five controls, zero/disabled behavior, letterboxes and clamping. The output is
1152x896 with a 992x768 viewport and the default Arcade CRT settings. Image
provenance records the source, binary, preset and published SHA-256. The new PNG
is the fifth exact-hash documentation exception; all other capture restrictions
remain. Conversion metadata is removed without altering compressed pixel data.
The first preparation incorrectly assumed frozen sources were at the worktree
root; they are under the fixture's preserved preparation. No source file or
rendering behavior was changed for the capture.

## README showcase and restricted screenshot export (2026-10-15)

The requested public README now includes real native captures: a Metal Accurate
textures Desert hero, CPU-rendered Forest and Mountain driving, and the SHORT
Practice Mountain Stage Clear with the Lancia. Each is the original 496x384
image from preserved validation evidence, not a fresh RC capture or a new
fidelity claim. Image provenance records the source and published hashes.
The first PNG import rejected conversion-added EXIF metadata; the final files
retain only image/color chunks with unchanged compressed pixel payloads.

This expressly requested showcase narrows the no-capture rule only for these
four reviewed documentation images. Source rules, contribution and release
notes state the exception and separate game imagery from the BSD source license.
The source gate accepts only the four exact paths/hashes. Unknown, renamed or
modified images remain rejected. All twelve gate fixtures pass, plus actual
hash/mutation checks for the selected files. No ROM, raw trace, generated game
code, texture/sample extraction, packaged app or broader capture bundle is added.

The first final-build preflight found documentation changing concurrently and
stopped before dependencies/build execution. Its failure remains in
`build/publication/clean-build-001/failure.json`. The build now uses a separate
clean checkout at `c3ffbb36220b8d32928d9ee8e6c8bbf6fafdd774`; this README/gate
change does not alter runtime or app packaging inputs. Final results must name
the tested revision rather than silently treating the later documentation commit
as the exact binary source. Continue CI/source-archive verification before the
source-only prerelease tag.

## Public history identity mapping (2026-10-15)

The isolated publication clone now has branch `main`. The metadata pass changed
178 maintainer author/committer email fields across 317 reachable commits while
retaining every source tree and commit message. No upstream signed commit was
changed; unexpected signature invalidation would stop the pass. The local
preparation tip is `46e0888d405b92caefdfc79261520c83a696f5da`; its public
counterpart is `764daacd3fd33b14fd590d0b1c2921adb2855276`. The original branch
and validation artifacts remain preserved. `docs/revision-map.json` and
`docs/source-history.md` keep historical evidence IDs resolvable without claiming
that a metadata change rebuilds or revalidates an older executable.

The source gate passes the integrated preparation tree. Public documentation
passes 89 local links/anchors and fifteen documented Python command checks.
The eleven gate fixtures and 35 notice/launcher tests pass. Next is a clean
Mac build of this public source, CI and the source-only prerelease tag. Only
`main` and the intended release tag are to be pushed; local worktree/reflog and
reference branches are not publication inputs.

## Public source prerelease preparation (2026-10-15)

The public target is the personal `LaurentGarcia/sega-rally-recomp` repository,
with `v0.1.0-rc.1` scoped as an experimental Mac source prerelease. The README,
getting-started guide, architecture, manual and build reference now lead with
fresh-checkout instructions and the exact SDL3, ImGui and 7-Zip pins. Detailed
historical evidence remains below and in the status reference. CONTRIBUTING and
release notes state the content boundary and remaining full-game/MAME/PCB work.
The governing Reference & validation and Platform layer & build sections retain
their fidelity criteria; this prerelease does not declare M6 complete.

Notice packaging now includes ImGui's two embedded font comment notices with
complete MIT terms. The division timing helpers retain the pinned MAME copyright
in a collectable source preamble; executable statements are unchanged. The
launcher requires the font notice before repackaging. SHA-256 documentation now
correctly records its use in the recording-enabled Rally host. All 35 focused
notice/launcher tests pass and real pinned-source collection produces eight
notice files. The first collector attempt matched ImGui's table-of-contents
label as well as its actual section; the corrected extractor requires the
fenced section and a regression fixture covers the duplicate heading.

A source-release gate checks one committed Git tree and optional exported files
or archive against a hashed file manifest. It rejects recognized game/build/
capture paths, unreviewed binary content and high-confidence credential markers;
the reviewed Gradle wrapper and project HLSL shader header are explicit classes.
All eleven ROM-free gate fixtures pass, including expanded path-byte limits,
ZIP special files and encrypted private-key markers. A separate ROM-free CI job
checks the gate and Git archive. This is a bounded
artifact check, not a legal/security certificate or historical audit. A separate
read-only scan of the existing 317-commit/308-tree history found no obvious ROMs,
game-generated code, captures or high-confidence credentials. Its ignored report
is `build/publication-review-001/report.json`; limitations remain explicit.

Publication uses a separate clone. Maintainer author/committer metadata will use
the personal GitHub noreply address; upstream authors, notices and commit trees
are retained. A revision map will relate historical local evidence IDs to public
IDs. The original checkout and recorded apps remain preserved. Before tagging,
run the documented Mac pipeline from the final publication checkout, inspect its
source archive and retain the final reports. No personal ROM, firmware, generated
game C++, app, capture or recording is a release upload.

## Reference firmware imported and ROM inventory audited (2026-10-15)

The additional personal Desktop archives are now moved to ignored
`roms/reference/srallyc.7z` and `roms/reference/segabill.zip`. The former is
12,092,851 bytes, SHA-256
`08b74986f6601d5cc909c7c0249ce35593ccf5a8c654f60f2fbe329c9b6f91c7`;
the latter is 3,117 bytes, SHA-256
`c3d6c3e3b16df85724e6767d1b99c92103c964053628db5722fef5c28521f713`.
The originals no longer remain on Desktop and destination hashes match.

All three formerly missing firmware images pass expected size/CRC32/SHA1.
Loose verified copies are in `roms/reference/firmware/`. The billboard images
from both new archives are byte-identical. The 7z archive contains the Revision
C parent files, not Revision B's main program pair or a complete merged family.
The two Revision B files are copied from the unchanged original personal ZIP
to `roms/reference/srallycb/` and checked against the committed program manifest.
The native app is not rebuilt or modified. MAME's B/C definitions change the
two main program ROMs while sharing the other ROM data; no reliable Sega
gameplay changelog was found, so specific physics/graphics fixes are not inferred.

macOS `bsdtar -xOf` selective extraction first failed with a truncated-body
diagnostic. This was not evidence of a corrupt download: the complete 7z header
range and CRC checked, installed MAME audited the archive successfully, and
full `bsdtar -xf` extraction succeeded with empty stderr. Both extractor results
and the full 33-file extraction remain under `build/validation/firmware-import-001`.
The independent parent audit is retained under
`build/validation/firmware-import-diagnostics-001/result.json`, SHA-256
`8f400910c296643e8f071fb9d09735a84610f9e96b53f233d141300345789f6f`.

Installed MAME 0.289 then audits the final reference folder with
`-rompath <checkout>/roms/reference -verifyroms srallycb`: exit zero,
`romset srallycb [srallyc] is good`, one set OK, empty stderr, 0.687 seconds.
The command has a 30-second watchdog; no guest or device executes. Final report:
`build/validation/firmware-import-001/result.json`, SHA-256
`755afa1dc1b34ed78078393616a8c516c886c0f8e7a303f04fb7ff34092f2f6c`.
All seven final archive/ROM paths are confirmed ignored by Git.

The missing-ROM prerequisite is resolved; full-machine fidelity is not.
Next reference work must establish its source/build identity and review Rally
input/timing capture, including the current Lua input list's missing IN2
handbrake. Installed MAME 0.289 is not automatically the pinned source/build
oracle. Do not invoke inherited `scripts/run_trace.sh` as a ready-made Rally
check: it replaces its output directory, defaults to Daytona and expects the
absent `extern/mame/m2`. README, status and build guide now reflect the available
firmware while retaining the remaining comparison limits.
Independent file-only review confirms destination/input hashes, all five staged
ROM files and Desktop source absence without repeating the MAME audit. All
36 local documentation links/anchors and `git diff --check` pass.

## Candidate 006 and complete SHORT Practice lifecycle (2026-10-15)

Clean source `0b9ba0eba47d6effa3378f9a2b7fae9010a1c11e` passes all ten build
pipeline steps and eleven Mac CTests. Package:
`build/mac-candidate-006/mac/Sega Rally Native.app`; signed executable SHA-256
`9fef7f0ef4e6dde4ff7b47d44666a0b360de79a7e1e8034295df84d6e138753e`.
The build report SHA-256 is
`c48ed22164e506c48856ca022009ad1a8e384b8ef00135429678c03df6afd6d3`.
Independent package audit passes at
`build/validation/mac-release-audit/fresh-candidate-006-review.json`, SHA-256
`92b72140dbb6637bbc7462adf63e9bb4f64d501349efb4b094e5aad125c50f05`.
Its derived inventory has 54 instruction chunks plus dispatch table, 55 main
objects and 79,783 instructions. Every instruction body and address/chunk mapping
matches frozen v29; indexed dispatch layout is the only generation difference.
The reviewed proof files, runtime, TGP, sound, assets, notices, 26 Mach-O sections
and previous candidate/capture identities pass. The first audit expected a
peer status label `PASS` instead of `PASS_NO_BLOCKER`; that checker failure is
preserved separately from the corrected passing file audit.

The unchanged SHORT Practice Mountain drive now completes the full observed
cycle: Stage Clear, rank 2nd and 1:16.23; NAME ENTRY; Mountain Top 8 with
1:16.23 DELTA AT; then attract with INSERT COIN(S), CREDIT 0/2. The first bounded
run stops normally on the score table at 14,000 frames / 2,681,728,390 instructions
/ 6,084,441,002 cycles / PC `005a37a4`; it does not by itself prove attract return.
A separate authorized extension reuses the same prepared helper, source and
binary, raising only the bound to 18,000 frames / 3.6 billion instructions /
240 seconds. It passes at 3,456,982,099 instructions / 7,822,841,003 cycles /
PC `005de474`, with no frontier. Attract is visibly present at frames 15,000
and 18,000. Its 103.06-second wall duration includes a concurrent build and is
not a gameplay benchmark.

Final frozen report:
`build/worktrees/course-driving/build/course-driving/short-practice-v29-18000-frozen/report.json`,
SHA-256 `7bcd02faf5842621b1e1fd60c88161aaff96700cc42abd2401dbb513a9e395aa`.
The 1,880-event full replay SHA-256 is
`f3ea3829fd9aa4e6157beae1f1d25c07851a013a413716c15e2430759f1b8963`;
its last event at frame 17,999 repeats the final pins to retain scripted control.
All earlier 14k cabinet/telemetry prefixes, 188 shared images and initial/operator
saves match exactly. Independent artifact review verifies 244 prepared inputs
and 214 run artifacts at
`build/validation/mac-release-audit/short-practice-v29-18000-review-001/result.json`,
SHA-256 `02eb5ff52213cf78f5713f385a29b4a66487fa4547987cd0b2e441685d2c0591`.
This CPU run uses sound-off native execution with shared board models. The final
sound-enabled Mac course captures below now pass; their evidence is distinct
from the independent two-billion-instruction CPU comparison.

Before those captures, source inspection confirms atomic recording/save `.tmp`
renames. The ignored monitor now tolerates only a vanished file during a live
size scan and checks bounds again after child exit. Five focused monitor cases
pass: ordinary files, atomic rename, total cap, text cap and propagated permission
failure. Independent review confirms unchanged game/identity/PCM/save assertions
and cleanup. No capture had yet run or been frozen. Previous helper versions
are retained; current course runner SHA-256 is
`b086b5d65a90ebfd8ce83e70b5bbf9dffd0e6b056bce363c7654cbb74cdf2a33`,
and the exact replay wrapper, changing only its pinned runner hash, is
`5faecd3866771d40f4f946064a996bcb5dbf4f755769ef94179a3c5e9012ded6`.

### Final candidate 006 Metal captures

All three frozen plans pass against the same signed candidate 006 with sound
generation, the instruction-boundary hook and Accurate textures enabled. They
run hidden and unthrottled, with CRT and playback devices off. Each verifies
every applied cabinet event and pin, contiguous recording resumes, finite PCM
interval hashes, the complete seal, initial/final saves, frozen app inputs and
zero fault/drop rows. Wall times are 177.63, 162.68 and 125.19 seconds respectively;
they are correctness-run durations, not paced gameplay benchmarks.

| Capture | Frames | Instructions | Reference cycles | Final PC | Resumes | Stereo PCM frames |
| --- | ---: | ---: | ---: | --- | ---: | ---: |
| NORMAL Championship | 20,000 | 3,662,512,354 | 8,692,041,001 | `005ca464` | 40,006 | 15,332,761 |
| SHORT Practice Mountain | 18,000 | 3,456,982,099 | 7,822,841,003 | `005de474` | 36,014 | 13,799,492 |
| NORMAL Practice Lakeside | 14,000 | 2,676,028,109 | 6,084,441,002 | `005ca4b0` | 28,011 | 10,732,954 |

Results are under
`build/validation/mac-candidate-003-courses/candidate006-<name>-001/capture-001/result.json`,
where names and SHA-256 values are:

- `normal`: `9e61b629bfa287f8f727aea435ef4a36a73ee09773063483d64e6933f1a79df1`.
- `short-mountain`: `e19040f8f5d95beec00193da3d1a691c6feef936bdfd3bbc0e805e5e6d192f66`.
- `lakeside`: `6e5a57043df39d339f1f94817a78ca36a461798d6108f36fe028af08523c82f5`.

NORMAL applies all 4,398 events and exactly preserves candidate 002's complete
resume payload/footer, observations, native WAV and initial/final saves. Its
executable and assets manifest identities are separately bound to the new app;
the eight payload images and every other manifest field remain unchanged. The
independent checker initially over-constrained those two deliberately different
identity fields; that failure is preserved. The corrected file-only audit did
not normalize headers, relax payload comparison or rerun the guest. No NORMAL
image equality is claimed across the two rendering modes.

SHORT applies all 1,880 events and reaches the complete finish/name-entry/score/
attract lifecycle. Lakeside applies all 2,168 events and returns to attract
after retirement near segment 297/300; it does not finish the course. Both full
final 496×384 RGB images exactly match their frozen CPU images. All three match
the explicit CPU terminal counters; the original Lakeside CPU run did not log
PC, so `005ca4b0` is a new host observation, not an independently matched CPU PC.

The independent combined artifact review passes at
`build/validation/mac-release-audit/candidate006-courses-review-001/result.json`,
SHA-256 `e3ca6adffb2f6b9bb69ecc4f0ccf1ef02413aa7806eb0cbc62db607f2b21b3a6`.
Its NORMAL, SHORT and Lakeside subreview SHA-256 values are respectively
`7fc4766c8bf44ab4d065b085161904d7ae96517e575b07b6824378dd81adda2d`,
`78d12d53c929513936d9fc7e6651ee4df6659524948e339991a94525eab9f3bb`,
and `5d34be914aafe30519797fd9a19e66d77e3b0301ffec160e2df3f9b82a377e86`.

One exact signed-executable offline replay of the final SHORT capture also
passes: 36,014 resumes, 13,799,492 stereo PCM frames, all cabinet pins, results,
fault fields and final saves. It reaches the same 18,000-frame terminal above,
including tick 312,913,640,120, with all frozen inputs unchanged. The production
consumer's early offline branch opens no SDL/GPU/audio device and writes no live
saves. The 153.83-second duration is not a gameplay benchmark. Preparation and
execution use a separate 300-second watchdog; no retry or changed identity is
used. Result:
`build/validation/mac-release-audit/short-mountain-final-offline-001/result.json`,
SHA-256 `c1b5827af4bed202159e3652e02163f787d424445346a20d76c475e342041fdb`.
This completes the planned local candidate capture/replay gates, not independent
full-machine validation or physical sound measurement.
Independent file-only replay review passes at
`build/validation/mac-release-audit/short-mountain-final-offline-review.json`,
SHA-256 `0f31b9e7b22c0ed325e57d70886bac036433dc5b6c2aaf0cb6915cf6ec2b7b3f`.
It verifies the exact command/output, empty stderr, exit zero and every bound
before/after input, app, capture and save artifact. A checker-creation tool error
and system Python 3.9's missing `hashlib.file_digest` preceded the successful
read-only check under the existing Homebrew Python; no guest replay was repeated.

README, player manual, build guide and status now identify candidate 006 and
separate its sound-on course checks from candidate 003's earlier paced playback
evidence. The two-file README/status patch was reviewed and frozen at SHA-256
`f343669b0e920b692cca955f6d676160668e7c7cf978f006086a8efad1a85622` before
integration; its 17 local links/anchors pass. No game/runtime changes accompany
this documentation update, and candidate 006 is not repackaged or re-signed.
The integrated four-document check passes all 35 local links/anchors, and
`git diff --check` is clean. No runtime tests are repeated for these prose edits.

### Reference firmware search

A targeted local search checks 64 members of the supplied Rally and Daytona
archives plus 31 matching-size loose files by name and size/CRC candidates.
None contains `epr-17891.ic12`, `epr-16726.bin` or `epr-18022.ic2`; Downloads has
no archives. Both archive SHA-256 values remain unchanged; no firmware was
downloaded or extracted. Frozen result:
`build/validation/firmware-local-search-001/result.json`, SHA-256
`48d625e744af59940079d908b158d277fc102992a0fde5fd910e25afa7acf8d5`.
The first two identities come from the retained pinned `model2.cpp`; the third
is separately confirmed by installed MAME 0.289 metadata and upstream
`segabill.cpp`, not the retained partial source. This does not establish the
installed binary's source commit. No official public download was found, and
MAME's authorized ROM collection does not list Sega Rally. The build guide now
records filenames, sizes and CRCs for checking another personal backup.

These images would unlock full MAME reference comparison; obtaining them alone
would not certify arcade fidelity. Full-race MAME/PCB comparison, extended-
precision floating point, whole-game graphics and all-course completion remain
open. The local signed Mac package is a development/test candidate, not a
notarized public release or a completed fidelity milestone.

## Practice results and name-entry continuation (2026-10-15)

Candidate 005 at clean `fc24ef8a0d059f552c277a442289f9ed6c500034` passes
the ten-step pipeline and eleven Mac CTests. Signed executable SHA-256 is
`542c3cd5cf7a53b8615565764a2a96cf67da2a360eba7a448a972ceabda77a07`.
Its independent package audit passes at
`build/validation/mac-release-audit/fresh-candidate-005-review.json`, SHA-256
`e2c928007cb3a1cafe1c5b2f07fe4ca57e51fd3b6a77590ca0a815ec86e2e3bf`.
All 51 instruction chunks and 76,257 address/chunk mappings equal v20b;
indexed dispatch is the sole generation-option difference. Candidate 005 is
preserved as an intermediate package; its new gameplay checks were held for
the remaining post-result callbacks.

The ordinary NORMAL-difficulty / SHORT-mode Practice Mountain drive now reaches
an actual **MOUNTAIN STAGE CLEAR!** result, rank 2nd, lap and total **1:16.23**
at frame 10,200. The unchanged input prefix reaches further missing native
entries during the post-result transition. Each first failure is retained and
each added seed has a separate provenance audit; generated C++ is not edited.

| Runtime entry | Canonical ROM | First-failure frame | Instructions | Reference cycles |
| --- | --- | ---: | ---: | ---: |
| `005b2820` | `00013820` | 10,506 | 2,011,354,615 | 4,566,355,705 |
| `005b285c` | `0001385c` | 10,506 | 2,011,354,625 | 4,566,355,728 |
| `005e7d30` | `00048d30` | 10,507 | 2,011,552,834 | 4,566,789,351 |
| `005b7400` | `00018400` | 10,508 | 2,011,751,422 | 4,567,223,771 |
| `005b7690` | `00018690` | 10,525 | 2,015,114,063 | 4,574,611,966 |
| `005b87a0` | `000197a0` | 10,525 | 2,015,114,068 | 4,574,611,988 |
| `005b8fb0` | `00019fb0` | 13,785 | 2,639,196,598 | 5,991,409,125 |
| `005b9150` | `0001a150` | 13,785 | 2,639,207,401 | 5,991,425,302 |

The second row is an actual RET reached ten instructions and 23 reference
cycles after the first; it was not inferred from an adjacent address. Audit
reports live under
`build/worktrees/audio-device-headroom/build/validation/frontier-<runtime>-review-001/result.json`.
Their SHA-256 values, in table order, are
`a250b30169cd5f79d0972faa8cdae0b2f27fb8c1a07d79b9d012ee2bda0976cb`,
`a9550b5bd11b497eeccff6dbbccab5086335fb0196a9afa6fb6c160f549ccd63`,
`2cd9bb050f7357d6ac4258be8612b2cc78c873b2426e94819ff290e27415d512`,
`ee6f6008f8bd66fd278847784241a6379593029392ed4ef0e89f6bbae900fc1c`,
`1d9edac4a6481ed56708f2ea11efe553b6af2f8c487750cac6243786dbfaed36`,
`71fb942d924396bc496687277aa1033ecf5986fa2b4738a4409ffcf9d90f288b`,
`2bb025467af6e622baf221a30e0ea6c1cd07419a870618ccb215a9ab0eab5030`,
and `7d59fef403bef71cfba0eddbb8b77a329ea0c5881de2f704dc77926a426b08aa`.
The v23 generation contains 76,359 instructions in 52 native objects. The v24
graph grows to 53 objects, so the ignored helper's inventory check now derives
the main object list from the boundary contract and checks the 26 non-main
objects separately instead of assuming the old total. There is no guest or
runtime semantic change. All eleven newly observed entries are integrated;
continuation beyond the latest result callback and the final Mac package remain
pending. The eleventh entry is reached after the reviewed table-derived
callback continues for another 10,803 instructions and 16,177 reference cycles
within frame 13,785. It is a separately observed block outside the table's four
entries. The v28 failure and exact NAME ENTRY image are retained; the four
reviewed proof files and all earlier observed rows remain unchanged.

The v26 continuation reaches a visibly confirmed NAME ENTRY screen and then
stops at the second callback in a bounded four-entry table. This is a real
discovery gap in the existing saved-object and unsigned-compare table patterns.
The i960 static recompiler design's indirect-target step supports a separate
decoded `direct_mask_callback` proof: literal AND3 bounds the index, a scale-four
load uses and replaces that register, and a combined nonzero branch goes directly
to CALLX through the loaded register. The null path has a short terminating
whitelist. Operands, reserved encodings, branch destination, table/body hashes
and reset-copy aliases are checked; the existing patterns are unchanged.

The table at `005b7670` has four static candidates: `005b87a0`, `005b8fb0`,
`005b9210`, `005b9410`. These stay separate from actual execution observations;
live RAM table mutation, other entry paths and complete coverage are not ruled
out. Existing generated-code guards remain in place. The new mode changes
discovery only, with no emitter, runtime, timer or game-state change.
Thirteen table tests, twenty-five continuation tests and ten bootstrap tests
pass, including twenty-five rehashed operand/control mutations. All 120 current
table candidates validate against the verified supplied program. Frozen report:
`build/worktrees/direct-mask-proof/build/validation/direct-mask-proof-review-002/report.json`,
SHA-256 `17edd011086c29c164beb4f949c68729f45a103d951fac22210da34b60665f36`.
Independent patch/proof/test-source review passes at
`build/validation/direct-mask-proof-independent-review/result.json`, SHA-256
`7ac916cd7fa9f081d9494fa7525b422e2d0a634e9cd778ce9f184c50eb7b3952`.
That review verifies the reported test scope and exact source identities; it
does not claim a second test execution or retained raw unittest logs. The
consolidated generated path and final Mac checks remain to be run.

The proposed SHORT Lakeside shortcut is rejected by decoded, ROM-checked
conditions: the finish test requires the event's segment-zero crossing before
testing cumulative progress above 290. Reaching segment 297 is insufficient.
The first allowance is also 75 seconds in SHORT versus 80 seconds in NORMAL.
Read-only report `build/worktrees/audio-device-headroom/build/validation/lakeside-short-finish-001/report.json`,
SHA-256 `fddc45fa4d2fc65051dbf302dbb7d7c309a21f562da9d1254bb011ba06a113c8`.
No additional Lakeside drive or controller tuning follows this rejected theory.

One independent comparison of the fixed SHORT prefix passes two billion
instructions, 58,120,650 ordered writes, 4,539,611,659 reference cycles,
110,449 interrupt entries and 10,445 renders, ending at PC `005a37a4`.
All 1,877 original cabinet events apply, with none pending. The independently
compared Practice start and finish entries execute 9,148 and 421 times.
The first finish visit is before instruction 1,921,603,185 and after reference
cycle 4,356,939,914 at frame 10,025; the four-cycle difference from the native
frontier witness is that the comparison logs after executing the opcode.
All 200 frozen inputs remain unchanged, and compile/guest stderr are empty.
Compile time is 7.28 seconds and comparison time is 1,141.28 seconds; neither
is a gameplay-speed benchmark. Result:
`build/validation/practice-short-v20b-i960-reference/attempt-001/result.json`,
SHA-256 `e4df250c8811ec7e8caa288a0712bc5496ed17a93ae67b670313d915d552a044`.
This is generated i960 versus unchanged I960Ref with independently instantiated
shared board implementations, sound off and a null Machine hook. Its fixed
bound excludes the post-result entries above and the separate NORMAL retirement
callback. It cannot establish full-machine, sound or hardware fidelity.

## Observed one-lap Practice finish callback (2026-10-15)

Candidate 004 at clean `b168631e3824cc287d327005cb9b17daed7ad37d` passes the
fresh ten-step pipeline and all eleven Mac CTests. Signed executable SHA-256
`e3d6767193f89bab03598b9b3f33a0f43d1a827093e8351cc8462c38f9bd7fb8`.
Independent package audit passes:
`build/validation/mac-release-audit/fresh-candidate-004-review.json`, SHA-256
`47757d27b6b286140c002dcf38262d7b9e95f300a51d67ade3a0192d79466331`.
All 51 instruction chunks equal v19. The dispatch table differs because the
production pipeline enables indexed dispatch; all 76,097 valid address/chunk
mappings and alignment/page/missing-target guards agree. Reachability differs
only in that option. The first audit expected an explicit false flag which v19
omits; its failure is retained separately from the corrected passing audit.
All twenty bundle files, eight asset images, notices and 26 raw/signed Mach-O
sections pass. Candidate 003 and its captured evidence remain unchanged.

The new Lakeside NORMAL Practice drive also reaches 14,000 frames without a
frontier: 2,676,028,109 instructions / 6,084,441,002 cycles. It retires near the
end of lap one and returns to attract; no completed Lakeside lap is claimed.
Its 2,168-event frozen replay SHA-256 is
`d2283f4f7ddfcbfdb1237f65f869b4a8ce964ea7306f7f226b2257ac315c1728` under
`build/worktrees/course-driving/build/course-driving/practice-lakeside-v19-frozen/`.

Ordinary Test/Service inputs select NORMAL difficulty / SHORT game mode, with
actual guest EEPROM saves and option bytes verified. SHORT Practice has one
Mountain lap. The unchanged controller reaches its finish line with time
remaining, then stops at observed missing target `0x005bfcf0`, frame 10,025 /
1,921,603,185 instructions / 4,356,939,910 cycles. The image shows 1:15.17 and
two seconds remaining; results have not executed, so this is not yet a completed
race. Canonical ROM `0x00020cf0` is the sole additional observed seed.
Independent provenance review passes at
`build/worktrees/audio-device-headroom/build/validation/frontier-005bfcf0-review-001/result.json`,
SHA-256 `47d14aec44cfab96269454d3f60b874bf345875d36cce7b1ada67077249a4c4d`.

The first v20 generation monitor encountered a temporary compiler object
renamed between directory listing and stat; it stopped before guest execution.
That failed attempt and partial output are preserved. A new bounded wrapper
tolerates vanished temporary files and regenerates into a separate directory;
ROMs, source and the three observed seeds are unchanged. Finish continuation,
independent CPU comparison and sound-enabled Mac course checks remain pending.
Candidate 004 gameplay gates were held to test the final build containing this
finish entry instead. No recorded replay identity requirement is weakened.

## Practice course coverage and two observed callbacks (2026-10-15)

The i960 static recompiler, Inputs and Reference & validation sections govern
this extension. Ordinary cabinet controls reach Practice, Lancia AT and Mountain.
The first navigation probe steered after the short mode-selection window and
remained in Championship; measured screenshots corrected that timing. The
successful Mountain selection then reaches missing native target `0x005c0fa0`
at 476,442,790 instructions. Its helper did not log exact frontier frame/cycles;
only completed periodic cuts bound that point. The original failure is preserved.
Canonical ROM `0x00021fa0` is now an observed seed, without generated-code edits.
Independent provenance review passes at
`build/worktrees/audio-device-headroom/build/validation/frontier-005c0fa0-review-001/result.json`,
SHA-256 `3234bc67646b0603f3af59795c28c67893843be1050516c68a7daf922504dff9`.

Fresh generation reaches a complete Mountain lap in 1:16.23, then finds missing
Practice callback `0x005bfe70` at frame 9,990 / 1,883,400,392 instructions /
4,341,713,718 cycles. That first failure and its actual inputs/save witnesses
are preserved separately. Canonical `0x00020e70` is the second observed seed.
Independent review passes at
`build/worktrees/audio-device-headroom/build/validation/frontier-005bfe70-review-001/result.json`,
SHA-256 `29b6893d34f3dd673100f1a1279464198d568ead55385e64048ac301cea92d14`.
The consolidated manifest SHA-256 is
`c99275da17c5ae3c3ad1c91c81d9e18b7dada4c31cd0cb58cc15c254fc629b49`.

The v19 generation has 76,097 reached instructions, 52 native objects and no
missing templates; imported images exactly match the prior generation.
The unchanged route controller passes 14,000 Practice Mountain frames /
2,657,203,072 instructions / 6,084,441,003 cycles, returning through normal
retirement/attract without another frontier. The first lap completes; the
second retires. This is not a completed two-lap Practice race. All 335 prior
telemetry rows, 334 menu rows, 55 selected images through frame 9,600 and the
old cabinet-input prefix match exactly. Evidence:
`build/worktrees/course-driving/build/course-driving/practice-mountain-v19-frozen/evidence.json`;
run result SHA-256 `7f35282fd08862e9bbf046c96eea6add441844496ffbf181fcc0736f42c369d7`.
The 2,946-event full drive SHA-256 is
`193ff85fd8609709f30efde3ffc82eb1b375e828a674911582bba23a94d6d591`.
An inherited helper guard rejected the first driving invocation before Machine
construction; the failed invocation remains preserved, followed by a distinct
helper version and actual guest run. Runtime and physics were unchanged.

Lakeside also has a built-in course-menu entry independent of Championship
finishing rank. The actual gear/brake sequence changes course 2 to 3 at frame
2,080 and reaches the Lakeside grid through 3,000 frames without a frontier.
Static address/hash audit:
`build/worktrees/audio-device-headroom/build/validation/lakeside-access-001/report.json`,
SHA-256 `98a3a9da7e0bde697902cc70a11b83db7d9003130afaf189299e0b110b3a4ee1`.
The two committed `config/segarally_practice_*_select.inputs` scripts use only
cabinet pins and release everything for manual control at frames 2,100
(Mountain) and 2,160 (Lakeside). They assume fresh default cabinet settings.
Both are verified in CPU-only native probes; the next Mac build must verify
these new paths with sound. No first-place Championship or Lakeside finish is
claimed by menu entry.

Operator findings: NORMAL/SHORT/LONG use the same one-lap Championship tables;
LONGEST changes it to five laps, so it is not a simple timer extension. EASY
really changes per-course allowance tables, but its gains/losses cancel across
the unchanged measured Championship drive, producing the same Mountain timeout.
It must not be described as ignored. SHORT Practice uses a different legitimate
one-lap configuration; its finish/results path is the next bounded investigation.
An independent sound-off CPU comparison through both callbacks is also pending.
The preserved candidate 003 remains unchanged. Next: fresh candidate 004, NORMAL
regression, sound-enabled new-course captures and exact recording replay.

## Candidate 003 visible playback passes (2026-10-15)

The fresh pipeline at clean `7beefcf315e8f60614b334eaec6fcc500836c528` passes all
ten steps and eleven dedicated Mac CTests. Current app:
`build/mac-candidate-003/mac/Sega Rally Native.app`; signed executable SHA-256
`a07578cb501012b0a2b96a3be7de28fcc8e0b3e1bd24989ea9c22c5fe7d51416`.
Independent source/generated-code/assets/notices/signature audit passes at
`build/validation/mac-release-audit/fresh-candidate-003-review.json`, SHA-256
`2e04a2912377994707aa0752c9331385a2fd6e4572649390f2bf8728aa68ea7e`.

One quiet visible capture enables CRT, Accurate textures, sound and the boundary
hook with fresh FF seeds and the original neutral frame-0/7,199 input schedule.
It passes 7,200 frames / 1,399,415,787 instructions / 3,129,161,000 cycles /
PC `005ca464`, exit zero. Native WAV, all 14,404 input/execution payload rows,
observations and final native image match candidate 002 exactly. Its 121 audio
telemetry rows report zero demands or chip/host drops. Actual device format is
44.1 kHz stereo / 1,024 frames; the target is 3,670 frames and 2,902 remain queued
at detach. Visible CRT swapchain initialization is logged. Whole-process duration
126.785 seconds is not a performance comparison with the hidden prior capture.
Result: `build/validation/mac-candidate-003-host/audio-001/result.json`, SHA-256
`af337c65b09df13387caa7e3b969becc247997086301201ac458c0e4947929ea`.

Independent audit verifies 48 frozen inputs, twenty retained outputs, recording
seal, every PCM interval hash, neutral pins, seed/final saves and the unchanged
signed app. All 5,516,288 postmix frames are finite and covered by 5,387 contiguous
packets with no capture failure. Report:
`build/validation/mac-release-audit/candidate003-audio-review.json`, SHA-256
`c33b68916bf5bada52d9cb979873c08655c797ba800d8fb89c5b91b9cd1ed74e`.
Pre-execution review caught a test-script assertion looking for the queue log
in redirected stderr instead of host.log; it was corrected and frozen before
the single device run. No capture was discarded or retried for that issue.

An unbound SDL stream using the production policy reproduces the entire new
postmix bit for bit, with zero queue/scalar mismatches, short callbacks or missing
frames. The old gap's corresponding native-input neighborhood has no inserted
zero tail in this capture. Report:
`build/worktrees/audio-device-headroom/build/validation/actual003-postmix-001/report.json`,
SHA-256 `7f5d270454bf62c98820d978dad4ce546e2de71e385734dde2f3941d37276031`.
Independent review binds twenty inputs and outputs at
`build/validation/actual003-postmix-independent-review/result.json`, SHA-256
`5e915e111fd769f98b53d42961b3f6f7053a16249312bd38f3cab620b6c262e2`.
The new visible run's longest post-prime producer pause is 18.460417 ms; it did
not repeat the old 52.918334 ms pause. The separate exact-old-schedule
counterfactual demonstrates coverage of that recorded longer pause. Together
these verify the captured starvation remedy, not every reported glitch,
physical speaker waveform, device route or full sound fidelity. The earlier
failed result remains preserved. No further audio device run is needed without
new evidence or a source change.

Course measurements also reject two speculative controller diagnoses. The first
observer watched an inactive pose-publishing routine and honestly produced no
latches; it was not evidence of incoherence. The corrected dual observer watches
both actual publication families and records 15,201 complete publications per
slot, with no observed partial/mixed primary-field frame cuts on the unchanged
normal-difficulty path. Node `0x500000`, handler `0x5d3f40` and global `0x2140cc`
associate the original `0x213b40` pose with pedal-driven physics; prototype
"camera" annotations were misleading. The other pose is a different car,
15–76 world units away on Mountain. Secondary velocity agrees with observed movement.
Exact terminal counters, inputs and selected images remain unchanged. Evidence:
`build/worktrees/course-driving/build/course-driving/dual-observer-analysis-001/result.json`,
SHA-256 `fe9fea8dca98e3941b83e1542c821c7869b9433fa4a4c75271935b75a30504ad`.

One measured Mountain-only early-braking experiment preserves the successful
Desert/Forest prefix but retires earlier, near route segment 194 versus the
best normal path's 281. That failure is preserved; neither a wrong-pose retarget
nor further blind steering/braking sweeps are justified. A further coverage path
uses only the game's ordinary Test/Service menu controls to select EASY, keeping
the normal replay unchanged. Actual menu probes show GAME DIFFICULTY EASY and
GAME MODE NORMAL. The game saves the setting and exits through its own menu;
the subsequent bounded drive still retires at the same Mountain point, with
the added 3,000-frame operator prefix accounted for. EASY did not produce the
assumed useful time extension; a completed race remains unverified.
EASY does not itself establish Lakeside access: the observed Mountain-finish
branch requires a further rank-related condition before selecting course 3.
The EASY run reaches 24,000 frames / 4,446,796,292 instructions /
10,430,441,000 cycles without a native frontier. Its racing pins and 476 telemetry
samples match NORMAL after the operator offset. The frozen coverage inventory
binds forty source/input files, sixteen prepared manifests and fifteen run
results, with actual initial/operator-saved/final save snapshots:
`build/worktrees/course-driving/build/course-driving/coverage-final-001/evidence.json`,
SHA-256 `59f452fc5afe3cd21ff1ff385d24963a23eb968d36f063d35caae89e9edcd6c9`.
Its accompanying README SHA-256 is
`2028b52d67a65135d3ea005b2efc3a3202b9372a8d3a0fba5b7193a17ad64b05`.
Operator GAME MODE has separate NORMAL/SHORT/LONG/LONGEST labels. Its effect on
the remaining course path and the front-end Practice route are the next bounded
coverage investigation; neither is established by the failed EASY test.

## Device-period audio headroom (2026-10-15)

Audio and Reference & validation govern a measured presentation-buffer fix.
An unbound SDL stream reproduces all 5,512,192 frames of candidate 002's captured
postmix bit for bit, including the 288-frame shortage, with all 14,404 recorded
pre-push queue values exact. It recomputes the existing 60 ms drift policy rather
than merely replaying recorded frequency ratios. On the identical PCM and
delivery/demand schedule, adding one measured device period to the target removes
all short packets. This establishes the captured starvation mechanism; the
specific cause of the host pause and every previously heard glitch remain open.

`AudioQueuePolicy` now converts the actual output period to 44.1 kHz source
frames, rounding upward, and adds it to the 60 ms base. The observed 44.1 kHz /
1,024-frame device uses 3,670 frames, about 83.22 ms: nominally 23.22 ms additional
audio buffering. Initial/re-priming, retained overflow tail and drift control
use that target. The 240 ms backlog cap and ±0.5% trim bounds remain unchanged.
The target is limited to half the cap so startup cannot repeatedly clear before
crossing the prime threshold. Invalid formats or device periods over 60 ms fail
explicitly; this compatibility limit and lack of adaptive route-change retuning
must remain visible. No generated PCM, board clock or game code changes.

The existing audio fixture now reproduces a three-callback late-producer case
(288 missing frames with the old policy, zero with the new), and checks rate
conversion, invalid/oversized periods, startup bounds, backlog recovery and WAV
behavior. It and the 24,389-check output-capture fixture pass, as does strict
host syntax. Independent source review finds no blocker. Counterfactual playback
begins later and has one fewer output packet; it is not a same-length or actual
device result. Source patch SHA-256
`78784f5fa0336e8b91768ae014bd129be0fb8d0d5ecbf44ffbc33c38fd22bd4a`;
report `build/worktrees/audio-device-headroom/build/validation/headroom-001/report.json`,
SHA-256 `4205f794345b02ce99e39e494808a4ca7574f9e01a436ce1d96039e0c399c5ed`.
All three integrated source files match the report. A fresh candidate 003 build
and one visible, paced, neutral paired capture are next. Original candidate 002
and its failed audio result are preserved.

## Fresh Mac candidate, championship coverage and captured audio starvation (2026-10-15)

Candidate 002 completes the entire fresh build/package pipeline at clean source
`481bf63764ae61c9cb3f1b46e1d3f7fff4775570`: all ten steps and eleven dedicated
Mac CTests pass. The ad-hoc signed app is
`build/mac-candidate-002/mac/Sega Rally Native.app`; executable SHA-256
`4d365a25d628cf26dbfa713598cc6bc762ca1d39cd7d46845269cf37507b53e8`.
Independent audit checks source/compile contracts, all 52 generated main files,
TGP/timed sound against v17, all eight images, seven notices, twenty bundle
files and 26 matching raw/signed Mach-O sections. Evidence:
`build/validation/mac-release-audit/fresh-candidate-002-review.json`, SHA-256
`b5870281c64f275b448398e2860023ee7a6321927d8a768f37e1d54657148c9a`.

A bounded direct invocation of the signed executable with no arguments and an
isolated save environment verifies the new Mac defaults: Metal, native sound,
instruction-boundary execution and enhancements off. It closes normally after
25 seconds at 1,293 frames, 254,204,037 instructions and 561,978,800 cycles,
producing 991,331 stereo PCM frames. This exercises direct executable startup,
not a Finder event-delivery test. One estimated SDL input shortfall is retained;
startup success is not an audio-quality pass. Result:
`build/validation/mac-candidate-002-host/startup-001/result.json`, SHA-256
`5275cf9c9a12b054b124714774b1f4efae24aff31fd6ef66cf550f21910e6ca5`.

The neutral paired capture then reaches 7,200 frames / 1,399,415,787 main
instructions / 3,129,161,000 cycles / PC `005ca464`, exit zero. The whole native
WAV is byte-identical to the preserved neutral reference: 5,519,841 stereo
frames, SHA-256 `c2dff02d3df9b30228802404c1e7dfc59547d549f806b6fb7a09862a5355bbdc`.
It nevertheless FAILS the planned zero-demand playback criterion: one request
estimates 2,296 additional input bytes. The failed result remains unchanged at
`build/validation/mac-candidate-002-host/audio-001/result.json`, SHA-256
`c60702abff489a0d1dd96a3ad9fedffd7eac8dfc9e45f8b6676a4574041103df`.

Independent artifact review passes all 14,404 neutral resumes, witness
continuity, every PCM interval hash, recording seal, initial/final saves and
full transcript/observation equality with the preserved neutral run. Postmix
contains 5,512,192 finite stereo frames in 5,383 contiguous packets, no capture
drops/format failure, and records 2,065 queued input frames on detach. This is
the app's logical SDL mix before CoreAudio, not physical speaker capture.
Integrity report: `build/validation/mac-release-audit/candidate002-audio-review.json`,
SHA-256 `c74860c5f022d77aa2507dd35254a313f02861549f2740b3852952c75dc3d3c8`.
The first review tried equating board nanoseconds with CPU cycles times forty
at every resume; that is invalid during the established synchronization phase.
The corrected review checks exact witness continuity and reference transcript
equality instead. No production code or captured evidence changed.

The paired stream exposes an actual 288-frame / 6.530612 ms zero tail absent
from native PCM. A 52.918334 ms producer pause exhausts the stream during a
1,024-frame output request. Native stepping and GPU presentation measurements
do not identify the source of that pause; oversleep, scheduling and unmeasured
host I/O remain distinct possibilities. Offline SDL schedule reproduction and
a device-sized buffer remedy are being validated. Do not report the audible
issue resolved merely because native PCM or capture integrity matches.

The separate championship gate proceeds under a new frozen plan that binds
the original audio failure and independent integrity review. It runs without
device playback and does not relax or relabel the failed audio criterion.
All 4,398 input events apply through 20,000 frames / 3,662,512,354 instructions /
8,692,041,001 cycles / PC `005ca464`, with 15,332,761 stereo PCM frames, no
chip/host drops, frontier or manual takeover. Terminal counters and all 670
CPU-only course checkpoints match the valid-input Desert/Forest/Mountain path.
That path clears Desert in 1:00.38 and Forest in 1:21.37, then retires near
Mountain's finish; neither Mountain completion nor Lakeside is established.
Result: `build/validation/mac-candidate-002-host/championship-001/result.json`,
SHA-256 `cf06426f683f75466388e94fca2d90fe1c00a7efc8b05f348b306b60cfa19185`.
It took 177.20 seconds with other work active; this is not a speed measurement.
Independent integrity review verifies all 40,006 resumes, the full twelve-pin
input schedule, every PCM interval hash and final saves. One bounded offline
replay with the exact signed executable then passes all of those comparisons
in 168.850059 seconds, with the app, recording and live saves unchanged. Result:
`build/validation/mac-release-audit/championship002-offline-001/result.json`,
SHA-256 `9b89fd17a7185d1cb2e2e39301cb0db4fa18427e8c3d8d11bd41113f2f0b5024`.
This remains same-Machine reproducibility, not independent CPU/device parity.
The final default-OFF Metal image differs from software composition, as expected
for that renderer mode; this gate did not assert optional accurate-mode equality.

Test runner evolution is explicit: original startup script/plan, neutral-capture
script/plan and failed result remain preserved; plan-v3 binds them before the
independent championship run. The neutral capture uses the preserved frame-0 /
7,199 input schedule to exclude live controls. No game code, physics or saves
were modified to extend course coverage. README/player/status now describe the
current build workflow and keep earlier checkpoints in the evidence record.

## Bounded output-audio diagnostics and longer input replays (2026-10-15)

Offline analysis of the preserved 125.166-second neutral-title capture found
no nonfinite/full-scale samples or increased discontinuity at drain boundaries.
Its two later silence gaps decay toward zero; the largest transients recur at
the same musical position across repeated music. This does not resolve the
reported audible issue. SCSP sample/loop/mix source matches the pinned reference
apart from the documented RNG provider; playback counters do not observe all
downstream CoreAudio buffering. Evidence:
`build/worktrees/audio-title-diagnosis/build/validation/title-signal-001/report.json`,
SHA-256 `221118d349648c0315b9f7aab63d8dba43196a1b235053c6bb48f508afd7b725`.

Audio and Reference & validation govern a new optional diagnostic:
`--audio-output-capture FILE.wav`, requiring paced sound, audio diagnostics and
a distinct native capture. It records only this application's SDL logical-device
float mix before the physical backend. A preallocated single-producer/consumer
ring keeps file I/O and allocation off the audio callback; the main thread
drains WAV data and packet/producer metadata. Overflow and format changes fail
explicitly. Callback removal synchronizes shutdown; capture records the queued
tail rather than waiting or changing playback. Native sample generation, normal
buffering and default 44.1 kHz stereo WAV bytes are unchanged.

The new fixture passes 24,389 checks covering ring wrap/limits, format errors,
concurrent draining, two isolated SDL dummy logical devices and detach behavior.
The original audio fixture and strict host syntax also pass. The first compile
attempt used an incompatible enum initializer; that failure is preserved before
the typed correction. Frozen source/tests:
`build/worktrees/audio-output-capture/build/validation/postmix-001/`;
patch SHA-256 `50f5fcafbbcb0b747eff9014dd719f38133120ce54743ad61ee4b01c8d4ea823`.
Independent review reconciles all nine retained WAV packet/footer records:
`build/validation/audio-postmix-independent-review/result.json`, SHA-256
`148690ff9e72d740677473cfbaeb51dae9c85e1da797f507ba70abdeeb62aadd`.
Actual physical-device paired capture is still next. Integration retains the
separate no-argument Mac startup fix; overlapping help text was merged explicitly.

The valid-input course experiment now clears Forest and drives Mountain, but
the best bounded path still retires before Mountain finishes. Its exact frame
input transcript has 4,398 events, exceeding the previous 4,096 parser bound.
The bound is now 8,192, retaining exact bytes, strict frame ordering, frame and
line limits, and the launcher's 1 MiB file limit. All 42 parser checks pass
normally and under ASan/UBSan, including accepting/applying 8,192 and rejecting
8,193 events. Evidence:
`build/worktrees/replay-event-limit/build/event-limit-review/`, patch SHA-256
`66f65bf04d8250fa3999cfb49a2b42c34f92d94aac8997aed92e212d67fa63fe`.
The Mac pipeline now builds both new diagnostic and input test targets before
CTest. The player manual uses the current build/launch workflow and keeps old
checkpoint details in the handoff rather than instructing users to repackage
preserved recording apps. Fresh candidate002 build/device gates follow.

## Fresh Mac build verified; direct startup defaults corrected (2026-10-15)

The first fresh candidate is generated from the supplied archive at commit
`d79ec86`, built Release with sound/hook/recording, and passes all nine dedicated
Mac CTests in 1.56 seconds. The one-command pipeline then correctly stops when
the sandbox denies `pgrep` access (exit 3, unavailable sysmond). The original
failure report/log remain unchanged. Retrying the exact package-only command
with OS access succeeds, followed by strict deep signature verification.
No game launch occurred. Evidence: `build/mac-candidate-001/`;
`package-recovery-001.json` SHA-256
`291f1d009ceab416e4bc0c8ada37bfa3e5daf74ac1f3df1b6b12c09f92284263`.

Independent audit verifies all 20 bundle files, eight original images and seven
notice files. All 52 freshly generated main C++ files and complete reachability,
TGP source and timed sound source match v17 exactly. The boundary contract is
valid; all 26 file-backed Mach-O sections match before/after signing. Report:
`build/validation/mac-release-audit/fresh-candidate-001-review.json`, SHA-256
`aeea2788271302058b24bcfd4b6230a33d8169440b059cc28ebda42a4ca8cc7a`.
This establishes fresh build/package reproduction, not new gameplay validation.

The packaging audit also found a real player-facing default: Finder invokes
the native executable without the launcher's arguments, so the previous app
started with sound and the faster instruction-boundary path disabled. Mac
startup with no arguments now enables those two capabilities when compiled in.
Explicit diagnostic commands and offline recording replay retain their existing
defaults; display preferences/effects and iOS are unchanged. The exact host
source passes strict syntax checks in ordinary, sound/hook/recording, and
hook-only configurations. Actual no-argument startup still needs a bounded
device check in the next build. This follows Platform layer & build; no game
instructions, clocks, audio generation or shader behavior changed.

## v17 manual replay passes; Mac candidate pipeline prepared (2026-10-15)

The latest v17 session closed normally at 4,729 frames, 890,065,301 main
instructions and 2,055,264,421 cycles, PC `005d662c`. Its 3,625,487 stereo PCM
frames have zero logged chip/host drops or SDL shortfall requests. One bounded
offline replay using the original signed executable passes all 9,459 resumes,
PCM hashes and final saves in 38.943 seconds. It opens no output devices and
preserves the app, recording and live saves. The captured input contains 350
post-handoff analog transitions, including 112 within a render frame; this
demonstrates manual input, not course completion or its physical source.

Evidence: `build/validation/mac-reciprocal-v17-playtest-001/`;
`offline-replay-001/result.json` SHA-256
`d2bdb9e1cd13a2a58e2409674b0f76ec3b4cf01178213afe361c1df33cf83c5c`.
Independent file/seal/output audit:
`build/validation/mac-release-audit/v17-replay-review.json`, SHA-256
`6217eb5b5aea3e0f75b7e7df32adc674a1cb72ccff8d4ee2edbababd83bc44c3`.
Scope remains same-Machine reproducibility, without a listening or full-course
verdict. The former live-session hold on heavy tests is lifted.

Platform layer & build and Reference & validation govern the new Mac packaging
work. The launcher now copies only audited image filenames and replaces its
owned asset directory, preventing old sound images or unrelated build files
from entering a repackaged app. Staging removes only its known optional sound
outputs when sound is disabled. Source asset symlinks are rejected. Packaging
requires retained project/dependency licenses and original source notices;
the collector copies existing text, including selected recording sources'
public-domain declarations, without generated game bodies. Dedicated Mac CTest
now includes launcher, staging, notice and pipeline regressions.

The isolated packaging patch passes 61 Python tests plus five configured
CTest entries using synthetic native inputs. Actual pinned dependencies yield
seven notice files and 80 source preambles. Patch evidence is under
`build/worktrees/mac-release-audit/build/release-review/`; patch SHA-256
`ee5d5c3269b93934864d54cdd8f82f57f6e186522077b0cc325dd8054fa9170b`.
No existing game app or runtime source was changed by these checks.

`scripts/segarally_mac.py` prepares a fresh sound/hook/recording-enabled Metal
candidate from a personal archive, builds the host tests, packages without
launching, and verifies the signature. It requires existing pinned dependencies,
rejects an existing output, records each command/failure and terminates a failed
build's process group. A discovered sound frontier cannot pass its startup gate.
Eight synthetic pipeline tests pass. The first test attempt compared macOS's
unresolved `/var` temporary path with canonical `/private/var`; normalizing the
fixture root fixes that assertion without changing production path behavior.
Actual end-to-end fresh build and package validation remain next; no release
milestone is claimed. The supplied archive is now at the neighboring prototype's
`ROMS/srallyc.zip`, rather than its old Downloads path.

## v17 Metal host checks pass; windowed playtest launched (2026-10-15)

Execution permission was granted. The frozen host002 OFF and ON gates each
complete 2,600 frames, 500,035,460 instructions and 1,130,001,001 cycles at
PC `0x005e6964`, with all 15 expected applied input boundaries. Both preserve
v16's complete WAV (1,993,322 stereo frames), initial/final saves, all 5,201
resume records and their observation payloads. No native frontier, chip drops
or host drops. OFF's whole final RGB image matches v16; ON's whole 496×384
image matches the same-runtime software renderer, with supplementary BGRA FNV
`c0d8039becbf87ce` on both sides. These captures precede CRT/UI; neither run
establishes all-frame graphics, independent CPU, physical speaker or speed
parity. Test process times were 24.692002 / 102.467292 seconds, not benchmarks.

Independent review rehashes all 4,966 frozen inputs and 18 OFF / 19 ON evidence
files, verifies recording seals and recomputes every per-resume PCM hash. No
retries or test-source changes were needed. Both original cancellation records
remain preserved. Platform layer & build and Reference & validation govern
the checks and their stated limits.

Evidence under `build/validation/mac-reciprocal-v17-host-002/`:
OFF result `1015c5be31cb5cc0b4226d85fa70887539180bd1581fc271141d9e8945bd5220`;
ON result `b34ed21d424d2cbf298339ea3cd085a7c618bbf63fbe81c72d870863f0178516`;
independent OFF review `a5265a64ee9367989e675246a32b8d1a455543ff1b4fa18e138be5af1c7d6701`;
independent ON review `d46b2246e62854b27a724471a59d1d99ac7faa7e1b2cb27337c4470b0a6300cf`.

Reverified all 15 signed candidate bundle files and strict deep codesign before
LaunchServices opened the separate v17 app. Signed executable remains
`d7b234ea972117a4e2408f085607f1a203255006e170fc5387dca99f2dd2f538`.
Windowed, normal-paced Metal/audio startup is observed with CRT and Accurate
textures enabled, native sound/hook, bundled Lancia selection replay and
fresh isolated saves/input recording/audio timing diagnostics. PID 6223 was
observed progressing; the frozen startup snapshot reaches frame 4,200 without
frontier or exit. Startup reports no connected controller; keyboard is available.
Heavy tests/builds are held during the user's playtest.

Live evidence: `build/validation/mac-reciprocal-v17-playtest-001/`.
Launch plan SHA-256 `d9cbcbb76208aab95994ed798aaab7c17f7427ea9c6709764fcda892fc9576ea`;
`launch-result.json` records the observed process and immutable log prefix.
The live session remains open, so no final seal/replay, listening verdict or
full-course claim is made. Earlier packages and saves are untouched. Docs now
reflect the passed host checks and actual launch. Next collect this session
after normal close, replay its recording, and use actual feedback to localize
title-screen audio, controller/menu behavior and continuous texture flicker.

## v17 device check still awaits execution permission (2026-10-14)

Prepared a separately frozen host runner that records `BaseException` and
always stops/reaps a live child process group in `finally`. The original runner
could skip cleanup on interruption. The new runner retains the same v17 binary,
2,600-frame / 600-million-instruction / 180-second limits, payload/image
comparisons and OFF-before-ON order, with new output directories. Independent
source review found no blocker; it has not executed a game.

The single new OFF submission remained at execution approval and was cancelled
after 108.1 seconds. No session/PID/output was returned, neither gameplay output
directory exists, and a subsequent process listing found no game, host runner
or sound/GPU fixture process. ON was never submitted; no automatic retry.
This was an unapproved/cancelled request, not an executed test failure or an
automatic approval rejection. The packaged v17 candidate remains unlaunched.

Evidence: `build/validation/mac-reciprocal-v17-host-002/`.
Runner SHA-256 `982a7c7003b32b1494dec1ac1d56d9a5a12302665c2e5b35244da70aadd6b283`;
plan `c8e451ab3f6a29bb44dfc7c15ac604aed6630a8abf93848a317f39c423ba3eea`;
`cancelled-submission-001.json`
`c48c795b487cea469dbb5ab98a690ce1bbfe08e14f45fbb44ab92bbc6847e76a`.
Original and revised runner/plan hashes remain unchanged. The next device gate
requires macOS execution permission; completed sound and launcher work below
is committed independently of this permission boundary.

## Independent sound comparison reaches nonzero PCM (2026-10-14)

The corrected reference observer completes the authentic UART transcript:
14,947,325 sound instructions, 186,594,177 cycles, 14,614 IRQ entries and
32,159,463 paired observations through 16,527,970,040 ns. All 55 actual UART
accesses and 1,929 original state cuts remain unchanged. It compares 8,124,293
ordered modeled-data transfers and independently checks 28,745,755 reference
instruction-stream reads. Registers, full SR, stacks, RAM, queues, UART and RNG
agree. This is independent MAME CPU execution using shared device models and
instruction-start MMIO, as required by the Audio and Reference sections; it is
not independent chip, physical bus-phase or full MAME-machine validation.

All 728,884 stereo PCM frames match bit for bit, including 22,784 nonzero scalar
samples. First nonzero frame is 717,489, raw bits `b8000000`; its derived sample
deadline is 16,269,591,837 ns and its first observed drain is 16,279,520,560 ns.
Peak is 0.04022979736328125 and RMS including startup silence is
0.0013018575635227812. The complete native payload also matches the previously
preserved neutral host WAV prefix. This does not identify a title-scene cue or
resolve the user's audible glitch. Executed coverage is 1,413 ROM PCs / 470
opcode words, distinct from the admitted 2,663 / 660 inventory.

Integrated `tests/test_segarally_sound_uart_nonzero_reference.cpp/.py` with the
exact passing source bytes. They consume archived prerequisite manifests,
objects, tape and observer proof; a clean checkout alone cannot recreate these
retained artifacts. Only the public reference provider is regenerated, with
default full-660 output byte-identical to the original and stripped tagged
output reconstructing that same CPU code. Native game/runtime code is unchanged.
Actual compiler includes bind the corrected provider and explicit SDK metadata.

The first new preparation compiled but rejected an unbound Xcode
`SDKSettings.json` dependency. The separate repaired attempt binds it before
and after compilation and requires it in the actual dependency list; no include
checks were waived. Preparation compiled in 11.720 seconds; the single guest
run passed in 310.080869 seconds. `nice` reported denied priority adjustment;
no lowered-priority or gameplay-performance claim. All original semantic,
diagnostic, compiler and SDK-metadata failures remain preserved.

Evidence under `build/worktrees/sound-nonzero-purpose-reference/build/`:
`sdk-bound-source-001/build/purpose-reference-prepared-002/prepared.json`
SHA-256 `9d788b870c34020ce94f809edf2e7f54f2920edb6aca09cece8fd50208712154`;
`sdk-bound-source-001/build/purpose-reference-run-002/result.json`
SHA-256 `b355e443c3e66a1cd5c2eb8f1a14d34b3719d86189fd0c61776b5a92dd7e8857`;
state `e7b5d7e5a2769bef52c3c523df0c5309a3042e70723d219fb26b1f6c2813fd6f`.
All 1,006 inputs and 16 artifacts remained unchanged. Independent audit
`build/validation/sound-purpose-nonzero-independent-review/result.json`
SHA-256 `5722fee4fc32e1b1c262d44f008c1061f59db22f87b70aec9000d8d4cdcf15e2`
recomputes original PCM/cut hashes and metrics, verifies final outputs, and
retains distinct native/reference phase ledgers. No second reference PCM file
was saved; equality is asserted per cut by the frozen comparator.

Current docs now distinguish v16 interactive evidence from the unlaunched v17
candidate and remove obsolete listening/graphics claims. Next: v17 OFF/ON host
checks, reproduce/localize the title audio report, controller menu feedback,
continuous flicker/cost and remaining full-course/independent/hardware checks.

## Mac packaging can stop before launch (2026-10-14)

The tracked Mac launcher now supports `--no-launch`: verify assets, package and
sign, then write the future argument list to `launch-command.json` with
`executed: false`. It refuses an active app and `--record-inputs`, creates no
live saves/log/recording, and never calls LaunchServices in this mode. Normal
launch and existing-app activation remain supported. A failed `pgrep` query now
stops packaging instead of being mistaken for "no running app", preventing an
unchecked overwrite of the package. Platform layer & build governs this change.

Sixteen launcher tests and four recording-launcher tests pass with subprocesses
mocked, covering failed process queries, unchanged existing package bytes,
no-launch arguments, replay bundling before signing and normal activation.
The launcher fixture is now registered with root CTest. This is CPU-only test
evidence, not a fresh codesign, device launch or gameplay check. The existing
v16/v17 packages, saves and frozen gameplay evidence were not modified.

Corrected the standalone-host README's obsolete claim that ImGui is optional,
and moved the default-on recording dependency's pinned 7-Zip setup into the
build guide's dependency section. Source/test evidence is
`build/worktrees/mac-launcher-contract/build/validation/launcher-contract-001`;
patch SHA-256 `9bd9d1a479ddbc282ca47083d69692b6a6ae58349c8d7393f2ea21bd30ef8985`.
The integrated launcher and test bytes match the tested sources. Sound observer
validation is complete at the synthetic level; the longer authentic comparison
is next. The v17 gameplay/device gates and reported title-screen glitch remain
open.

## Sound observer distinguishes PC-relative operands (2026-10-14)

Added opt-in `--board-access-roles` to the test-only M68000 provider and a
ROM-free regression. Program function-code space includes PC-relative data
operands; treating every such access as instruction prefetch caused the longer
authentic comparison to reject instruction `0x337b` at `0x600d52`. Purpose now
comes from the pinned microcode function-code selection and is propagated
through its control flow to bus completion. Ambiguous or unclassified cycles
fail generation. Addresses, expected native counts and IRC-latch heuristics do
not classify accesses. Production runtime and generated game code are unchanged.
This follows the design's Audio and Reference & validation sections.

The first ROM-free attempt generated all headers successfully but failed C++
compilation: one combined `auto` declaration mixed distinct plain/tagged nested
Access types. The separately preserved repair splits the declarations. The next
bounded attempt passes 486 checks in 1.109561 seconds across nine public opcode
cases plus reset/IACK, with empty stderr. It verifies both internal observations
and the mapped callback, full registers/microstate/RAM, addresses, values, masks
and phases. Old/default headers are byte-identical; stripping only annotation
assignments/scaffold reconstructs the same generated CPU implementation.

Evidence: `build/worktrees/sound-observer-reference/build/`;
failed `observer-purpose-fixture-001` and original sources remain unchanged.
Passing `observer-purpose-repaired-source-001/build/observer-purpose-fixture-002/result.json`
SHA-256 `9beb63e92096cee42aa8e0ea3617b6579a91f4670d2c4c475e5ef4d60ffabff7`.
Independent audit `build/validation/sound-observer-independent-review/result.json`
SHA-256 `4323f60f979eeb7956ac74c1ea516506be710a310bf6c40255c8f596c033338b`
rechecks all nine inputs and 22 artifacts, both byte-equality proofs and the
unchanged original source closure. The four integrated source files match the
passing fixture. No actual tape rerun is claimed yet. Next generate a separate
corrected full provider and repeat the bounded authentic comparison, preserving
the original failed trace; title-screen audio quality remains unresolved.

## Reusable reciprocal regression added (2026-10-14)

Added `tests/test_gpu_reciprocal.cpp/.hlsl/.py`, a manual ROM-free Metal
regression importing the production reciprocal helper. It checks 65,536 public
input/seed rows using an independent integer quotient/remainder rounding oracle,
a separate legacy-only shader, input/tag echoes and two raw integer readbacks.
The rendering and reference-validation design sections govern: one common HLSL
path, no scene data in Git, and no device claim from source checks.

The explicit prepare/run split pins SDL from `scripts/setup.py`, accepts only
pristine SDL or the documented exact Vulkan application-name patch, records
source/tool identities, rejects reused output directories, clears inherited
SDL/game overrides and bounds subprocess groups. No project CMake or production
runtime changes. The build guide includes the manual commands and limitations.

Independent source review, strict C++20 syntax and Python syntax checks pass.
Full preparation and device execution remain unvalidated: the first preparation
stopped at Docker socket access; a pending escalated submission was cancelled
without starting. No GPU process or readback exists for this reusable fixture.
Earlier accepted production arithmetic/scene results remain separate.

Frozen source review: `build/worktrees/reciprocal-regression/build/source-review-004`;
plan SHA-256 `d5930ad094072e6785fa9db26698fd42f9f5ef1238e5f153dbd8958cc6fc392d`,
patch `fc20562523e0504c48e06515f6dc5a2896fdd7549c8941f60582ffd074311339`.
All three integrated files match their frozen hashes. Earlier preparation
attempts and cancellations remain recorded there. Do not infer a successful
device check from the fixture's presence. Next priority is completing the sound
observer correction and authentic nonzero-PCM reference comparison; v17 host
checks and full-course/hardware validation remain open.

## v17 candidate packaged; sound observer mismatch localized (2026-10-14)

A separate v17 Release host at `82c06af` builds and passes all three platform
CTests. Its 203 generation files remain identical to v16; only the three
integrated reciprocal shader files differ among production sources. The
candidate is packaged at
`build/worktrees/mac-reciprocal-v17/build/mac-v17/Sega Rally Native v17 Candidate.app`
with a separate bundle identifier and empty saves. Ad-hoc signature verification
passes. It has not launched: the submitted OFF host check was cancelled after
waiting for execution approval, and ON was never submitted. There are no
gameplay outputs or remaining host processes. Packaging does not establish
v17 gameplay, speaker output, temporal flicker or performance.

The existing launcher is unchanged. An ignored mechanical packaging copy
requires no-launch mode and removes activation/launch calls. Its recorded
future command enables sound, the instruction hook, CRT, accurate textures and
the selection-only Lancia replay; compiled defaults remain unchanged. All
4,966 frozen host-plan inputs and the preserved v16 app/saves remain exact.
Package manifest:
`build/validation/mac-reciprocal-v17/package-001/result.json`, SHA-256
`751f3ab20d475911807ea98e8a9b3f8ac8006e106310afc44f8c85135756972c`.
Independent file audit: `package-001/independent-review.json`, SHA-256
`5a5ac9535fb882cd616519dd63d38e6fe4ec1e0945f329c0a87a42329e861daf`.
The existing bounded OFF/ON host plan remains the next device validation.

The separate sound diagnostic preserves the failed comparison and pinpoints
its first count mismatch at instruction 14,803,284, previous PC `600d52`,
next PC `600d58`, cycle 183,673,204, board time 16,269,237,263 ns. Native
execution reads a PC-relative operand at `600d9a` and writes it to `00280e`.
The independent CPU performs both operations with the same values and reaches
the same architecture/RAM state, but the test labels every opcode-space read
as prefetch and excludes the operand. The reference port denotes program
function-code space; it does not by itself identify instruction fetching.
The preserved diagnostic has 1,905 completed cuts and no successful terminal
state. All 972 input identities and four prepared artifacts remain unchanged.

This is an observer-classification finding, not evidence for changing the
production sound CPU or a title-screen audio fix. Pinned microcode review also
rejects using the IRC latch alone: some immediate/address extensions enter
DBIN without updating IRC. Next distinguish access purpose from the pinned
bus-start controls, test PC-relative and instruction-extension cases, then
repeat the unchanged longer comparison using a separately frozen provider.
The original failed sources and evidence remain preserved. This follows
**Reference & validation**, **Audio**, and **Platform layer & build**.

## Longer sound reference stops at a transfer-count mismatch (2026-10-14)

The first independent CPU run against the nonzero transcript fails after
305.693664 seconds at `per-event modeled data transfer count`. The last
flushed checkpoint has 31,000,000 paired observations, 14,454,514 sound
instructions, 179,376,509 cycles, PC `602a20`, 1,813 cuts and board time
15,888,649,643 ns. This is a lower bound, not the precise failing instruction
or time. Full CPU registers/SR, RAM and queue/UART/RNG/sample progression are
compared before the failing count assertion. The fixture does not print the
counts or offending rows there, so neither the exact opcode nor a runtime
bug can yet be attributed. No success state is written, no assertion is
relaxed, and no runtime or generated code changes are justified by this alone.

Preparation passes in 11.107108 seconds. All 965 measured inputs and four
prepared artifacts remain unchanged after failure. The OS rejects the requested
nice priority; the reference executes, but lower scheduling priority is not
established. The preserved first failure has no retry. Evidence under
`build/worktrees/sound-init-reference/build/`:
`uart-nonzero-reference-run-001/result.json`, SHA-256
`6bb5267eaca27014e543cc59ab03a026c278e5b3eca66a32be3ecb99406bb9fe`;
`uart-nonzero-reference-outcome-001/summary.json`, SHA-256
`f41e30e07c187d0acd043148b6f125840cb2d57e47dcee6fcf93535ccec80b21`.
The separately passing native nonzero capture/recapture remains valid. A new
read-only comparison also finds its entire 5,831,072-byte PCM payload identical
to the corresponding preserved neutral v15 host WAV prefix, including the
nonzero tail. Report:
`build/validation/sound-uart-nonzero-integration/neutral-host-PCM-prefix.json`,
SHA-256 `a85e2c3208a20c8f26c2ee2726193ccb16e192bd5bb7ac5d63de50ed137cb8d2`.

Next use a separately frozen diagnostic fixture to print the first differing
transfer counts, CPU boundary and ordered rows before the unchanged assertion.
Preserve the original sources and failed evidence; do not infer an audio fix,
completed nonzero independent comparison or title-scene identity. This follows
**Audio** and **Reference & validation**.

## Authentic sound capture reaches nonzero PCM (2026-10-14)

The separate first-nonzero fixture captures 55 actual UART accesses and 1,929
complete sound cuts, then reproduces every cut in a fresh native sound board.
The old seven-access, 163-cut prefix remains byte-exact. Capture stops after
186,212,000 main instructions at 16,527,970,040 ns; sound has executed
14,947,325 instructions, 186,594,177 cycles and 14,614 IRQ entries. Physical
UART transmission is empty at both the old prefix boundary and the final cut.
All 25 observed MIDI consumption/IRQ-clear events are actual program accesses.

The raw payload contains 728,884 stereo frames, including 22,784 nonzero scalar
samples. The first nonzero scalar is index 1,434,978, stereo frame 717,489,
bits `b8000000`, matching the previously retained neutral WAV. Its sample
deadline, derived from the verified sample scheduler, is 16,269,591,837 ns;
the first drain that observes it occurs at 16,279,520,560 ns. These timestamps
are distinct. Peak is 0.0402297974 and RMS is 0.00130185756 over the complete
payload, including startup silence. Every interval PCM hash is independently
recomputed from the raw bytes. This follows **Audio** and **Reference &
validation**; it establishes native reproducibility through nonzero output,
not independent CPU/chip fidelity, title-music identity or a glitch fix.

The first capture and conditional recapture pass in 9.703119 and 4.455683
seconds with no source fixes or retries. Bounds remain 250 million main
instructions, 20 seconds of board time and 180 seconds per execution, stopping
at a normal budget boundary after the first nonzero sample plus 250 ms.
Evidence under `build/worktrees/sound-init-reference/build/`:
`uart-nonzero-capture-001/result.json`, SHA-256
`6de64ac568dfeae917fa16209ae8176359dfc6928cee1a7e3f55e30e28bae719`;
`uart-nonzero-recapture-001/result.json`, SHA-256
`ef006ba586fe75f71df379413e3b7c4b4fef2fefd168bc1615713c5149f5e651`.
Raw PCM SHA-256:
`b7126acd4336b0ee036696f9c1ccd8d7171720affb8b76910e9fad2e94c5619e`.
Independent review verifies all 930 inputs, six prepared artifacts, all cuts,
metrics and output identities:
`build/validation/sound-uart-nonzero-independent-review/result.json`, SHA-256
`c5b960a8bc7cd3ad303e01bed7435bcfc8a0b78fa0d649ac9537b84b5c3ba018`.
All 113 local source inputs map exactly to MAIN; integration record
`build/validation/sound-uart-nonzero-integration/result.json`, SHA-256
`b9e45435c3455f64dee54382848bc70b1b7616a61ac24651dea9fce536b86786`.

Next compare this immutable nonzero transcript with the independent sound CPU
under separately reviewed longer bounds. The title-screen audio symptom and
independent sound-enabled recording consumer remain open. The separate v17
Mac Release build and three CTests pass; gameplay OFF/ON checks are prepared.

## Independent sound CPU matches authentic UART prefix (2026-10-14)

The first real UART transcript now passes persistent pinned-MAME sound CPU
execution as well as native recapture. The single bounded run ends at
1,444,941,000 ns with 1,260,947 sound instructions, 16,312,806 cycles and 660
IRQ entries. All 2,761,745 paired observations, 925,827 ordered modeled data
transfers and 163 original native cuts match. The 63,722 stereo PCM frames
also match bit for bit and remain silent. MAME independently reads the
observed `ff` byte at 1,443,240,416 ns. Its microcode-phase stamp is 16,293,612
cycles; the native boundary stamp is 16,293,607. Both are retained separately.

The original transcript contains native phase-bearing interval hashes, so
the test keeps a fresh native recapture matching those original cuts exactly,
then compares independent CPU state, full SR/stack/RAM, queue/RNG/UART state,
ordered modeled data and raw PCM against that recapture. It neither rewrites
the tape nor normalizes reference phases. Native guards remain separate from
bus transfers; reference prefetch words and CPU-space IACK are checked
independently. This follows **Audio** and **Reference & validation**. Shared
devices and instruction-start MMIO remain explicit limits; independent chip,
physical timing, title-music and sound-on recording claims remain open.

Compile-only preparation passes in 11.276108 seconds without fixes. The first
guest run passes in 28.065956 seconds with empty stderr and unchanged inputs;
this is correctness elapsed time, not a performance comparison. Evidence:
`build/worktrees/sound-init-reference/build/uart-reference-run-001/result.json`,
SHA-256 `1eca628de6f1e8a3d1b60a1ae49dfdcfcf5bf6f32c5011a97b256160893569aa`;
`state.json` SHA-256
`85ef72a8dfc118d948aba7dd8bc567de60b950e9cd5f21f977f08e53536f7968`.
Independent review checks all 959 measured inputs, four prepared artifacts,
the result ledger and all silent PCM hashes:
`build/validation/sound-uart-reference-independent-review/result.json`, SHA-256
`5fb266e063151dcbe0207eb2d52a2b82f0c9335a09f3a03a10f598f239f47376`.
All 113 local source inputs map exactly to MAIN; other inputs retain their
original external/build identities. Integration record:
`build/validation/sound-uart-reference-integration/result.json`, SHA-256
`bee409b78eecc08f121191750fc285a36834c0fb1593183661b272c3253f200d`.

Next extend authentic main traffic to first nonzero PCM, require native
recapture, then extend the independent CPU comparison. A read of the preserved
neutral 7,200-frame WAV locates its first nonzero at stereo frame 717,489,
16.269591837 seconds at 44.1 kHz, raw first scalar `b8000000`. This supplies a
measured discovery target, not a title-scene or music label. The new fixture
must preserve the completed short prefix and use new explicit bounds. The
separate v17 Mac worktree at `82c06af` is building the integrated shader;
no new app playability or paced-render result is claimed yet.

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
