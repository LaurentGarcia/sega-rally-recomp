# Offline original model capture

These tools prepare private model data for the native geometry study. They run
only in the host reference. The PS2 native executable reads ordinary mesh records
and does not link these tools, the geometry reference, or an arcade processor.

`RALLY_OFFLINE_MESH_CAPTURE` enables a read-only hook at `Geo::geo_object_data`,
after source selection and before transformation, focus, clipping or face culling.
The hook copies the source cursor. An explicitly selected frame records complete
object link streams and the current twelve matrix elements. Direct geometry
commands are counted separately and excluded because they do not establish an
original model-space mesh boundary. Capture failures abort the offline run;
partial directories have no completed `capture.json` marker.

The independently written decoder reads the six initial XYZ words and ten-word
link records. A link's low two attribute bits distinguish terminators, triangles
and quads. Connection bits select which preceding edge endpoints survive.
Hidden connectors update that edge without drawing. Quads become two triangles
with the reference's winding; repeated-index triangles are counted and skipped.
Normal-mode terminators consume four words; no-normal terminators consume one.
Normals, texture parameters, original shading and camera semantics are not
recovered by this tool. Format and matrix behavior can be checked against the
existing `src/runtime/geo.cpp` object parsers and polygon processing.

To use a separately prepared host reference, compile only its `geo.cpp` and game
`main.cpp` with the macro, and add `mesh_capture.cpp` and `mesh_decode.cpp` to that
host target. Keep normal target definitions identical. Set
`RALLY_PS2_MESH_DIRECTORY` to a new directory under ignored `build/` and optionally
`RALLY_PS2_MESH_FRAME` (default1634). The source path of this helper must be
available when it runs so it can resolve the repository's private build root.

```sh
RALLY_PS2_MESH_DIRECTORY=build/ps2/private-model-capture \
RALLY_PS2_MESH_FRAME=1634 \
build/ps2/host/build/core-probe build/ps2/assets/game.rpk 1634

python3 platform/ps2/tools/export_native_mesh.py \
  --capture build/ps2/private-model-capture \
  --polygons build/ps2/native/video/images/polygons.bin \
  --objects 1,2,3,4,5,6 \
  --output build/ps2/private-native-mesh/selected.bin
```

Object ordinals identify that capture's command order, not durable model names.
Select and inspect bounds and transforms before choosing a subset. The exporter
re-derives every object's coordinates and connectivity from captured source
words, checks each ROM-backed stream against the verified local polygon region,
then writes the bounded format documented in `include/native_mesh.h`. Captured
polygon RAM is explicitly identified as RAM provenance. The selected mesh keeps
untransformed XYZ and each captured object-to-reference-view matrix separately.
It uses an original opaque diagnostic material; it does not claim original
textures, light, camera, animation or gameplay. Unsupported coordinate ranges or
capacities fail explicitly. Empty objects are reported, never fabricated.

All raw captures, native meshes, previews and detailed provenance remain below
ignored `build/`. The tracked tests use original synthetic geometry only:
`python3 tests/test_ps2_mesh_export.py`. For a real capture, compare every host
checkpoint against a capture-disabled baseline and retain the hashes privately.
