# Private driving observations

This host-only observer follows the governing design's **Reference & validation**
and **Platform layer & build** contracts. It records actual execution in the
separate translated reference. It is not linked into either native PS2 target,
does not implement a driving system and cannot establish physical-console or
arcade arithmetic fidelity.

The opt-in `RALLY_OFFLINE_DRIVING_CAPTURE` hook runs after synchronization and
interrupt decisions at the existing pre-instruction boundary. Normal player
builds leave it disabled; EE builds reject the macro. The observer and its host
executable remain outside every native source role.

```sh
python3 platform/ps2/tools/capture_driving.py \
  --output build/ps2/driving-capture-new --jobs 4
```

The defaults use the owner's audited Revision B generation in `build/ps2/native`,
the verified paged asset pack and the tracked nineteen-event Lancia cabinet
input replay. The tool creates a new host executable, executes 2,850 rendered
frames with observation enabled, then repeats them with observation disabled.
It compares all seven checkpoint fields at every frame: frame number, completed
instructions, reference cycles, modeled board time, PC, register hash and
framebuffer hash. This is a same-reference observation check, not independent
processor validation or a comparison of all RAM bytes.

`validation.json` receives `status: passed` only after both processes succeed,
the checkpoints match, the capture completion marker exists, and the source,
executable, pack and input identities remain unchanged. It binds the executable,
all capture files, generated-source provenance and imported region identities by
hash. A failed or incomplete report cannot approve live arithmetic evidence.
The output directory must be new and beneath this checkout's real ignored
`build/` directory. Never export captured data, private images or generated code.

## Boundaries and records

Records preserve actual guest PCs. The observer recognizes the inspected
original-ROM locations and their copy relocated by `0x59f000`; accepting either
location does not establish that both executed. The outer integrator entries
are ROM `0x2cce0` and `0x2e650`. Each recorded return must match its entry's saved
return PC and caller frame pointer. The entry's raw branch selector, actual
object address and entry PC remain attached to every detailed event.

The return PC comes from the **saved caller frame**, using the same selection as
the reference CPU's `do_ret_0`: a matching register-cache entry when available,
otherwise 16 words in whitelisted ordinary RAM. Each detailed record binds this
as `return_frame` with the address, words, source and cache index. Its word 2 is
the return PC. The child's raw `r2` remains in `registers` for diagnosis; an
interrupt at function entry can overwrite it with the interrupted PC before
the first observed instruction. Treating that register as the actual return
address caused the preserved failed captures 005 and 006. The observer reads
CPU state and RAM only; it does not modify register-cache or runtime behavior.

The ordinary-RAM whitelist is `0x200000..0x23ffff` and
`0x500000..0x5fffff`. The observer never fetches an additional device/FIFO word.
Optional dependency blocks outside these ranges are omitted. Required object,
orientation and contact-buffer identities fail explicitly when invalid.
`object_words` is a **0x400-byte raw RAM window**, not a proven object size;
adjacent objects may share that window.

| Stream or event | Recorded evidence |
| --- | --- |
| `cadence.jsonl` | Every bounded outer entry/return, object, branch selector and instruction/cycle/frame witnesses |
| `selectors.jsonl` | Dispatcher ROM `0x2bfa4`: actual `r5` object and its selector word at `+0xe8` |
| `callbacks.jsonl` | Dispatcher ROM `0x2aba8`: actual `g0` object and `g4` callback, without additional memory reads |
| `steering.jsonl` | Independently paired cabinet steering entry/output, byte input, return identity, AC and integer registers; no inferred object |
| `steering-filter.jsonl` | Full filter entry/output with the input word, previous/current history and output registers |
| `main-contact.jsonl` | Separately sampled main candidate contact passes, including four query/result slots, optional normalization and the cross/metric/accumulation/update path |
| `main-contact-cadence.jsonl` | Every bounded main-pass entry/return, object, frame and completed cross-pair count |
| `main-vehicle.jsonl` | Independently paired main callback, candidate prefix, four slot helpers and the balance helper's parameter-selection/scalar boundaries |
| `main-vehicle-cadence.jsonl` | Every bounded main callback entry/return, object, frame and completed slot-pair count |
| Detailed events | All 32 integer registers, AC, raw bits of four floating registers, object window and named RAM blocks |
| `code-coverage.jsonl` | Nonzero instruction counts for the bounded ROM `0x20000..0x3ffff` analysis region and its relocated copy |

