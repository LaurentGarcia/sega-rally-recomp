# Main candidate contact pass

This dependency study follows the governing design's **Platform layer & build**
native rewrite exception. Individual typed dependencies now have separate
implementations and proofs linked below; the complete pass is not recovered
or connected to gameplay. Original
instructions, local point data and private inspection output stay under
`build/ps2-native-player-research-001/`.

## Identity and caller

The verified Revision B program identity is recorded in
[cabinet steering](cabinet_steering.md). The dependency called from the
[main callback](main_vehicle_recovery.md) at ROM `0x35218` occupies
`[0x34380,0x34cd8)`: 2,392 bytes, 496 linearly decoded instruction sites,
SHA-256 `1d5502a695aedaa92bbd6380ffcafb2a32fd0f54896079f9c60b4f08e4b7ea9e`.
Its return is at `0x34cd4`; padding precedes the next routine at `0x34ce0`.
The span contains 44 static FIFO write sites and 16 read sites, excluding
callees. These are instruction sites, not per-frame transaction counts.

The caller constructs candidate position and orientation triples with separate
binary32 additions immediately before this call. Its three arguments are the
main object, mutable candidate position and mutable candidate orientation.
It resumes at ROM `0x3521c` and consumes those candidate triples. The pass
returns a scalar in its first result register; its physical units and broader
caller domain have not been established.

Four fixed local triples occupy ROM `[0x33e30,0x33e60)`, 48 bytes,
SHA-256 `1f2b37b346c38c1a535c6aa8dd85af25d5897483480148ce5ce424b423aeda7a`.
Their array index is a contact-point index in this study. Calling these slots
wheels, assigning physical dimensions, or assuming a generic suspension model
would go beyond the evidence.

## Direct state contract

The following sets are established by the decoded routine, including its
explicit shared-query arguments. Offset names indicate storage, not units.

| Input or output | Established access |
| --- | --- |
| Candidate position triple | All three words read; first and third words can be replaced after response aggregation. |
| Candidate orientation triple | All three words read; middle word can be replaced after response aggregation. |
| Object `+0x88` | Signed 16-bit segment index read for every point's neighborhood query. |
| Object `+0x14`, `+0x1c`, `+0x3c` | Read to reconstruct candidate first/third position and middle orientation components after a response. |
| Object `+0x2c`, `+0x34`, `+0x48` | Read and conditionally updated by response aggregation. |
| Object `+0x50`, `+0x54`, `+0x58` | Read as response arithmetic parameters/divisors; a safe numerical domain remains to be measured. |
| Global `0x214354` | Course descriptor selector read once and retained for the four queries. |
| Global `0x2142c8` | Read to decide whether the two final feedback integers are stored. |
| Four result slots at `0x2138d0 + 36*i`, `i=0..3` | Each is passed to the existing `0x2acc0` result adapter. That helper preserves its first three words and writes the next five; the ninth word is spacing, not part of that helper's output. |
| Global `0x213960` | Cleared, then accumulates one distinct index bit for each reply whose interpreted flag byte has bit 7 set. |
| Globals `0x213964`, `0x213968` | Conditionally written from scaled, truncated response arithmetic. |

The query-preparation helper also changes its existing staging cursor and
geometry command state. The full side-effect contract therefore includes those
callees; the table above must not be mistaken for an exhaustive bus trace.
The normal calling convention supplies a zero-valued scratch register used
throughout the routine. A future typed implementation should express zeros and
external values directly, rather than expose registers or mapped addresses.

## Work performed and unresolved dependencies

The pass installs the four local triples in geometry state, prepares the
orientation transform, transforms each indexed triple and retrieves it. It adds
the resulting offset to candidate position using separate binary32 additions,
producing four query points. It restores the surrounding matrix state before
course queries. The explicit matrix/point arithmetic is now recovered by
[rounded affine transformation](rounded_affine_transform.md), including its
translation-first operation order. Matrix construction, surrounding state and
table-dependent rotation rounding still need their own differential proof.

The first `0x2abc0` query is issued at ROM `0x346d8`. The loop retrieves the
current point's interpreted reply through `0x2acc0` at `0x3470c`, and issues the
next point's preparation at `0x34724` until all four are handled. Each uses the
same descriptor selector, signed segment index and result slot zero. The
recovered [native search](collision_query_recovery.md) supplies the raw selected
polygon value. The bounded [contact-result adapter](contact_result.md) now
recovers the `0x2acc0` interpretation, including its immutable-table normal
calculation. Its final native/original/live comparison matches all five output
words for 104 sampled main replies and 34 alternate replies. These components
remain disconnected from the native application; they do not implement the
following per-contact response or complete main pass.

