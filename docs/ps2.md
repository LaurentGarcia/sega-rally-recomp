# PlayStation 2 development

This is an experimental homebrew port, not a finished PS2 player. It uses the
actual translated Sega Rally i960/TGP code and shared Model 2A devices for its
game-core probe. A separate, ROM-free bring-up ELF exercises the display and
controller with synthetic geometry. A successful build does not establish
hardware execution, playable speed, sound or arcade fidelity.

## Architecture

The PS2 target uses the open PS2SDK directly, without SDL, Metal or Vulkan.

| Component | Implemented boundary | Remaining work |
| --- | --- | --- |
| Emotion Engine | Native generated C++; shared board devices; bounded ROM cache; compact CPU tile compositor | Runtime memory peaks, profiling, EE floating-point validation |
| GS/GIF/DMA | Double-buffered display, reference-frame uploads, ordered opaque solid triangle batches | Textured game polygons, shading/checker rules, edge coverage, hardware comparison |
| DualShock | Analog steering, pedals, gears, coin/start/view cabinet input | Physical controller validation and configuration UI |
| IOP storage | Open USB mass/FAT modules, fileXio, bounded startup retry | USB throughput, cache stalls and real loader/device checks |
| VU0/VU1 | No game geometry offload yet | Compare transforms/clipping with the shared reference before enabling microprograms |
| IOP/SPU2 | No game sound output yet | PCM transport, sample-rate handling, buffering and synchronization |

The initial game probe uses the CPU reference rasterizer and uploads its result
to GS. Native GS triangles are currently a separate diagnostic path; unsupported
texture/checker/edge-proxy packets are rejected explicitly. The intended final
renderer accelerates the game through the GS, with vector-unit work introduced
only after numerical and frame comparisons. PS2 floating-point behavior must
not be assumed equivalent to desktop IEEE arithmetic.

The hardware direction is to keep board/game execution on the EE, send bounded
geometry batches through VIF/VU only where comparisons permit, and submit
ordered textured primitives to GS through GIF DMA. DMA buffers need explicit
ownership and cache synchronization so preparation can eventually overlap
transfer. The IOP can service storage and transport buffered PCM to SPU2.
These are implementation targets, not a claim that VU or audio offload already
works. Pixel, numerical and timing measurements decide each optimization.

Native GS integration also needs its own bounded tile/texture staging path.
Enabling the existing desktop external renderer allocates full tile planes
and GPU snapshots again, undoing the EE memory savings below. Keep that
reference path available for comparison, but measure the complete EE and GS
allocation budgets before selecting a console renderer.

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

The measured compact executable occupies 16,786,584 loaded bytes and leaves
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
audit; do not edit an old audit to suppress a mismatch. The PS2 probe currently
disables sound and leaves performance/timing unqualified.

```sh
python3 platform/ps2/tools/asset_pack.py pack \
  --assets build/native-metal-player-001/assets --output build/ps2/assets/game.rpk
python3 platform/ps2/tools/asset_pack.py verify --pack build/ps2/assets/game.rpk
python3 platform/ps2/prepare_core.py \
  --native-build build/ps2/native --output build/ps2/core-manifest.json
python3 platform/ps2/build_ps2.py build --target core-probe \
  --core-manifest build/ps2/core-manifest.json --output build/ps2-core --optimization Os
```

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
establish the unrecorded intervening states. No physical PS2 run, playable
race or sound output has been demonstrated. The CPU-rendered probe advances
at roughly 0.33 game frames per second in this emulator test. Play!'s display
refresh counter is not the game's frame rate or a console speed measurement.

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

Before a player release: boot and measure the actual game on the destination,
finish native textured GS rendering, validate VU optimizations, add sound,
measure heap/stack/cache peaks, then run complete courses and restarts against
the reference. Compiled artifacts remain development probes until those
checks pass.
