# Rounded affine point transform

`rounded_affine_transform(matrix, point, output)` is a typed native recovery of
the indexed point-transform arithmetic used by the main car's four-point
contact pass. It accepts three matrix columns, a translation and a point.
It has no indexed geometry store, command decoder, matrix stack, guest
addresses or processor state. Its separate original-firmware comparison is an
offline research tool, excluded from the native game and reusable engine.

For each output axis, start with its translation component, then add the three
column-component × point-component products in column order. Each product and
each sum rounds separately to binary32 nearest/even. Translation is added
first. Reassociation, a dot product followed by translation, or fused
multiply-add can change the result. An explicit test distinguishes each of
these arithmetic boundaries.

All fifteen inputs must be finite with absolute value at most 2^20. Signed
zero and subnormal values are supported. Products have magnitude at most 2^40;
partial sums remain below 2^42. Independently written integer dyadic arithmetic
preserves gradual underflow and nearest/even rounding without relying on the
EE floating-point unit. Exact products need 48 significand bits and aligned
sum numerators need at most 56 bits. Operands separated by more than 32 binary
exponent steps cannot affect the larger operand's rounding. No general floating
processor or exception model is included.

The function returns false without changing the destination if any input is
outside this domain. On success the destination can alias the point, any
matrix column or the translation. All inputs are captured before assignment;
no pointer survives the call. Coordinate units and matrix construction remain
outside this contract.

## Original boundary and validation

The private 6,660-byte firmware has SHA-256
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
Command 0x48 occupies words `[0x44c,0x46c)`, 128 bytes with SHA-256
`229d95dd4c345dc0a0eda94ba6988aa858b20b18bb99143cad0753e93626b273`.
The offline oracle maps the explicit matrix and point to an isolated original
store, supplies one index, executes all 32 instructions and checks the return
boundary and three final output words. External reads/writes, FIFO output and
extra FIFO input fail the check.

The portable independent float reference and original-firmware oracle match
**84,273 evaluations**, fingerprint `7005cd920cda465e`. Cases cover all 32,768
sign patterns of fifteen zero inputs, selected normal/subnormal and domain
boundaries, cancellation and 50,000 deterministic random matrices/points.
These are evaluations rather than a claim of exhaustive nonzero coverage.
The original comparison executes 2,696,736 instructions and 84,273 FIFO reads;
these are reference counts, not PS2 timings. All five destination alias modes
and 720 atomic-rejection evaluations pass. AddressSanitizer and
UndefinedBehaviorSanitizer pass the same portable suite.

The standalone EE fixture checks 4,065 cases with fingerprint
`eb07dfee53139c87`, the same rejection checks and explicit order/fusion guards.
Its fixture count and fingerprint are checked before success is reported.
This fixture passed in Play 0.72 HLE with empty guest stderr. Its executed ELF
has SHA-256 `93035e5363e73de1cfae54d4988fd5d485aa83752d2f5bea4bccd422d048619b`
and 1,226,380 file bytes. The 20.181-second observation is not measured fixture
execution time. Source/build/ELF/log identities are bound in
`build/ps2-native-rounded-affine-ee-001/runtime-validation.json`.

```sh
python3 tests/test_ps2_rounded_affine_transform.py
python3 platform/ps2/tools/check_rounded_affine_transform.py \
  --tgp-program /path/to/owned/program.bin \
  --output build/rounded-affine-oracle
```

The checker restricts output to ignored `build/`, verifies private firmware
identity, generates a fresh reference, and binds source, firmware, generated
code and executable hashes before/after comparison. The initial valid report
is `build/ps2-native-rounded-affine-oracle-002/result.json`. Attempt 001 rejected
an extra FIFO read because its oracle stopped one instruction late; the
after-instruction hook now stops at the completed return, before another
command executes. The native arithmetic did not change for that harness fix.

This proves the bounded transform in isolation. It does not recover matrix
construction, connect the main car's contact response, establish a live matrix
capture, or validate physical hardware. The native driving loop remains
unconnected. See [the full contact-pass study](main_contact_pass_recovery.md).
