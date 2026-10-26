# Main-helper pair reduction and parameter selection

Two independently written value operations recover the initial input reductions
inside main callback dependency `0x37870`. `make_paired_balance` returns two sums
and two signed balances from four explicit scalars. `select_vehicle_parameters`
chooses and copies a caller-owned raw parameter record. Neither operation contains
original processors, mapped state, device accesses, allocations or embedded game
data. The governing design's **Platform layer & build** native-rewrite exception
applies; physical meanings and units are not inferred from the field locations.

## Remaining helper boundary

The complete helper occupies `[0x37870,0x38674)`: 3,588 bytes, 696 linearly decoded
instruction sites, SHA-256
`3503a497f3ad7ac13f9adf9c3290d888caf3f8b3798a48ee4fba02e9df9f3162`.
It has nine distinct direct/BAL dependencies (`0x38680`, `0x388d0`, `0x38958`,
`0x389b8`, `0x38a18`, `0x39750`, `0x39c40`, `0x3a320`, `0x3aa38`), 35 static
FIFO write sites and eight read sites. These are decoded sites, not runtime
transaction counts or timings. The native reductions below do not replace this
complete helper or connect a driving loop.

The original caller supplies an object, two three-word vector pointers and a
scalar. Both vector arguments are **in/out**: the tail adds to the second vector
and writes a geometry result back through the first. Treating the first pointer
as read-only would be incorrect. Parameter lookup, subsequent global filters,
transcendental/geometry operations and final state updates still need recovery.
Private disassembly and the complete helper identity remain in
`build/ps2-native-player-research-001/`.

All compared instructions belong to the verified 2,097,152-byte Revision B image,
SHA-256 `bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.

## Paired balance

The original region `[0x37900,0x379d0)` is 208 bytes, SHA-256
`f634c8ea2f277ad29173f03c24b7afbdf77c1038be6bb54d5cc4d77e0a189a17`.
It reads object fields `+0x64/+0x68` and `+0x6c/+0x70`. It also loads the existing
global at `0x21425c`, retaining that raw word for later code; this value does not
participate in the isolated calculation. The four calculated outputs are local
register values. This region makes no memory write and calls no dependency.

For each consecutive input pair `(first, second)`, with `R32` and `R64` denoting
separate nearest/even binary32 and binary64 rounding:

```
sum = R32(first + second)
if sum == 0:
    balance = +0
else:
    difference = R32(second - first)
    weighted = R64(binary64(difference) * binary64(0x3ff199999999999a))
    balance = R32(R64(weighted / binary64(sum)))
