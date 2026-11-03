# PlayStation 2

The current target is a [native game rewrite](../../docs/ps2-native-rewrite.md)
using the open PS2SDK, direct GS/DMA rendering and native input/audio services.
Its developer application advances a complete recovered car update from one
private checkpoint. The textured mode connects the original chase camera,
body, shadow and four wheel transforms to a bounded road neighborhood. Platform
diagnostics and the earlier static car study remain separate modes.

Shared services and the independent `engine-demo` target are documented in
[PS2 engine architecture](../../docs/ps2-engine-architecture.md). The build checks
dependency direction between hardware services, game rules, captured-scene
adapters and offline recovery. See [PS2 development](../../docs/ps2.md) for build
commands and current hardware limits.

The [main update](native/main_vehicle_update.md) matches 601 consecutive original
updates, including state, pose and sound requests. The
[render bridge](native/driving_render_view.md) and
[visual scene adapter](native/rally_visual_scene.md) describe the recovered
camera/model relationship. The generic engine accepts owned
[textured assets](native/textured_assets.md); offline conversion stays outside
the native executable. Lighting and the visible course neighborhood are fixed
in this development bundle. Race initialization, opponents, HUD, game audio,
full-stage traversal and physical-console validation remain open.

The optional [world renderer](native/vu1_world.md) performs transforms,
projection and GS packet production on VU1. `--drive-world-vu` uses synchronous
submission; `--drive-world-async` schedules EE gathering between submit and
finish. Both use per-presentation material binding/state reuse.
`--drive-world-no-memo` retains the uncached synchronous comparison. All take
checkpoint, course and visual-directory paths; automatic startup retains Frame.
World modes skip repeated setup for validated VU fallback triangles wholly
outside a clip plane. `--drive-world-no-reject` selects memoized synchronous
submission with the original per-face clipper for comparison.

World modes also use owned prepared corners, with reciprocal texture dimensions
stored once and repeated static numeric checks omitted. `--drive-world-raw`
selects fully validated raw corners while retaining material reuse and early
rejection. The no-memo/no-reject controls keep prepared corners; all three
controls take the same checkpoint, course and visual-directory arguments.

`build_ps2.py --help` lists the native application, original engine sample,
hardware fixtures and separate arcade-reference probe workflows. None is a
finished PS2 player release. All original game data, generated reference source,
module arrays, captures and ELFs belong under ignored `build/`.

## Play developer preview

The Qt frontend's default fit mode displays a 640×448 GS buffer at that
pixel ratio. That stretches this application's logical 496×384 picture
horizontally. [preview_play.py](tools/preview_play.py) creates a separate portable
Play configuration, uses fill mode and sizes the output widget for a raw
496:384 comparison. It changes presentation only; it does not change the
recovered camera, focal length or guest renderer.

Run from the repository root with an existing local emulator, ELF and `host:`
directory. The output directory must be new or empty:

```sh
python3 platform/ps2/tools/preview_play.py \
  --emulator build/ps2/emulator/Play.app/Contents/MacOS/Play \
  --elf build/ps2/native-059/segarally-ps2-native-port.elf \
  --host "build/ps2/emulator/Play Data Files/vfs/host" \
  --output build/ps2-preview-001 --mode raw --seconds 20
```

Use `--prepare-only` to write the configuration and launch command without
opening Play. The launcher records executable/ELF/config hashes and logs.
The explicit `--seconds 20` above bounds a validation run; omit it or use
`--seconds 0` to keep the preview open until its window closes. Ctrl+C or a
duration limit stops only the process it started. It does not copy game assets or
change an existing Play profile. `--renderer opengl` selects the alternative
backend; the tested macOS setup uses Vulkan.

The defaults request a 744×576 content area plus 27 points for the status bar
and its divider. The tested Play 0.72 macOS capture contained exactly
1488×1152 gameplay pixels, three times the logical image dimensions. Other Qt
styles, desktop limits or HiDPI settings can change the result; use `--height`
and `--status-bar-height` and verify the content bounds. The separate
`--mode 4:3` option is a display
choice, not a physical arcade/PS2 CRT calibration. This preview does not
establish a full playable race or a performance target.
