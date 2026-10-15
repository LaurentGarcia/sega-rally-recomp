# Sega Rally Championship native recompilation

Sega Rally Championship (1995, Model 2A) running as native C++ generated from
verified personal ROMs, with native board devices, SDL3 and Metal on macOS.

**v0.1.0-rc.1 is an experimental source prerelease.** Build your own local app
from ROMs you supply. This project distributes source and analysis, not the game,
ROMs, extracted assets or a ready-to-play app. macOS is the active target;
phone work is deferred.

## What works

- Windowed Mac play with native SCSP sound, keyboard and recognized SDL gamepads.
- Analog steering and pedals, remappable controls, cabinet saves and paused Settings.
- Optional CRT effects and Accurate textures, off by default on a fresh setup.
- Cabinet-input recordings and exact same-executable offline replay.
- Recorded Championship paths through Desert and Forest clears, plus a separate
  SHORT Practice Mountain finish, name entry, scoreboard and return to attract.

**Full Championship completion is not established.** Championship Mountain and Practice Lakeside still
lack finish evidence. Full-game graphics, extended-precision floating
point, physical timing and full MAME-machine/PCB parity remain open. A measured
playback-buffer starvation bug is fixed; broader listening feedback is still
needed. See [evidence and limits](docs/segarally-status.md).

## Build and play on Mac

Start with [getting started](docs/getting-started.md) to install the required tools
and three pinned source dependencies. Then run from the checkout:

```sh
python3 scripts/segarally_mac.py --roms /path/to/srallyc.zip \
    --output build/segarally-player --jobs 4
```

The pipeline verifies Revision B ROM identities, generates main CPU/TGP/sound
code, builds the Metal host, runs its tests and creates an ad-hoc signed local
app. Open `build/segarally-player/mac/Sega Rally Native.app`. The app targets
the build Mac's architecture and is not notarized. Keep it and its personal
assets local.

Allow startup checks to finish. Press and release **C twice**, then **Return**
to start with fresh cabinet settings. **Left/Right** steer, **Z** accelerates,
**X** brakes and **V** changes view. **F1** or controller **Y** opens Settings.
The [player manual](docs/mac-player-manual.md) covers controller defaults,
Practice selection, saves and recording.

## How it works

The importer verifies your ROM bytes against committed manifests. Static
recompilers turn reached i960, TGP and 68000 instructions into local C++ build
artifacts. A shared runtime supplies Model 2A memory, events, I/O, video and sound;
SDL3 handles the Mac window, controls, audio output and Metal presentation.
Missing code or unsupported hardware stops with a diagnostic rather than skipping
game behavior. See the [architecture](docs/segarally-architecture.md).

The [documentation index](docs/README.md) links the build guide, player manual,
replay contract, design and engineering record. Inherited Daytona targets remain
separate reference targets; their results do not establish Rally compatibility.

## Source, credits and boundaries

Started from [alphanu1/daytona-arcade-recomp](https://github.com/alphanu1/daytona-arcade-recomp)
at `b33baeb28a1e1c94d90a4a80811f0b2c7f61106b`, retaining its history and
[BSD-3-Clause license](LICENSE). Dependency revisions, licenses and retained
notices are recorded in [THIRD_PARTY.md](THIRD_PARTY.md). This project reconstructs
behavior; it is not Sega's original source code and is not affiliated with Sega.
Sega Rally Championship and related names belong to their respective owners.

Only original tools, hardware code, manifests, address annotations, tests and
analysis belong in the source repository. Personal ROMs, generated game code,
extracted images, captures and apps remain ignored. No leaked source or SDK is
used. The public source repository is
[LaurentGarcia/sega-rally-recomp](https://github.com/LaurentGarcia/sega-rally-recomp).

For changes and feedback, read [Contributing](CONTRIBUTING.md) and the
[prerelease scope](docs/releases/v0.1.0-rc.1.md).
