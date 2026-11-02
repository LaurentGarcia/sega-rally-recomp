# Native GS texture residency

The native `Display` caches packed PSMT4 textures in an owned 96-page VRAM arena.
The page allocator in `include/native_texture_cache.h` has 96 fixed metadata
entries and performs no dynamic allocation. It is independent of Rally assets,
material order and model dimensions. Presentation still submits triangles in
the supplied order; the cache does not sort or regroup draws.

A GS PSMT4 page covers 128 by 128 texels and occupies 8,192 bytes. Each supported
power-of-two texture dimension, from 8 through 256, reserves one, two or four
whole pages. The upload and sampling buffer pitch is 128 texels for widths up
to 128, and 256 for width 256; TEX0's width/height exponents retain the actual
image dimensions. Whole-page allocation deliberately avoids assuming that a
narrow image's packed input bytes occupy a contiguous sub-page GS footprint.
First fit uses page-count alignment, and global least-recently-used eviction
frees bounded entries until a suitable contiguous range exists. Fragmentation
can cause eviction even when the sum of live image bytes appears to fit; the
cache makes no general all-assets-resident promise.

The hardware layout follows the GS User Manual's PSMT4 storage layout and TEX0
buffer-width rules, checked against the installed open SDK's
[VRAM allocator](https://github.com/ps2dev/ps2sdk/blob/master/ee/graph/src/graph_vram.c).
The SDK uses 32-bit words for VRAM addresses and 2,048-word page alignment. The
single 1024-by-1536 allocator reservation is only a way to reserve the arena's
786,432 bytes; it is never submitted as a sampled texture. Uploads retain the
actual image width and height and the matching page pitch.

Texture identity remains the pixel pointer, generation and dimensions.
Replacing bytes requires a new generation. Before releasing or reusing an
allocation identity, the owner must invalidate the cache or use an owner API
that supplies globally distinct lifetime generations. `invalidate_textures()`
forgets metadata without DMA, allocation or a statistics change. Copied-content
CLUT entries remain valid. The existing 256-entry CLUT storage allocation is
unchanged.

The frame is validated before cache or GS mutation. On a miss, metadata chooses
a slot, then the existing GIF/GS completion fence finishes preceding primitives
before any VRAM overwrite. Already queued GIF words contain their resolved
addresses, not pointers to mutable cache entries. Upload storage, cache
publication and final completion retain the existing synchronous ownership
contract. Texture replacement cannot race outstanding draws under that
contract. Checker-mask drawing uses the same cache and fences; its DATE,
framebuffer alpha and depth behavior is unchanged.

The arena shares VRAM with two PSMCT32 framebuffers, a PSMZ16S depth buffer,
the font and 256 block-aligned CLUT slots:

| Output | Total allocated bytes | Remaining from 4 MiB |
| --- | ---: | ---: |
| 640 by 448 | 3,751,936 | 442,368 |
| 640 by 512 | 4,161,536 | 32,768 |

These totals include alignment and are checked by portable tests. Runtime
evidence below used height 448. PAL and physical-console execution remain
unverified. The 64 KiB upload staging allocation is EE memory, separate from
these VRAM totals. `Stats.texture_pages` reports occupied pages, and
`Stats.texture_evictions` counts evicted texture identities in that frame.

The original portable allocator fixture passes 4,802 requests and 1,255,229
checks under ASan/UBSan. It covers all 36 supported dimension pairs, alignment,
nonoverlapping ownership, full-cache eviction, sparse holes, identity changes,
invalidation, invalid-input preservation and allocation-free requests. Evidence
is bound in `build/ps2-texture-pages-001/allocator-validation.json`; the retained
runner is `tests/test_ps2_texture_cache.py`.

The EE fixture builds with:

```
python3 platform/ps2/build_ps2.py build --target core-probe \
  --core-manifest platform/ps2/tests/native_texture_cache_manifest.json \
  --output build/ps2-texture-pages-ee-001
```

Use a fresh output directory for new evidence. The frozen fixture ran in
Play 0.72 HLE: eight frames, 35,648 sampled GS framebuffer pixels and zero
mismatches, hash `53a520d4e28c7147`. Original images cover all dimension pairs at
first, middle and last texels. Warm phases upload no textures and evict nothing;
forced eviction, restoration, a generation change and invalidated/reused
storage also pass. The generation-only phase uploads one image; invalidation
reloads 77 images including the checker asset. The source/ELF/log binding is
`build/ps2-texture-pages-ee-001/runtime-validation.json`.

The same renderer passes the existing resource fixture with 464 checks,
24 injected setup failures and 320 sampled pixels, hash `a6f9c6418b875dc3`, bound
in `build/ps2-texture-pages-resources-ee-001/runtime-validation.json`. The depth,
cutout, checker and diagnostic-page fixture passes 373 frames and 2,688 sampled
pixels, including 512 checker samples, preserving hash `c76f6a70b0160483` in
`build/ps2-texture-pages-depth-ee-001/runtime-validation.json`. These are actual
emulated GS readbacks of original diagnostic assets, not whole-game fidelity
or a physical-console claim. Observation wall time includes waiting/capture
and is not fixture execution time.

The first actual moving textured package contains 65 unique model textures and
392,704 packed index bytes. Including the checker texture, their conservative
whole-page footprint is 86 pages, below the arena's 96 pages. This sum alone
does not prove that an aligned allocation order will fit. A snapshot-bound host
trace of the actual bundle002 scene runs 601 retained native updates from game
counter 2318, preserving the seed controls once and using neutral controls
thereafter. It submits triangles in the scene's order through the production
page allocator. The first frame uploads 66 textures and 392,736 bytes; all 600
later frames upload nothing, evict nothing and retain 86 pages. The old 14-slot
policy repeatedly uploads 126 textures / 1,032,224 bytes at updates 2 and 3.
Evidence is `build/ps2-texture-pages-scene-001/validation.json`.

That trace uses host binary32 geometry without a VU provider. It establishes
residency for this asset/order sequence, not integrated EE timing, a full race
or every possible fragmentation pattern. Its sources precede the subsequent
authored-facing integration; later asset revisions need separate measurements.
The actual native041 baseline's matching repeated-upload count identifies the
cache pressure. The subsequent native042 integrated observation measures about
14.97 native updates/presentations per second over 480 complete periods, versus
about 8.89 over 240 periods for native041. This combines cache and authored-facing
changes, so it is not an isolated allocator timing comparison. Neither meets
the complete-frame 60 Hz target.

The subsequent facing-enabled bundle003 trace is separately bound in
`build/ps2-texture-pages-scene-002/validation.json`. Across 601 native updates it
uploads 53 textures / 361,504 bytes on the first frame, then one newly visible
512-byte texture at update 118. There are no evictions; residency grows from
72 to 73 pages. This supports the actual native042 warm observation of zero
texture uploads and evictions at update 60. Host and VU geometry are still
distinct implementations; this cache-order trace is not pixel equality.

The trace also counts the remaining palette lookup work. At bundle003 update 2,
1,160 of 1,451 requests immediately repeat the previous copied palette. A
last-slot check retaining full 16-word content equality would reduce slot
probes from 323,994 to 64,350 and valid-palette comparisons from 116,853 to
22,789. Across all 601 updates, 80.41% of requests repeat the last palette;
the corresponding probe reduction is 80.62%. These are operation counts, not
measured EE time. The narrow last-slot implementation preserves the existing
fallback scan, age updates, copied colors, uploads and fences. It checks the
previous slot's valid flag and all 16 copied colors, then performs the same age
and hit updates. Its pointer refers to stable `Display::Impl` slot storage;
changing palette data through the same caller pointer cannot bypass equality.

The independent host differential in
`build/ps2-palette-host-differential-001/result.json` passes 53,504 calls in both
optimized and ASan/UBSan builds, including 11,609 uploads and 41,895 hits. After
every call it compares all 256 slots' validity, ages, addresses and colors,
returned slot, global age/counters, ordered fence/upload requests and converted
CLUT bytes. It covers same-pointer mutation of every color, equal data at
different pointers, eviction, age wrap and fresh owners. This proves those
method results against the preserved scan implementation, not GS execution.

The first same-executable EE A/B exposed a diagnostic limitation rather than
establishing pixel equality. Both modes' drawing-packet and palette-state
fingerprints matched, but the second owner read the preceding owner's final
image. A baseline-to-baseline control with MRU disabled for both owners repeats
the same mismatch at check 261; its report is
`build/ps2-palette-baseline-control-ee-001/runtime-validation.json`. The earlier
incomplete runs and debug logs remain private evidence and are not passing
pixel proofs.

The pinned Play Vulkan implementation tracks local-to-host transfers by the
whole `BITBLTBUF` value. Once a key is considered recurring, it skips command
buffer flush and queue-idle waiting before copying mapped transfer memory into
the CPU readback cache. Its history survives a guest `Display` replacement;
GS FINISH does not force that Vulkan copy to finish. This source path explains
how transfer completion can accompany stale pixels. See
[the transfer implementation](https://github.com/jpd002/Play-/blob/8de4a71f5215ef38e357ac77a07948eb3424448f/Source/gs/GSH_Vulkan/GSH_Vulkan.cpp)
and [history policy](https://github.com/jpd002/Play-/blob/8de4a71f5215ef38e357ac77a07948eb3424448f/Source/gs/GSH_Vulkan/GSH_Vulkan.h).
Matching repeated readback hashes alone therefore cannot validate these phases.

A separate private fixture varies only the ignored destination base in each
local-to-host `BITBLTBUF`, with a bounded, never-reused process counter. GS User
Manual v6 section 4.1.1 (printed page 74) specifies source-buffer settings only
for that transfer direction; Play's source-range and pixel readers likewise
use only source fields. Unique keys select Play's existing synchronous copy
branch. Source addresses, formats, rectangles, DMA and fences remain unchanged.
This diagnostic workaround lives under `build/ps2-palette-mru-sync-001`; it
does not alter production readback or emulator code. Independent review
confirmed that each new key selects flush-and-wait before mapping and that
destination fields cannot change the source range or pixel-reader addresses.

The synchronous fixture passes 480 checks, six phases in both modes and
1,720,320 compared framebuffer pixels, using 336 unique readback keys. Drawing
GIF packet, CLUT state/age and framebuffer FNV fingerprints agree across the
two modes; this is fingerprint equality, not a retained literal-byte image
comparison. Both framebuffer parities, same-pointer palette mutation, more
than 256 palettes, restoration and texture invalidation are covered. The
submit hook hashes drawing packets, not direct upload-chain payloads; copied
CLUT state and synchronous readback cover their effects separately. The final
source/ELF/log binding is
`build/ps2-palette-mru-sync-ee-001/runtime-validation.json`, report SHA-256
`6a47680306ba8330ba010066ad249b65fcf912acd8abdf7ca10d832afb6464f4`.

Sixteen warm timings alternate scan/MRU order, with hashing disabled and zero
texture or palette uploads. Mean packet-building time falls from 3,235,104 to
1,303,072 timer ticks, or 21.939 to 8.837 ms at the SDK's 147,456,000 Hz
timer frequency: a 13.102 ms reduction (59.72%) for this original 1,630-triangle
fixture. Both modes retain last-slot bookkeeping, so this is an instrumented
search-versus-MRU comparison. The 45.134-second Play observation includes
readback, captures and waiting; it is not execution time. The test uses NTSC
448-line output and does not establish physical-console timing, PAL execution,
full-race fidelity or the complete-frame 60 Hz target.
