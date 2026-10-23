# PS2 hardware decisions and measurements

The port follows the design's **Platform layer & build** and **Reference &
validation** sections. This record connects public PS2dev documentation to
specific implementation choices. Desktop-style GPU assumptions do not apply
to the GS, and emulator timings do not establish physical-console speed.

## Data paths

The current [native rewrite](ps2-native-rewrite.md) uses typed game and render
state. The hardware backend receives no arcade processor state or device
registers:

```text
Native input -> EE game systems -> native geometry -> VU work -> GIF -> GS
                     |
                native sound events -> PCM mixer -> IOP -> SPU2
```

The game systems and mixer in that diagram are the destination architecture;
the executable currently connects original diagnostic content only. VU0 final
XYZ packing works; VU1 geometry processing remains to be implemented.

The measurements below come from the separate translated reference probe:

```text
Host/USB pack -> bounded ROM cache -> EE game + Model 2 devices
                                         |
                            shared geometry / tile layers
                                         |
                      CPU reference OR ordered GS primitives
                                         |
                     optional VU0 XYZ packing -> GIF DMA -> GS

EE translated sound CPU + SCSP -> PCM -> SIF RPC -> IOP -> SPU2 DMA
```

The [PS2dev architecture introduction](https://ps2dev.com/playstation-2-programming/an-introduction-to-ps2dev/)
describes the three graphics paths and the distinction between VU0 macro/micro
mode and VU1 microprograms. Our current renderer uses GIF Path 3. VU0 performs
only the tested final coordinate conversion; it is not the whole geometrizer.
VU1 is inactive. The sound engine still runs on the EE; the IOP handles PCM
queueing and SPU2 transfer, rather than replacing Sega's synthesis behavior.

## PS2 Linux documentation review

The supplied [PS2 Linux document index](https://darrenrainey.github.io/PS2-Linux-Website/docs.html)
is a discovery aid, not a runtime dependency or blanket provenance statement
for every mirrored file. The public Sony-authored
[Coding for PlayStation 2 guide](https://ps2-linux.tensioncore.com/coding-on-playstation2.html)
emphasizes DMA, the graphics paths, scratchpad use and keeping the processing
units occupied together. The next geometry experiment therefore needs bounded
VIF1/VU1 batches, explicit memory ownership and separate measurements of EE
preparation, transfer and vector work. Installing Linux or its SPS2 module is
not necessary for our standalone SDK executable.

The linked public Toshiba
[TX79 Core Architecture revision 2.0](https://github.com/DarrenRainey/PS2-Programming-Docs/blob/master/tx79architecture.pdf)
was checked at sections 2.5–2.8. It describes a related C790 implementation,
including cache and floating-point properties that must not be substituted
for the EE's. Its discussion is useful background; PS2 cache sizes, numeric
behavior and counter events must be established from PS2-specific references
and tests. The subsequent explicit request to study the whole named hardware
collection is recorded as a narrow local-study exception in rules.md. All 16
PDFs, the controller specification and README were downloaded and made
searchable; the source repository is cloned under ignored build/ps2/references.
No manual or SDK implementation is included in the public source tree.

## Keep texture indices resident; shade with CLUTs

The initial RGBA cache had two slots and included each polygon's shaded
palette in the texture key. On captured frame 1357 it uploaded 93,134,848
texture bytes for 2,451 textured triangles. Frame 1634 uploaded 84,713,472
bytes for 2,380 textured triangles. Most of that traffic was repeated data.

The SDK defines [PSMT4 as four-bit palettized storage](https://ps2dev.github.io/ps2sdk/gs__psm_8h.html).
Its [VRAM allocator](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/graph/src/graph_vram.c)
establishes the indexed buffer-width alignment and allocation units. The
public [gsKit texture implementation](https://github.com/ps2dev/gsKit/blob/8ef73d05f022d14b3ded7c70cba670fe333ed61a/ee/gs/src/gsTexture.c)
also provides a reference for the 8 by 2 PSMCT32 upload of a 16-color CLUT.
It was inspected only; no gsKit implementation was copied or linked.

The replacement uses fourteen 32 KiB index slots and 256 palette slots within
the previous 512 KiB VRAM budget. Palette/luma changes update a 64-byte CLUT
without invalidating texture indices. Texture generation, source sheet,
coordinates, dimensions and mirror mode still participate in the index key.
The ROM-free fixture's initial traffic fell from 8,192 texture bytes to 512
index bytes plus 128 palette bytes; subsequent unchanged frames upload neither.
Its captured viewport is pixel-identical to the previous RGBA implementation.

This retains the experimental renderer's nearest/base-level sampling. It does
not reproduce Model 2 index filtering before nonlinear palette/gamma lookup,
mipmap blending or all edge rules. CPU reference rendering remains the
default, and unsupported packets require a complete reference fallback.

## DMA ownership, cache visibility and bounded staging

[Using the DMAC in Games Programming](https://www.ps2dev.com/mirror/hsfortuna/articles/dmac2a.htm)
explains REF chains and prearranged transfer data. The SDK's
[DMA implementation](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/dma/src/dma.c)
requires special care: flushing chain storage does not flush every referenced
payload. Our texture, CLUT and font uploads synchronize their own source data
and retain it until the transfer completes. No CPU writer may recycle that
storage while DMA owns it.

The initial 768 KiB staging allocation is now 64 KiB. Layer uploads use
page-aligned 512 by 32 strips; the GS base address advances in its own word
units. The complete ROM-free viewport matches the previous full upload
exactly. This saves 720,896 EE bytes without reducing image precision.
Two staging buffers and DMA overlap remain future measured work; the current
code waits for ownership to return before overwriting the shared buffer.

The SDK's [packet2 VU helpers](https://ps2dev.github.io/ps2sdk/group__packet2__utils.html)
show double-buffer setup, aligned UNPACK/REF transfers and microprogram
start/continue commands. A future VU1 path should batch work through those
boundaries rather than issue a synchronous round trip for each vertex. It
must first match the shared transform, clipping and priority contracts.

## EE cost and memory

Boot frame 3 of the reference probe measured 378,808,832 EE bus-clock ticks
inside machine execution. Tile preparation/draw/composition accounted for
26,947,072 of those ticks; the remainder was core/device work. GS submission
and waiting accounted for 1,831,168 ticks. The total was 380,640,768 ticks.
The SDK timer frequency used by this instrumentation is 147,456,000 ticks/sec.
These are Play! measurements of one boot frame, not a course profile or a
hardware throughput benchmark. They justify investigating the instruction
boundary/device scheduler before assuming graphics is the only bottleneck.

The compact renderer also defers its 1,310,720-byte CPU raster allocation until
reference 3D is needed. Compact sound tables and removal of unused legacy
sound CPU RAM save 458,752 bytes, with exact gain/PCM comparisons. Together
with the staging reduction, these changes recover 2,490,368 bytes before a
native-rendered frame needs CPU fallback. Runtime peaks, allocator fragments,
the stack and live geometry still have to fit together.

## Readable diagnostics

Normal play has no diagnostic overlay. **Select + Start** toggles a full-screen
40-column, 23-line page with doubled glyphs. The same bounded ASCII report is
printed to guest stdout for the first three frames and every 60 frames.
The page displays the last completed frame, not a guessed instantaneous load.
Its own drawing/waiting is included in the submission measurement.

Optional `rally.cfg`, beside `game.rpk`:

```ini
renderer=reference
vu0=0
audio=0
debug=0
```

`renderer=gs-nearest` enables the experimental native renderer; `vu0=1`
selects its tested coordinate packer. `audio=1` requires a `--with-sound`
build. `debug=1` opens the diagnostics page at startup. Unknown or malformed
settings fail atomically with a visible error. The page explicitly labels
inactive units and unmeasured GS busy time. ROM page misses are software-cache
misses, not EE instruction/data-cache miss counters.
