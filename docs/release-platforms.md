# App packages and platform status

[Source prerelease v0.1.0-rc.1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-rc.1)
contains source archives. Both compiler-bootstrap preview releases have been
withdrawn: they required installed development tools and were not the requested
prebuilt game apps. [Player Preview 1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.1)
supplies the actual precompiled Mac game app. It imports the player's own ROM
locally, without a compiler or developer-tool installation.

The distribution work targets these four platforms:

| Platform | Graphics | Package | Current validation |
| --- | --- | --- | --- |
| Mac, Apple Silicon | Metal | Public prebuilt DMG; private data-inclusive DMG | macOS 13 deployment target; signed ad hoc, not notarized. Actual public app imports ZIP and passes a 2,100-frame CRT/sound check, with native audio/image/saves matching the prior player. Actual native-picker interaction and execution on macOS 13 remain untested. |
| iPhone, arm64 | Metal | Signed personal IPA with game data | iOS 15+ development IPA built and signature/assets/notices verified for two registered devices. Installation and physical testing remain pending; this personal IPA is not a public release asset. |
| Windows, x86-64 | Vulkan through SDL3 GPU | Prebuilt game ZIP and private data-inclusive edition | Real frontend and thirteen host tests pass; complete native game cross-build in progress. |
| Linux, x86-64 | Vulkan through SDL3 GPU | Prebuilt game archive and private data-inclusive edition | Real frontend and thirteen host tests pass; complete native game cross-build in progress. |

The personal game package contains the owner's local game data and generated
game instructions. It stays local. The requested public player executable also
contains statically recompiled game instructions, but no ROM archive or extracted
game data. Its native importer asks for the owner's verified Revision B ZIP and
prepares local game data without a compiler, Python or a network connection.
The project source license does not license Sega game content. Compiled game
instructions are a separate distribution exception; the source repository and
source archives still exclude generated game code and ROM bytes.

iPhone requires Apple signing for its destination. The completed personal IPA
uses existing development provisioning and includes the owner's game data.
Registered-device distribution and TestFlight are separate routes; the personal
IPA does not establish TestFlight distribution or a public ROM-import edition.

## One renderer, four hosts

The long-term runtime target is SDL-free. Direct Metal and Vulkan backends will
consume common geometry and shader inputs, while native host adapters supply
windows, lifecycle, audio and controls. The existing SDL implementation remains
the working comparison baseline during migration. Removing SDL also requires
replacing its non-graphics duties; a direct renderer alone does not finish that
work. The game simulation, polygon ordering, Accurate textures and CRT shader
rules remain shared. Direct Metal is not presumed faster without measurements.

Build checks, GPU checks and game checks answer different questions. The root
CI matrix compiles tools and runs ROM-free tests. A native frontend compile/link
fixture can exercise platform APIs without distributing game instructions, but
such a fixture cannot play the game. Real Vulkan device creation, game and CRT
rendering, audio, controllers and a full race still need platform execution.
The existing full-machine/PCB fidelity requirements remain unchanged.
