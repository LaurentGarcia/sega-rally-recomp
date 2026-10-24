# Native geometry reference

`clip_project_triangle` is an original platform operation that turns one
homogeneous triangle into ordered triangles for the native PS2 display API.
It is a correctness reference, currently outside the active drawing loop.
Its coordinate convention is explicit; it does not establish Sega Rally's
camera, projection, clipping rules or arithmetic.

## Contract and storage

The input vertex contains `x, y, z, w, u, v`, with raw texel UV coordinates.
All components must be normal binary32 values or signed zero, bounded in
magnitude by 65,536. Encoding checks reject subnormals, infinities and NaNs
before arithmetic. The material pointer must be present; the display retains
its existing texture/material validation and lifetime requirements.

Clipping applies these planes in order: `w >= 1/1024`, `x >= -w`, `x <= w`,
`y >= -w`, `y <= w`, `z >= 0`, `z <= w`. The positive-w guard bounds division;
it is not a recovered game near distance. Raw UVs are interpolated with the
homogeneous edge parameter before calculating `q = 1/w`. Projection maps
y-up clip coordinates into the existing y-down 496 by 384 logical viewport,
without a half-texel offset.

Seven half-planes can grow a convex triangle to at most ten vertices, or eight
fan triangles. Two fixed polygon buffers and local output staging require no
allocation. Material identity and fan order are preserved; the utility performs
no sorting or backface culling and omits exactly zero projected-area fans.
Every failure leaves caller output unchanged. Capacity is checked against the
actual surviving fan count, which is returned on capacity failure.

Intersections snap their active coordinate to the selected plane. A bounded
binary64 roundoff repair applies only after strict clipping; it does not
classify outside input vertices as inside. Larger drift and nonfinite or
subnormal final output fail before publication.

## Arithmetic and validation

Intersections and projection use binary64 scratch arithmetic. EE disassembly
confirms compiler software helpers for addition, subtraction, multiplication,
division, comparisons and conversion. The clipper's own stack frame is
2,304 bytes, excluding callees. Its performance is unmeasured: this is not
an optimized VU kernel and is not enabled in the native diagnostic loop.

Run the portable test with:

```sh
python3 tests/test_ps2_native_geometry.py
```

Host and sanitizer tests pass 280,831 checks, including 20,000 randomized
triangles checked against an independent exact rational construction in
barycentric coordinates. This oracle enumerates half-plane intersections
instead of repeating the production clipping algorithm. Its random inputs
are dyadic values in `[-8, 8]`; it does not exhaust the full API domain.
The tests observed up to six output triangles; the capacity of eight follows
the convex clipping bound, not that observation.

A standalone EE fixture passed 322 selected exact, boundary, degeneracy and
failure-atomicity checks in Play 0.72. Its result fingerprint matches the host:
`fe8dc06b88de7a24`. ELF SHA-256:
`b74b7e906f5d7c45377795f259a46b380b28c9160ade6d43de3658c1fbbd171b`.
Private evidence is `build/ps2/validation/native-geometry-001.json`.
Physical PS2 execution, throughput and recovered game projection remain unproven.

## VU batch implications

The proposed VU1 bank holds 96 output vertices, or 32 independent triangles.
That is an output limit. Eight output fans per input triangle allow at most
four input triangles under worst-case expansion within that same bank.
Alternatively, clipping can precede submission, with at most 32 already
clipped triangles packed into each batch. Preserve primitive order in either
case. A future fast path must compare its numerical error and measured cost
against this reference before integration; readback, transfer and fence costs
belong in that measurement.
