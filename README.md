# Sega Rally Championship native recompilation

A dedicated reverse-engineering project for Sega Rally Championship (1995,
Model 2A), based on the Daytona USA static recompilation tools and native
hardware implementation.

The target is the original game running as generated native C++ with shared
Model 2 hardware code, SDL3, and Metal on iPhone. This is reconstructed behavior
from the user's ROMs, not Sega's original source code.

## Current state

The repository is in bring-up. The expanded profile produces
73,945 native i960 instructions and 51 compiled objects. The core executes
reset at `0x420`, copies and checks its RAM code, initializes the I/O chip,
and displays the original startup warning and Sega Rally 1995 logo. Independent
CPU execution agrees through 321,115,347 instructions and 1,634 rendered frames,
including the first car/course scene.
An exact two-coin/start/steering/gas replay reaches Car Select and selects the
automatic Lancia. Independent CPU/device parity follows that path through
376,736,993 instructions and 1,915 rendered frames, stopping at a missing
callback during the transition after selection. The versioned replay is
`config/segarally_lancia_select.inputs`; playable racing remains unverified.
The longer native run renders the Martini Lancia, another rally car and a
forest/gravel course before an unresolved callback. This establishes a first
3D scene; completed attract mode and racing are not established. I/O, EEPROM, communication, UART, video, timers and screen
events use native hardware components with isolated reference tests. A shared
event queue preserves deadline and insertion order, with CPU IRQ delivery at
instruction boundaries. The runner charges pinned-reference opcode cycles
before memory effects and supplies device time at 25 MHz; this is not measured
PCB timing. An optional diagnostic connects the generated sound 68000, native
SCSP and physical serial link. Instruction totals, waits and ordered writes
pass independent public-microcode comparisons; exact bus phases remain
unverified. The linked machine reaches real sound IRQs and another native-code
frontier. Whole-game audio and playback are not established. The SCSP chip
passes isolated sample, DSP, timer and serial comparisons. The actual game uploads and runs its 1,665-word TGP
program through Daytona's shared hardware code. These are bring-up checks, not a complete game
boot. The existing lifted-C iPhone prototype lives
separately in `../segarally95-recomp`; its rendering captures provide regression
evidence, not proof that this new runtime already runs a race.

The dedicated SDL3/Metal frontend builds on macOS and iPhone. Its first native
iPhone package is installed as **Sega Rally Native**, separately from the
earlier prototype. It launches and initializes the A15 Metal device; phone frame
capture and gameplay validation remain pending. On macOS,
the startup warning frame matches the CPU renderer pixel for pixel. A paired
3D car capture has 8.31% differing pixels; a frozen-scene diagnostic attributes
most average error to whole-polygon versus triangle-fan interpolation. Exact
graphics fidelity and racing-scene parity remain unresolved.

Daytona's instruction decoder, C++ generator, CPU services, geometry, rasterizer,
SDL3 host, Metal shaders, and tests form the starting codebase. Game-specific
ROM layouts, entry points, hardware mappings, and sound behavior must be checked
before reuse. The existing Daytona build targets remain as reference targets.

## Reproducible inputs

Supply your own Sega Rally ROM archive. ROMs, extracted images, generated game
C++, display-list captures, and compiled apps belong in ignored local folders.
They are never committed. Verified manifests, address lists, source tools, and
tests are versioned.

With Python 3 and a C++20 compiler, run:

```sh
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip --seed-profile lift-labels
python3 scripts/segarally_tgp.py --roms /path/to/srallyc.zip
python3 scripts/segarally_video.py --roms /path/to/srallyc.zip
python3 scripts/segarally_boot.py --instructions 100000000 --with-tgp --with-io --with-comm --with-control --with-video --with-screen --with-uart --capture-frame
```

