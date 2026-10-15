# App packages and platform status

[Source prerelease v0.1.0-rc.1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-rc.1)
contains source archives. It does not contain four playable game binaries.

The distribution work targets these four platforms:

| Platform | Graphics | Package | Current validation |
| --- | --- | --- | --- |
| Mac, Apple Silicon | Metal | Personal game DMG; separate empty local-builder DMG | Personal package built, signed ad hoc and checksum verified. Empty builder has 20 coordinator tests and a compiled/signed package; full first import is being checked. |
| iPhone, arm64 | Metal | Locally generated app, signed for its destination | Native platform build and touch integration in progress. No installable public IPA or TestFlight build yet. |
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

SDL3 GPU already supplies the rendering abstraction. The host selects a backend;
the game renderer, polygon ordering, Accurate textures and CRT pass stay shared.
The common shader sources produce SPIR-V for Vulkan, MSL for Metal and DirectX
formats where supported. Adding Windows/Linux does not call for a second game
renderer or changes to guest simulation timing.

Build checks, GPU checks and game checks answer different questions. The root
CI matrix compiles tools and runs ROM-free tests. A native frontend compile/link
fixture can exercise platform APIs without distributing game instructions, but
such a fixture cannot play the game. Real Vulkan device creation, game and CRT
rendering, audio, controllers and a full race still need platform execution.
The existing full-machine/PCB fidelity requirements remain unchanged.