```

The constant is the binary64 encoding of 1.1. Substituting binary32 multiplication
or omitting its rounding boundary is outside this contract. A zero sum includes
either signed zero; its returned balance is positive zero, while the sum itself
retains its computed sign. The native function accepts finite input components
in `[-2^20,2^20]`, including subnormals. Every rejection preserves all output
fields. It never changes the inputs.

There is no arbitrary minimum denominator. The numerator and denominator are
the difference and sum of the same two binary32 values. For unequal magnitudes,
their spacing bounds a nonzero ratio even at near cancellation; equal magnitudes
take either the zero-sum branch or produce a zero difference. All nonzero balance
results within this domain are normal, with magnitude below `2^26`. Subnormal
sums are therefore supported without making the final quotient overflow.

The implementation forms the exact 53-by-24-bit weighted product in three
32-bit limbs and explicitly rounds it to 53 bits. A scaled numerator of at most
77 bits is then divided by the denominator's at-most-24-bit significand, using
integer quotient and remainder to perform the second 53-bit rounding. Finally
it rounds to binary32. During review, the minimum subnormal significand was
identified as a zero-shift product case; the explicit branch avoids an invalid
64-bit shift. No host or EE floating arithmetic determines the production result.

The isolated original oracle checks output words, the complete ordered read
sequence, the unrelated retained global word, and the absence of writes. An
independent volatile-floating host formula also agrees. The full synthetic set
passes 101,130 evaluations and 4,344,222 original instructions, fingerprint
`205c18818a38f128`. Coverage includes selected signed-zero/subnormal/boundary
combinations, opposite-sign adjacent values throughout the input exponent range
and 100,000 deterministic cases. It is not exhaustive over all accepted inputs.
The smaller common host/EE fixture has 2,154 cases, fingerprint
`1fd698b609c8f960`. There are 24 separate atomic domain-rejection checks, and the
complete host fixture passes ASan/UBSan.

## Parameter selection

The region `[0x3789c,0x378f4)` is 88 bytes, SHA-256
`23e90e789234c677e4b34dc7b7e08c9d7f12357f918d7ccbc4c55dba49d3b531`.
The highest set bit among bits 9 through 1 selects the matching record index;
otherwise it selects record 0. Bit 0 and bits 10 through 31 do not change this
decision. Exactly 13 words are copied without numerical interpretation.

`select_vehicle_parameters(flags, records)` receives ten immutable typed records
and returns the chosen index and record by value. Arbitrary word encodings are
valid. The original pointer table aliases several entries; a native owner may
provide repeated records without retaining those mapped pointers. No record
bytes are supplied by the public implementation.

The portable fixture covers every low-ten-bit mask with three different upper-bit
patterns: 3,072 selections, fingerprint `cf0278eb2ab1a5a1`. The original oracle
also repeats those cases with aliased source records, checking the table access,
all 13 reads and writes, copied words and selected index. Its 6,144 evaluations
execute 98,256 original instructions, fingerprint `47a8a162519318d5`.

## Captured comparison and reproduction

Capture 012 retains all 2,850 reference checkpoints and supplies 26 complete
main-callback groups, each with both isolated pre/post pairs. The common reader
binds the outer and helper saved-caller proofs, instruction ordering, all expected
call boundaries and completion counts. The scoped reader additionally verifies
unchanged input/object storage, the otherwise unused prior read, exact field
locations and copied parameter words. It adds no FIFO or device read.

All 26 pair results and all 26 selections agree with both native and isolated
original execution. The combined pair comparison has 101,156 evaluations,
4,345,340 original instructions and fingerprint `611fdba5efd25614`; selection has
6,170 evaluations, 99,140 instructions and fingerprint `6767e7e5a9bd359d`.
Every sampled selection uses index 2, and no sampled pair has a zero sum.
Other indices and zero/subnormal behavior therefore rely on synthetic evidence.
These are comparisons of the two named regions, not a full-helper or full-frame
native replacement.

Source, private image, capture and executable hashes are recorded in
`build/ps2-native-paired-balance-live-002/result.json`, SHA-256
`047eaffce0bd2755aed9bf5b47668d156c85ed98f6a780dff9ea7e8114928ca9`.
This refresh binds the final shared caller-proof validator. Native game
integration and physical PS2 validation remain pending.

The standalone fixture built with the pinned open PS2SDK toolchain and executed
in Play 0.72 with its built-in HLE:
`build/ps2-native-paired-balance-ee-001/segarally-ps2-core-probe.elf`, SHA-256
`e587aaa9d227876b609f1573f63cf6f9bde0596575ae4ed23b560eaded186a54`,
1,225,936 file bytes / 179,808 load-segment bytes before dynamic allocations.
It passed all 2,154 paired cases, fingerprint `1fd698b609c8f960`, all 24 rejection
checks and all 3,072 parameter selections, fingerprint `cf0278eb2ab1a5a1`.
Guest stderr was empty and two window captures succeeded. The 20.255-second
observation is not measured fixture execution time or a console performance
result. Source/build/ELF/log identities are bound in that directory's
`runtime-validation.json`; no connected gameplay was executed.

```sh
python3 tests/test_ps2_paired_balance.py
python3 platform/ps2/tools/check_paired_balance.py \
  --program /path/to/owned/program.bin \
  --capture build/ps2/driving-capture-012/capture \
  --output build/paired-balance-comparison
```

Omit `--capture` for synthetic comparison alone. The output directory must be
new and confined to ignored `build/`; original program bytes, parameter records,
captures and disassembly remain private.
