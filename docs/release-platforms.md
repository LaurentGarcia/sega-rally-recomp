# App packages and platform status

[Source prerelease v0.1.0-rc.1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-rc.1)
contains source archives. Both compiler-bootstrap preview releases have been
withdrawn: they required installed development tools and were not the requested
prebuilt game apps. [Player Preview 3](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.3)
provides precompiled Mac, Windows and Linux games. They import the player's own
ROM locally, without a compiler or developer-tool installation. Windows/Linux
add opt-in direct Vulkan; the Mac package retains the tested direct Metal build.

The distribution work targets these four platforms:

| Platform | Graphics | Package | Current validation |
| --- | --- | --- | --- |
| Mac, Apple Silicon | Metal | Public prebuilt DMG; private data-inclusive DMG | macOS 13 deployment target; signed ad hoc, not notarized. Actual public app imports ZIP and passes a 2,100-frame CRT/sound check, with native audio/image/saves matching the prior player. Actual native-picker interaction and execution on macOS 13 remain untested. |
| iPhone, arm64 | Metal | Signed personal IPA with game data | iOS 15+ development IPA built and signature/assets/notices verified for two registered devices. Installation and physical testing remain pending; this personal IPA is not a public release asset. |
| Windows, x86-64 | SDL GPU Vulkan default; direct Vulkan opt-in | Public ROM-import ZIP; private data-inclusive ZIP stays local | Full game cross-compile/link, dependency checks and Wine startup/import pass, including eight imported image identities. The user's earlier PC audio confirmation applies to the preceding SDL package. New direct Vulkan GPU, controller and full-race validation remain open. |
| Linux, x86-64 | SDL GPU Vulkan default; direct Vulkan opt-in | Public ROM-import archive; private data-inclusive archive stays local | Three 2,600-frame Linux X11/lavapipe game comparisons pass. A freshly extracted package imports owned data and repeats the 2,600-frame run without source/build mounts, preserving final image/audio. Requires X11/XWayland; native Wayland is not compiled. Physical GPU and speaker output remain untested. |

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

The current [Player Preview 3](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.3)
contains three platform binaries, their manifests and checksums. Its
`Sega-Rally-Native-macOS-arm64.dmg` (12,953,680 bytes) is byte-identical to
the [Mac Preview 2](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.2)
artifact. Its SHA256 is
`2d1c4d4703904ecc9584f452df23404673139a1a21e0b271d8e007c1c1c41b9d`.
Game, CRT and Settings use direct Metal; SDL retains window/input/audio duties.
The source is `8360ea9b7781947dc2c58ba935f7ba2d1dd19953`, with all five CI
workflows green. The actual game passes a three-arm 2,600-frame comparison and
a separate 2,100-frame visible check with sound/CRT defaults and zero recorded
audio drops/shortfalls. See [renderer evidence](native-renderers.md) for scope.

The matching private ready-to-play DMG is local at
`build/installers/mac-direct-metal-personal-001`, 32,140,096 bytes, SHA256
`dedd27a907c14c6d43c0c6427c2a7105e929e55afb41ffb915bf006090d6779b`.
Both editions include the original app icon and pass mounted-image byte,
signature and dependency-notice checks. They are ad-hoc signed, not notarized.
The personal app has launched windowed with its bundled data, sound and CRT.
These packages supersede the local Mac icon packages described below.

The preceding [Mac Player Preview 1](https://github.com/LaurentGarcia/sega-rally-recomp/releases/tag/v0.1.0-player.1)
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

Windows and Linux import editions are public in Preview 3; their personal
editions remain local. iPhone remains separate, with no public IPA or TestFlight
release. These checks do not establish four completed platform releases.

The new original app icon is integrated into both Apple packaging paths.
Earlier Mac import/personal DMGs remain local at
`build/installers/mac-player-{import,personal}-icon-001`; the current direct
Metal packages above supersede them. Both earlier bundles include the
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

## Linux package

The audited ROM-import archive is
`Sega-Rally-Native-Linux-x86_64-import.tar.gz` (8,586,382 bytes), SHA256
`3168a945b6caadb0749acb8954e04886f8bb1671b8f00f9d945468a7cdec9ad9`.
The separate personal archive is 25,097,897 bytes, SHA256
`f679d994b53ca828bcced96d209c2539df9703393c0e7098d2a80e8530cb3211`;
it includes the owner's game data and stays private.

Extract the whole archive and run `./play.sh` from its folder, or
`./play-vulkan.sh` for direct Vulkan. The ROM-import
edition asks for the owner's Revision B ZIP; `./play.sh --rom ZIP` selects it
directly. No compiler or Python is needed. This preview targets Ubuntu 24.04 or
compatible Linux with glibc 2.38+, the GCC 13 `libstdc++` runtime
(`GLIBCXX_3.4.31`), `libvulkan.so.1`, Vulkan drivers,
and an X11 desktop (or XWayland on a Wayland desktop) with its audio libraries.
Native Wayland support is not compiled into this preview. It is not a static
binary or an AppImage. Both launchers enable sound and CRT unless saved display
preferences override the default. The 2,600-frame relocated game check uses
X11/lavapipe, not a physical GPU or speaker. The personal archive separately
passes a 120-frame startup using its bundled data without import. Both preserve
the measured executable from `1a59ceafc6ff6870d06db723a1e6cf035bbebe57`.

## Windows package

The audited ROM-import archive is `Sega-Rally-Native-Windows-x86_64-import.zip`
(17,114,899 bytes), SHA256
`edc84f1ab5b31efe6162fc289fabde6817ea507a9072394f6b367f5893806052`.
The separate personal archive is 33,669,831 bytes, SHA256
`fac3761c7ac3b5c090c5834701413ab4b1f4b091f43f4819c649b1861c48b1d0`;
it includes the owner's game data and stays private. Both bind their compiled
source dependencies to `1a59ceafc6ff6870d06db723a1e6cf035bbebe57`.

Extract the entire ZIP and open `play.cmd` for SDL GPU, or `play-vulkan.cmd`
for direct Vulkan. To import a particular ZIP, use
`.\play.cmd --rom "C:\path\to\srallyc.zip"` from PowerShell. Keep
`libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll` beside
`segarally_native.exe`. Their versions, identities and notices are recorded in
[THIRD_PARTY.md](../THIRD_PARTY.md#windows-gcc-and-mingw-w64-runtime).
No compiler, Python or development SDK is required; a 64-bit Windows system
with a Vulkan 1.1-capable driver is needed. The driver supplies `vulkan-1.dll`;
it is not bundled. Both launchers enable sound and CRT unless saved display
preferences override the default.

The executable/import automation runs under Wine. A user confirmed audio
working on a PC with the earlier SDL package on 16 October; that report does
not establish audio or GPU behavior for this new Windows executable. Physical
graphics, controller and complete-race validation remain open. The import
still enforces its existing reparse-point guard; no runtime guard was relaxed
for Wine testing. Earlier SDL-only archives remain available locally for
comparison and were not overwritten.

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
such a fixture cannot play the game. Physical Windows/Linux GPU, device audio,
controller and full-race checks remain open.
Offscreen, window, Settings and bounded game comparisons are described in
[native renderers](native-renderers.md). Direct rendering removes SDL GPU from
those paths, while the packages still use SDL for windows, controls and audio.
The existing full-machine/PCB fidelity requirements remain unchanged.