Integrator-entry snapshots additionally include `object_pointer_globals`
(`0x2140c0`, 16 words) and `baseline_object` (`0x500000`, 64 words). These permit
comparison with decoded ownership relationships. An address, branch selector or
per-frame cadence alone does not identify a player-controlled object.

The existing velocity/blend and course-neighborhood records remain available.
Each query at ROM `0x2abc0` is paired with its own saved return PC/frame pointer;
one hardcoded return site would miss nested helpers and alternate callers.

## Alternate orientation

The ordered events are `alternate_orientation_pre` at ROM `0x2ecb4`,
`alternate_orientation_folded` at `0x2ece0`, and
`alternate_orientation_post` at `0x2ee00`. All occur before the instruction at
the named PC. The last boundary follows the final orientation store.

The required `orientation_candidate` block has four words at entry `fp+0x90`.
The first three are the candidate triple, folded in place by original code;
the fourth is an uninterpreted word read by its four-word loads. The current
orientation is in the parent object's words at `+0x38/+0x3c/+0x40`. The observer
checks `r12 == parent object`, `r5 == fp+0x90`, constant frame identity, ordered
boundaries and completion before the parent returns. A checked capture supplies
raw inputs and outputs; numerical equivalence requires a separate native/original
comparison.

## Recorded validation

The preserved capture 004 recorded 1,202 alternate calls and 32 detailed groups;
its [direction blending](../native/direction_blend.md) and
[contact-neighbor selection](../native/contact_neighborhood.md) comparisons
passed. It did not establish player ownership. The primary integrator and its
different orientation tail were absent.

Capture 007 completed all 2,850 enabled/disabled checkpoints with identical
results, using unchanged source, executable and input identities. Its private
report is `build/ps2/driving-capture-007/validation.json`, SHA-256
`046fd97e43f67c7dc7524b2515e9e6d98ca17b6883291bd1097af4952a9249a7`.
The bounded window contains 1,202 complete alternate orientation/contact calls,
34 detailed groups, 32 sampled steering pairs and 32 sampled full-filter pairs.
There are 733 steering-leaf calls and 601 full-filter calls; those counts must
not be assumed to represent the same call population.
The separate native/original consumers subsequently matched all 34
[alternate-orientation groups](../native/alternate_orientation.md), all 32
[steering byte pairs](../native/cabinet_steering.md), and all 32
[full-filter pairs](../native/cabinet_steering_filter.md). Live instruction spans
can include interrupt handlers and must not be reported as the isolated
function's execution cost.

The callback stream records `0x500000 -> 0x5d3f40` 601 times, while each object
`0x500100..0x500700` uses `0x5cacf0` 601 times. All 34 detailed samples of global
`0x2140cc` contain `0x500000`. These are raw ownership witnesses; the separate
caller analysis must establish their meaning. The capture passes observation
parity, not the complete native vehicle implementation.

Capture 008 added the independent main stream and passed the same full replay.
Its cadence located 36 aggregate cross operations in nine main passes at frames
2,571 through 2,579, between the original snapshot checkpoints. It therefore
established execution cadence but recorded no detailed cross operands. Capture
009 explicitly added that bounded observed window to main sampling, preserving
008 unchanged. It passed all 2,850 exact checkpoints and records 601 complete
main passes, 2,404 query/adapter pairs, 26 detailed passes (878 events), and all
36 aggregate cross pairs. All six older detailed/cadence/selector/callback/
steering/filter streams remain byte-identical to capture 007.

The capture 009 private report is `build/ps2/driving-capture-009/validation.json`,
SHA-256 `d704254e2536fb19bca05a971aba7447224a9ba73bb25663a2e239e1ac7afd90`.
The main stream SHA-256 is
`e6845aae16d47a88592e37adad91a02c9d723d565b511c735e09ff13379023dd`.
The combined streams use 7,739,713 bytes, below the unchanged 16 MiB cap.
All 120 source-input hashes and recorded file hashes matched when the run was
sealed. Subsequent build-inventory changes do not rewrite that frozen evidence.
The observation result alone does not prove the contact pass or its arithmetic
has been implemented natively; the separate component consumers make narrower
numerical claims.

