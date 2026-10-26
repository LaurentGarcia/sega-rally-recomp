# Native candidate contact pass

`resolve_candidate_contact_pass` joins orientation construction, four point
transforms, course searches, result interpretation and the complete response
update. The typed native implementation matches all 26 captured main-car calls
in the current driving sample. The surrounding main-car callback and continuous
driving loop remain unconnected.

This follows the governing design's **Platform layer & build** native rewrite
exception. The production function has no original CPU, bus, device state,
geometry command interpreter, original addresses or allocation. It consumes
immutable course polygons, normal coefficients and four owner-supplied local
points through ordinary native values. It belongs to the game layer; shared
PS2 hardware services remain unchanged.

## Data flow and state

```text
candidate position/orientation + four local points
                      |
         recovered polynomial orientation
          and rounded point preparation
                      |
             four offsets + query points
                      |
       current / next / previous course search
                      |
     result interpretation + previous contact slots
                      |
       four ordered responses + scalar selection
                      |
         conditional scalar/response aggregation
                      |
  motion + candidate pose + feedback + mask + contacts
```

The input carries a typed course, immutable normal table, local points, signed
segment index, three response parameters, candidate reconstruction bases and a
feedback enable flag. The state carries the prior car response state and four
prior interpreted contact slots. Status-only misses preserve each slot's old
direction/gap bits; they do not manufacture zero payloads.

The function copies the state, prepares all four points from the original
candidate pose, and searches/interprets contacts in index order. It then calls
the [complete response pass](contact_pass_response.md). Only after every stage
succeeds does it publish the new state. A fourth-contact failure or late
aggregate rejection therefore preserves earlier slots and the entire car state.
The numerical and asset bounds of the individual components all apply.

The native state intentionally omits original query scratch, staging cursors,
matrix-stack entries and command ports. It models the game's explicit result
values; it does not promise an equivalent hardware bus trace. Matrix restoration
and original command sequencing are covered by separate offline evidence.

## Complete recorded comparison

`check_candidate_contact_pass.py` verifies the original program, course image
and coefficient table by their pinned complete identities. Local points must
match the verified program interval. It requires capture 011's complete,
hash-bound observer parity record, all four child contacts, saved parent-frame
proof, nearest/even rounding, unchanged preparation inputs and coherent
course/segment selection. Original slot prefixes/spacing must be preserved;
prior adapter payloads and final contact outputs must match their parent state.

The comparison executable decodes owner-supplied assets into native polygon
arrays and normal coefficients. It links only typed native components, with no
original CPU, generated geometry program or device runtime. Starting from the
recorded candidate pose and previous slots, it computes the four queries and
the complete response itself. Captured interpreted contacts are expected
outputs, not inputs supplied to the native pass.

All **26 passes**, **104 contacts** and **nine active state updates** match.
For each pass the checker compares all 12 offset words and all 33 final state
words: 13 car/mask/scalar words plus 20 contact words. The final state comparison
covers 858 words, fingerprint `2e948c3490a06a63`; 312 offset words also match.
The report `build/ps2-native-candidate-contact-live-001/result.json` binds every
project source dependency, executable, private input, record and capture hash.

This is a comparison against recorded execution of the existing reference,
not an isolated execution of the entire original CPU pass or independent
arcade-board certification. It covers the measured replay window, not every
course, vehicle, mode or possible input. The larger collision system and main
vehicle update remain separate recovery work.

## Portable and EE validation

An original square fixture supplies a hand-calculated centre contact: determinant
32, gap 1/32, four +X responses of 1/64, zero torque and a 1/16 motion increment.
Another 768 cases cover 512 active passes and 256 misses, including raw nonfinite
prior-slot payload preservation. Six atomic rejection checks cover invalid
preparation, course index, result divisor, aggregate inputs and a fourth-contact
failure following three valid results. A successful control verifies that the
fourth contact reaches the intended polygon before its divisor is invalidated.
These tests supplement the original comparison; their hash is a regression and
cross-platform fingerprint, not an independent fidelity proof.

Host and AddressSanitizer/UndefinedBehaviorSanitizer checks pass. The final
standalone EE fixture passes in Play 0.72 HLE with fingerprint
`b8ed3cad65bf6be4`, empty guest stderr and all six rejection checks. Its ELF is
`ebf2e239611f9069d9bb4940fc7ce67af412c4416421a572dee2d9363a2a0910`,
1,253,380 file bytes / 204,896 resident bytes. Runtime evidence is recorded in
`build/ps2-native-candidate-contact-ee-003/runtime-validation.json`.
The 20.150-second observation window is not measured execution time. Window-image
attempts failed; the guest emitted both exact numerical completion markers.

```sh
python3 tests/test_ps2_candidate_contact_pass.py
python3 platform/ps2/tools/check_candidate_contact_pass.py \
  --program /path/to/owned/program.bin \
  --tables /path/to/owned/geometry-tables.bin \
  --copro /path/to/owned/course-data.bin \
  --capture build/ps2/driving-capture-011/capture \
  --output build/candidate-contact-proof
```

All decoded assets, captured records and executables remain private under
ignored `build/`. CI runs the portable synthetic tests and cross-links the EE
fixture without any game data. Physical PS2 execution, native driving and
gameplay performance remain unverified.