The importer verifies the Revision B (`srallycb`) program pair present in the
supplied archive by published size, CRC32, and SHA1. It rejects mismatches.
Output goes to `build/segarally/`: imported images, generated C++, native
objects, `coverage.json`, and the bounded execution report `boot.json`.
The boot command exits 3 for an unimplemented board access or 4 for a missing
code target; those are incomplete frontiers, not successful game boots.
Use `--instructions 10000000` to run past the initial RAM-copy loop; the
default one-million-instruction limit stops inside it.
`--step-frames` exercises the same resumable machine one rendered frame at a
time while retaining the exact instruction limit.
`--input-replay FILE` additionally supplies complete cabinet input snapshots
at completed rendered-frame boundaries; it requires `--with-screen --with-io`.
See [input replay format](docs/segarally-input-replay.md). The runner saves
the exact input file and its SHA-256 beside the boot report, and records each
applied event's instruction/cycle count. It does not change game memory.

The expanded profile has 403 annotated label candidates, seven separately
recorded executed targets, three checked table-derived targets, and RAM aliases. It retains 402 unresolved
indirect sites and 13 invalid decode stops for analysis. The default
`--seed-profile bootstrap` uses a smaller label set. Every generated RAM
instruction checks its live word(s) before execution. Generation success does
not prove that every game path has been found.

Tests for the bootstrap, instruction templates, native/reference execution,
and TGP inputs are:

```sh
python3 tests/test_segarally_bootstrap.py
python3 tests/test_segarally_table_seeds.py
python3 tests/test_segarally_continuations.py
python3 tests/test_segarally_native_templates.py
python3 tests/test_segarally_boot_parity.py --tgp --io --comm --control --video --screen --uart
python3 tests/test_i960_runtime_timing.py
python3 tests/test_segarally_tgp_import.py
python3 tests/test_segarally_video_import.py
python3 tests/test_segarally_sound_import.py
python3 tests/test_segarally_sound_recompile.py
python3 tests/test_segarally_sound_link.py
```

The separate sound reset probe uses the same verified archive:

```sh
python3 scripts/segarally_sound.py --roms /path/to/srallyc.zip
python3 scripts/segarally_sound_recompile.py
```

Its successful audit means the expected SCSP access frontier was reached;
it does not mean audio played. This default reset probe counts completed
instructions without a board clock or asynchronous interrupts.

For the optional instruction-timed sound diagnostic, generate and audit the
sound core before linking it to the main board:

```sh
python3 scripts/segarally_sound_recompile.py --timed
python3 scripts/segarally_boot.py --instructions 100000000 --with-tgp --with-io --with-comm --with-control --with-video --with-screen --with-uart --with-sound
```

Use matching `--build-dir` arguments for a nondefault import. An explicit
`--sound-build-dir` can select a separately audited sound build. Changed source,
assets or generated code require a new audit. This mode connects serial pins,
SCSP samples and IRQs; it reports native-code frontiers and does not yet play
audio through the host. MMIO remains at instruction boundaries.

After importing/generating the main, TGP and video inputs above, build the
dedicated frontend with the pinned SDL3 checkout described in
[the host instructions](platform/segarally/README.md):

```sh
SEGARALLY_NATIVE_BUILD="$PWD/build/segarally" bash platform/segarally/build.sh mac
SEGARALLY_NATIVE_BUILD="$PWD/build/segarally" bash platform/segarally/build.sh ios
```

The current controls are touch and keyboard. EEPROM/backup RAM persist across
launches; code or hardware frontiers leave a diagnostic instead of being skipped.

See [the project design](docs/sega-rally-design.md) for scope and validation.
Full MAME tracing currently needs three files absent from the supplied archive:
`epr-17891.ic12` (drive), `epr-16726.bin` (communication), and `epr-18022.ic2`
(billboard). This does not prevent native CPU/TGP bring-up; a full reference
race has not been captured.
The [inherited Daytona documentation](docs/daytona-upstream-readme.md) describes
the upstream reference build; its claims do not establish Sega Rally support.

## Source base

Started from [`alphanu1/daytona-arcade-recomp`](https://github.com/alphanu1/daytona-arcade-recomp)
at `b33baeb28a1e1c94d90a4a80811f0b2c7f61106b`. The upstream Git history and
BSD-3-Clause license are retained. `daytona-upstream` is a read/reference remote;
this project currently has no publication remote.

The shared polygon-priority component is used by the native CPU and GPU
renderers. It keeps Model 2 window, sort-bucket, submission-order, and face rules
consistent; camera-space depth does not substitute for arcade polygon priority.
