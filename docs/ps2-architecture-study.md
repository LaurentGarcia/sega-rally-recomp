# PS2 architecture study for the native rewrite

The complete requested [reference collection](https://github.com/DarrenRainey/PS2-Programming-Docs/tree/b13fb347ceebd3f71a0ce978adb15b60c64ed6ec)
is cloned locally at `build/ps2/references/PS2-Programming-Docs`, revision
`b13fb347ceebd3f71a0ce978adb15b60c64ed6ec`. All 18 repository files match the
download manifest: 16 PDFs, one controller specification and a README,
17,971,685 bytes in total. The PDFs contain 2,470 pages. Searchable extracts
and SHA-256 identities are under `build/ps2/docs`.

The review covers architecture and implementation constraints across the
collection, with deeper reading of the sections below. Instruction manuals
remain references for individual operations; downloading or indexing every
page is not a claim that every instruction or hardware behavior was tested.
Some mirrored manuals retain their original confidentiality markings. The
explicit local-study exception is recorded in `rules.md`; the collection is
not included in the public source tree or licensed as project code.

## Reading map

Page numbers refer to the downloaded PDF unless a printed page is specified.

| Document | Pages | Focus and use |
| --- | ---: | --- |
| EE Overview | 64 | pp.15–19, 39, 50–56: independent processors, local storage, DMA and VIF |
| EE User's Manual | 218 | pp.99, 110–116, 153, 157–158, 217: VIF synchronization, PACKED GIF ordering and transfer ownership |
| EE Core User's Manual | 180 | pp.24–29, 130–136, 163–165: issue restrictions, cache coherency and non-IEEE arithmetic |
| EE Core Instruction Set | 408 | CACHE, PREF, LQ/SQ and SYNC reference; validate actual compiler output before assembly changes |
| EE Programming | 9 | Cache misses, packed stores and parallel work; advice must be measured on our workload |
| GS User's Manual | 176 | pp.43, 60, 77, 145: texture coherency, STQ register order and completion |
| GS Supplement | 22 | pp.6–10: texture pages, primitive size and repeated pixel access |
| VU User's Manual | 368 | pp.18–19, 26–29, 44–55, 129, 195–196: memories, arithmetic, pipeline and transfer lifetimes |
| VU instruction companion | 128 | XGKICK p.125 and instruction scheduling constraints; cross-check assembler encodings |
| SPU2 Overview | 80 | pp.62, 66 and transfer sections: distinct gain scales, PCM input and DMA streaming |
| PS2 Optimisations | 39 | Data locality, overlap and workload-specific profiling; not a substitute for measurements |
| TX79 architecture | 727 | §§2.5–2.8: related C790 background; its cache and FPU specifications are not EE specifications |
| MIPS Calling Conventions Summary | 13 | Educational calling convention reference; not authority for the pinned EE compiler ABI |
| Sony's Emotionally Charged Chip | 7 | Historical system overview; announced frequencies and peak figures are not measured console performance |
| Vector Unit Architecture for Emotion Synthesis | 8 | Printed pp.41–45: scheduling vector work and transfer together |
| DOT3 normal mapping | 23 | §§3.1–3.8: compression and repeated geometry passes; optional effects deferred |
| Controller specification | Text | Protocol reference; keep the tested SDK pad lifecycle rather than adding a second bus driver |

## EE: keep game state compact and arithmetic deliberate

The EE core manual describes 16 KiB instruction cache, 8 KiB data cache and
16 KiB scratchpad, with 64-byte cache lines and no hardware data-cache
snooping. Its floating-point unit is not interchangeable with desktop IEEE
arithmetic; rounding and special values differ. The related TX79 manual cannot
be used to replace those facts.

Our resulting implementation policy is to keep frequently updated car/race
state separate from meshes, materials and cold metadata. Use aligned,
contiguous batches and explicit cache synchronization at DMA handoffs. Moving
an entire machine model into scratchpad would not solve the original problem;
the new runtime has typed native systems instead.

Physics calculations need recovered input/output contracts before an EE or VU
fast path. A tiny exact integer formulation is appropriate when it implements
a proven game operation. Blindly using native floats, reciprocal approximations
or compiler fast-math can change the race. PREF, MMI instructions and scratchpad
double buffering are candidates only after an actual native workload exists
and cache/transfer measurements justify them.

## VIF/VU1: establish ownership before overlap

VU1 has separate 16 KiB instruction and data memories. Data indexing uses
quadwords, while microinstruction addresses count eight-byte instructions.
Direct EE access to VU memory requires a stopped VPU. Vector arithmetic has
pipeline and rounding constraints; Q-dependent work needs explicit waiting.
XGKICK starts a transfer without making its packet storage immediately reusable.

The first experiment transforms original vertices and reads results back. It
does not draw, clip game geometry or change the current native display API.
The proposed memory layout reserves 16 qwords and uses two 504-qword banks.
Each bank allows an eight-qword header, 96 positions, 96 UV vectors and 291
qwords of eventual PACKED drawing output, leaving 13 guard qwords.

Three distinct ownership rules must remain visible in code:

1. EE submission storage becomes reusable after its DMA has consumed it.
2. VU working/output storage remains owned until computation and any PATH1
   consumer finish. DMA completion alone does not establish this.
3. GS texture, palette and drawing allocations remain valid for outstanding
   draws. A completed EE upload does not prove a draw finished.

Use VIF stop/flush operations, sequence markers and bounded timeouts for the
initial serialized test. Alternate both banks with different matrices and
sentinels. Exact dyadic cases establish arithmetic and scheduling; general
floating-point inputs need separately recorded differences. Add overlap only
after these checks pass. Near-plane clipping is a separate implementation;
CLIP flags alone do not create clipped triangles.

The intended renderer pipeline is:

```text
EE prepares inactive batch -> publish cache -> VIF1 transfer
                                                   |
EE game work + short VU0 work                 VU1 geometry
                                                   |
                                        future PATH1 packet -> GS
```

These are separate storage lifetimes, not a single "frame complete" flag.
Record batch size and bytes transferred, EE preparation ticks, time blocked
waiting for a free bank, submission-to-marker ticks and GS completion waits.
The EE core manual (PDF pp.70/72) specifies one CP0 Count tick per CPU clock;
unsigned subtraction handles one wrap, with explicitly bounded intervals.
CPU timestamps around submission do not measure VU instruction occupancy,
and emulator timings do not establish physical-console throughput. The
current numerical fixture tests storage and computation; it is not yet this
renderer pipeline or its performance instrumentation.

## GS: preserve locality and avoid moving unchanged data

The GS manual requires deliberate texture-cache management and drawing order.
For PACKED textured output, ST supplies Q used by RGBAQ, so the register order
matters. TEXFLUSH follows relevant uploads; FINISH provides drawing-completion
evidence. The supplement explains why page crossings, tiny polygons and
repeated writes can limit throughput even when vector processing is fast.

The existing native backend already keeps four-bit indices separate from
palette changes. Preserve that design when adding object-space geometry.
Budget upload bytes, cache misses, primitives and waits independently. Retain
game-required primitive order; a texture-sorting optimization must not change
the image. DOT3 and other repeated-pass effects remain optional future work,
after the native game and its base image are established.

## IOP/SPU2: distinguish queue starvation from late service

The SPU2 manual identifies different scales for master volume and PCM input
volume. This review found a concrete error: using `0x3fff` for both makes the
PCM input roughly half scale. The correction keeps master volume at
Q14 near-unity and sets PCM input to Q15 near-unity (`0x7fff`).

A second finding concerns service timing. The open SDK's looping DMA restarts
the next half before calling the completion handler. If the worker misses a
half-buffer deadline, stale data can repeat even while the input ring contains
samples. Empty-ring silence and delivered-but-unserviced completion events
are now separate counters. A completion-generation guard also prevents a stale
semaphore wake from consuming a second batch into the same DMA half. Neither counter can establish audible
quality or account for an interrupt that never reached the handler.

Controller review found another asynchronous boundary in the open SDK:
acceptance of an analog-mode request is not completion. The adapter now waits
for success, retries later failures, and keeps checking a temporarily missing
mode table without disabling valid digital input. Mock tests cover delayed
metadata, pending/failing requests and disconnects; physical pads remain
untested.

## Validation order

The immediate work is the confirmed audio correction, isolated VU1 numerical
execution, and recovery of actual car mathematics. Each proceeds with original
fixtures and retained private original-program comparisons. Then add bounded
native mesh submission, clipping and PATH1 rendering, followed by carefully
measured overlap. Complete game replays and a physical PS2 remain the final
tests; manual peak rates and emulator display counters are not game-speed
certification.

## Executed VU1 experiment

The original microprogram now passes an independent numerical fixture in Play
0.72: 168 program runs, 13,608 transformed vertices and 430,080 comparisons
covering complete VU data memory after completion fences. Its fingerprint is
`232e0bdb6fed9a5f`. Cases include zero and maximum-size batches, both banks,
inactive-bank preservation, exact dyadic matrices, perspective division,
fixed-point outputs and alternate-bank upload while VU work is outstanding.
The matrices produce clip W values of exactly 1, 2 or 4, so the division
checks cover exact power-of-two reciprocals, not general perspective precision.
This is no claim about arbitrary floating-point inputs or physical-console
throughput. It sends no XGKICK drawing packets and remains outside the game.

The initial source promotion reproduced the exact validated ELF, SHA-256
`8e26bd695aae5729727236cbd05ac47d001f709c7d766da17837e37135948366`.
The original microcode and fixture are in `platform/ps2/tests/vu1`; build with:

```sh
python3 platform/ps2/tools/build_vu1_fixture.py \
  --output build/ps2-vu1-check
```

Use a new output directory: the helper preserves previous evidence. It records
the pinned compiler/assembler, input hashes, executable segments and assembly
listing. Compilation alone is explicitly not an execution result.

The first run exposed an emulator diagnostic limitation: Play 0.72 returns
zero for direct TOP/TOPS register reads. Its
[release implementation](https://github.com/jpd002/Play-/blob/0.72/Source/ee/Vif.cpp)
has no cases for those reads. Bank verification instead uses addresses written by
VU1's XTOP instruction and complete memory/sentinel comparisons, keeping the
actual ownership test.

## Cooperation diagnostic: unresolved

The first combined run preserved 12,400 independently checked VU0 output
components and all VU1 data, but observed no outstanding work at VU0 entry.
Play's [launch implementation](https://github.com/jpd002/Play-/blob/0.72/Source/ee/Vpu.cpp)
executes up to 100 quotas of 5,000 instructions before returning to the EE.
Submission order alone therefore did not establish overlap.

A separate bounded stress entry repeats the same transforms. A two-iteration
control passes; a 1,024-iteration control reaches the existing 300-million
CP0-tick timeout before any VU0 conversion runs. This isolates that failure
from VU0 conversion, but its cause remains unresolved. The default fixture
keeps the passing baseline; reproduce the unresolved diagnostic explicitly:

```sh
python3 platform/ps2/tools/build_vu1_fixture.py --dual-unit \
  --output build/ps2-dual-vu-check
```

The timeout is retained. No cooperation, simultaneous execution or speedup
claim follows from these results. Busy-register snapshots are diagnostic
observations, not proof of physical instruction issue. Repeated final outputs
also cannot detect transient corruption overwritten by a later iteration;
the vertex counter records requested work, not measured execution. Production
VU1 rendering remains disabled pending the required numerical and ownership
validation.

The current default baseline was rebuilt and executed independently: ELF
SHA-256 `d185235be35d70a5611409c8614bd1ca18739636fa2209996bb1e9b5306c78d6`,
168 submissions, 430,080 complete-memory comparisons and the unchanged
fingerprint `232e0bdb6fed9a5f`. The optional diagnostic's unresolved result
does not replace this baseline execution record.
