# README screenshot provenance

These four selected screenshots document actual native output from local
validation runs on macOS. They are the explicitly requested documentation
exception to the source repository's exclusion of raw game captures. They are
not extracted textures, ROM files, generated game code or a playable game.
Game imagery and trademarks remain the property of their respective owners;
the project's BSD source license does not grant rights to those game assets.

Each image retains the original 496 × 384 pixels. PPM captures were converted
losslessly to PNG; conversion metadata was removed without changing the
compressed pixel payload. No artwork, retouching, interpolation or invented
screen content was added. GitHub may scale the display to fit the page.
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
four exact reviewed PNG paths/hashes are allowed by the source-release gate.
Other captures, changed images, recordings, assets, personal ROMs and packaged
apps remain excluded. New documentation images require an explicit review and
updated provenance; the allowlist is not a general image-export permission.
