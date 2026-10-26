# Native contact response pass

`resolve_contact_pass_response` joins four interpreted contacts to the recovered
per-contact force, scalar accumulation and final car-state update. It is a typed
native value operation under the governing design's **Platform layer & build**
exception. It has no processor, mapped bus, device state or command transport.
The complete preparation/search composition is documented in
[candidate contact pass](candidate_contact_pass.md).

## Contract

Inputs are four `ContactResult` values and their transformed offsets, the three
explicit response parameters, three candidate reconstruction bases and a feedback
enable flag. The mutable state holds motion, candidate position/orientation,
two feedback integers, the contact mask and selected scalar. Input and output
fields retain storage-based names because physical units are not yet established.

The pass clears its mask and selected scalar, then evaluates contacts in index
order. An interpreted status with bit 7 set contributes the corresponding mask
bit. The [per-contact response](contact_response.md) preserves the original
maximum-selection comparison, including the new gap winning equality.

When the selected scalar has zero magnitude, the car state is preserved exactly;
only the mask and selected scalar change. Unused parameters and car fields may
therefore retain arbitrary encodings on that branch. Otherwise, for each point:

1. Replace both offset and response middle components with positive zero.
2. Compute the separately rounded cross product of offset and response.
3. Supply the recovered metric's two points as `(0,0,cross.y)` and
   `(0,cross.x,cross.z)`. This reproduces the original interleaved wire order;
   substituting a conventional distance call would change the result.
4. Accumulate the metric with the recovered sign rule and apply the aggregate
   state update with the explicit parameters and bases.

Each constituent's bounded numerical domain applies. Any rejection leaves every
output field unchanged, including contacts processed earlier. The production
function allocates no memory and changes no external state.

## Original and captured evidence

The isolated reference executes original CPU instructions from ROM `0x34728`
through the boundary at `0x34cc0`. Its course-query and adapter calls are
deliberately excluded: the contract starts with four interpreted contacts.
The harness checks their original call arguments and resumes the loop using
those explicit values. Thus this is a post-query composition comparison, not
execution of the complete original candidate pass.

All response arithmetic, loop control and final state stores execute normally.
Commands 58 and 5b execute the verified original geometry primitives with
initialized constants; neither accesses an external table. Memory, command
counts, operand consumption, completion boundaries and preserved storage are
checked. Original program and firmware bytes are supplied privately, verified
by their complete image identities and excluded from published source.

The synthetic suite covers 4,224 passes, all 16 contact masks, positive/negative
gaps, both zero signs, enabled/disabled feedback and unused nonfinite payloads.
Its fingerprint is `f99dcd3347d0f255`. The original comparison also checks all
26 captured main-car passes from observer 011, including nine active aggregates,
against all 13 output words: motion, candidate pose, feedback, mask and scalar.
The combined result is 4,250 passes, 2,128,380 CPU instructions and 615,149 geometry
instructions, fingerprint `6b4f4fd1f1c199bf`. The source-bound report is
`build/ps2-native-contact-pass-response-live-002/result.json`.

The capture reader enforces complete four-contact parents, saved caller-frame
identity, exact event boundaries, ordered contacts, ordinary aligned operand
addresses, nearest/even rounding and preservation of unrelated object/candidate
fields. Disabled feedback must stay unchanged. Capture completion and file
identities are tied to the observer-enabled/disabled parity report.

Host, AddressSanitizer and UndefinedBehaviorSanitizer checks pass. The standalone
EE fixture passes 640 cases with fingerprint `3a490cc705f0b6f2`, plus inactive
preservation and late atomic rejection, in Play 0.72 HLE. Its executed ELF is
`c9c873140d76c3426fd1d51c14f957d7423413f8bbdb744802b59023bde142d3`,
1,244,676 file bytes. Guest stderr is empty. The 20.255-second observation window
is not execution time; both window-image attempts failed. Numerical runtime
evidence is bound in
`build/ps2-native-contact-pass-response-ee-001/runtime-validation.json`.

```sh
python3 tests/test_ps2_contact_pass_response.py
python3 platform/ps2/tools/check_contact_pass_response.py \
  --program /path/to/owned/program.bin \
  --tgp-program /path/to/owned/geometry-program.bin \
  --capture build/ps2/driving-capture-011/capture \
  --output build/contact-response-pass-proof
```

This bounded comparison does not establish a connected native driving loop,
all-course behavior, physical-console execution or gameplay performance.