Capture 010 adds the numerical boundaries described below and again passes all
2,850 enabled/disabled checkpoints with all nineteen input events. It records
601 completed main passes, 2,404 query/adapter pairs and 26 detailed passes
(994 events). All 36 cross operations have paired metric and accumulation
observations; nine aggregate updates and thirteen normalization pairs are
complete. The six older streams and `main-contact-cadence.jsonl` are
byte-identical to capture 009. The expanded streams use 8,258,956 bytes, still
below the unchanged 16 MiB cap.

The capture 010 report is `build/ps2/driving-capture-010/validation.json`, SHA-256
`3565df75062bf756e90842e0bc5eb24b3ba54acb51c3fb064d75774cdaf16f73`.
Its main stream SHA-256 is
`c9fd5cfa9b1dcf704edad9f1f32288535ddd80849369b7c261272da85ab9ad29`.
All 120 source-input hashes and capture-file hashes matched when the report was
sealed. These counts establish observed execution and sampling, while the
separate native/original consumers establish each bounded numerical comparison.

Capture 011 adds the paired response inputs and the fixed local-point block.
It passes all 2,850 enabled/disabled checkpoints with all nineteen replay events
in 125.712 seconds including compilation. All 120 source-input identities and
capture-file hashes matched at completion. It records 601 complete main passes,
2,404 complete response pairs and 104 detailed response pairs across the same
26 sampled passes (1,098 main events). The six older streams and main cadence
are byte-identical to 010; prior numerical-operation counts are unchanged.
Combined streams occupy 8,719,816 bytes under the same 16 MiB cap.

Its private report is `build/ps2/driving-capture-011/validation.json`, SHA-256
`b2c5d4b0f8655e306bf7f05dfba1ca088e788a00a6efcaf0f9d1e4710e4f5359`.
The main stream SHA-256 is
`a6dc646caf1569f4bb71a5bddfb89d9f6a92cd069a271abba6610de8fbb71600`.
This preserves all four responses per sampled pass, including inactive ones;
the added snapshots alone do not certify a composed native pass.

Capture 012 adds the independent vehicle stream described below. All 2,850
enabled/disabled checkpoints match, including the same nineteen input events,
in 114.485 seconds including compilation. All 120 source-input hashes and all
capture-file identities were verified before releasing the source snapshot.
The eight previous streams and instruction-coverage file remain byte-identical
to 011. There are 601 complete main callbacks, 2,404 complete slot helpers, and
601 complete prefix, parameter-selection and paired-balance observations.
Twenty-six sampled parents produce 468 new records, including 104 slot pairs
and 26 pairs for each scalar region. All streams together occupy 9,863,066 bytes
under the unchanged 16 MiB cap.

The private report is `build/ps2/driving-capture-012/validation.json`, SHA-256
`26fa5c2cd5865204a0489daea81d528456fed276896556b2f5e531fc5d453358`.
The vehicle stream SHA-256 is
`aac6be0db399dc96f820d8bbb5b921c1f7221e30dd4d9d382e7e6c7a379b22e1`.
These records establish actual caller relationships and raw inputs/outputs;
they do not establish a complete native vehicle update or physical-console
fidelity. Separate native/original comparisons make narrower numerical claims.

## Main vehicle callback

The independent parent starts at ROM `0x34f40` and returns to the dispatcher at
`0x2abac`, using the actual saved caller PC and frame. Before accepting that
return, the observer must see the callback's own return instruction at
`0x371f0` with unchanged saved caller words. The saved frame can move between
the CPU's register cache and ordinary RAM without changing its identity. The
raw child `r2` is retained for diagnostics and never substitutes for that proof.

Every sampled parent has eighteen ordered records:

1. `vehicle_entry`.
2. `prefix_pre` at `0x34ff0`, then `prefix_post` at `0x35218` before the
   existing contact call. Both retain global `0x213974` and the candidate
   triples at parent `fp+0x130` and `fp+0x140`, including their old raw words.
