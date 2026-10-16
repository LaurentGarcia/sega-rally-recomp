# App packages and platform status

[Source prerelease v0.1.0-rc.1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-rc.1)
contains source archives. Both compiler-bootstrap preview releases have been
withdrawn: they required installed development tools and were not the requested
prebuilt game apps. [Player Preview 1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.1)
is published with the actual precompiled Mac game app. It imports the player's own ROM
locally, without a compiler or developer-tool installation.

The distribution work targets these four platforms:

| Platform | Graphics | Package | Current validation |
| --- | --- | --- | --- |
| Mac, Apple Silicon | Metal | Public prebuilt DMG; private data-inclusive DMG | macOS 13 deployment target; signed ad hoc, not notarized. Actual public app imports ZIP and passes a 2,100-frame CRT/sound check, with native audio/image/saves matching the prior player. Actual native-picker interaction and execution on macOS 13 remain untested. |
| iPhone, arm64 | Metal | Signed personal IPA with game data | iOS 15+ development IPA built and signature/assets/notices verified for two registered devices. Installation and physical testing remain pending; this personal IPA is not a public release asset. |
| Windows, x86-64 | Vulkan through SDL3 GPU | Local ROM-import and private data-inclusive ZIPs prepared; not uploaded | Independent archive audit, Wine `--help`, 4,260 importer-fixture checks and owned-ZIP import pass, verifying 23 ROMs and eight image identities. A user confirmed audio working on a PC on 16 October. GPU capture, controller and full-race validation remain open. |
| Linux, x86-64 | Vulkan through SDL3 GPU | Local ROM-import and private data-inclusive archives prepared; not uploaded | Ubuntu 24.04/glibc 2.39+ preview; final archive audit passed. Fresh relocated package passes `./play.sh --help` and CPU-only `--rom ZIP --import-only` with no source/build mount, verifying 23 ROMs and eight image identities. Linux desktop/GPU, device audio and gameplay remain unvalidated. |

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

## Available artifacts