For replies whose interpreted bit 7 is clear, the local response triple is
zeroed. The other path performs cross-style vector arithmetic, a normalization
with a small-length fallback, further square roots, divisions and mixed
binary32/binary64 expressions using the reply direction/scalar and object
parameters. The aggregate path runs only when the selected scalar accumulator
is nonzero. It changes the object and candidate fields listed above and can
produce feedback integers. None of those numerical/exceptional domains is
established by the bounded polygon-search proof.

Useful review boundaries, which are regions rather than standalone functions:

| Region | ROM interval | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Point preparation and first query setup | `[0x34380,0x346d8)` | 856 | `4298396bc5e5ca77d1b9c95485dabaaec2534c5adca65ac2f11a8a6fb117e776` |
| Per-contact response | `[0x34728,0x34a30)` | 776 | `c9a1bab4ddc45d7701dd07c632abd980172de75e639ca1dda58be55df622f5c5` |
| Aggregate response | `[0x34a54,0x34cc0)` | 620 | `6cd9e1e5b3e2e56e3244c8917c87c37c69656a579efee581063b21207dd41c0f` |

## Geometry primitive boundaries

These command entries refer to the verified 6,660-byte geometry firmware
identified in [query recovery](collision_query_recovery.md). Intervals below
are word addresses; sizes and hashes describe their instruction bytes, without
embedding those bytes. Entries reached by calls are additional dependencies.

| Command | Word interval | Bytes | Established role / dependency | SHA-256 |
| --- | --- | ---: | --- | --- |
| `0x42` | `[0x404,0x40c)` | 32 | Store an indexed triple. | `aa5d74fbff1a4e5f5899b418540a307c81486a7c605023c5e08deeee5b37e6b4` |
| `0x43` | `[0x40c,0x414)` | 32 | Retrieve an indexed triple. | `64be87b41bd17c0a26706e1aa2b94ee064b0eb7fc84f4a81fc28027addf43ca2` |
| `0x48` | `[0x44c,0x46c)` | 128 | Transform an indexed triple with current matrix state. | `229d95dd4c345dc0a0eda94ba6988aa858b20b18bb99143cad0753e93626b273` |
| `0x58` | `[0x51c,0x530)` | 80 | Six scalar inputs and one result; calls the numeric primitive at `0x1ab`, using firmware-initialized constants rather than an external lookup table. | `3ae2c280643488b6e1a161908b726c7e1bf4ec72d05d5bdfe2317a95b25a1bc2` |
| `0x5b` | `[0x55f,0x577)` | 96 | Six scalar inputs and three separately rounded product-difference results. | `fa2fb8f0f29928fda96ab2c57bed7bf65048cc86215ead05afc63d6b5680004f` |

Command `0x5b` is now recovered as a bounded
[rounded cross product](rounded_cross_product.md). Mapping its
six transport words to two explicit triples yields the ordinary cross-product
component expressions, with **each multiplication and each subtraction rounded
separately to binary32**. The first component is
`R32(R32(a.y*b.z) - R32(a.z*b.y))`; the other components use the corresponding
cyclic coordinates. It uses neither the course tables nor a square-root table.
This pass supplies zero middle components and uses the resulting signed middle
component during accumulation. `rounded_cross_product(a,b,out)` exposes typed
values without the unusual FIFO input ordering. Its documented finite domain,
zero signs, alias/rejection policy, original-firmware differential and
standalone EE execution are established separately. Recovering that operation
does not establish the surrounding response aggregation.

By contrast, substituting a generic host square root or a generic physics
response for the larger pass would skip known dependencies. The separate
[main-pass observer](../tools/driving_capture.md#main-candidate-contact-pass)
now captures ordinary RAM at entry/return, all four transformed offsets and
query points after ROM `0x346d4`, the four interpreted result slots, response
vectors, feedback and the object fields above. Actual saved caller-frame proofs
pair entries/returns through interrupts without additional device/FIFO reads.

Private capture 009 passed all 2,850 enabled/disabled reference checkpoints.
It observed 601 complete main passes on object `0x500000`, each with four
query/adapter calls; 26 passes received detailed snapshots. All 36 aggregate
cross operations were captured across frames 2,571 through 2,579. That bounded
window was chosen from capture 008's execution cadence, which had missed their
operands between its existing sample checkpoints. The final evidence is
`build/ps2/driving-capture-009/validation.json`, SHA-256
`d704254e2536fb19bca05a971aba7447224a9ba73bb25663a2e239e1ac7afd90`.
These raw inputs/outputs support separate typed-component comparisons. They
do not establish a complete native contact-pass implementation, the full
response arithmetic domain, or native driving.