3. Four `slot_pre` / `slot_post` pairs around `0x37200`. Each call retains its
   four three-word argument spans, indexed result slot and requested globals.
   Its return is matched to the saved caller among `0x35b88`, `0x35bb4`,
   `0x35bf0` and `0x35c28`. Both inspected internal return instructions are
   supported, and a missing internal return fails completion.
4. `balance_helper_pre` at `0x37870`, `selection_pre` / `selection_post` at
   `0x3789c` / `0x378f4`, `balance_pre` / `balance_post` at
   `0x37900` / `0x379d0`, then `balance_helper_post` at its saved return
   `0x36760`. Selection records flags at `0x214208` and thirteen destination
   words at `0x2141c0`; it does not read the original selection table again.
   Balance records four object words at `+0x64` and the prior word at
   `0x21425c`. The saved `g1` and `g2` vector spans are both **in/out** across
   the full helper; the name `balance_output` does not imply write-only data.
5. `vehicle_return`.

These records use their own `call_id`, `vehicle_frame_pointer`, child
`helper_frame_pointer`, `return_frame` and `parent_return_frame`. They retain
all 32 integer registers, four floating-register bit patterns and AC. Their
`object_words` contains exactly **64 words**, a bounded raw `0x100`-byte span;
the older integrator/contact streams retain their separate 256-word windows.
Both the object's callback word and global `0x2140cc` are recorded as ownership
witnesses. Named pointers are captured at entry and reused after return because
argument registers may then contain results.

The observer tracks all bounded calls for completion, samples at most 32
parents on the established contact-sampling frames, and limits the new stream
to 1,024 events. It enforces a two-million-instruction bound per parent,
including any interrupt work. None of these spans is a hardware timing result.
The existing ordinary-RAM whitelist and shared byte cap remain unchanged.

`main_vehicle_capture.py` supplies the common strict reader.
`read_vehicle_capture(directory)` returns events, marker and input hashes;
`vehicle_envelopes(events, marker)` returns complete ordered parent groups.
The reader checks the sealed parity report, marker and stream hashes, both
checkpoint CSV identities, parent/child frames, fixed operand addresses,
nearest-even AC domain, event order and completion/sample counts. Numerical
wrappers still have to validate each recovered component's domain and outputs.
Source-only observer and reader tests cover malformed pointers, missing or
duplicated boundaries, cached/spilled saved frames, coherent operand repointing,
wrong ownership, count mismatches, redirected files and altered evidence.

## Contact result

The events are `contact_pre` at ROM `0x2acc0`, `contact_reply` at `0x2adac`,
then `contact_post` at the dynamically matched return PC. Entry `g0` identifies
the three-word point and entry `g1` the eight-word output. These saved identities
are used at return because the returned register values are not stable pointers.

At the reply boundary, the original helper has already read all 17 FIFO words.
Their order is reconstructed without another FIFO access:

1. `contact_reply_prefix`: 12 words at helper `fp+0x60`.
2. `contact_reply_tail`: four words at helper `fp+0x50`.
3. Raw integer register `g5`, `registers[21]`: the final word.

`contact_point` and `contact_output` preserve the saved entry addresses at all
three events. A reply must belong to the same helper frame, and a helper cannot
complete without its reply observation. These records do not assign meanings
to the 17 words or certify the contact solver.

A contact pairing failure writes a bounded `contact-failure.json` diagnostic
with the pending return/frame identity and last observed helper return registers.
That file explicitly marks itself incomplete and does not replace the successful
completion marker.

## Cabinet steering

The independent leaf boundaries are ROM `0x395d8` (`steering_pre`) and
`0x39664` (`steering_post`, before its return branch). Entry `g14` supplies the
return address, which must equal output `g1`; the frame pointer must remain
unchanged. The byte at ordinary RAM `0x202050` is recorded at entry using the
low byte of its aligned word, and output `g0` retains the result bits.

Records use their own `pair_id`, not an integrator call ID or a guessed car
object. Up to 32 pairs are sampled: the first call at each 20-frame boundary,
plus frame 2,750. All observed pairs are tracked for completion even when not
sampled. Establishing the leaf's caller ownership and numerical equivalence
requires separate code inspection and native/original comparison.

