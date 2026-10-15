# README screenshot provenance

These five selected screenshots document actual native output from local
validation runs on macOS. They are the explicitly requested documentation
exception to the source repository's exclusion of raw game captures. They are
not extracted textures, ROM files, generated game code or a playable game.
Game imagery and trademarks remain the property of their respective owners;
the project's BSD source license does not grant rights to those game assets.

The four original-output images retain the original 496 × 384 pixels. The CRT
image below is the real production Metal presentation of the same Desert frame.
PPM captures were converted
losslessly to PNG; conversion metadata was removed without changing the
compressed pixel payload. The original-output images have no retouching or invented
screen content. The CRT image applies the documented renderer settings. GitHub may scale the display to fit the page.
These captures precede the public prerelease build and do not constitute a
new RC validation run. The Metal hero is captured before CRT/UI effects;
the gallery uses the native CPU renderer. See the
[current evidence and limits](../segarally-status.md).

## hero.png

Desert, Metal with Accurate textures, frame 2600.

- Published PNG SHA-256: `80119f52162b2249459b67a5bb8ad3fe5724fb6b0cea4913f95bff1deebfabd0`.
- Local source: `build/validation/mac-reciprocal-v17-host-002/gameplay-on-001/native.ppm`.
- Local source SHA-256: `66d2a925764f32f98d1b1105af4660f6d2b0d6358016c45eca0e4d4f83cbb5d2`.

## forest.png

Championship Forest, native CPU renderer, frame 9000.

- Published PNG SHA-256: `9ad7f662b6d2c7d62b1c17c2fb094a2655a0938269ed566c8a96a9a6a6151a01`.
- Local source: `build/worktrees/course-driving/build/course-driving/championship-attempt-006/guest/frame-9000.ppm`.
- Local source SHA-256: `4cefa36811e550190668b15ee1131f0167955ce2a8afb6072c85f93121d3f128`.

## mountain.png

Championship Mountain, native CPU renderer, frame 15000.

- Published PNG SHA-256: `005bfe587184233f00d62b03b5da3f9e9e9d33017896c5684bb56417249b1b44`.
- Local source: `build/worktrees/course-driving/build/course-driving/championship-attempt-006/guest/frame-15000.ppm`.
- Local source SHA-256: `a9690358492f3529eb5df43398496291f9b2c970e8584301e81397c25da0e655`.

## finish.png

SHORT Practice Mountain Stage Clear, native CPU renderer, frame 10200.

- Published PNG SHA-256: `7cc3f11cf43021a69f3e6847bee571c05b756fa6c76443ebe508ac54d2838b96`.
- Local source: `build/worktrees/course-driving/build/course-driving/short-practice-v29-18000-frozen/stage-clear-10200.png`.
- Local source SHA-256: `7cc3f11cf43021a69f3e6847bee571c05b756fa6c76443ebe508ac54d2838b96`.

The local source files and their broader capture bundles stay ignored. Only the
five exact reviewed PNG paths/hashes are allowed by the source-release gate.
Other captures, changed images, recordings, assets, personal ROMs and packaged
apps remain excluded. New documentation images require an explicit review and
updated provenance; the allowlist is not a general image-export permission.

## crt.png

The real production `app::CrtRenderer` processed the preserved Metal Desert frame
through the existing saved-image mode in `tests/test_gpu_crt.cpp`. This is an
offscreen Metal render/readback, not a photograph or a new live gameplay capture.
The input RGB values were converted losslessly to BGRA8888 for the GPU upload.
The native scene is 496 × 384; the target is 1152 × 896 with a 992 × 768 viewport
at (80, 64). The preset uses scanlines 0.25, phosphor mask 0.12, glow 0.10,
curvature 0.035 and chromatic displacement 0.25 pixels. The test completes its
20-draw bound, including disabled/zero-effects behavior, all five controls,
letterbox clearing and clamping. The renderer and shader sources match this
public source. This verifies the presentation example, not additional gameplay.

- Published PNG SHA-256: `50e67500868e0305da5f1f24d5c9bb743f79c69a66994f1118422ed6ccf80e14`.
- Local output: `build/validation/readme-crt-001/render/crt-default.ppm`.
- Scene: the same `hero.png` input described above.
- Fixture executable SHA-256: `d2cc7c174369ffedc1abc8251dffed946334ce6f8036e875d39e4d50305a7824`.
