# Sega Rally documentation

**v0.1.0-rc.1 is an experimental macOS source prerelease.** Build locally with
your own verified Revision B (`srallycb`) ROMs. No ROMs, game assets or ready-to-play
app are distributed. Phone work is deferred.

| Guide | Purpose |
| --- | --- |
| [Getting started](getting-started.md) | Fresh checkout, Mac requirements, exact dependency pins and first play |
| [Mac player manual](mac-player-manual.md) | Controls, Practice selection, settings, sound, saves and recordings |
| [Build and validation](segarally-build-validation.md) | Pipeline, individual build stages, tests and reference setup |
| [Technical architecture](segarally-architecture.md) | Generated CPUs, native devices, clock, Metal and SDL3 audio |
| [Evidence and status](segarally-status.md) | Recorded behavior, measurements and unresolved validation |
| [Cabinet input replay](segarally-input-replay.md) | Script format, frame boundaries and exact host recordings |
| [Project design](sega-rally-design.md) | Rally scope, implementation contracts and findings |
| [Handoff](../HANDOFF.md) | Detailed chronological evidence and historical failure modes |

The [project rules](../rules.md) and
[inherited design](daytona-usa-recomp-design.md) govern development. The
[inherited Daytona README](daytona-upstream-readme.md) and
[Daytona native audio backend](native-audio.md) describe separate reference
targets. Daytona results and the earlier lifted-C Rally prototype do not prove
this runtime's fidelity.

Source is licensed under [BSD-3-Clause](../LICENSE); dependency notices and
revisions are in [THIRD_PARTY.md](../THIRD_PARTY.md). Personal ROMs, imported
images, generated game C++, captures and apps remain in ignored local folders.
The status and handoff documents refer to local evidence paths; those captures
are deliberately absent from the public source tree.