The full filter has separate `steering_filter_pre` / `steering_filter_post`
events at ROM `0x39a90` / `0x39ae8`, the latter before its `ret`. The named
`steering_history` block records the word at `0x2142c0`; `steering_input` records
the aligned word at `0x202050`. Both boundaries retain all integer registers and
AC. The current frame and saved caller return identity must agree across the
pair. Sampling and pair limits match the independent leaf stream. These
observations do not equate the filter's output with a complete vehicle model.

## Main candidate contact pass

The separate main stream observes the inspected
[main candidate contact pass](../native/main_contact_pass_recovery.md), ROM
`0x34380..0x34cd8`. It uses its own `call_id` and context; none of its records is
silently added to the older alternate-object stream. `main_entry` captures the
object in entry `g0`, candidate position at entry `g1`, candidate orientation at
entry `g2`, and the actual saved caller return. The return must resume ROM
`0x3521c` with the original caller frame. Ownership still depends on the separate
main-callback analysis, rather than the stream's name alone.

Entry and prequery additionally retain `local_contact_points`, twelve words at
ordinary relocated RAM `0x5d2e30`. These bind the four original local triples to
the candidate inputs and transformed outputs. There is no additional matrix
or geometry-device read; the intermediate matrix is internal to the reference
geometry program and requires its own offline arithmetic comparison.

`main_prequery` at ROM `0x346d8` follows all four point stores. From that boundary
onwards the snapshots include twelve-word `transformed_offsets`, `query_points`
and `response_vectors` blocks at main `fp+0x70`, `fp+0x40` and `fp+0xa0`.
The response vectors are raw scratch contents until each response has executed;
their inclusion does not imply they are already initialized at prequery.
The candidate triples retain their original addresses throughout the pass.
Other named blocks are the four nine-word-spaced `result_slots` at `0x2138d0`,
three `feedback` words at `0x213960`, `feedback_enable` at `0x2142c8`, and the
course selector and object-pointer globals. The ninth word of each result slot
is spacing, not part of the adapter's eight-word output.

Each of the four queries has `query_pre/count/post` events with `query_index`;
each adapter invocation has `contact_pre/reply/post` with `contact_index` and
the same raw 17-word reply reconstruction described above. The observer checks
the actual pipelined order: query zero, adapter zero, query one, response zero,
and so on. `response_pre` occurs at ROM `0x3472c`, immediately after the
game loads the current status byte. This avoids duplicating the next-query
return event at `0x34728`; it also covers the final contact, which issues no
next query. The record binds the current result slot, contact/index offsets,
scratch pointers, already-read status in `g4`, previous selected scalar in
`r12`, parent frame and saved caller. `response_post` occurs at ROM `0x34a30`,
before the loop index is incremented, and retains the resulting response array,
mask and selected scalar. Every contact, including inactive responses, requires
this ordered pair. Query returns must be ROM `0x346dc` or `0x34728`; adapter returns
must be `0x34710`, all in the saved main frame. Every event carries a
`parent_return_frame`; helper events additionally bind their own saved caller
proof in `return_frame`. Child `r2` is diagnostic only.

The aggregate path can skip cross operations or execute four. `cross_pre` at
ROM `0x34a88` follows the original zeroing of both middle operands;
`cross_post` at `0x34b04` follows all three result stores. `cross_index` follows
the observed byte offset in `g7`: 0, 12, 24 and 36. The three-word
`cross_left`, `cross_right` and `cross_result` blocks are at main
`fp+0x70+12*i`, `fp+0xa0+12*i` and `fp+0xd0`. Pre-entry `g5` must equal
`fp+0x40+12*i`; `r13` must retain the parent object. These are raw observed
operands/results for a separate numerical comparison, not a native contact
response implementation. No extra FIFO or geometry-state reads occur.

`cross_post` is also the metric-input boundary. Its `metric_index` and
`metric_cross` block bind the three stored cross words at main `fp+0xd0`.
`metric_post` at ROM `0x34b5c` follows the original FIFO read and records the
scalar in `g12` (`registers[28]`). It is simultaneously the input to scalar
accumulation: `step_index`, prior `r11`, and the sign source in `g5` are recorded
once at this shared instruction boundary. `step_post` at `0x34b84` records the
updated `r11`. All three boundaries retain raw registers, AC, the parent object,
frame, saved caller proof and `g7` byte offset; `g14` must remain zero.
The observer obtains the result from the CPU's already-read register, never by
performing another FIFO read.

