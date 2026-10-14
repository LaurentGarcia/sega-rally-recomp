# Sega Rally project wiki

The active target is **macOS**. Phone work is deferred. This wiki covers the
generated Sega Rally Championship Revision B (`srallycb`) runtime and its
dedicated SDL3/Metal host.

| Read this | For |
| --- | --- |
| [Mac player manual](mac-player-manual.md) | Starting a prepared app, controls, coins and troubleshooting |
| [Build and validation](segarally-build-validation.md) | Importing personal ROMs, recompiling, building and reproducing checks |
| [Technical architecture](segarally-architecture.md) | Native execution, device timing, graphics and optional sound |
| [Evidence and status](segarally-status.md) | Observed behavior, measurements and open validation gaps |
| [Cabinet input replay](segarally-input-replay.md) | Replay format and input boundary semantics |
| [Project design](sega-rally-design.md) | Rally scope, hardware contracts and detailed findings |
| [Handoff](../HANDOFF.md) | Dated engineering record, hashes and local evidence paths |

[Project rules](../rules.md) and the
[inherited design](daytona-usa-recomp-design.md) govern engineering work.
The inherited [getting-started guide](getting-started.md),
[Daytona README](daytona-upstream-readme.md) and
[native audio backend](native-audio.md) describe Daytona targets. Their
launcher, ROM sets, audio modes and platform claims do not establish Rally
support. The earlier lifted-C Rally prototype is a separate project; its
captures are useful comparisons, not validation of this generated runtime.

Only source tools, hardware code, manifests, address annotations, tests and
written analysis belong in Git. Personal ROMs, imported images, generated game
C++, captures and apps remain in ignored local directories. No game assets or
original game source are distributed here.
