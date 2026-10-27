# Native VU1 geometry batches

`Vu1Transform` is an engine rendering service. It sends up to 96 model vertices
through separate object-to-world and world-to-clip matrices on VU1. The native
game update never calls it; its arithmetic and persistent state are unchanged.
VU0 remains responsible for final GS 12.4 XY conversion. Both units perform
actual work, with a synchronous boundary between their operations.

The native renderer calls `transform_native`: normal binary32 arithmetic on
VU1, followed by binary32 clipping/projection on the EE and VU0 GS-coordinate
packing. The separate `transform` and projection-guard APIs retain the
binary64 diagnostic comparison path. The native call does not calculate those
error bounds or invoke software-double helpers.

The caller owns an exclusive VIF1/VU1 lease. Initialization requires enabled,
unsuspended DMA, an idle forward VIF1 channel, no VIF1 interrupt handler, and no
other VU1 program. An initialized owner may initialize again; another owner is
rejected. Unsupported numerical input leaves caller output unchanged. A
transport fault retires the owner until process restart. Only DMA submitted by
this owner may be stopped; source storage is retained if that stop cannot be
established.

## Transport and microprogram

The independently written kernel loads eight matrix columns, then performs two
four-component multiply/add chains for each vertex. It writes clip coordinates
back to VU data memory. It performs no game arithmetic, perspective division,
XGKICK, texture upload or GS submission. Four-cycle register hazards have explicit
spacing; accumulator chains use their documented forwarding path.

VIF1 uploads matrices, count and input vertices into fixed owned data addresses.
Each submission ends with `FLUSHE`, an incremented `MARK`, alignment NOPs and one
additional NOP qword. The extra qword is valid hardware input and avoids the
previously isolated Play 0.72 final-prefetched-qword resume issue. The complete
DMA source remains alive and cache-published until completion. The EE reads VU
output only after the marker, DMA completion, and idle command/FIFO checks.
There is no in-flight buffer reuse, and no claim of simultaneous VU0/VU1 issue.

Packet cache publication uses the pinned SDK's normal DMA send with flags zero.
Inspection of the installed `libdma.a` confirms that it calls `SyncDCache` over
the complete `qwc * 16` source range before starting the channel. The backend
does not repeat that same flush. Source lifetime and completion checks are
unchanged; this removes redundant publication work without adding overlap.

The generic build driver assembles this `.vsm` with the pinned DVP assembler and
records its version, source hash, object hash and instruction listing. It does
not copy a proprietary SDK implementation.

## Presentation arithmetic contract

Matrices and inputs contain normal-or-zero binary32 values with magnitude at
most 65,536. Object matrices are affine; input `w` is exactly one. For the native
path these limits keep the first-stage magnitude below `2^34` and the second
below `2^52`, safely inside the VU exponent range. Intermediate values may exceed
65,536; final clip coordinates must again be normal-or-zero and bounded by
65,536. Underflow flushing is part of native presentation arithmetic. Outside
vertices are accepted for the native float clipper. Numeric rejection preserves
all output and leaves the completed transport usable; it is distinct from a
transport fault.

The diagnostic path additionally rejects batches whose conservative intermediate
magnitudes exceed 65,536 and applies the following reference error contract.

VU arithmetic has different rounding and underflow behavior from the binary64
reference renderer. The error calculation explicitly permits a two-ulp local
arithmetic allowance, flush-to-zero, and reference narrowing to binary32. A
four-term dot product allows seven rounded operations, including the separately
bounded product and addition of a multiply/add instruction. With
`u = 2^-22`, the accumulated factor is `7u / (1-7u)`. Bounds propagate through
both matrices; signed-unit copy rows are exact apart from propagated input
error. Binary64 evaluation of the enclosure is inflated separately. The manual's
last-bit caveat makes target validation necessary: this allowance is not a
formal guarantee for every physical VU implementation or a bit-equality claim.

For clip result `c` and absolute enclosure `e`, the projection guard first
requires an interior margin at all seven clip planes, including positive `w`.
For a component `x`, its ratio error is bounded by
`(e_x*w + abs(x)*e_w) / (w*(w-e_w))`. The screen factors are 248 and 192 for
the 496×384 logical viewport. The configurable developer budget is 1/8 logical
pixel and `2/65535` of reverse depth. These describe continuous vertex
coordinates, not identical GS pixel coverage or guaranteed identical occlusion.

For exact unit `w`, `PreparedProjectionGuard` computes inward-rounded binary32
plane and budget thresholds once per batch. Vertex checks then use encoding and
integer comparisons. Endpoint checks retain the original expression order;
correction is limited to four steps before falling back to the general guard.
The cached error estimate bounds the entire accepted box. Other `w` values use
the original division formula. A separate adversarial review checked 100,000
plans and 4,800,000 boundary-neighbor cases: every prepared acceptance passed
the original guard and its cached estimate enclosed the scalar estimate.

WorldScene gathers at most 32 faces into each provider call, excluding culled
groups and deduplicating cached vertices. In reference mode, unsupported batches
and rejected vertices use the binary64 reference. A triangle requiring clipping
restores every approximate corner to the reference result, including cached
corners: endpoint bounds alone do not bound the intersection of a nearly
parallel clipped edge. Only wholly inside triangles retain VU coordinates.
Cache provenance and actual consumed-corner counters distinguish submitted
work from results ultimately used for drawing.

