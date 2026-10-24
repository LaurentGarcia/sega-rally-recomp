# Native PS2 static geometry study

This development path loads ordinary meshes into the native PS2 renderer.
It is an asset and rendering study, not a recovered race loop. Original game
instructions, processor state, arcade registers and device scheduling stay in
the separate host reference tool; none belongs to the native executable.

## Data provenance

The offline reference can observe object submissions before the geometrizer
transforms or culls their vertices. The converter retains object-local XYZ
and each submitted affine transform separately. Complete object links are
decoded, including faces that the reference view would have culled. Direct
geometry commands have a different provenance and must not be relabeled as
object-local meshes.

The transform is named `object_to_reference_view`. Its name describes the
observed geometrizer operation; it does not establish the game's world axes,
car orientation conventions or camera behavior. Flat colors in the study are
diagnostic colors, not recovered materials or lighting. Any orbit or fit camera
is also a diagnostic control, not a playable driving model.

All captured records, converted meshes and screenshots remain under ignored
`build/`. The `.bin` asset extension is rejected by the source-release gate.
The public repository contains the conversion and loading code, not game data.

## Native mesh contract

The version-one format starts with a 64-byte little-endian header, followed by
XYZ binary32 vertices, indexed opaque-color triangles and object records. Each
object owns contiguous vertex and face ranges and a column-major affine matrix.
Faces can reference only vertices belonging to that object. The payload has an
IEEE CRC32; the private conversion report separately identifies its inputs and
the complete output file by SHA-256.

Limits are 65,536 vertices, 32,768 faces and 1,024 objects. The maximum payload
is 1,392,640 bytes. The loader validates counts and exact lengths before typed
allocation, rejects malformed indices, ranges, matrices and float encodings,
and leaves its destination unchanged on failure. File loading stages at most
one bounded raw buffer plus the typed mesh; replacing an existing mesh retains
the previous allocation until validation succeeds. This is a loader budget,
not the complete application's peak memory.

The display has a separate limit of 16,384 output triangles. Clipping can
expand a face, so accepting a mesh file does not guarantee that every view fits
the output budget. Capacity failure must be reported without truncating the
scene. No frame-rate or full-game memory claim follows from loading a mesh.

## Rendering boundary

The scene path uses the native transform and clipping interfaces, then direct
GIF/GS submission. Its optional depth test uses PSMZ16S with the existing
PSMCT32 framebuffers. The pairing and depth-test state requirements are recorded
in the [hardware study](ps2-architecture-study.md). Native opaque depth ordering
is useful for this study; it does not reproduce the arcade's polygon priority,
transparency, texture filtering or full image composition.

Object identification, conversion completeness, projection agreement and
console behavior each need their own evidence. A recognizable car in this
viewer would establish a geometry milestone, not recovered physics, collision,
AI, course progression, game audio or playable Sega Rally on PS2.

## Build and run

Build the native executable using the commands in [native rewrite](ps2-native-rewrite.md).
No game data is needed to compile it. Prepare a private mesh with the
[offline capture workflow](../platform/ps2/tools/mesh_capture.md). That optional
host reference uses the owner's verified data; it is not linked into the native ELF.

Supply a loader argument such as `host:study.bin` or `mass:study.bin`. With no
argument, the application probes only `host:native-scene.bin`; if absent, it
runs the original checkerboard diagnostic. Invalid explicit files fail visibly.
The USB path initializes the native storage modules; a host path preserves the
loader's host service across input initialization.

The study starts with geometry visible and a two-line diagnostic caption.
Select + Start toggles the full information page, Left/Right step the orbit
and L1/R1 change distance. Cross retains the original quiet test tone. These
controls do not drive a car. A load or rendering error replaces the caption
and information page so that the failure remains readable.

## GS framebuffer diagnostics

The display's optional `readback_rect` reads a bounded region of the last
completed CT32 framebuffer through GS local-to-host transfer and VIF1 reverse
DMA. It requires exclusive idle transfer ownership, a payload divisible by
128 bytes and at most 64 KiB. It fences FINISH, maintains cache ownership and
returns raw GS RGBA words only after complete receipt. Timeouts are explicit;
a failed or unread reverse transfer quarantines the renderer until restart
rather than switching bus direction with unread data.

