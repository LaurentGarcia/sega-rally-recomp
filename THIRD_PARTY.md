# Third-party components

## PlayStation 2 development toolchain

The optional PS2 build uses the official [PS2dev](https://github.com/ps2dev/ps2dev)
container. Exact multi-architecture and platform image digests are pinned in
`platform/ps2/toolchain.lock.json`; the tested arm64 image is
`sha256:6b7c60ca61f40905e0aa22c0939707d91b6df2d5d8995f45c803c14031f06015`.
The installed EE compiler reports GCC 15.2.0. The image is the reproducible
binary pin; separately observed repository heads do not establish which
source revision built every installed library.

[PS2SDK](https://github.com/ps2dev/ps2sdk) supplies draw, graph, DMA, packet,
controller, fileXio, open libsd/freesd audio and embedded IOP modules. The installed SDK `LICENSE` is
AFL-2.0, SHA256
`1ecee940922a6886baccddd9133d17f1ce677d32c5a954fac8e48224f2766fe8`.
The inspected public API/source reference is
[`329e2bb43287e16af16c4dde1c614e2662c24468`](https://github.com/ps2dev/ps2sdk/tree/329e2bb43287e16af16c4dde1c614e2662c24468).
SDK libraries/modules are linked unchanged; no SDK sample implementation is
copied into the project's BSD source. Binary distributions must retain the
SDK license and source reference. Our source license does not replace its
terms. Open IOP modules are embedded locally with the SDK's `bin2c` tool;
generated module arrays and resulting ELFs remain outside source Git.
The PCM queue/RPC server in `platform/ps2/iop` is original code using that
public libsd API. It does not link or copy audsrv. The public gsKit texture
implementation at `8ef73d05f022d14b3ded7c70cba670fe333ed61a` was consulted for
PSMT4 CLUT transfer layout only; no gsKit code or dependency was added.

PS2dev build scripts at observed
[`594e2b21863a574e1923927f6349cceb87c88109`](https://github.com/ps2dev/ps2dev/tree/594e2b21863a574e1923927f6349cceb87c88109)
use MIT terms. The installed libstdc++ headers identify GPL-3.0-or-later with
the [GCC Runtime Library Exception 3.1](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html).
The compiler/toolchain and newlib retain their upstream notices. No proprietary
Sony SDK, BIOS or firmware is included in this target.

The separate local [Play!](https://github.com/jpd002/Play-) emulator is used
only for development ELF boot tests and is not linked or redistributed. The
downloaded macOS 0.72 DMG has SHA256
`ad9e03164a5bfe79c99e6d9d8c04db7b706a749eba29b33bef39bb7db2a36f36`.
The inspected upstream source reference is
`8de4a71f5215ef38e357ac77a07948eb3424448f`; its
[core license](https://github.com/jpd002/Play-/blob/8de4a71f5215ef38e357ac77a07948eb3424448f/License.txt)
is BSD-2-Clause. Its bundled dependencies retain their own licenses. No
emulator implementation is included in this project's source release.

A private checkout at that pin, under ignored
`build/ps2-play-version-dispatch-001/`, corrects dynamic-library version
resolution: independent major-version export tables coexist, newer minor
versions replace older ones, and release/save-state handling retains the
correct table. Built-in HLE fallback remains available. The private patch is
`version-dispatch.patch`, SHA256
`ae32008bf3647aef6dbb79c28dfa479ad55341dd53831bfb780ebf1dab8dfd87`.
Forty-six core checks passed. Paired headless runs of the same SDK pad probe
reproduced the transfer-lock stall without the patch and verified advancing
receive buffers and successful pad closure with it; the source-bound record
is `paired-runtime-001.json` in that directory. These runs used a neutral host
pad provider. Physical controller input, the unchanged GUI app and real PS2
hardware remain untested by this experiment. The patch and emulator binaries
remain private; no SDK or game input workaround was applied.

## Sega Rally project base and board reference

This tree starts from `alphanu1/daytona-arcade-recomp` at
`b33baeb28a1e1c94d90a4a80811f0b2c7f61106b` (BSD-3-Clause, original Git history
and notices retained). The shared `src/runtime/model2_priority.h` extracts its
existing geometry/raster/GPU priority functions without changing their rules.
The dedicated Sega Rally frontend retains the inherited `GpuRenderer` alongside
direct Metal/Vulkan adapters using the same generated shaders. Its SDL3
checkout uses the exact pin listed below without the Daytona launcher's Vulkan
application-name patch. The optional display panel uses the pinned Dear ImGui
core, SDL3 platform backend and SDL_GPU/Metal/Vulkan rendering backends listed
below, without local modifications.
Its default fonts are embedded by that same pinned `imgui_draw.cpp`, not copied
into this repository: ProggyClean (MIT, copyright 2004, 2005 Tristan Grimmer)
and ProggyForever-Regular-minimal (MIT, copyright 2026 Disco Hello and
2019, 2023 Tristan Grimmer). The upstream font records are in `docs/FONTS.md`;
original projects are https://github.com/bluescan/proggyfonts and
https://github.com/ocornut/proggyforever. Local Mac packaging retains both exact
font comment notices and the complete MIT terms in `ImGui-embedded-fonts.txt`,
alongside the unchanged Dear ImGui license. No font data is extracted or added
to source control.

The Linux Wayland player additionally compiles protocol interface code generated
by `wayland-scanner` from all 24 XML files in the pinned SDL checkout's
[`wayland-protocols/`](https://github.com/libsdl-org/SDL/tree/fa2c02bb6e21974a89ea9824bc53c9932abe5f9c/wayland-protocols)
directory. Those inputs are unchanged and remain outside source Git. Their
per-file copyright/permission terms are retained separately from SDL's zlib
license in `licenses/SDL-Wayland-protocols.txt` in the Wayland-enabled Linux
package, with input paths and hashes. Most carry MIT-style permission notices;
`text-input-unstable-v3.xml` carries its own permissive terms, including the
copyright-holder-name advertising restriction. The pinned
`pointer-gestures-unstable-v1.xml` supplies no per-file copyright block; the
notice inventory records that absence and also retains SDL's license. No
protocol XML or generated interface C is added to the source repository.
Wayland client/cursor/EGL and xkbcommon shared libraries are loaded from the
host system, rather than bundled in the player archive. This protocol notice
record describes the new Linux package; earlier Mac/Windows binaries and
the X11-only Linux preview do not add this compiled Wayland backend.

The Model 2A memory adapter is implemented from the public address-map contracts
in MAME `src/mame/sega/model2.cpp` at the same pinned MAME revision listed below,
`dddd73680656e355bb2b5beecab1167c9f07bf81` (BSD-3-Clause). The inspected file's
SHA-256 is `359d7fa309c0bca6c93f5c0826011a7054ae8f04068e9df995188a6256c995b6`.
`model2_base_mem`, `model2_tgp_mem`, and `model2a_crx_mem` supply the region,
mirror, and burst contracts. Unsupported device callbacks are explicit runtime
frontiers during bring-up; the board is not claimed complete.

The observed reset and legacy-I/O gaps at `0x00f80000` and `0x01c00202` use
MAME's unmapped read-zero and
discarded-write semantics from `src/emu/addrmap.cpp` (constructor,
`m_unmapval=0`) and `src/emu/emumem_heun.cpp` (unmapped handlers), same pinned
commit, BSD-3-Clause. These files were read for contracts, not copied.
SHA-256 respectively: `a7217d512c8b63517d2f17529411036f8f796d1e47c0fc0d33c54d2a75ced469`
and `3a4014cdb03eb6239b340832dd02b34ee17e1eeab0ded10f8824483448239d92`.
The adapter confines this behavior to the observed four-byte gaps and counts
its accesses; it does not silently answer unimplemented hardware devices.

The native `model2_io` register chip adapts MAME `src/mame/sega/315_5649.cpp`
and `.h` at the same pinned commit (BSD-3-Clause, Dirk Best). SHA-256:
`06cc79c2b00b478bbcbba7c5ec7735f0cb48fe25795bc5b3d94a77ebca03943d`
and `2524f4e9581f224e1494c176457654374851e496b31bb55096638af1d0f4e2f4`.
The framework becomes callbacks and explicit register state, with a peek flag
for reads without side effects. The reference's fixed serial-status shortcut
is retained and named. `model2a_io` adapts Rally's wiring/default inputs from
the pinned `model2.cpp` above; drive firmware execution is absent, with raw
drive/lamp output callbacks instead.

The native `model2_eeprom` adapts MAME's 93C46 behavior from
`src/devices/machine/eepromser.{cpp,h}` and `eeprom.{cpp,h}`, same commit,
BSD-3-Clause (Aaron Giles and contributors, notice retained). The respective
SHA-256 values are
`51431fe4a70163b0bf56a30bbb293c0ba8872e7cd6fe051a6bdf2e17f8824caa`,
`0877cdad4d2e8c300386d2dce1737c51210c40d94115837c00482b628a7adf74`,
`e28f650aa8eda8c54897ae08047092bab3053f4dd84c66c72d6711a950423ef6`,
and `415e84a2e995904615f4c7532c6b73664a4e513832c0bdbde1844af8ef8887fb`.
Device timers become explicit monotonic nanosecond arguments; NVRAM becomes
a 128-byte image with no host file access. MAME's non-streaming default,
write/erase completion edges, WRAL bitwise programming, and fixed busy times
remain reference contracts, not measured arcade hardware facts.
[ST M93C46 DS1077](https://www.st.com/resource/en/datasheet/m93c46-w.pdf),
revision 20 sections 5–8, was read as an independent command-format reference;
no text or code was copied. Its CS-falling programming behavior differs from
the pinned MAME implementation and is recorded in the EEPROM source.

`src/i960/timing.cpp` converts the fixed deductions from pinned MAME's
`src/devices/cpu/i960/i960.cpp` into compiler-only metadata, with the BSD-3
notice retained. Source SHA-256:
`f7d887add0cb070f91f585b22f3b35acc4fc94ab5c94a527eab5205ee9440fdd`.
Generated instructions charge the runtime's monotonic counter; the test-only
interpreter charges at its original deduction sites, independently of the
metadata table. Shared call/return helpers restore the reference's 9/7 costs.
Fixed guesses, zero-cost flushreg and absent burst-stall continuation remain
explicit limits; these are not measured hardware cycles.

`model2a_comm` supplies Model 2A's mapping to the unchanged inherited CommBoard
simulation listed below. The pinned `src/mame/sega/m2comm.cpp` and `.h` hashes
are `4fdc712070b8766e76ba03fdf64be1844f159ea48b6023ee865162973467d204`
and `6f626674189b0c14fb4a576f7e70a846213ca409dbf1ab19b435f8e533e15ae6`.
The header enables `M2COMM_SIMULATION`; the adapter supplies permanently closed
transport endpoints to preserve no-peer initialization without socket access.
This does not execute or replace the missing communication firmware.

`model2a_control` adapts the pinned `model2.cpp` IRQ, four-timer and delayed
enable handlers, preserving maskless acknowledgments and reset behavior.
`model2a_clock` is a native event queue whose insertion and screen-phase
contracts were checked against the same MAME revision. These additional
BSD-3-Clause reference files have SHA-256 hashes:

| Reference file | SHA-256 |
| --- | --- |
| `src/emu/schedule.cpp` | `d7a54f21126144df71ba9a4b40fc3451c5bef1096fe96f89653922015b3c21f9` |
| `src/emu/screen.cpp` | `00950a7da23d4468573dc0c1314b441821802ceccdf67a2e4b8320e6c6006ce0` |
| `src/emu/screen.h` | `a3ee394293462d5f4319349b09d8c70209ab9dce73dc58925508d633c7f4631b` |
| `src/emu/emumem.h` | `4285ca50a0c14b6038c7443bab45340834f0e4b741f0549a127c4ba7cc1c3e2e` |
| `src/emu/emumem_hedp.cpp` | `27b25423ab39477d1b719b8922dc7dab16f712cac43a24d4974890ad7af1aeee` |

`model2a_video` adapts the inherited M2Board mapping and pinned Model 2A
video/register contracts to the existing Video/Geo components. MAME framework
storage becomes owned vectors; exact mirrors and byte-lane handlers replace
coarse page mappings. Notices are retained. Additional source SHA-256 hashes
are `64511d032b7382df5ded6829b3f3e618c8f09fb602016dc7be35a7cd924e6892`
for `src/mame/sega/model2_v.cpp` and
`d6e2283b1e02e27fe4e28c78d8c86f6e3c2177e6c7db396e7b666d397a037b96`
for `src/mame/sega/segaic24.cpp`. The video ROM manifest records public
`srallycb` and `MODEL2A_VID_BOARD` size/checksum/loading metadata from the
pinned `model2.cpp`; no ROM content is stored in source control.

`model2_uart` adapts pinned MAME's asynchronous i8251 and serial-interface
logic (BSD-3-Clause, smf, Robbbert, Carl, Miodrag Milanovic, Vas Crabb; notices
retained). Device framework and internal timers become explicit physical pin
edges and callbacks. Idle sync programming/reset is supported; synchronous
activity is an explicit frontier. Reference frame/parity/reset quirks remain.
`model2a_uart` supplies the exact Model 2A lanes and clock-pin wiring from
the pinned `model2.cpp`. Additional source SHA-256 hashes:

| Reference file | SHA-256 |
| --- | --- |
| `src/devices/machine/i8251.cpp` | `274e29a659c75302acdc8ab746034b88c7e337e8122135eac9372db6b0832931` |
| `src/devices/machine/i8251.h` | `a99ee4e12ad03574fd99901ec5a88c84314c3972f5111be0897a07fd2f5f94f2` |
| `src/emu/diserial.cpp` | `34ab348975fcafebaf7f5154fcce722b942ea1aefe10a0f5f78296b467310803` |
| `src/emu/diserial.h` | `a787f56d47cc003b6f13918e81ba7115908de28549d3d69a8fc6da951637c53f` |
| `src/devices/machine/clock.cpp` | `97d1b6bf24b452c0984790a4ecc45c1b405f144d13d664ecbf1b575af2b75d4f` |

The sound manifest imports public `srallycb` layout/checksum metadata from
the same `model2.cpp`. Portable big-endian byte normalization was checked
against `src/emu/romload.cpp` (BSD-3-Clause), SHA-256
`854873d6f1b33d1dd3614a219b4b744f8fefb11708c8389141157825599f275f`.
`model2a_sound_bus` implements the same pinned file's `scsp_map` and reset
contract: 16 vector bytes copied to shared RAM, fixed program/sample windows,
SCSP word masks, ignored ROM writes and Rally's inert sample-bank register.
The unchanged 68000 instruction templates come from the Daytona base above;
the added address-space interface, map-aware reachability and live instruction
guards keep the existing Daytona mapping as the default. No SCSP device is
linked into this reset probe.
The optional timed generator derives instruction totals, data/prefetch counts,
mapped waits and autovector IACK costs from the pinned MAME M68000 microcode
(BSD-3-Clause, Olivier Galibert). `src/m68k/timing.cpp` retains that notice.
The cycle-only DIVU/DIVS helpers in `src/runtime/snd_cpu.cpp` adapt the same
microcode's bounded subtraction/restore/sign paths and retain its attribution.
The independent test generates public microcode handlers separately from the
production emitter; generated reference code remains ignored. Instruction
totals, architectural state and ordered writes are compared. The separate
sound-board tests retain the same pinned CPU's continuous register,
prefetch, reset and IRQ state, with a harness-owned mapped-bus callback. Its
generated microcode and arithmetic helpers are unchanged. Devices run on
separate instances of the shared board models under instruction-start MMIO;
physical bus-phase timing and independent chip parity remain outside this
comparison. Coverage includes synthetic UART/IRQ/PCM cases and the authenticated
actual program's first million instructions through initialization and 383
IRQ/RTE pairs, with idle serial and silent PCM. All program-derived inputs
and generated code remain ignored. Source hashes at the same revision:

| Reference file | SHA-256 |
| --- | --- |
| `src/devices/cpu/m68000/m68000.cpp` | `a1d4daea1f61cdc4779822c7e59d561ff48c7337aaa7f920854fe2c624d70883` |
| `src/devices/cpu/m68000/m68000.h` | `cdfb07d21b62dbf042cdebccd0d57d1be61cf84b8be257b3173408ab307425fb` |
| `src/devices/cpu/m68000/m68000.lst` | `9b4605ef5e0bd2bb2e17a03146c37c6cf4757cf926ab1b06352d95ab09695631` |
| `src/devices/cpu/m68000/m68000gen.py` | `87bcba366263ae97daa88fce3a082e85563bc903df89f0692f589d668aced05b` |

Sound-board IRQ line replacement and hold acknowledgment were checked against
`src/emu/diexec.cpp/.h` (BSD-3-Clause, Aaron Giles), unchanged reference inputs.
Their SHA-256 hashes are
`4205a1f2cc15ca8fa358646faea259c14dcbeaa83137ed996fe1ede8beb03fe7` and
`9c7dad8cbc1a1d52d166623a9ea349488c8b21748a923aeba95acb98cf3b5273`.

`model2_scsp` and `model2_scsp_dsp` adapt the pinned SCSP sources below
(BSD-3-Clause; ElSemi, R. Belmont, Carl, Miodrag Milanovic; notices retained).
Framework memory becomes shared big-endian RAM; timer/serial callbacks use
the common native event queue; sample output is a bounded stereo buffer.
An explicit random provider preserves the reference noise sequence. Chip and
DSP algorithms retain the reference's documented limitations. The component
is linked into isolated tests; whole-game sound CPU scheduling is separate.
The independent oracle compiles the following source unchanged behind test
host services:
`src/devices/sound/scsp.cpp/.h`, hashes
`11750fa03db0324cedc8a56ac481537df1991e58d3ef522e6ca737694dc1a259` and
`82636b35d049e2e07f0df5b778d1fd87cc3152e5a7d53fe0da64e3bd1206b1d5`;
`scspdsp.cpp/.h`, hashes
`9d4b1de166e5d7d5128b591c612eff3fd5cec78af79c1acabb9d5e93341917fd` and
`db522ecaa04665063e46f7f6c699eb7781c672fd7624ac58a60b0b180652d3cc`.
All are BSD-3-Clause at the same pinned MAME revision.
Additional SCSP scheduling/memory references at that revision have SHA-256:

| Reference file | SHA-256 |
| --- | --- |
| `src/emu/machine.cpp` | `a537d3033b37a795781e076ff55c76c11a7b17cc3f03617bf82d81809a875dbc` |
| `src/emu/disound.cpp` | `d65485c3597fbd93f6c8e16ce1c51204974fe58c8af4a846423eb2fd805df87c` |
| `src/emu/disound.h` | `fb1e938113a21aa8fa228dea42f2c9e3e1c4c9f42d6c247426759a4962a7fa4c` |
| `src/emu/emumem_aspace.cpp` | `2b7411f12eb6305070070f16ff781b5b80a14e52160a59eefc09c018c4bda03c` |
| `src/emu/dirom.h` | `3c2390ecb74fba46b4807813001fcbc76948dbb523e3c39f902562e0593f5da9` |
| `src/emu/attotime.cpp` | `bb397e1b1cf1908ad01471d40904cf8cfd362de74a1aa0a2e9e04e9a2dc52ec5` |
| `src/emu/attotime.h` | `a712bcf59c83af0ef38d64f15aa982427380377e3703443e5d7dad543efe15dd` |

This project's own code is under the BSD-3-Clause licence (`LICENSE`). The
code below keeps its own licence and notices; every component linked into the
game is under a licence compatible with it (BSD, zlib, MIT, public domain).

Every component this project fetches, links or reads from is recorded here:
upstream, pinned commit, licence, how it is used and what was changed.
Vendored checkouts live in git-ignored `extern/` and are never committed.

| Component | Upstream | Commit | Licence | Use | Changes |
| --- | --- | --- | --- | --- | --- |
| MAME `src/devices/cpu/i960/i960dis.cpp`, `i960dis.h` | https://github.com/mamedev/mame | `dddd73680656e355bb2b5beecab1167c9f07bf81` | BSD-3-Clause (file header) | Compiled unmodified into the `mame_oracle` test only, as the reference disassembler. Not linked into any tool or shipped binary. | None. `tests/mame_shim/emu.h` (ours) stands in for MAME's `emu.h`. |
| MAME `src/devices/cpu/i960/i960.cpp` (decoder reference) | same | same | BSD-3-Clause | Read: the executor's opcode set, operand, EA and branch-target rules are what `src/i960/decode.cpp` follows. | — |
| MAME `src/devices/cpu/i960/i960.cpp` (instruction semantics) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/cpu.cpp` (context, call/return, interrupts, memory; linked into the game) and `src/refcore/i960_ref.cpp` (the instruction interpreter; test oracle only, never linked into the game), BSD-3 notice kept in both. MAME's semantics are used verbatim so the runtime diffs cleanly against MAME in lockstep. Licence-compatible with any project licence. | Device framework and burst-stall machinery removed; reference deductions feed a monotonic cycle counter while generated code uses independent compiler metadata; memory through `rt::Bus`; `fatalerror` throws; interrupt-taken hook added. Every change is marked in the file. |
| MAME `src/mame/sega/model2*.cpp`, `src/mame/shared/segam1audio.cpp` | same | same | BSD-3-Clause | Read, not compiled: hardware figures in the design document. | — |
| MAME `src/devices/cpu/mb86233/mb86233.cpp` (TGP semantics) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/tgp.h` as inline templates on each instruction's constant fields; `tools/m2tgprecomp` emits calls to them, so the TGP program is statically recompiled. No MB86233 interpreter exists in this project. BSD-3 notice kept. | Split per ALU op, addressing mode and register into `if constexpr` templates; stall handled by returning to the caller; shift counts made explicit mod 32 and `s32(float)` made explicit (x86 result), matching MAME's x86-64 build on any host. |
| MAME `src/mame/sega/model2.cpp` (board registers: IRQ controller, timers, video/render mode, TGP ID, model2o memory map) | same | same | BSD-3-Clause | **Register semantics transplanted** into `src/runtime/m2_board.cpp`, the native board the game runs on outside lockstep. BSD-3 notice kept. The I/O board's mailbox protocol is not from MAME (which runs the board's Z80 BIOS): it was read from the game's own traffic in our traces. | No device emulation: the I/O board, sound UART and comm board are native runtime code answering the game's protocol. |
| MAME `src/mame/sega/segaic24.cpp` (System 24 tilemap chip), `model2_v.cpp`/`model2.cpp` (`screen_update`, `palette_w`, `colorxlat_w`, CRTC sync handlers) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/video.cpp`: `tile_info`, `draw_rect` (rgb32), `draw_common` with its special scroll/window modes, the palette pen rules and the screen composition order. BSD-3 notices kept. | MAME's generic tilemap engine (`src/emu/tilemap.cpp`, read for its pixel rule, not copied) replaced by a direct pixmap build and a one-scroll-value draw with the same category/opacity test. |
| MAME `src/mame/sega/model2rd.ipp`, `model2_v.cpp` (renderer) and `src/devices/video/poly.h` (triangle/polygon setup) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/raster.cpp`: `model2_3d_project`, `render_polygons` order, `model2_3d_render`, the scanline shaders and bilinear texel fetch, and poly.h's `render_triangle`, `render_polygon`, `round_coordinate`. The CPU reference for the 3D layer. Adapted in `src/app/gpu/gpu_renderer.cpp` and `m2.hlsl` for texture shading and optional triangle/polygon edge/span setup. BSD-3 notices kept. | CPU work is drawn immediately instead of through poly.h's queue (same per-pixel order); overflowing/NaN float conversions pinned to the x86 result. The GPU adaptation uploads polygon edge chains or triangle slopes/parameter planes, rejects fragments outside rounded spans and computes texture parameters per fragment; CPU repeated-addition rounding is not reproduced exactly. |
| MAME `src/mame/sega/model2_v.cpp` (geometrizer HLE, rasterizer front end) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/geo.cpp`: `geo_parse` and every geometrizer command, `model2_3d_push`, `model2_3d_process_polygon`, `float_to_zval`, `clip_polygon`, `check_culling`, the 3D math helpers. BSD-3 notice kept. The original Model 2's geometrizer runs undumped code from ROM inside its DSP, so MAME's HLE is the reference. | Raw pointers become bounds-checked cursors (running off memory is a hard error, not undefined behaviour); kept polygons go to a flat list instead of per-z buckets; `logerror` dropped, `fatalerror` throws. |
| MAME `src/mame/sega/model2.cpp` (`model2_tgp_state` TGP I/O; copro and geo ports) | same | same | BSD-3-Clause | **Code transplanted** into `src/runtime/tgp.cpp` (sin/cos, atan, 1/x, 1/sqrt table ports, banked window) and `src/runtime/m2_tgp_board.cpp` (`copro_ctl1_w`, function port, FIFO ports, `geo_r`/`geo_w`/`geo_prg_w`/`geo_ctl1_w`, `push_geo_data`, buffer RAM reset value). BSD-3 notice kept in both. | Memory through `rt::TgpBus`; MAME's device FIFOs, halts and stalls replaced by running the recompiled TGP on demand; an out-of-range geometrizer push is a hard error (MAME writes past the array). |

| `patches/mame/0001-i960-indirect-branch-harvest.patch` | ours, against MAME `dddd7368` | — | BSD-3-Clause, as the file it patches | Applied by `scripts/fetch_mame.sh`. Logs every `bx`/`balx`/`callx`/`calls` target, interrupt vector target, ICR value set by `synmov` and IAC reinitialise target to `$M2TRACE_BRANCHES` at exit; counts completed instructions; `$M2TRACE_IRQLOG` logs interrupt lines and takes by instruction count; `$M2TRACE_PCLOG` logs count, PIP, AC and a register hash per instruction. All off unless the variables are set. | Adds `device_stop`, an instruction counter and log points to `src/devices/cpu/i960/i960.{h,cpp}`. |
| `patches/mame/0002-tgp-and-geometrizer-log.patch` | ours, against MAME `dddd7368` | — | BSD-3-Clause, as the files it patches | Applied by `scripts/fetch_mame.sh`. `$M2TRACE_TGPLOG`: the TGP program upload and every TGP-side FIFO pop and push, bank write and banked memory access, in order. `$M2TRACE_TGPPC`: PC, A, B, D, P and ST after each TGP instruction (capped by `$M2TRACE_TGPPC_MAX`). `$M2TRACE_GEOLOG`: each vblank with the i960 instruction count, every word the geometrizer hands the rasterizer, every polygon kept after culling and clipping, a hash of each rendered 3D layer with the CRTC offsets, and each screen update's i960 instruction count and composed-screen hash (`$M2TRACE_FBDUMP_DIR`/`$M2TRACE_FBDUMP_EVERY` dump raw frames). Off unless set. | Log points in `src/mame/sega/model2.cpp` (TGP register-file FIFO handlers wrapped, `screen_vblank`), `src/mame/sega/model2_v.cpp` (`model2_3d_push`, `model2_3d_process_polygon`, `render_polygons`, `screen_update`) and `src/devices/cpu/mb86233/mb86233.cpp`; `i960.h` gains a public `insn_count()` (patch 0001). |
| `patches/mame/0003-sound-cpu-log.patch` | ours, against MAME `dddd7368` | — | BSD-3-Clause, as the files it patches | Applied by `scripts/fetch_mame.sh`. `$M2TRACE_SNDLOG`: the sound 68000's instruction count, every interrupt it takes, its registers every 4,096 instructions and after each interrupt, and every device access (UART, MultiPCMs and their banks, YM3438) with the index of the instruction making it. Off unless set. | Counter and log points in `src/devices/cpu/m68000/m68000.{h,cpp}`; the sound board's device handlers in `src/mame/shared/segam1audio.cpp` wrapped to log. |
| MAME `src/mame/sega/m2comm.cpp`, `m2comm.h` (Model 2 communication board, `M2COMM_SIMULATION`) | https://github.com/mamedev/mame | `dddd73680656e355bb2b5beecab1167c9f07bf81` | BSD-3-Clause (Ariane Fugmann) | **Code transplanted** into `src/runtime/comm_board.{h,cpp}`: the link protocol (shared RAM set-up, the ring's numbering tokens, per-frame data and vsync frames), linked into the game for link play. BSD-3 notice kept. | `osd_file` sockets replaced by a non-blocking `LinkTransport` (TCP in `src/app/link_socket.cpp`, ours), so partial frames are kept until whole; the frame-sync wait gives up after 3 s (MAME waits forever); a closed line on either side loses the link; verbose logging dropped. |
| MAME `src/devices/sound/multipcm.cpp`, `gew.cpp` (Yamaha YMW-258-F MultiPCM) | https://github.com/mamedev/mame | `dddd73680656e355bb2b5beecab1167c9f07bf81` | BSD-3-Clause (Miguel Angel Horna) | **Code transplanted** into `src/runtime/multipcm.{h,cpp}`: register writes, sample header loading, envelope, LFO and the sample loop, linked into the game. BSD-3 notice kept. | Device framework, streams, save states and `device_rom_interface` removed (sample ROM and bank as plain members); a write to a slot number with no voice is ignored (MAME indexes past its array). |
| MAME `src/devices/cpu/m68000/m68kdasm.cpp` | same | same | BSD-3-Clause | Read, not compiled into anything we ship: `src/m68k/decode.cpp`'s `format_mame` reproduces its text so decodes can be checked against it (done once over the whole sound program with MAME's built `libdasm`). | — |
| ymfm | https://github.com/aaronsgiles/ymfm | `81aec25ccbb98f4873a255f7551ac4dadac59b4a` | BSD-3-Clause | Linked into the runtime: the YM3438 (`ymfm::ym3438`) on the sound board. Fetched by `scripts/setup.py` into git-ignored `extern/ymfm`. The OPN2 path is identical to MAME's copy at `dddd7368`. | None. |
| MAME Lua scripting API (`docs/source/luascript/`) | same | same | BSD-3-Clause | Read, not compiled: the API `tools/mame-plugins/m2trace` is written against. No code copied. | — |
| Berkeley SoftFloat 3e | https://github.com/ucb-bar/berkeley-softfloat-3 | `a0c6494cdc11865811dec815d5c0049fba9d82a8` | BSD-3-Clause | Linked into the `i960` library as the extF80 reference model for i960 FP (`src/i960/fp.cpp`, `ref_*`). Fetched by `scripts/fetch_softfloat.sh`; built from `cmake/softfloat_sources.cmake` (FAST_INT64, 8086-SSE specialisation). | None. Built with its own options; `THREAD_LOCAL` set so rounding mode and flags are per thread. |
| SDL 3 | https://github.com/libsdl-org/SDL | `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c` (release-3.4.16) | Zlib | Linked statically into the `daytona` executable: window, input, gamepads, and SDL_GPU (Vulkan, Direct3D 12, Metal) to present frames. Fetched by `scripts/setup.py` into git-ignored `extern/sdl3`, built from source with the project. | `patches/sdl3/0001-vulkan-application-name.patch`, applied by `scripts/setup.py`: the Vulkan instance reports the application's name (SDL_SetAppMetadata) and no engine name instead of "SDLGPU", so overlays such as MangoHud show Vulkan. |
| Dear ImGui | https://github.com/ocornut/imgui | `f1cc2ae15e53a861a874c3034aae6798fde194ab` (v1.92.9b) | MIT | Linked into the `daytona` launcher and Sega Rally's optional display panel, with its SDL3 and SDL_GPU backends (shaders precompiled upstream for SPIR-V, DXIL, MSL). Direct players additionally use the unchanged `imgui_impl_metal.mm` or `imgui_impl_vulkan.cpp` backend for the same Settings interface. Fetched by `scripts/setup.py` into git-ignored `extern/imgui`. | None. |
| 7-Zip LZMA SDK (`C/` of 7-Zip) | https://github.com/ip7z/7zip | `0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17` (26.03) | Public domain (only files whose headers state "Public domain" are built: 7zArcIn, 7zDec, 7zBuf, 7zCrc, 7zCrcOpt, 7zStream, 7zAlloc, Alloc, LzmaDec, Lzma2Dec, Bra, Bra86, BraIA64, Bcj2, Delta, CpuArch, Ppmd7, Ppmd7Dec, Sha256, Sha256Opt, Sha1, Sha1Opt; none of 7-Zip's LGPL files) | Linked into the runtime to read 7z ROM sets (LZMA, LZMA2, PPMd, branch filters). Fetched by `scripts/setup.py` into git-ignored `extern/lzma`. The separate `sha256_digest` library wraps `Sha256.c`, `Sha256Opt.c` and `CpuArch.c` for bounded capture-identity hashing; linked into the Rally player for verified ROM import and optional input recording/replay. The Rally digest target also links unchanged public-domain `Sha1.c` and `Sha1Opt.c` for the pinned ROM SHA1 identities. The original ZIP reader supplies bounded deflate/CRC handling; no 7-Zip archive decoder is added by Rally import. The wrapper hashes imported images, loaded capture inputs and per-resume PCM. | Upstream files unchanged; archive reader built with `Z7_PPMD_SUPPORT`. The SHA-256 wrapper selects `SHA256_ALGO_SW` through the public per-context API without global initialization. |
| pypcode | https://github.com/angr/pypcode | PyPI 3.3.3 | BSD-2-Clause | Test-only: its SLEIGH compiler and decoder run the i960 module in `tests/ghidra_oracle.py`. Not shipped; the test skips (77) without it. | — |
| ghidra_i960 (third-party Ghidra i960 processor module) | https://github.com/mumbel/ghidra_i960 | `727ef7872c5b1cd6ceb5a81f5e474d1ced92945c` | Apache-2.0 | Test-only second decode reference (SLEIGH), and the Ghidra module for the analysis workbench. Fetched by `scripts/fetch_ghidra_i960.sh` into git-ignored `extern/`. Not shipped. | None; compiled `.sla` is a local build artefact. |
| lupa (Python binding with Lua 5.4) | https://github.com/scoder/lupa | PyPI release, not pinned | MIT | Test-only: runs the plugin's Lua under `tests/lua_*_test.py`. Not shipped. Tests skip (exit 77) without it. | — |

Fetch with `scripts/fetch_mame.sh`, which checks out exactly the commit above.

Supermodel (https://github.com/trzy/Supermodel, GPL-3.0,
`Src/Model3/DriveBoard/WheelBoard.cpp` at its master branch, read in October
2026) was read as a reference only, for the meanings of Sega's drive board
commands. No code was taken from it. Daytona's drive board turned out to use
other meanings; `src/runtime/drive_board.h` now follows Daytona's own drive
board program (EPR-16488A), read and run in MAME.

The Model 2 MiSTer core (https://github.com/alphanu1/sega-model2-mister,
GPL-3.0-or-later, read at `591e148e87d27e03d50cbf7318bf0b1d1328c4bf`) is read as
a reference only. Nothing is lifted from it: it is GPL, and this project is
BSD-3-Clause (`LICENSE`).

## Windows GCC and MinGW-w64 runtime

The direct Vulkan Windows cross build generates its loader import library from
KhronosGroup/Vulkan-Loader tag `v1.3.275`, commit
`e2b7bb120754cc580f2100bef353c1dfb2277b6a`:
[loader/vulkan-1.def](https://github.com/KhronosGroup/Vulkan-Loader/blob/e2b7bb120754cc580f2100bef353c1dfb2277b6a/loader/vulkan-1.def).
This unmodified Apache-2.0 export list (Khronos, Valve and LunarG notices
retained) is used only by `x86_64-w64-mingw32-dlltool` to make
`libvulkan-1.a`. Its SHA256 is
`166014c0b24e6d6b728a05fed1ea4b6a123ed2ac5d98f63f3a176da4e93ed75d`;
the upstream LICENSE.txt SHA256 is
`43c0a37e6a0fa7ff3c843b3ec5a4fac84b712558ddac103fbd4c1649662a9ece`.
Vulkan 1.3.275 development headers come from the isolated toolchain's
`libvulkan-dev` package. These dependencies and generated import objects stay
in ignored build directories. No Vulkan loader DLL is copied from Wine or
bundled with the player; Windows obtains `vulkan-1.dll` from its installed GPU
driver. The loader import-library build is not a Windows GPU execution test.

The local Windows x86-64 player build uses Ubuntu's
`g++-mingw-w64-x86-64-posix` and `gcc-mingw-w64-x86-64-posix-runtime`
`13.2.0-6ubuntu1+26.1`, with `mingw-w64-common` and
`mingw-w64-x86-64-dev` `11.0.1-3build1`. The three unmodified runtime DLLs below
are copied beside the game executable; no compiler is required to run it.
These are separately licensed runtime components, not BSD-licensed project code.

| Component | Upstream reference | License and use |
| --- | --- | --- |
| GCC 13.2.0 `libstdc++-6.dll`, `libgcc_s_seh-1.dll` | [GCC source](https://gcc.gnu.org/), upstream release commit [`c891d8dc23e1a46ad9f3e757d09e57b500d40044`](https://github.com/gcc-mirror/gcc/tree/c891d8dc23e1a46ad9f3e757d09e57b500d40044) | GPL-3.0-or-later with [GCC Runtime Library Exception 3.1](https://github.com/gcc-mirror/gcc/blob/c891d8dc23e1a46ad9f3e757d09e57b500d40044/COPYING.RUNTIME). C++ standard-library and compiler-support runtime; no project changes to the DLLs. |
| MinGW-w64 11.0.1 `libwinpthread-1.dll` and toolchain CRT support | [MinGW-w64 source](https://github.com/mingw-w64/mingw-w64), upstream release commit [`c3e587c067a00a561899d49d3e63a659e38802ec`](https://github.com/mingw-w64/mingw-w64/tree/c3e587c067a00a561899d49d3e63a659e38802ec) | [Winpthreads COPYING](https://github.com/mingw-w64/mingw-w64/blob/c3e587c067a00a561899d49d3e63a659e38802ec/mingw-w64-libraries/winpthreads/COPYING): MIT and retained BSD-3-Clause terms for Lockless-derived parts. Other CRT/header terms are retained in the MinGW-w64 package copyright notice, including ZPL-2.1 and per-file exceptions. No project changes. |

The upstream commits identify release references; the actual binaries come from
the Ubuntu package versions above, including distro packaging. Exact bundled
DLL identities are:

| File | SHA256 |
| --- | --- |
| `libstdc++-6.dll` | `dc04a17d6e9fe93bcb0639e93aca2811ad53c81c523e1909a2c24daf827fd2cd` |
| `libgcc_s_seh-1.dll` | `306983c483d96a6d08a2df044e360f0962e21dac021baf8311d3c0cb3b2c9def` |
| `libwinpthread-1.dll` | `fc9f14551f6609dd51801daec2c194579d35345551ef84fe465cbdb3339c2a6f` |

Windows packages retain the installed GCC and MinGW-w64 copyright notices,
GPL/LGPL texts and GCC runtime exception in `licenses/compiler-runtime/`.
The game executable and these libraries are distinct components; the exception
does not relicense the runtime libraries as BSD. Windows system DLLs are
provided by Windows and are not copied into the package. No runtime DLL or
compiler binary is committed to this source repository.

## Shader compilers (tools only)

The hardware renderer's shaders (`src/app/gpu/m2.hlsl`, including the MAME
adaptations recorded above) are compiled by `scripts/build_shaders.py` into
`src/app/gpu/shaders_gen.h`. The shader compilers below are not part of the
game or linked into it.

| Tool | Version | Licence | Used for |
| --- | --- | --- | --- |
| [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) | v1.9.2609 (`linux_dxc_2026_09_28`) | University of Illinois/NCSA | HLSL to SPIR-V and DXIL |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Ubuntu 24.04's package | Apache-2.0 | SPIR-V to MSL |

Vita GPU display buffering builds MIT libvita2d at
`a8f15ab09d5233f0a4e4ad0e8f6ade0da888cbed` from
https://github.com/xerpi/libvita2d. The source hash, small display-ring adapter
and shipped license are documented in [the Vita dependency record](platform/vita/THIRD_PARTY.md).