Native float mode instead accepts bounded outside vertices, uses float clipping
and float scalar fallback, and does not restore binary64 corners. Its float group
bounds widen every nontrivial operation outward; uncertain bounds disable group
culling. The mode has no universal pixel-identity guarantee relative to the
reference renderer. Game arithmetic is independent of either presentation mode.

## Executed evidence

The authored fixture runs the production kernel and real VU0 conversion after
each main batch. It checks arithmetic enclosures, plane/budget rejection,
input encodings, tiny normal underflow, aliasing, atomic failure and owner
exclusion. Host ASan/UBSan tests exercise the portable contract; they do not
establish VU execution.

| Play 0.72 HLE execution | Authored raw/reference fixture | Full native-scene corpus |
| --- | ---: | ---: |
| Main checked vertices | 15,168 | 1,364,846 |
| Successful hardware batches | 323 | 70,552 |
| Accepted projected vertices | 11,155 | 677,886 |
| Near-plane/reference guard rejections | 4,013 | 686,960 |
| Maximum accepted screen difference | 0.000842572 pixel | 0.002821506 pixel |
| Maximum accepted depth difference | 2.38419e-7 | 8.94070e-7 |

The full scene corpus comes from all 601 independently validated native vehicle
poses, observation frames 2200–2800, through the current native-float
RallyBodyScene bridge, developer camera and gathered WorldScene batches. Each
of the 35,276 batches executes through both native and diagnostic VU APIs:
all 1,364,846 result vectors match bit for bit. The 70,552 successful submissions
also exercise the 16-bit completion-marker wrap. Every result stays within the
numerical enclosure; no vertex exceeds the guard budget or input domain.
Near-plane counts describe diagnostic rejection, not native-mode work discarded.
These measurements compare transformed vertices; they are not a complete-frame
raster comparison.

The later authored raw/reference run completed 146,895 checks, including exact
raw-versus-diagnostic VU result equality, a supported wide intermediate, late
numeric rejection without output changes or lease loss, and in-place output.
The compiled native call path was also checked for absence of double helpers.
The numerical kernel is unchanged between these execution snapshots.

After removing the duplicate packet cache flush, the complete 601-pose corpus
was executed again with the current backend: 70,552 submissions and 1,364,846
native/diagnostic result pairs passed. The completed 22,119,772-byte output
stream is byte-identical to the previous execution, including every frame and
batch identity. This validates the revised cache-publication path without
changing the numerical evidence; it does not isolate a performance gain.

The completed raw VU output was then replayed through native float clipping for
all 601 scenes. Each current matrix, vertex batch and frame identity had to
match the sealed input byte for byte. Against the binary64 reference, an
independent host pixel-center reverse-depth raster found 305 differing color
pixels out of 114,468,864, at most four in one frame, and 14 coverage changes.
The native result contained 546,152 triangles versus 546,073 reference
triangles; 173 frames differed in triangle count. Corresponding vertices had
maximum coordinate/depth differences of 0.043061 logical pixel and 2.05637e-6.
The maximum depth difference at a covered raster sample was 0.027692; the
recorded worst pixel changes which surface wins at a rounded triangle edge.
This is actual VU output followed by host float clipping and a diagnostic
raster, not a GS framebuffer comparison or a pixel-equality guarantee. All 601
VU-derived frames differ in some geometry bits from the host-float transform
path, so that path is not substituted as proof of VU execution.

Private evidence:

- `build/ps2-vu1-engine-ee-004/runtime-validation.json`, ELF
  `d0ff8f830210fcef732aaa46f5b513ff9d8fdbb4debd4cf43d805f1ddb8c28f0`.
- `build/ps2-vu1-corpus-ee-004/runtime-validation.json`, ELF
  `cdc4b75b7753bda0bcb2ba808776240200e2edc4ea5c351a391d57f880181cf7`.
- `build/ps2-vu1-corpus-ee-005/runtime-validation.json`, ELF
  `ab3e4a8598990a48257ef9fa6aac84a4ee74e086a26b7dca62adf655cfa1f792`,
  validates the current cache-publication path;
  `build/ps2-vu1-native-corpus-002/output-comparison.json` binds its exact
  output identity to the preceding execution.
- `build/ps2-world-vu-float-002/corpus-result.json` binds the completed VU
  execution report and complete-frame replay inputs/results.
- Private full input SHA-256
  `1d1b01c158ddd1ca63bcdc4aa3335d9ce95b61138ec389371e6fa0176b53f4c3`.
- Private completed raw output SHA-256
  `ac3c4d3375531d2257c524fe5e4f0464251e6825b6fcaebc12fd9d6e2d7023c4`.

The corpus fixture can write a bounded private `RLYVUO1` result stream for
complete-frame replay. It contains each frame/batch identity and actual float4
outputs. A reader must require exact counts, the completion footer, and a sealed
successful execution report. A malformed or truncated input never reaches the
footer; an output flush/close failure also prevents the PASS report even if
footer bytes were written. Result bytes and all game-derived inputs remain
under ignored `build/`.

The reports bind executable, compiled sources, logs and capture provenance.
Observation-window durations are not kernel performance measurements.
Physical PS2 execution and comparative frame timing remain separate checks.
Hardware reasoning uses the locally authorized VU User's Manual sections 2.2–2.4
and pipeline chapters, plus the EE User's Manual DMA suspension/ownership rules.
The manuals and game-derived corpus remain private under ignored `build/`.