The published [Mac Player Preview 1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.1)
contains `Sega-Rally-Native-macOS-arm64.dmg` (9,839,642 bytes), a player manifest
and [SHA256SUMS](https://github.com/LaurentGarcia/sega-rally-recomp/releases/download/v0.1.0-player.1/SHA256SUMS).
GitHub's reported DMG digest matches the local package:
`d86c7e4d584ea4a897e2e157ae4086e8ab93b50df9fc58c3f5b40e8f0a53da10`.
This public app contains compiled game instructions and no ROM archive or
extracted game assets. It imports the player's verified ZIP locally.

Two data-inclusive personal artifacts are prepared locally and are not public
release downloads:

- **Mac personal DMG:** 28,995,081 bytes; ad-hoc signed and not notarized.
  SHA256 `b3c5d8d2e2893ab77419157cbf584403a0caba72eace848439fba6f76921e268`.
- **iPhone personal IPA:** 24,341,339 bytes; development-signed for the two
  devices covered by the existing profile. The actual signing certificate
  expires on **2026-10-27 at 18:31:31 UTC**, earlier than the profile's
  2027-10-14 expiry. The profile date is not a promise of app usability until
  that date; certificate validity, revocation and device trust also apply.
  SHA256 `a108448667ffcce4e2db26e2cc1dbe1fb8e661ee8127e604b5737594532224c1`.
  Installation and physical testing remain pending. It cannot be installed on
  arbitrary devices and is not a TestFlight release.

The requested friends distribution route is external TestFlight. Preparation
is local only: distribution rights are unresolved, no build has been uploaded,
and no tester invitations have been sent. The current development IPA is not
an invitation-based install for friends.

The local signing audit found a development identity, but no matching
distribution identity/profile for this app. Personal App Store Connect access,
an app record matching `com.lgarciacarro.segarally.native`, and an unused upload
build number remain unverified. An unrelated app's cached profile is not used.
The iOS script can plan a local `app-store-connect` export; its destination is
always `export`, never `upload`. That option does not establish distribution
signing or App Store readiness. Beta metadata, required privacy/export declarations
and physical device testing also remain before submission.

Apple's
[Ad Hoc distribution](https://developer.apple.com/help/account/provisioning-profiles/create-an-ad-hoc-provisioning-profile/)
requires a distribution certificate and a provisioning profile containing each
registered device. The current personal development IPA is not that package.
[External TestFlight](https://developer.apple.com/help/app-store-connect/test-a-beta-version/testflight-overview/)
provides invitations and updates, requires review for the first external build,
and limits each build to 90 days. Neither signing route grants rights to SEGA
game content. Bundled-data distribution requires a separate rights assessment;
credit and private sharing do not supply permission.

The current iPhone build uses bundled data. Its pinned SDL dialog backend does
not implement the desktop ROM chooser on iOS. A user-import edition still needs
a native Files picker and sandbox-safe data import; desktop import evidence
does not establish that iPhone workflow. Excluding extracted data would not
remove the translated game instructions from the executable.

Windows and Linux packages remain local; neither is a published download at
this checkpoint. Local build, import and archive checks do not establish four
completed platform releases.

The new original app icon is integrated into both Apple packaging paths.
Updated Mac import/personal DMGs are local at
`build/installers/mac-player-{import,personal}-icon-001`; the published Mac
release still contains the preceding package. Both new bundles include the
same icon before signing and retain the existing game executable, CRT and sound
defaults. These packaging checks do not add gameplay or fidelity evidence.
The corresponding icon-equipped personal iPhone IPA is version 0.1.0/build 2,
27,769,177 bytes, SHA256
`ede1db2a9262c7b874da4cdfa5a390ab08abc6b4984a99219f51ee662999b1ee`.
It passes archive/export, compiled icon, signature, data and notice checks;
installation and physical testing remain pending. It retains the same
two-device development profile and October 27, 2026 certificate expiry.
All three updated Apple packages are collected locally in
`build/installers/apple-icon-preview-001`, with installation notes and checksums.
No TestFlight upload or invitations were made.

## Local Linux package

The audited ROM-import archive is
`Sega-Rally-Native-Linux-x86_64-import.tar.gz` (8,578,159 bytes), SHA256
`58f89c547850b1117a4fc1749cc1e1d474322ee06fbf3081fbab6e9d772f3557`.
The separate personal archive is 25,091,934 bytes, SHA256
`34ee3cdcc9d703bdb337938e0191c7e33b99d67410a6d95fd87730cfc879cb11`;
it includes the owner's game data and stays private.

Extract the whole archive and run `./play.sh` from its folder. The ROM-import
edition asks for the owner's Revision B ZIP; `./play.sh --rom ZIP` selects it
directly. No compiler or Python is needed. This preview targets Ubuntu 24.04 or
compatible Linux with glibc 2.39+, the GCC 13 `libstdc++` runtime, Vulkan drivers,
and an X11 or Wayland desktop with its audio libraries. It is not a static
binary or an AppImage. The package checks above are CPU-only, not desktop game
or playback validation.

## Local Windows package

The audited ROM-import archive is `Sega-Rally-Native-Windows-x86_64-import.zip`
(17,000,917 bytes), SHA256
`065e1ab8bddf7ffebbb32c56a0e75e6b1de83d9c3d12a8307d45c1ccfa39e6c0`.
The separate personal archive is 33,555,527 bytes, SHA256
`a36b707c45cfb0b38b04cbb695c6f5d230e9a80614a217ad43e4b29877253231`;
it includes the owner's game data and stays private. Neither is uploaded.

Extract the entire ZIP and open `play.cmd`, or use
`.\play.cmd --rom "C:\path\to\srallyc.zip"` from PowerShell. Keep
`libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll` beside
`segarally_native.exe`. Their versions, identities and notices are recorded in
[THIRD_PARTY.md](../THIRD_PARTY.md#windows-gcc-and-mingw-w64-runtime).
No compiler, Python or development SDK is required; a 64-bit Windows system
with working Vulkan drivers is needed for the intended desktop player.

The executable/import automation runs under Wine. A user subsequently confirmed
audio working on a PC on 16 October; no hardware details or recorded full race
accompanied that report. Graphics captures, controller behavior and complete-race
validation remain open. The import still enforces its existing reparse-point
guard; no runtime guard was relaxed for Wine testing.

## One renderer, four hosts

The long-term runtime target is SDL-free. Direct Metal and Vulkan backends
consume common geometry and shader inputs, while planned native host adapters supply
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
The new offscreen backend comparisons are described in
[native renderers](native-renderers.md); they do not change the playable
packages' SDL dependency. The existing full-machine/PCB fidelity requirements
remain unchanged.
