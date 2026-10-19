# iPhone development preview

The personal iPhone build shares the desktop game, sound and display code. It
uses direct Metal for game pixels, CRT presentation and Settings. SDL3 still
provides the window, touch/controller events and audio; this is not yet an
SDL-free host. The deployment target is iOS 15 on arm64.

The current personal preview is version 0.1.0/build 5. Its full device build,
signature and archive checks pass. Installation is pending after a device
connection reset; no physical gameplay result is claimed. Artifact identity
and signing limits are recorded in [platform status](release-platforms.md#personal-iphone-touch-preview).

## Driving interface

The original touch interface uses translucent graphite panels, ivory labels
and orange active controls. Steering shows its center and current travel;
accelerator and brake provide pedal feedback. Coin, Start, View, shifting,
handbrake and Settings remain separate controls. Touch ownership and cabinet
input mapping use the same tested logic as the earlier mobile build.

Settings opens a paused **Pit Stop** screen inside the device safe area. It has
larger controls, Original/Arcade CRT presets and a **Tune the screen** section
for the existing display options. **Back to the stage** remains at the bottom
while the settings content scrolls. The Controls tab retains optional gamepad
configuration. This custom rendered interface is not a native UIKit interface
and does not yet provide VoiceOver navigation.

Sound and Arcade CRT start enabled for a fresh player profile. Saved display
preferences take precedence. Performance, thermal behavior, safe-area layout,
multitouch, background/resume and speaker/controller behavior still require
physical iPhone checks; desktop previews alone do not establish those results.

## Build locally

Start with the tools and pinned dependencies in [Getting started](getting-started.md).
Xcode must also contain the iPhoneOS SDK. From the repository root, build a
personal unsigned device app using your verified Revision B ZIP:

```sh
python3 scripts/segarally_ios.py --roms /path/to/srallyc.zip \
    --output build/iphone-preview --graphics metal --jobs 2
```

Direct Metal is the default. `--graphics sdl` builds the SDL GPU comparison
backend. `--plan` prints the commands without building. Choose a new output
directory each time: the script refuses to overwrite previous apps or evidence.
For an existing complete native generation, replace `--roms` with
`--native-build build/segarally-player/native`.

An unsigned app cannot be installed on a normal iPhone. For a locally provisioned
personal device, add `--signing development --team YOURTEAMID`; the team value
must be your own ten-character Apple team identifier. Signing requires an
appropriate existing certificate/profile and matching bundle identifier.
`--bundle-id` selects that identifier. Provisioning changes are opt-in through
`--allow-provisioning-updates`; the default does not request them.

The script builds the app at
`build/iphone-preview/ios/Release-iphoneos/segarally_native.app`, or creates an
archive with `--archive`. It never installs, launches or uploads a build.
`--export-method` exports an archive locally and does not submit it to Apple.
See [platform status](release-platforms.md) for the separately recorded signed
personal packages and their validation limits.

## Distribution and ownership

This is a personal development preview, not an App Store or TestFlight release.
The current app contains the owner's bundled game data and compiled game
instructions. An iOS Files importer remains to be implemented; omitting game
assets would still leave translated game instructions in the executable.

The UI and platform work are original project contributions. The Sega Rally
game, branding, graphics and sound belong to their respective rights holders;
the source license does not grant rights to those materials. A commercial
release requires separate rights clearance. TestFlight and signing do not
provide it. An independently created rally game with original code and assets
would be a separate commercial project.
