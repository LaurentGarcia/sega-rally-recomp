# Native body and collision-course presentation

`RallyBodyScene` accepts externally advanced native position/orientation and
prepares an ordinary engine `Frame`. It neither advances a vehicle nor reads
captured motion. `StudyScene`, `native_mesh.h` and their reference-view matrices
are absent from this runtime path. `WorldScene` supplies clipping, projection,
owned frame storage and native depth-test presentation.

Load a private `RLYBODY` asset with an explicit fixed `model_to_car` affine
transform. The body owns copied vertices/faces and preallocates two render banks.
`attach_course(assets.course(), error)` optionally copies the immutable collision
polygons in unchanged world coordinates. Attribute bit 2 selects four active
corners and two diagnostic triangles; otherwise three active corners produce
one triangle. The unused fourth corner of a triangular record is neither
validated nor rendered. Nonempty course segments form optional engine culling
groups; they retain the original face order.
The course is submitted with an identity world transform and the body with its
supplied pose. Updates allocate no memory; there is no fitting or recentering of
the body or course. A caller may deliberately change its camera, but the adapter
does not do so. Failed loads, course attachments and updates preserve the last
valid model/frame. Calls and frame submission must be serialized.

`update(pose, view, error)` obtains the middle/first/third orientation basis from
the already validated `prepare_contact_points` operation, supplies the native
position as translation, and composes that with the fixed model-to-car bridge.
The final render transform uses binary64 CPU scratch arithmetic before narrowing
to the engine matrix. This is a presentation path, not an additional gameplay
rounding claim, original camera reconstruction or VU1 transform workload.

The raw body model's relationship to the contact frame is not yet independently
proved. Its first exported object has local bounds approximately X ±0.985,
Y −0.068..1.15 and Z −1.900..1.912, compatible with a car-sized local model, but
those bounds do not establish axis semantics. An identity bridge is explicitly
diagnostic until the original body draw's pose/matrix provenance is verified.
The known captured object-to-reference-view matrix is never silently reused as
object-to-world. Separate wheel assembly, materials, textures and the original
camera remain outside this body-only presentation milestone.

The bounded body format has a 64-byte header followed by XYZ binary32 vertices
and indexed opaque flat faces. The header checks version, lengths, strides,
reserved bytes and payload CRC32. It contains no matrices, poses, game addresses
or command streams. Limits are 65,536 vertices and 16,384 body faces. With a
course attached, totals must remain within 65,536 vertices and 32,768 source
faces. Output storage has at most 16,384 triangles; clipping overflow rejects the
whole frame rather than dropping geometry. Coordinates and supplied render
matrices obey the engine's finite normal-or-zero ±65,536 domain. Loading stages
at most 1,048,640 file bytes before bounded typed allocation. Replacing a live
model temporarily retains both old and new storage; `persistent_bytes()` reports
the retained CPU allocation size, not a peak or a performance measurement.

The offline exporter re-verifies the owner's polygon ROM and captured raw
object records using the existing source-geometry exporter, then writes only
the selected object's local vertices and faces. Every captured reference-view
matrix is discarded. Private result `build/ps2-rally-body-001/result.json`
binds object 1 from frame 1634 to `body.bin`: 1,122 vertices, 880 faces and
27,608 bytes. This is game-derived asset data and stays under ignored `build/`.
The fixed flat colors are diagnostic, not recovered arcade materials.

The two portable tests cover format isolation, resource ownership, malformed
headers/geometry, allocation failures, no allocation in updates, and a fixed
camera translation control. Moving an original synthetic triangle by +0.25 X
moves it 62 logical pixels while an attached collision quad remains stationary.
Changing only the captured matrix in a synthetic exporter input leaves the body
asset byte-identical. The C++ fixture passes 91 checks, including an unused NaN
fourth corner accepted for a triangle and rejected when the quad flag is set.
The private body is
additionally loaded and transformed under ASan/UBSan with a fixed diagnostic
camera and synthetic poses: 845 surviving triangles, 1,295,112 retained host
bytes, and the expected 15.5-pixel translation at quarter-scale projection.
Evidence is bound in `build/ps2-rally-body-sanitized-003/result.json`.

Attaching the actual course-0 collision data also passes ASan/UBSan. Together,
body and course contain 12,910 vertices and 6,644 source faces; the deliberately
fixed test view emits 845 triangles. Retained host render storage is 3,948,472
bytes, plus 290,183 bytes for the separately owned driving assets. The source
and private asset identities are in
`build/ps2-rally-body-sanitized-003/course-result.json`. Of 300 course groups,
281 are conservatively rejected in this view, leaving 3,744 transformed corners
including the body. These host sizes are not
PS2 allocator measurements, and the synthetic pose/view is not a playable
driving demonstration or physical-console validation.

The earlier `sanitized-001` reports are historical: the adapter incorrectly
treated every collision record as a quad. Course 0 contains 260 triangular
records, whose unused fourth corner often contains zero. Rendering that corner
created extra faces toward the world origin. The old 13,170-vertex/6,904-face
figures and 1,106-triangle image therefore describe the faulty adapter, despite
passing memory-safety checks. The active-corner regression and corrected
`sanitized-003` evidence supersede those geometry counts.

```cpp
RallyBodyScene body;
RallyBodyError error;
// bridge is an explicit, caller-owned diagnostic or separately proven mapping.
if (!body.load("host:body.bin", bridge, error)) { /* show error */ }
if (!body.attach_course(assets.course(), error)) { /* show error */ }
// Each tick, pose must come from the actual native vehicle update.
if (body.update({position, orientation}, view, error)) display.present(body.frame());
```

```sh
python3 tests/test_ps2_rally_body_scene.py
python3 platform/ps2/tools/export_rally_body.py \
  --capture build/ps2/native-mesh-capture-final-001 \
  --polygons build/ps2/native/video/images/polygons.bin \
  --object 1 --output build/ps2-rally-body-export
```
