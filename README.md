# Sega Rally Championship native recompilation

A dedicated reverse-engineering project for Sega Rally Championship (1995,
Model 2A), based on the Daytona USA static recompilation tools and native
hardware implementation.

The target is the original game running as generated native C++ with shared
Model 2 hardware code, SDL3, and Metal on iPhone. This is reconstructed behavior
from the user's ROMs, not Sega's original source code.

## Current state

The repository is in bring-up. The initial annotated seed set produces 4,081
native i960 instructions and four compiled objects. The bounded native core
executes reset at `0x420` and currently stops after 88 instructions at an
unimplemented read of `0x00f80000`. This is a reported bring-up frontier, not
a complete game boot. The existing lifted-C iPhone prototype lives
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
python3 scripts/segarally_recompile.py --roms /path/to/srallyc.zip
python3 scripts/segarally_boot.py
```

The importer verifies the Revision B (`srallycb`) program pair present in the
supplied archive by published size, CRC32, and SHA1. It rejects mismatches.
Output goes to `build/segarally/`: imported images, generated C++, native
objects, `coverage.json`, and the bounded execution report `boot.json`.
The boot command exits 3 for an unimplemented board access or 4 for a missing
code target; those are incomplete frontiers, not successful game boots.

The initial coverage report has 71 indirect sites and 10 invalid decode stops
requiring analysis. Generation success does not prove that every game path has
been found. Tests for the bootstrap and added instruction templates are:

```sh
python3 tests/test_segarally_bootstrap.py
python3 tests/test_segarally_native_templates.py
```

See [the project design](docs/sega-rally-design.md) for scope and validation.
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
