# PlayStation 2

Shared native services and the independent `engine-demo` target are documented
in [PS2 engine architecture](../../docs/ps2-engine-architecture.md). Game rules,
captured-scene adapters and offline recovery remain separate source roles.

Experimental direct GS/DMA homebrew target. See the
[architecture, build commands and current limits](../../docs/ps2.md).

`build_ps2.py` builds the ROM-free hardware test or an explicitly supplied
game-core probe manifest. Neither is a finished player release. All generated
source, game packs, module arrays and ELFs belong under ignored `build/`.
