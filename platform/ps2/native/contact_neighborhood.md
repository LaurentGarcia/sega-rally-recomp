# Contact-query neighborhood recovery

This component recovers the index-selection part of the car integrator's
course-query preparation. It does **not** calculate an intersection, height,
normal, wheel load, or force. The governing design's **Platform layer & build**
native-rewrite exception permits a typed implementation with a separate private
original-program oracle. The native leaf contains no guest addresses, firmware,
processor, bus, floating-point operation, allocation, or extracted data.

## Original boundary and corrected table identity

Revision B ROM `0x2abc0..0x2acbb` is 252 bytes, SHA-256
`ff2df39653904b67d7172b6765604008e5c7c981bd003836e980943bf821a736`.
The program identity is the one recorded in [angle folding](angle_fold.md).
The integrator calls it at ROM `0x2d2d4`, within its four-iteration loop.
There are fourteen decoded direct call sites for the same query leaf, so a
live observer must match each query's own saved return address. In particular,
the integrator's `0x2f8a0` dependency makes its own queries at `0x2fc2c` and
`0x2fc78`; observing only return `0x2d2d8` would miss those completions.

The higher caller loads object `+0xe8` at ROM `0x2bfa4`. Value zero reaches
the call to `0x2cce0` at `0x2bfe8`; value one reaches a different integration
routine, `0x2e650`, through `0x2c018`. That second routine also calls the
shared velocity helper `0x30e00` and this query leaf. These are control-flow
facts, not an established player/AI or driving-mode interpretation.

The word loaded from `0x214354` is a **descriptor selector**, not a pointer.
The original indexes 12-byte records at live `0x5dcac0`, corresponding to ROM
`0x3dac0`. The independently checked descriptor window contains eight records,
96 bytes, SHA-256
`c6b8b8d66bbd1a50a7d9e44a71363fe2a6ed31fddb69d4522b23583eb22d0eb9`.
This proves the inspected window's layout; it does not establish every possible
writer or lifetime domain of the selector global. Original table bytes remain
private.

| Record byte offset | Established use |
| --- | --- |
| `+0` | Coprocessor-data **word** offset. TGP command 7 reads one word there and returns the count used to bound the signed segment index. |
| `+4` | Base passed to TGP command `0x52` together with the scaled neighborhood. The firmware treats it as a coprocessor-data word address. Full subordinate contact-record semantics remain unrecovered. |
| `+8` | Main-data header pointer used separately by ROM `0x2b0b0`. Header word 0 agrees with the TGP count for every inspected descriptor. This field is not read by `0x2abc0`. |

The actual uploaded TGP firmware was exercised with a bounded host-only probe,
not replaced by an assumed response. Each of the eight command-7 queries made
exactly one coprocessor-data word read and returned the CPU-side header count.
Selectors 0 through 4 returned 300; selectors 5 through 7 returned 10.
Each query completed 27 firmware instructions, 216 in total, result fingerprint
`4669617b8d3deeef`. These are executed reference instructions, not hardware
cycles or throughput measurements. This result establishes the count relationship;
it does not validate command `0x52` intersection arithmetic.

## Query contract and native boundary

The original receives the selector, the signed halfword from object `+0x88`,
a pointer to three point words, and a fourth integer argument. The integrator
passes zero for that fourth argument at this call site. Its wider meaning and
the coordinate frame/units of the three point words are not established here.

The routine always obtains the count first. A negative index or an index greater
than or equal to the count skips the neighborhood query. Otherwise it constructs
the ordered previous, current and next indices, wrapping each adjacent index
once at the count boundary. This wrap is directly present in the original;
it does not by itself prove a physical track is a closed loop.

The private oracle also checks the surrounding preparation: the original first
stages the selected count address, then stages the three point words unchanged,
the second descriptor word, count and neighborhood indices scaled by four, and
the fourth argument shifted left once. It checks the two command writes and the
staging-cursor change: four bytes for a rejected index, forty for an accepted
query. The routine's original return register is **not** a boolean query result.
The native boolean instead means that its explicit typed neighborhood is valid.

