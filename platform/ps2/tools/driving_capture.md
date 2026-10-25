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

## Limits and tests

Runs are limited to 10,000 rendered frames; the observation endpoints may differ
by at most 1,200 frames. Cadence is limited to 16,384 calls; selector and callback
streams each to 16,384 records; steering and its full filter are each limited
to 16,384 calls. At most four calls at each selected input/60-frame
checkpoint receive detailed snapshots, with 128 detailed calls and 4,096 events
overall. Frame 2,752 is additionally sampled to diagnose a previously unmatched
contact return; it does not establish special game semantics. The combined
cadence/detail/selector/callback/steering/filter streams are capped at
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
interrupt-altered child `r2`, independent steering/filter identity and
output-directory confinement.
