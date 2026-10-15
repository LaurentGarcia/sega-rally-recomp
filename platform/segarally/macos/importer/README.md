# ROM-free Mac local builder

This Apple Silicon/macOS 13+ bootstrap contains original UI/coordinator source and the project BSD license. It contains no game images, game programs, promotional screenshots, compiled game app, SDL, ImGui or 7-Zip payload. It is ad-hoc signed, not notarized. Do not distribute a completed personal import as this empty edition.

The user selects their own Revision B ZIP. The coordinator checks Git, Python 3.10+, CMake/CTest 3.25+ and Apple Command Line Tools; it gives setup guidance without installing them. It downloads the explicitly pinned public commit and SDL/ImGui/7-Zip commits, hashes a local ZIP snapshot, then runs the unchanged `scripts/segarally_mac.py`. Network commands fetch source only; no command uploads user inputs or output.

Every attempt receives a new folder under `~/Library/Application Support/Sega Rally Importer/Imports/`. Existing apps and saves are preserved. A failed or cancelled attempt retains logs and never receives `completed.json`. Launch verifies the receipt, exact executable/assets and signature again and uses the import's own `player-saves` folder. A running game or redirected save folder prevents launching a second copy. The last completed folder is remembered for the next bootstrap launch. Cancel/quit asks the coordinator to stop; the pinned pipeline receives SIGINT so its existing interruption handler reaps its separately grouped build step before the coordinator exits.

Preview 2 pins game source `6c22e241286b9e936395c47039a0403225e53ac8`
and launches with player defaults: compiled sound and Arcade CRT enabled on
fresh preferences. Saved display choices remain authoritative; F6 toggles CRT.
The arcade boot is initially silent before attract audio starts. An import made
with preview 1 must be rebuilt with preview 2 to use the new game executable;
installing the new builder does not replace an existing personal app or saves.

Build locally from the repository root (no game files needed):

```sh
python3 tests/test_segarally_mac_importer.py
python3 platform/segarally/macos/importer/package.py --output build/local-builder-001 --dmg
```

Choose a new output directory on every package attempt. Packaging compiles only the SwiftUI/AppKit bootstrap, signs/verifies it, creates an Applications-alias DMG and verifies its checksum; it does not launch the GUI or game. `result.json` records all bundle bytes and bootstrap source identities. The bootstrap's first actual import is a separate integration check; successful bootstrap packaging alone does not establish a successful fresh game build on every Mac. A working internet connection and several GB of disk space are required for the first import.
