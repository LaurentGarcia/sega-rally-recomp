# Explicit world presentation

The reusable engine's `WorldScene` (`include/world_scene.h`,
`src/world_scene.cpp`) is the presentation bridge for a persistent native vehicle update.
It receives model-local points, indexed opaque flat faces, an explicit
`object_to_world` matrix per instance, and a fixed `world_to_clip` matrix.
The existing `StudyScene` and its captured reference-view transforms are not
used or changed. There is no automatic fitting, recentering, inferred camera,
recorded movement or game-state advancement.

Both matrix types use column vectors and column-major storage. Object matrices
must be affine. The view may be projective and follows `native_geometry`'s
y-up clip coordinates, positive-w guard, `0 <= z <= w` depth convention and
496×384 logical viewport. The adapter uses the existing homogeneous clipping
and projection operation, then sends an ordinary `Frame` with native reverse
depth enabled. It does not reconstruct original lighting, textures, polygon
priority or game camera behavior.

`configure(source_faces, output_triangles, error)` allocates two bounded banks
of materials and triangles. Limits are 32,768 source faces, 16,384 output
triangles, 1,024 instances and 65,536 declared vertices across all instances.
`update()` allocates nothing. The inactive bank is prepared before committing;
failure preserves the last complete frame. No clipping overflow is truncated.
Caller model storage and transforms need only survive `update()`. The returned
frame refers to owned storage and expires at the next successful update or
reconfiguration, or owner destruction. Calls must be serialized with display
submission. `stats().persistent_bytes` reports actual bank/object storage.

A model may provide `WorldFaceGroup` ranges that partition its faces in order.
Each range must be nonempty and contiguous, with every face covered exactly
once. The engine derives a bounding box from validated active vertices each
update; it never trusts an external bounding box. Extrema are selected using
exact comparisons of validated binary32 normal-or-zero values; only the six
selected endpoints are widened to binary64. Sequential binary64 interval
transforms widen every multiply and add outward by one ULP. A group is skipped
only when its complete rounded clip bounds lie outside a common clip plane and
prove that no transformed component could violate the ordinary clip-input
domain. Bit-based endpoint classification preserves rejection of subnormals on
the EE. Uncertain bounds use the existing per-face path. Face/material order,
transformation order, clipping, output limits and frame commit behavior remain
unchanged. Counters report groups, rejected groups and transformed corners;
the latter counts triangle corners, not unique model vertices.

The transform currently uses binary64 CPU scratch arithmetic and the existing
reference clipper. This is an explicit correctness path, not a VU1 workload or
a throughput claim. Moving transformation work onto VU1 requires a separately
validated agreement with this contract.

The original synthetic fixture passes 50 checks. With an unchanged camera,
translating one instance by +0.25 on world X moves it exactly 62 logical pixels;
a second fixed instance does not move. An asymmetric affine/view case has an
independently calculated first vertex at (240.25,198), reverse depth 0.625.
The fixture also checks no update allocations, five initialization allocation
failures, malformed models/transforms, output overflow, clipped-away geometry
and preserved frames. The portable runner is `tests/test_ps2_world_scene.py`.
The same original fixture passes ASan/UBSan. These are host geometry/ownership
tests, not a new console execution or recovered driving demonstration.

The separate culling fixture compares the grouped path with the unchanged
ungrouped path across 2,052 original synthetic frames. All 60,534 checks pass,
including exact ordered output vertex/material bits, errors, malformed ranges,
subnormal and transform-limit cases, and no update allocations. It rejects
146,843 groups and transforms 299,742 corners versus 1,180,800 without grouping.
ASan/UBSan and source identities are recorded in
`build/ps2-world-groups-002/group-result.json`.

An additional private comparison uses the owner's actual course at 60 fixed
diagnostic views. Both paths produce the same 2,512 output triangles, hash
`95c6d30c2f94d9c1`; 16,105 of 18,000 groups are rejected and transformed corners
fall from 1,037,520 to 113,772. Evidence is bound in
`build/ps2-world-groups-002/course-result.json`. These counts demonstrate work
removed and image agreement; they are not PS2 frame-rate measurements.

Real car integration still needs proven model-local assembly and its mapping
from persistent native position/orientation to world space. The current study
mesh records object-to-reference-view transforms. Multiplying those by a
moving world pose would silently combine incompatible spaces. Deriving relative
part transforms by removing a body transform also requires proof of the shared
view and treatment of dynamic wheel transforms. This adapter deliberately
accepts only explicit caller-supplied transforms until that provenance is ready.
