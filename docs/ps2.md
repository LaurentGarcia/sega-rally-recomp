# PlayStation 2 development

**Current direction:** a [native PS2 rewrite](ps2-native-rewrite.md) with no
arcade processor/device emulation in its executable. The translated game-core
probe described below remains the separate behavioral reference. The new
`native-port` target includes platform diagnostics and an optional
[static mesh study](ps2-native-scene.md); game systems are being recovered
and rewritten before it can be called playable.

This is an experimental homebrew port, not a finished PS2 player. Its targets
have different purposes:

| Target | Runtime | Purpose |
| --- | --- | --- |
| `native-port` | Independently written game systems and direct PS2 platform code | Current rewrite; platform diagnostics or private static mesh study, no connected gameplay |
| `core-probe` | Translated i960/TGP/68000 code and shared Model 2A devices | Separate behavioral and hardware comparison tool |
| `bringup` / `audio-fixture` | ROM-free PS2 hardware diagnostics | Display, controller and PCM output checks |

Build and run the current rewrite using the
[native target instructions](ps2-native-rewrite.md#build-and-check). The
following architecture and memory measurements describe the **separate
reference probe**, unless explicitly marked native. They do not describe the
new game's runtime or establish its playable speed or arcade fidelity.

## Reference probe architecture

The PS2 target uses the open PS2SDK directly, without SDL, Metal or Vulkan.

| Component | Implemented boundary | Remaining work |
| --- | --- | --- |
| Emotion Engine | Native generated C++; shared board devices; bounded ROM cache; compact CPU tile compositor | Runtime memory peaks, profiling, EE floating-point validation |
| GS/GIF/DMA | Double-buffered display, compact layer uploads, indexed textured triangles, CLUT cache, checker/cutout rules | Model 2 filtering and edge fidelity, DMA overlap, hardware comparison |
| DualShock | Analog steering, pedals, gears, coin/start/view cabinet input | Physical controller validation and configuration UI |
| IOP storage | Open USB mass/FAT modules, fileXio, bounded startup retry | USB throughput, cache stalls and real loader/device checks |
| VU0/VU1 | Optional VU0 final coordinate packing; VU1 inactive | Transform/clipping comparisons before broader offload |
| IOP/SPU2 | Original bounded PCM server, open libsd, integer-phase resampling and DMA output | Whole-game speed, sustained queue behavior and physical audio validation |

The game probe defaults to the CPU reference rasterizer and uploads its result
to GS. `renderer=gs-nearest` explicitly enables the experimental native path;
unsupported packets fall back to complete reference composition. Its nearest
sampling is not arcade pixel parity. The two captured real scenes reproduce
the same native GS image with scalar and VU0 coordinate packing. Broader
vector-unit work requires additional numerical and frame comparisons; PS2
floating-point behavior must not be assumed equivalent to desktop IEEE.

The reference probe keeps board/game execution on the EE, sends bounded
geometry batches through VIF/VU only where comparisons permit, and submit
ordered textured primitives to GS through GIF DMA. DMA buffers need explicit
ownership and cache synchronization so preparation can eventually overlap
transfer. The IOP can service storage and transport buffered PCM to SPU2.
Geometry offload beyond final XYZ conversion remains a target. Audio synthesis
still runs on the EE; only the PCM transport/output runs on IOP/SPU2. Pixel,
numerical and timing measurements decide each optimization. The
[hardware decisions and profiling record](ps2-hardware-notes.md) links the
public PS2dev documentation to the cache, DMA and VU choices.

Its native GS integration uses compact layers and bounded texture staging.
Enabling the desktop external renderer without that mode would allocate full
tile planes and GPU snapshots again, undoing the EE memory savings below.
The rewrite's independent renderer receives native material and vertex data;
it does not consume arcade video registers or tile planes.

## Memory

Retail EE RAM is 32 MiB; see the public
[PCSX2 memory contract](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/MemoryTypes.h).
The [open SDK linker script](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/startup/src/linkfile)
starts user code at 1 MiB. Executable, heap, stack, DMA buffers and all native
board state must fit together; an ELF segment check alone is insufficient.

The eight current game images occupy 102.75 MiB. Packing identical/zero 4 KiB
pages reduces disk storage to 28.125 MiB while reconstructing every original
byte. The default reader keeps 2 MiB of page data and about 0.21 MiB of index,
cache bookkeeping and zero-page storage. The initial game probe selects a
smaller 512 KiB data cache to leave room for the EE executable. Native texture
RAM, board RAM, renderer buffers and executable memory are additional
allocations.

`M2_PAGED_ROM` enables the existing paged geometry/TGP interface and adds
Model 2A program/data paging. The guest's ROM windows, mirrors, zero padding,
unaligned accesses and ignored writes keep their resident behavior. This flag
does not enable Dreamcast-specific skipped commands or arithmetic shortcuts.

`M2_COMPACT_TILEMAP` replaces four full decoded tilemap planes with four cached
512-pixel rows. It defers decoded-character snapshots and per-tile generation
storage until an external renderer requests the full planes. CPU-only hosts
also avoid unused GPU layer copies. The compact path preserves scroll,
category, palette and window-mask behavior; it recomputes rows and is not
claimed as a speed improvement.

The original compact baseline executable occupies 16,786,584 loaded bytes and leaves
15,719,272 bytes for heap and stack after the SDK's 1 MiB load origin. Known
persistent allocations account for at least 13,246,204 bytes; the constructor
also temporarily holds a 262,144-byte TGP table copy. Native GS sprites draw
diagnostic text without a temporary framebuffer. In Play!, the EE allocator
reports 13,263,920 allocated bytes within a 13,577,064-byte arena after machine
construction and at completed frames 1 and 60. These are observed checkpoints,
not peak measurements. SDK/stack use, FIFO growth and busiest-scene peaks
remain to be measured before claiming complete runtime fit.
Global `-fno-inline` increased executable residency;
representative `-Oz` and `-fno-jump-tables` trials produced identical objects
to `-Os`. The probe therefore retains `-Os`.

The current native path defers 1,310,720 bytes of CPU raster storage until
reference 3D is needed. Bounded GS staging saves another 720,896 bytes, and
compact sound tables plus bus-owned sound RAM save 458,752 bytes. The indexed
texture and CLUT cache retains the prior GS VRAM budget. Actual reference-only
boot with the new staging/raster policy reports 11,232,608 allocated EE bytes.
The sound-linked candidate occupies 17,764,288 static bytes, leaving 14,741,568
bytes above the load image for heap/stack; an executable-only figure still
does not establish busiest-scene fit. See the hardware record for scope.

## Build the ROM-free hardware test

Install Python 3.10+, Docker and a host C++20 compiler. The driver uses the
official image pinned in `platform/ps2/toolchain.lock.json`; Docker downloads
about 273 MiB on arm64 and retains about 1 GiB unpacked.

```sh
python3 tests/test_ps2_memory.py
python3 tests/test_ps2_core.py
python3 platform/ps2/build_ps2.py smoke --output build/ps2-bringup
python3 platform/ps2/build_ps2.py build --output build/ps2-bringup
```

The output is `build/ps2-bringup/segarally-ps2-bringup.elf`, with a JSON build
report identifying source hashes, compiler/image, ELF load segments and test
limits. `--backend native --ps2dev /path/to/ps2dev` uses an existing native
toolchain. The bring-up screen clearly labels its synthetic geometry and does
not contain Sega game instructions or assets.

## Prepare the local game probe

Use the project's verified native generation with indexed dispatch and exact
instruction-boundary hooks. A changed generation dependency requires a fresh
audit; do not edit an old audit to suppress a mismatch. Sound and approximate
GS rendering are explicit development options; performance/timing remains
unqualified on physical hardware.

```sh
python3 platform/ps2/tools/asset_pack.py pack \
  --assets build/native-metal-player-001/assets --output build/ps2/assets/game.rpk
python3 platform/ps2/tools/asset_pack.py verify --pack build/ps2/assets/game.rpk
python3 platform/ps2/prepare_core.py \
  --native-build build/ps2/native --output build/ps2/core-manifest.json
python3 platform/ps2/build_ps2.py build --target core-probe \
  --core-manifest build/ps2/core-manifest.json --output build/ps2-core --optimization Os
```

For sound, first generate the timed compact sound core using
`scripts/segarally_sound_recompile.py --timed --compact-scsp` with the same
verified game inputs, then pass `--with-sound` to `prepare_core.py`. The manifest
includes the sound audit, `M2_COMPACT_SOUND`, original IOP server and open SDK
audio module. `build_ps2.py build --target audio-fixture` builds a separate
ROM-free non-silent PCM transport test.

Optional `rally.cfg` sits beside the pack. Defaults are `renderer=reference`,
`vu0=0`, `audio=0`, `debug=0`; see the
[configuration and diagnostics page](ps2-hardware-notes.md#readable-diagnostics).
**Select + Start** toggles the larger full-screen diagnostic page. The same
ASCII snapshot is printed to stdout, while normal play remains unobstructed.

Keep game-derived data and generated source private. Neither `.rpk` packs nor
ELF/IRX binaries belong in source Git. The console game-probe path is
`mass:/game.rpk`. If a development loader provides `host:game.rpk`, that path
is selected while preserving the loader's IOP modules; this is also the HLE
emulator test path. An explicit first argument selects a path. USB storage
initializes after the final IOP reset and before opening the pack. A homebrew
loader capable of running ELF files is required on a real console. Storage and
loader behavior still require testing on the destination setup.

## Evidence and limits

Portable tests cover malformed packs, full reconstruction hashes, bounded
reads, eviction and pointer lifetime. Shared-core tests compare 10,000 reads
plus ROM boundaries, zero padding, mirrors, devices, TGP and geometry behavior.
A real generated-game host comparison completes 1,634 frames with identical
per-frame instruction/cycle counts, board time, PC, register and screen hashes
for resident and paged images, with both compact and full-plane tile rendering.
The 64-case tile-compositor comparison and the existing 144-case GPU packet
fingerprint also pass. This is host equivalence, not PS2 floating-point or
full-race certification. Local evidence is in `build/ps2/validation`.

The ROM-free diagnostic and actual game ELF boot in Play! 0.72. The game
displays its own `SOUND INITIALIZE ...` screen through reference-frame GS
uploads. The observed EE checkpoints at frames 1, 2, 3 and 60 match all seven
host CSV fields exactly. This compares four sampled checkpoints; it does not
establish the unrecorded intervening states. No physical PS2 run or playable
race has been demonstrated. The original CPU-rendered probe advances
at roughly 0.33 game frames per second in this emulator test. Play!'s display
refresh counter is not the game's frame rate or a console speed measurement.

New hardware-specific evidence is bounded and separate:

- VU0 packing matches 295,872 scalar/independent binary32 components exactly
  in Play!, including boundary, invalid-input and tail cases.
- Captured actual game frames 1357 and 1634 render through GS, including the
  Martini Lancia scene. Scalar/VU0 images are identical. Against the CPU image,
  mean absolute channel errors are 7.54 and 12.24 out of 255 respectively;
  nearest sampling remains a visible fidelity difference.
- Indexed textures preserve those same GS images exactly while reducing repeat
  texture traffic by 98.91% and 99.34%. Emulated renderer timings improve 6.48x
  and 8.95x; neither ratio is a full-game or physical-console speed claim.
- The original IOP/SPU2 fixture submits and consumes 48,001 non-silent frames,
  with explicit silence on underrun. There is no physical or loopback audio
  recording. The authentic sound-board replay separately matches 1,929 UART
  checkpoints and 728,884 stereo PCM frames exactly with compact/paged sound.
- Compact layers plus continuous reference fallback match all seven host
  checkpoint fields across 1,634 frames. Fallback preserves already composed
  tiles and the once-per-geometrizer-frame raster cache.

Compare archived guest checkpoints with a host reference using:

```sh
python3 platform/ps2/tools/compare_checkpoints.py \
  --reference build/ps2/validation/paged-lazy-1634.csv \
  --guest-log build/ps2/emulator/core-final-001-ps2_stdout.txt \
  --output build/ps2/validation/checkpoints-new.json
```

The tool checks every observed seven-field row, rejects malformed or
out-of-order checkpoints, records input hashes and refuses to overwrite
evidence. It explicitly labels sparse comparisons as partial-run evidence.

Play! is a separate BIOS-free development tool. Enable
`iop.fileio.stdlogging` in its portable configuration to save actual guest
stdout/stderr. Set `ui.pausewhenfocuslost` to `false` for unattended runs;
otherwise moving focus to another application pauses the guest. Static
diagnostic screens were observed one GS submission behind in Play!, while
continuous game-frame uploads displayed correctly. Use guest logs and
timestamped captures together when diagnosing an apparent stall.
Use Play!'s Vulkan graphics handler for checker validation on macOS. Its OpenGL
path silently omits destination-alpha testing when framebuffer-fetch support
is unavailable; the same ELF's checker mask passes in Vulkan. That emulator
backend choice does not make Vulkan part of the PS2 application.

Before a player release: boot and measure the actual game on the destination,
resolve native GS fidelity limits, broaden validated VU work, sustain game audio,
measure heap/stack/cache peaks, then run complete courses and restarts against
the reference. Compiled artifacts remain development probes until those
checks pass.