The original depth fixture checks small solid/textured overlap regions. A
separate scene fixture can write the owner's static study image in 640 by 16
strips to `host:native-study-frame.ppm`, refusing an existing output. This is
an image of GS framebuffer memory before TV filtering, not a capture of the
emulator window. Failed or partial captures must not be used as evidence.
Both fixtures are development diagnostics and contain no game assets.

```sh
python3 platform/ps2/tools/build_native_depth_fixture.py \
  --output build/ps2-depth-check
python3 platform/ps2/tools/build_native_depth_fixture.py --scene-capture \
  --output build/ps2-scene-capture
```

Each output directory must be new or empty beneath ignored `build/`. The helper
records the fixed source inventory, compiler, ELF and input hashes. CI compiles
both fixtures; it does not execute them or supply the private scene mesh.

## Recorded validation, 24 October 2026

The source-bound offline run matched every checkpoint in a capture-disabled
1,634-frame reference run. It recorded 56 objects (52 ROM-backed, four RAM-backed),
with 7,494 raw points and 5,816 source triangles. No direct or empty object
submissions were excluded in that capture. The exporter independently decoded
the raw link streams and checked ROM-backed bytes against the verified polygon
region. Object ordinals are local to this capture, not stable model identities.

A six-object car-shaped subset contains 1,302 vertices and 1,040 triangles in
32,808 bytes. Its SHA-256 is
`1def378046fcc070e7fd6e11b7a888b3b342df7eb9d019e50f5ad68eba547d73`.
The host preview shows a recognizable body and wheels; this does not identify
the exact car model or establish PS2 raster fidelity. One large background
object exceeds the native coordinate domain and is explicitly rejected by a
whole-capture export. This subset is not a complete course conversion.

The native-008 execution loaded this mesh and submitted all 1,040 triangles
in Play 0.72, with 1,324,224 bytes of persistent scene storage. Load segments
use another 592,048 bytes; those figures exclude the retained mesh and other
runtime allocations. Preparation took 52,295,680 emulated bus ticks. It happens
on load or camera changes, not on each repeated frame. Neither that count nor
repeated static-frame cadence measures physical PS2 gameplay performance.

The portable PS2 suite passed all 74 tests. Loader tests include independent
wire fixtures, malformed inputs, allocation failure and transactional replacement;
scene tests include independent projection expectations, clipping, transforms
and capacity failure. Sanitizer runs passed for the new loader, decoder and
scene paths. The original depth fixture passed 206,609 quantization checks on
both host and EE, with fingerprint `ce7160f9d89dd495`.

Initial window captures failed, so submission logs were kept separate from
pixel evidence. Later native-009 window capture succeeded and showed the car
through the native GS renderer. Native-010 binds the final sources to that
same ELF byte for byte. The screenshot remains private.

The original depth fixture then read 512 RGB samples directly from GS memory
with zero mismatches, fingerprint `d1d172cf244cce83`: solid and textured overlap
in opposite draw orders, the diagnostic page, far-only blue after clearing,
and reversed draw orders returning to near red. This is scoped evidence for
those regions, not a whole-image or original-game fidelity certification.
Five invalid readback requests also leave their caller storage unchanged.

The separate scene fixture also captured the complete 640 by 448 GS framebuffer
in 640 by 16 strips. Its private PPM and window capture both show the static car
mesh. The PPM records framebuffer memory before TV filtering; this successful
dump does not establish agreement with an original arcade reference image.

The first reverse-DMA run correctly rejected Play's `D_CTRL.DMAE=0` startup.
The pinned SDK's channel initialization does not enable that global bit, while
Play 0.72 permits drawing transfers without it. The standalone fixtures now
explicitly set DMAE while preserving other bits. The display's hardware
precondition remains enforced; it was not bypassed to obtain a passing result.
Private build, capture, comparison and execution records remain under `build/`;
no mesh or game image is published with this document.

## Next work

Expand GS image comparisons beyond the sampled depth regions, then recover
original texture and material bindings. Establish course/collision and camera contracts before
connecting the typed game components. Move measured geometry work to VU kernels
only after comparison against this bounded EE reference; preserve bank capacity,
transfer ownership and synchronization. Physical hardware validation remains
required for memory, timing, controller and sound claims.
