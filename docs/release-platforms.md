# App packages and platform status

[Source prerelease v0.1.0-rc.1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-rc.1)
contains source archives. Both compiler-bootstrap preview releases have been
withdrawn: they required installed development tools and were not the requested
prebuilt game apps. There are not yet public playable game binaries. The target
is a precompiled app that imports the player's own ROM locally, without a
compiler or developer-tool installation.

The distribution work targets these four platforms:

| Platform | Graphics | Package | Current validation |
| --- | --- | --- | --- |
| Mac, Apple Silicon | Metal | Personal game DMG; prebuilt app with ROM import in development | Personal package built, signed ad hoc and checksum verified; CRT and audio defaults checked. Public compiler-bootstrap previews withdrawn. |
| iPhone, arm64 | Metal | Locally generated app, signed for its destination | Unsigned iOS 15+ build passed with sound and new touch controls; assets/notices verified. Signing, installation and physical testing remain pending. |
| Windows, x86-64 | Vulkan through SDL3 GPU | Local builder and personal native game package | Desktop build portability and real-host compilation CI in progress; no validated game binary yet. |
| Linux, x86-64 | Vulkan through SDL3 GPU | Local builder and personal native game package | Desktop build portability and real-host compilation CI in progress; no validated game binary yet. |

The personal game package contains the owner's local game data and generated
game instructions. It stays local. Public local-builder packages contain only
the importer and supporting project code. They ask each person for their own
verified Revision B ZIP and generate/compile their game locally. An empty
builder is not a ready-to-play game, and installed build tools are required.
The Mac builder needs Python 3.10+, Git, CMake/CTest 3.25+ and Apple Command Line
Tools. It downloads exact source/dependency revisions, preserves existing apps
and saves, and records the local build result. See its
[build instructions](../platform/segarally/macos/importer/README.md).

iPhone requires a desktop build and Apple signing after local game generation.
A generic IPA cannot generate and compile the game's native instructions on an
iPhone. Registered-device distribution and TestFlight are separate signing and
distribution routes; neither is established by an unsigned build. A personal
game app is not an asset for the public source release.

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
