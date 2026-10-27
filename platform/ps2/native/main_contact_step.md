# Main candidate and contact step

`resolve_main_contact_step` composes the recovered
[candidate preparation](main_candidate.md) and
[four-point contact pass](candidate_contact_pass.md) into one typed operation.
It starts before the main callback's candidate prefix at ROM `0x34ff0` and ends
after the contact call at `0x35218` returns to `0x3521c`. It does not include
the later contact-slot work, the complete vehicle update or a driving loop.
This follows the native PS2 exception in the governing design's
[Platform layer & build](../../../docs/daytona-usa-recomp-design.md#platform-layer--build).

The function derives contact inputs from its newly prepared vehicle state:
first/third position bases, middle orientation base, first/third motion and
middle angular motion. Callers cannot provide contradictory copies of those
values. The contact pass transforms the four local points, performs the course
searches, interprets the results and aggregates their responses. Only the three
proven motion components are written back, together with the resulting candidate
triples, contacts, feedback, mask and selected scalar. The prefix's position,
pending delta, marker and other preserved fields remain part of the same state.

The API contains immutable course/normal spans, local points, a signed segment
index, three response parameters and a feedback gate. No original addresses,
processor state, device transport, allocation or platform calls are present.
Each constituent's domain applies to the values it actually consumes. A valid
prefix may produce an unsupported later contact input; any rejection then
preserves all 46 serialized state words, including the earlier prefix changes.

## Original-boundary comparison

Capture 013 retains the vehicle and contact streams byte-for-byte from 012.
The joined reader first validates each complete stream independently, then
matches the same object, frame and instruction interval. The contact entry is
exactly one instruction after the prefix exit. Its saved caller frame matches
the vehicle frame, all saved local words match except the call-written return
instruction pointer, and global arguments/object/candidate words agree. The
join does not depend on independently assigned call IDs merely coinciding.

The native checker receives the prefix's original input state and the owner's
verified course, normal and local-point data. It calculates the intermediate
candidate and contact states itself; captured intermediate results are not
native inputs. It links no original processor or generated firmware.

All 26 sampled steps match, including 104 final contacts, nine active aggregate
responses and 1,196 state words, fingerprint `9a47fb54b88fea84`. The private
source-bound evidence is
`build/ps2-native-main-contact-step-live-001/result.json`.
Two record details are explicit: the final response marker is inherited from
the observed prefix exit and the checked callee-preservation contract, rather
than a separate post-contact marker snapshot; the unused initial selected scalar
is set to zero because the contact pass resets it before reading it. Neither is
misrepresented as an additional observed value.

All captured prefixes have zero pending movement correction. The live comparison
therefore exercises composition with that prefix branch, including active contact
responses. Conditional prefix consumption/clamping has the separate original
synthetic proof and the authored controls below. This replay window does not
establish every course, full-race fidelity or independent arcade-board behavior.

## Authored controls and PS2 execution

Sixteen independently calculated cases combine active/inactive prefix behavior,
both signs, square hits/misses and enabled/disabled feedback. Zero local points
and an authored square give a first-edge determinant of 32, a gap of `1/32`
and four responses adding `1/16` to first motion with zero torque. Explicit
expected words check the consumed delta's rounding, updated bases, candidate
values, marker, untouched channels and raw miss payloads. These controls have
fingerprint `76ae45f11adbe279`; nonzero torque is supplied by the live comparison,
not these zero-offset controls.

Four rejection paths verify rollback at the prefix, course search, aggregate
divisor and fourth-contact interpretation. The last has a successful matching
control before its divisor is made invalid, so it demonstrates rollback of
the prefix and three new contacts. The final host fixture passes ASan/UBSan;
evidence is `build/ps2-native-main-contact-step-sanitize-002/validation.json`.

The first EE fixture exposed an expected-value error: the pinned target compiler
encoded the test's decimal `0.1f` as `0x3dcccccc`, while the native function
correctly returned the verified original `0x3dcccccd`. Its compiled integer
constant stores establish that this discrepancy preceded runtime arithmetic.
The fixture now spells both signs of the expected clamp with exact bits.
Production arithmetic and the host fingerprint did not change. Failed evidence
is retained in the private `ee-001` directory and is not reported as a pass.

The corrected `ee-002` fixture passes all sixteen controls and four rejection
paths in Play 0.72 HLE, with empty guest stderr and two window captures.
Its ELF SHA-256 is
`009213c7a322e2dabfe2eaaf07817b26b5a0a7ceac16c4530aef43251d470399`.
The 20.232-second observation window is not execution timing. Physical-console
and connected-gameplay validation remain pending.

```sh
python3 tests/test_ps2_main_contact_step.py
python3 platform/ps2/tools/check_main_contact_step.py \
  --program build/ps2/native/rom_cache/program.bin \
  --tables build/ps2/native/tgp/copro_tgp_tables.bin \
  --copro build/ps2/native/tgp/copro_data.bin \
  --capture build/ps2/driving-capture-013/capture \
  --output build/ps2-native-main-contact-step-live-new
```

Original images, extracted assets and captured values remain under ignored
`build/`. The public source contains the typed implementation, authored fixtures,
comparison adapters and analysis only.
