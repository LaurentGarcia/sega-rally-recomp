# Native textured models

The engine's `TexturedModel` owns ordinary model-local XYZ vertices, indexed
triangles with per-corner texel UV, materials, and packed PSMT4 textures. It has
no board runtime dependency, source addresses, camera matrices, or executable
instructions. `RallyVisualScene` is the separate game adapter that assigns
course, shadow, body and wheel roles and generates their transforms from the
native pose and recovered chase-camera state.

## Ownership and bounds

`decode_textured_model` and `load_textured_model` replace a complete owner only
after validation succeeds. Failed loads preserve the previous view. Borrowed
`WorldModel` views and frames must be retired before replacing their owner;
`PreparedWorldModel` copies referenced materials and texture data into its own
snapshot. The loader assigns texture generations in the low half of the
64-bit namespace; prepared snapshots reserve the high half. External asset
owners still obey `Display::invalidate_textures()` when releasing or reusing
texture storage.

Limits are 65,536 vertices, 32,768 faces, 4,096 materials, 1,024 textures and
4 MiB of packed indices. Position, UV and authored facing components must be zero or normal
binary32 values with magnitude at most 65,536. Texture dimensions are powers
of two from 8 through 256. A caller must still budget the simultaneous file,
decoded model and prepared-scene allocations. A valid maximum-size file is
not a promise that every application has enough free EE memory to load it.

## RLYTEXM2 wire format

All integer and binary32 words are little endian. The 64-byte header contains
magic `RLYTEXM2`, then fourteen u32 fields: version 2, header size 64, vertex
count, face count, material count, texture count, packed-index byte count,
IEEE CRC32 of the complete payload, payload byte count, vertex stride 12,
face stride 68, material stride 76, texture stride 16, and reserved zero.
The payload contains the following arrays in order:

| Record | Fields |
| --- | --- |
| Vertex, 12 bytes | Three binary32 XYZ components |
| Face, 68 bytes | Three vertex indices, three binary32 UV pairs, material index, authored normal XYZ, authored point XYZ, facing enabled (0 or 1) |
| Material, 76 bytes | Texture index (`0xffffffff` for solid), opaque ARGB color, flags, sixteen ARGB palette words |
| Texture, 16 bytes | Width, height, offset in packed indices, byte count |
| Packed indices | Linear PSMT4, left pixel in the low nibble |

Version 1 (`RLYTEXM1`, version 1, face stride 40) remains readable and has
no authored facing data. Version 2 preserves the source normal and a point
on the original polygon for both fan triangles. The caller must explicitly
enable `WorldView::use_facing`; the engine transforms the normal without
translation and the point with translation, then retains a nonnegative dot
product relative to the supplied facing origin. Two-sided faces disable this
test. Existing callers remain unchanged when the view opt-in is false.

Material flag bit 0 enables transparent-index cutout; bit 1 enables the native
screen-space checker mask. Other bits are rejected. Palette alpha is zero or
255. Texture ranges must be contiguous, ordered, nonoverlapping and exactly
`width * height / 2` bytes. Counts, strides, CRC, indices, encodings and exact
file length are checked before publication; trailing data is rejected.

## Private conversion and original-execution evidence

The host-only `capture_visual_assets.py` runner uses the owner's validated
Revision B data. Its bounded visual observer records raw model geometry and
per-link material state **before visibility and back-face rejection**, then
captures the texture, palette and shading memories. CPU camera/draw callbacks
are observed independently using ordinary RAM/register state and saved caller
proofs. No additional device or FIFO reads are introduced. The script executes
the same input replay with observation enabled and disabled and requires an
exact checkpoint match for every requested frame.

For example, from the repository root with the existing private imported data:

```sh
python3 platform/ps2/tools/capture_visual_assets.py \
  --output build/visual-capture-example --first 2399 --last 2403
```

`--reuse-host` can reuse a prior capture build only when its confined CMake
source inventory matches exactly. No original data or generated source is
exported to the repository. The capture output, all intermediate records and
converted `.bin` files stay under ignored `build/`.

`export_textured_model.py` validates a passed capture marker and hashes, then
independently checks raw polygon-source topology against the owner's polygon
image. A single source model may be exported without a spatial binding, but
is explicitly labeled unbound in its private sidecar. Combining objects
requires a private, hash-bound spatial proof identifying world, car-local or
wheel-local coordinates. Proven transforms are baked once offline; matrices
and source addresses appear only in the private provenance, never in the
runtime model file.

The first integrated bundle uses nine original world-space road objects, a
shadow, the body plus its same-transform attachment, and one wheel model
shared by four independently transformed instances. It excludes the captured
opponent cars, unknown procedural effects and HUD. Its background is the
observed uniform ARGB `0xff2a5cc8`. Observation frame 2401, geometry frame 2402
and the actual game counter 2318 are distinct coordinates; the runtime config
must match the checkpoint's **game counter**, and its first pose guard must
match the first native update output.

This is a bounded moving scene, not a complete recovered visual race. Material
lighting and translated CLUTs remain fixed at conversion time. Nearest
base-level sampling does not reproduce the original filtered-index/mipmap
pipeline. Authored single-sided facing is evaluated for the moving instance and camera,
without inferring normals from triangle winding or flipping UVs. The exporter
currently supports supplied-normal source modes and rejects unproven
computed-normal single-sided modes. Native depth presentation is not a claim
of original polygon-priority equivalence. Additional scenery, lighting, effects, HUD and
streaming require their own recovery and validation.

## Validation

`tests/test_ps2_textured_model.py` exercises the real C++ loader with authored
assets, malformed headers/counts/checksums/indices/materials/floats/textures,
truncation, trailing bytes and nonempty replacement atomicity. Its independent
synthetic sheet checks packed nibble order, mirrored periods and palette
translation. Facing regressions preserve opposite authored normals without
changing corner UVs, check vector versus point translation, and verify v1/v2
replacement and malformed-facing rollback. The same C++ fixture accepts a private converted asset path for
bounded loader verification; the sanitizer run covers rejection and an actual
textured body asset. Source-derived images and texture bytes are never test
fixtures in Git.