Four completed cross/metric/step groups permit `aggregate_pre` at ROM `0x34b90`
and `aggregate_post` at `0x34cc0`. Their existing object, candidate, response and
feedback blocks retain the before/after state. The inactive branch also reaches
the latter PC; it emits no aggregate event without the paired input boundary.

Within each contact response, optional `normalization_pre` at ROM `0x348ec`
and `normalization_post` at `0x34968` capture the three-word
`normalization_vector` at main `fp+0xd0`. `contact_index` must equal `r5`;
`r7` and `g9` must identify `fp+0xd0` and `fp+0xd8`. A response may contain at
most one complete normalization pair. The next query may already have completed
before this pair because the original response loop is pipelined.

`main_return` requires all four query/count/adapter/reply/response groups to
complete, plus either zero cross operations or four complete cross/metric/step
groups and their aggregate update. No pending normalization, metric, step or
aggregate pair may survive the return. Marker fields
`main_calls`, `main_completions`, `main_samples`, `main_events`,
`main_queries`, `main_query_completions`, `main_contacts`,
`main_contact_completions`, `main_cross_pairs` and `main_sampled_cross_pairs`
describe the distinct populations. The replay tool also requires at least one
complete sampled main pass before it can report success.

New marker families `main_metric_*`, `main_step_*` and
`main_normalization_*` each contain `entries`, `completions` and
`sampled_pairs`. `main_aggregate_*` contains `entries`, `completions` and
`samples`. Counts include all bounded passes; detailed records include only the
explicitly sampled passes. Completion totals must match before sealing.
The later `main_response_*` family likewise records `entries`, `completions`
and `sampled_pairs` for the complete per-contact response. Historical captures
through 010 precede these response-input records and cannot establish their
paired-input contract.

## Limits and tests

Runs are limited to 10,000 rendered frames; the observation endpoints may differ
by at most 1,200 frames. Cadence is limited to 16,384 calls; selector and callback
streams each to 16,384 records; steering and its full filter are each limited
to 16,384 calls. At most four calls at each selected input/60-frame
checkpoint receive detailed snapshots, with 128 detailed calls and 4,096 events
in the alternate stream. Main passes have a separate 16,384-call limit,
32 detailed samples (the first call at each selected frame), and 4,096-event
limit. Main sampling additionally includes the observed aggregate-operation
window at frames 2,571 through 2,579. Frame 2,752 is additionally sampled to diagnose a previously unmatched
contact return; it does not establish special game semantics. The combined
cadence/detail/selector/callback/steering/filter/main streams are capped at
16 MiB. Code coverage uses a fixed 65,536-counter array. At most three private
reference images are written. An outer call must return within two million
completed instructions. Process timeouts also bound compilation and both runs.

```sh
python3 tests/test_ps2_driving_capture.py
```

The ROM-free fixture exercises the actual observer with an ordinary-RAM-only
bus that rejects all writes and device accesses. Tests cover raw values,
original/relocated PCs, saved pointer identity, ordered orientation/contact
boundaries, duplicate suppression, overflow and invalid ranges, unfinished
calls, no completion marker on failure, saved cached/spilled return frames,
interrupt-altered child `r2`, independent steering/filter identity,
four-slot main query pipelining, exact cross block mapping, optional no-cross
passes, and output-directory confinement. Missing, duplicated or mismatched
main boundaries and non-RAM arguments cannot seal a successful capture.
The added cases exercise raw normalization/metric/accumulator/feedback fields,
cached and spilled parent frames, skipped aggregate/normalization paths and
23 malformed numerical sequences. Repeated normalization, missing metric or
step output, an unpaired aggregate update or a changed saved parent cannot
produce a completion marker. The actual observer also passes focused
AddressSanitizer/UndefinedBehaviorSanitizer runs on valid and rejected sequences.
Response-input tests additionally check all four slot/pointer/index mappings,
the already-read status byte, the prior/result scalar, cached and spilled
caller proofs, and ten missing/duplicated/mismatched-input cases. Event counts
remain strictly increasing even at the next-query return. Missing response
inputs and changed caller proofs fail without a completion marker. All 17
observer tests pass; eight focused response sequences also pass sanitizer runs.
