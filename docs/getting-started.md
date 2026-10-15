# Getting started on macOS

The prebuilt Mac player runs without Python, CMake, Xcode, a compiler or a network
connection. It contains compiled native game instructions and imports the game
data from a supported ZIP you supply. The separate **v0.1.0-rc.1 source
prerelease** is for developers who want to build from source.

Prebuilt packages are being validated; this guide does not announce their
publication. Check [package and platform status](release-platforms.md) for what
is available. Earlier compiler-bootstrap previews were withdrawn. Windows,
Linux and iPhone readiness is tracked separately there.

## Play a prebuilt Mac app

The current Mac package targets Apple Silicon and Metal; Intel execution has
not been validated. The app is ad-hoc signed, not notarized.

1. Open the Mac **ROM-import edition** DMG, copy **Sega Rally Native.app** to
   Applications, and open the app.
2. At first launch, choose your own complete Sega Rally Championship Revision B
   ZIP. The filename can be `srallyc.zip`; its contents must match `srallycb`.
   The app verifies each required ROM's size, CRC32 and SHA1, then prepares and
   checks eight local runtime images. It does not download ROMs or compile code.
3. Let the original reset checks finish. Press and release **C twice**, with a
   gap between presses, then press **Return**. **Left/Right** steer, **Z** is gas
   and **X** is brake. **F1** opens paused Settings; **Escape** quits when Settings
   is closed. CRT and sound start enabled for fresh player preferences.

Your selected ZIP stays where it is. Imported data, cabinet saves and settings
live under `~/Library/Application Support/sega-rally-recomp/SegaRallyNative/`.
Later launches reuse the verified import. Cancelling the picker exits without
starting the game; a failed import leaves existing saves and imports intact.

A separate **personal edition** contains the owner's verified game data and
starts without the picker. Keep that data-inclusive package private. The
ROM-import edition includes no ROM archive or extracted game assets; its
executable does contain statically recompiled game instructions. The project
BSD license covers the original runtime and tools, not Sega game content.

To select a ZIP directly, the installed prebuilt executable accepts:

```sh
"/Applications/Sega Rally Native.app/Contents/MacOS/segarally_native" \
    --rom /path/to/srallyc.zip
```

Add `--import-only` to verify and save the import without starting the game or
opening a graphics/audio device. Use `--save-dir DIR` for an isolated import and
save set; keep existing saves. See the [player manual](mac-player-manual.md) for
controllers, display settings and sound.

## Build from source instead

The remaining sections are the developer workflow. These tools are **not
required to run the prebuilt player**. A source build uses your verified ROMs to
generate and compile native game instructions before packaging a personal app.

## 1. Prepare the tools

Use a Mac with Metal support and a working desktop/audio session. Current Mac
validation is on Apple Silicon; Intel Mac execution and a minimum macOS release
have not been established by the recorded Rally checks. The build produces the
host architecture, not a universal binary.

You need Git, Python 3.10 or newer, CMake 3.25 or newer, and Apple's C/C++ and
Objective-C toolchain with C++20 support. Xcode Command Line Tools can be installed
with `xcode-select --install`; a full Xcode installation also supplies the
toolchain. Install Python and CMake if they are absent. Verify what your shell uses:

```sh
git --version
python3 --version
cmake --version
xcrun --find clang++
```

An ordinary build uses committed generated shader headers. DXC, SPIRV-Cross and
Docker are not required unless you change shaders or run specific developer GPU
fixtures. No Apple developer account is required for local ad-hoc signing.

## 2. Get the source and pinned dependencies

```sh
git clone https://github.com/LaurentGarcia/sega-rally-recomp.git
cd sega-rally-recomp
mkdir -p extern
git clone https://github.com/libsdl-org/SDL.git extern/sdl3
git -C extern/sdl3 checkout --detach fa2c02bb6e21974a89ea9824bc53c9932abe5f9c
git clone https://github.com/ocornut/imgui.git extern/imgui
git -C extern/imgui checkout --detach f1cc2ae15e53a861a874c3034aae6798fde194ab
git clone https://github.com/ip7z/7zip.git extern/lzma
git -C extern/lzma checkout --detach 0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17
```

These are SDL3 3.4.16, Dear ImGui 1.92.9b and the selected 7-Zip sources used
for import verification and recording hashes. The pipeline checks their exact revisions and refuses
modified tracked dependency files. Use pristine checkouts; the inherited general
Daytona setup is not the Rally onboarding command. Preserve existing local
changes before changing a dependency revision. Dependencies stay ignored and
retain their own licenses; see [THIRD_PARTY.md](../THIRD_PARTY.md).

## 3. Supply your ROM archive

Keep your own ZIP archive outside the checkout or in ignored `roms/`. The native
importer requires the Revision B (`srallycb`) bytes specified by the
[committed manifests](../config). A filename such as `srallyc.zip` does not
identify its revision: size, CRC32 and SHA1 checks must pass. Unsupported or
missing entries stop the import; do not rename another revision's entries to
bypass it. The importer does not download ROMs.

## 4. Build the local app

From the repository root, replace the archive path with your own:

```sh
python3 scripts/segarally_mac.py --roms /path/to/srallyc.zip \
    --output build/segarally-player --jobs 4
```

Choose a new output directory for each build. The pipeline verifies/imports
inputs, recompiles the main CPU, TGP and sound CPU, builds Release with native
sound and recording support, runs the dedicated Mac tests, packages notices
and verifies an ad-hoc signature. It does not launch the game. It prints the app
path, report and optional automatic-selection launch command.

All generated game instructions, assets, binaries and logs remain in ignored
`build/`. This source-built personal app contains game data and must not be uploaded as
a public release asset.
The package is neither notarized nor a universal build.

## 5. Start and drive

Open `build/segarally-player/mac/Sega Rally Native.app`. Direct startup enables
compiled-in sound and the faster instruction boundary execution path. The game
opens windowed with normal pacing; original reset checks run first. On a fresh
cabinet, press and release **C twice**, then **Return**. Use **Left/Right** to
steer, **Z** for gas and **X** for brake. **F1** opens paused Settings; **Escape**
quits when Settings is closed. Current source builds enable CRT for fresh player preferences; F6 toggles it. Other enhancements remain opt-in.

For scripted coin/start/automatic-Lancia selection followed by manual takeover:

```sh
python3 platform/segarally/macos/launch.py \
    --build-dir build/segarally-player/mac --sound --instruction-boundary-hook \
    --input-replay config/segarally_lancia_select.inputs
```

The launcher repackages the prepared app and uses its separate `live-app-save`
directory. Quit an active copy first. Read the [player manual](mac-player-manual.md)
for controllers, Practice courses, save locations and recording precautions.

## If a source-build step stops

A failed pipeline preserves `build-result.json` and `logs/` in its output.
Read the first failed step before choosing another new build directory. An
existing output path is rejected intentionally to preserve recordings and saves.
Dependency errors require the exact clean pins above; ROM errors require the
manifest-matching bytes. Packaging needs access to the process list to avoid
replacing an app that is still running.

A game window labeled `frontier` has reached missing native code or unsupported
hardware. Preserve the log and record the build revision, course, controls and
visible state. Host regression tests passing does not mean every game path is
complete. The [build and validation guide](segarally-build-validation.md) explains
bounded checks, independent comparison and diagnostic options; the
[status reference](segarally-status.md) records the tested scope.
