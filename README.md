# Sega Rally Championship native recompilation

A dedicated reverse-engineering project for Sega Rally Championship (1995,
Model 2A), based on the Daytona USA static recompilation tools and native
hardware implementation.

The target is the original game running as generated native C++ with shared
Model 2 hardware code, SDL3, and Metal on iPhone. This is reconstructed behavior
from the user's ROMs, not Sega's original source code.

## Current state

The repository is in bring-up. The i960 generation pipeline and Model 2A memory
mapping are being validated. A complete Sega Rally game executable from this
repository is not yet verified. The existing lifted-C iPhone prototype lives
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
