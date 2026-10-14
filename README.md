# Sega Rally Championship native recompilation

A dedicated reverse-engineering project for Sega Rally Championship (1995,
Model 2A), based on the Daytona USA static recompilation tools and native
hardware implementation.

The target is the original game running as generated native C++ with shared
Model 2 hardware code, SDL3, and Metal on iPhone. This is reconstructed behavior
from the user's ROMs, not Sega's original source code.

## Current state

The repository is in bring-up. The expanded address-label profile produces
66,537 native i960 instructions and 46 compiled objects. The core executes
reset at `0x420`, copies and checks its RAM code, initializes the I/O chip,
and displays the original startup warning and Sega Rally 1995 logo. Independent
CPU execution agrees through 100 million instructions and 513 startup frames.
The longer logo run still stops at an unresolved indirect target; attract mode
and racing are not established. I/O, EEPROM, communication, UART, video, timers and screen
events use native hardware components with isolated reference tests. A shared
event queue preserves deadline and insertion order, with CPU IRQ delivery at
instruction boundaries. The runner charges pinned-reference opcode cycles
before memory effects and supplies device time at 25 MHz; this is not measured
PCB timing. The SCSP sound board remains bring-up work; its verified ROMs have
a dedicated importer. The actual game uploads and runs its 1,665-word TGP
program through Daytona's shared hardware code. These are bring-up checks, not a complete game
boot. The existing lifted-C iPhone prototype lives
separately in `../segarally95-recomp`; its rendering captures provide regression
evidence, not proof that this new runtime already runs a race.

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

The expanded profile has 403 annotated label candidates, three separately
recorded executed target, and checked RAM aliases. It retains 400 unresolved
indirect sites and 13 invalid decode stops for analysis. The default
`--seed-profile bootstrap` uses a smaller label set. Every generated RAM
instruction checks its live word(s) before execution. Generation success does
not prove that every game path has been found.

Tests for the bootstrap, instruction templates, native/reference execution,
and TGP inputs are:

```sh
python3 tests/test_segarally_bootstrap.py
python3 tests/test_segarally_native_templates.py
python3 tests/test_segarally_boot_parity.py --tgp --io --comm --control --video --screen --uart
python3 tests/test_i960_runtime_timing.py
python3 tests/test_segarally_tgp_import.py
python3 tests/test_segarally_video_import.py
python3 tests/test_segarally_sound_import.py
```

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
