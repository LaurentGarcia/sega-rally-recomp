# Native driving view and car render bridge

`update_driving_render_view` implements the recovered chase-camera branch as
typed values. `build_car_render_bridge` builds the body, shadow and four wheel
transforms from the published `RenderPose`, immutable wheel attachments and the
current view. Neither component contains an arcade processor, mapped memory,
geometry command stream, captured matrix sequence or host trigonometric
replacement. They use the project's independently written rounded arithmetic
and recovered geometry polynomial.

This is one driving view and one full-car transform path. Camera selection,
intro/close views, model/LOD selection, dynamic texture adjustments, lighting,
stage streaming and other vehicles remain separate work. A fixed textured
course subset is not the recovered full stage renderer. These interfaces do
not change vehicle simulation.

## Recovered value contract

The chase branch at original program interval `[0x1ff60,0x202d8)` retains one
yaw-history value. It folds the rounded sum of the published middle angle and
history once, subtracts a separately rounded binary64 correction using the
original `0.05` constant, narrows and folds again. The view is constructed in
the original order: initial displacement, first-axis rotation, middle-axis
rotation, then negative published position. The pitch uses the original
binary64 constant and half of the first angle. No axis names beyond the
observed first/middle/third order are needed by the interface.

The output also preserves the original inverse-helper position calculation,
copied orientation and focal lengths of 300. Input position is finite within
±2^20, orientation/history within ±rounded-binary32 π, and the explicit
distance adjustment within ±64. Rejection publishes neither state nor output.
The caller seeds history once and retains it across updates.

The geometric part of the full draw routine beginning at `0x23a80` constructs
the body by translating the view and rotating middle, first, then third. Each
wheel starts from that body transform, adds its model attachment and published
suspension displacement, applies steering to the first two descriptor entries,
then applies the negative accumulated wheel phase. The shadow uses the four
published contact heights with the recovered average, offset and two tilt
coefficients. Source draw selection and additional effects are not implemented
by this value operation.

Wheel phase is an accumulated value: actual driving captures already exceed
the existing small-angle helper's ±16 domain. The bridge extends the same
project-authored polynomial reduction to phase ±2^20, including the cosine
shift. Its quotient remains below 2^19 and exactly representable in binary32;
the centered polynomial argument stays bounded by 2. This is not a modulo or
host-sine substitution. Body angles and steering admit ±8, attachments and
suspension ±64, and position/contact heights ±2^20. Derived intermediate
domains are checked. Unread raw pose fields are preserved as uninterpreted
data. All output matrices commit together, including when the input view is
one of the destination's matrices.

`AffineTransform3` stores three XYZ columns followed by translation. Native
scene conversion copies those columns directly into a column-major 4×4 matrix;
it neither transposes nor recomposes a captured view.

## Evidence

The owner-supplied program SHA-256 is
`bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439`.
The original geometry firmware SHA-256 is
`68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346`.
All original images, generated reference code, captures and extracted assets
remain under ignored `build/`.

`check_render_bridge.py` builds a fresh original firmware reference, verifies
both images, binds the current native/oracle sources, and checks the sealed
capture's hashes, completion counts, ordered call/return proofs and unchanged
inputs. The camera oracle executes the original CPU branch and real geometry
firmware. The body oracle independently constructs the recovered geometric
operations and compares the real firmware's six matrices, including saved
matrix/depth restoration and rejection of external/table accesses. The latter
is not an execution claim for the entire original car-draw routine.

Private report `build/ps2-native-render-bridge-final-001/result.json` records:

| Comparison | Recorded result |
| --- | --- |
| Chase camera | 1,024 authored inputs plus 3 live calls; 12,324 matrix words and 9,243 state/output words; hash `7b95f4f8895068b8` |
| Body/shadow/wheels | 2,048 authored inputs plus 2 live calls; 147,600 firmware matrix words, with a further 144 captured matrix words checked; hash `30473be6c283b9bb` |
| Observer neutrality | All 2,850 original-machine checkpoints match with capture enabled/disabled |
| Portable guards | 85 native checks plus saved-frame/block parser regressions; ASan/UBSan clean |

The authored body corpus includes 1,024 wheel phases extending to ±2^20.
Portable tests cover signed zeros/subnormals, phase endpoints, invalid input
atomicity, input-view aliasing, ignored raw fields and persistent camera
history. These are bounded corpus results, not exhaustive floating-point or
physical-console certification.

## Driving publication and asset join

Sealed capture `build/ps2-visual-assets-003` contains consecutive chase calls at
observation frames 2401–2403. The original full-car draws observed in frames
2402/2403 consume the preceding camera/pose publication. Native integration
therefore uses the first vehicle update's published pose, computes its camera,
and renders the corresponding car geometry. Observation frame 2401 is game
input counter 2318; those counters must not be interchanged.

The first checkpoint is before that update. Its initial controls are consumed
once so the first resulting 22-word pose exactly matches the sealed guard;
subsequent updates consume live controls. No later pose, view matrix or captured
motion is supplied to the native loop. Reset restores only this explicitly
chosen developer checkpoint and initial view history.

The observed main-car descriptor has model indices 104 (shadow), 102 (wheel),
96 (body), 106 (alternative body), and 103 (additional body geometry). Actual
frame-2402 bindings are shadow object 186, body objects 187/188, and four wheel
objects 189–192; frame 2403 shifts these indices by two. The additional body's
matrix equals the body's exactly. Two complete sets of six matrices match the
native bridge bit for bit. This identifies the draw-role geometry without
inferring a model name from its numeric index.

For the first fixed-course subset, frame-2402 objects 175–183 have matrices
exactly equal to the recovered camera matrix: their vertices already inhabit
the game's world coordinates. No inverse bake is needed. Opponent objects
0–9, unclassified procedural geometry, distant backgrounds outside native
bounds and camera-facing scenery are not silently treated as static course.
Broader environment reconstruction requires its own evidence.

The main viewport projects to `x = 248 + focus_x*x/z` and
`y = 192 - focus_y*y/z`, after the original center/CRTC offsets. Native near/far
planes are explicit presentation settings, not claimed as recovered arcade
depth behavior. Native float geometry and GS rendering still require their
own clipping, material, coverage and image comparisons.
