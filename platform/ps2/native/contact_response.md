# Per-contact response

`make_contact_response(input, previous_scalar, output)` recovers the response
calculation inside the main four-point contact pass. It consumes an interpreted
`ContactResult`, a transformed offset, two explicit parameters and the preceding
selected scalar. It returns a response vector, the next selected scalar and an
active flag. The production implementation contains no original processor,
mapped addresses, device access, tables or allocations. It follows the governing
design's **Platform layer & build** native-rewrite exception.

The names `parameter50` and `parameter54` identify their recovered source fields;
physical units and a broader force model are not assigned. This component does
not perform course search, advance a vehicle or update a contact mask. Its caller
combines the four results in index order; see [post-query composition](contact_pass_response.md).

## Exact bounded contract

When status bit 7 is clear, the response is three positive zeros, the previous
scalar retains its complete raw encoding, and `active` is false. The other inputs
are unused and may contain arbitrary encodings. The active branch accepts finite
direction and offset components with magnitude at most 16, gap and previous
scalar magnitudes at most 1024, and positive parameters in `[2^-10, 2^16]`. It also
requires the separately rounded dot product below to be nonnegative; negative
zero is accepted. These are explicit native bounds, not the original program's
complete input domain. Every rejection leaves the complete output unchanged.

Let `R32` and `R64` mean nearest/even binary32 and binary64 rounding, respectively.
All products and sums in the two crosses use the separately rounded contract of
[rounded cross product](rounded_cross_product.md). With `D` the contact direction,
`O` the offset, `p = parameter50`, `q = parameter54`, and `g` the contact gap:

```
first  = rounded_cross(D, O)
second = rounded_cross(first, D)
unit   = normalize_contact_direction(second)
dot    = R32(R32(R32(unit.x*O.x) + R32(unit.y*O.y)) + R32(unit.z*O.z))
length = R32(sqrt(dot))
denom  = R32(q + R32(R32(length*p)*length))
numer  = R32(p*q)
factor = R32(R64(R64(binary64(numer)*0.5) / binary64(denom)))
gain   = R32(factor*g)
response = {R32(gain*D.x), R32(gain*D.y), R32(gain*D.z)}
selected_scalar = g >= previous_scalar ? g : previous_scalar
active = true
```

Normalization includes its recovered positive-Z fallback; it is not replaced
with a generic normalization function. The square root and the following two
multiplications remain separate operations: substituting `dot*p` can change the
result. Equal scalars select the new gap, including opposite signed-zero inputs.
The aggregate caller starts the selection at positive zero and separately
decides whether to execute its final update.

The implementation uses bounded integer significands, exact quotient remainders
and an integer square root. The numerator lies in `[2^-20, 2^32]`, so its halving
is exact and normal. The denominator and quotient are positive normals in this
domain. For a quotient of two 24-bit significands, a nonzero displacement from a
binary32 midpoint exceeds the binary64 rounding interval; the original
binary64-then-binary32 division therefore agrees with direct nearest/even
binary32 division here. This is a bounded equivalence, not a claim about general
floating-point division. Existing normalization and cross-product source files
were not changed.

The original region has a raw-pointer alias branch when the input direction
overlaps its workspace. Ordinary main-pass calls keep the global contact slot
and caller-frame workspace separate. This API represents those as separate
values and deliberately does not expose that alias case.

## Original and captured comparison

The verified Revision B program is 2,097,152 bytes, SHA-256
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The region `[0x34728, 0x34a30)` is 776 bytes, SHA-256
`c9a1bab4ddc45d7701dd07c632abd980172de75e639ca1dda58be55df622f5c5`.
It is a region of a larger function, not a separately callable original helper.

The isolated host oracle executes that original region, restricts instruction
and data access to the declared bounds, and compares the complete response,
selected scalar and active status. It checks the original contact-mask addition,
unchanged contact slots, and every ordinary-RAM word outside the documented
response/workspace stores. The inactive path must leave its scratch storage
untouched. Original execution and an independent volatile-floating host formula
both agree with the native integer implementation on accepted inputs.

The synthetic fixture evaluates 18,432 cases. Of those, 18,360 are accepted:
13,860 active and 4,500 inactive. The other 72 exercise the explicitly unsupported
negative-dot domain and atomic rejection; they are **not** counted as passing
original executions. Accepted cases execute 2,001,510 original instructions.
The complete fixture fingerprint, including rejection results, is
`3684bd3cf078fbc4`. There are 33 additional rejection checks, and ASan/UBSan pass.

Capture 011 supplies 104 response pairs in 26 main passes: 13 active and 91
inactive. The observer retains all 2,850 reference frame checkpoints unchanged.
The response boundary at live PC `0x5d372c` follows the program's own status-byte
read; it adds no FIFO read. The strict consumer verifies parent/caller proofs,
indices, slot and frame pointers, nearest rounding, status, prior scalar/mask,
unchanged input storage, and exact completion counts. Each pair compares native
response/scalar output with both original execution and the recorded words,
while deriving the active flag from the observed status gate.

The combined comparison passes 18,536 fixture evaluations / 18,464 accepted
cases, including all 104 live calls, with 2,003,904 original instructions and
fingerprint `b7152b8cdc529ab3`. Source/capture/executable identities are recorded in
`build/ps2-native-contact-response-live-001/result.json`, SHA-256
`6076ea1ec48fd1bd4d69b4db9fc7a0d4c4baaed6d24ebc6d445ac5b3d7ec4a5b`.
Synthetic and captured coverage are bounded evidence, not an exhaustive proof
over every accepted input or every course.

The standalone EE fixture ran in Play 0.72 with its built-in HLE and passed 1,200
cases / 1,128 accepted, fingerprint `cd1b0a7946e70d88`, plus all 33 rejection checks.
Its ELF SHA-256 is
`8b1a941152a090e73cc40990bffb6d3326f033f8f11267f134a916fcbd1fff3e`,
1,233,912 file bytes. Guest stderr was empty. The 20.037-second observation is
not measured fixture execution time; window captures failed, so no visual result
is claimed. The source/build/ELF/log binding is
`build/ps2-native-contact-response-ee-001/runtime-validation.json`. Physical PS2
execution and connected native driving remain unverified.

## Reproduction

```sh
python3 tests/test_ps2_contact_response.py
python3 platform/ps2/tools/check_contact_response.py \
  --program /path/to/owned/program.bin \
  --capture build/ps2/driving-capture-011/capture \
  --output build/contact-response-comparison
```

Omit `--capture` for the synthetic original comparison. The output directory must
be new and confined to ignored `build/`. Private program bytes, captures and
disassembly are not included in the public source or native executable.