`contact_neighborhood(count, signed_index, output)` supports counts **1..65,535**
and every `int16_t` input. Counts outside that policy, negative indices, and
indices outside the count return false without modifying output. This count
limit is an explicit native asset policy, not a claim that the original rejects
all larger counts. The three output indices are `uint16_t`: for index zero and
a large count, the previous index may exceed `INT16_MAX`. Count one returns the
same index three times, and count two legitimately repeats the other neighbor.

The leaf does not reproduce command transport. A future native contact system
must supply a validated asset count, transform the real query points correctly,
perform the recovered search/intersection, and produce the downstream contact
record with independently verified arithmetic. Those steps are still pending.

## Validation

- ROM-free tests perform **1,179,642 checks**, fingerprint
  `219618ade1210792`: all 65,536 signed-halfword encodings at twelve selected
  counts, plus boundary/current samples at every supported count. They cover
  zero, unsupported counts, both wrap sites and unchanged rejection outputs.
  AddressSanitizer and UndefinedBehaviorSanitizer pass the same checks.
- The private original-i960 comparison performs **524,288 cases**, covering
  every signed-halfword input at counts 0, 1, 2, 10, 300, 32,767, 32,768 and
  65,535. It observes 98,616 accepted and 425,672 rejected cases, executing
  10,529,461 reference instructions, 13..51 per case before the return.
  Fingerprint: `874c2bf9c8bbc6d6`. Descriptor selection cycles across the eight
  records; point bits and the fourth argument vary. This count response is an
  explicit fixture input, separately supported for actual assets by the TGP probe.
- The same exhaustive native test passed as an EE ELF under Play! 0.72 with
  fingerprint `219618ade1210792`. ELF SHA-256:
  `c8dc243bdac15cf98b3287ea20af63cb7e5703abe9a5f0aedf595154f0cd0e48`.
  The first bounded 12.077-second run produced no guest result; the longer
  45.199-second observation obtained PASS with empty guest stderr. These are
  emulator observation durations, not measured execution time or physical-PS2
  performance evidence.
- A completed private driving capture then supplies **32 complete query pairs**
  from sixteen sampled frames between 2,220 and 2,760, spanning objects
  `0x500100` and `0x500200`. All return through `0x5cd898`, the alternate
  integration routine. All 32 native neighborhoods match the captured result,
  fingerprint `f04c0719a03c245f`: selector 0, count 300, observed signed indices
  12..39, no rejected samples. The observer-enabled and observer-disabled host
  runs match all 2,850 frame checkpoints. This establishes those actual caller
  samples, not every car/course/mode or an independent PCB result. The earlier
  observer looking only for `0x2cce0` collected no entries; broadening the
  capture to the actually reached alternate routine corrected that scope error.

Private reports are under `build/ps2-native-contact-001`,
`build/ps2-native-contact-count-final-001`, and the `native-contact-ee-002` emulator
logs. The bound live-query report is
`build/ps2-native-contact-live-001/result.json`, using validated capture
`build/ps2/driving-capture-004`. No original table or captured game data is committed.

Reproduce the original-i960 comparison with:

```sh
python3 platform/ps2/tools/check_contact_neighborhood.py \
  --program /path/to/owned/program.bin \
  --output build/contact-index-check
```

The separate count probe verifies exact private input identities and regenerates
its firmware reference beneath the new ignored output directory:

```sh
python3 platform/ps2/tools/check_contact_count.py \
  --program /path/to/owned/program.bin \
  --tgp-program /path/to/owned/tgp/program.bin \
  --tgp-tables /path/to/owned/copro_tgp_tables.bin \
  --copro-data /path/to/owned/copro_data.bin \
  --main-data /path/to/owned/main_data.bin \
  --output build/contact-count-check
```

`check_driving_contact.py` compares completed, observer-parity-validated private
driving captures. It requires every sampled query's pre/count/post sequence,
checks its object/index/selector identity and staging extent, and compares the
captured register result with this native leaf. Incomplete captures are refused.
The observed live result above does not establish the complete caller domain,
intersection solver, race integration, or physical console. The native leaf
remains an isolated recovered dependency; it is not yet connected to a native
driving loop.
