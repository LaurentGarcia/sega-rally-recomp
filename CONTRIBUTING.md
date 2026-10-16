# Contributing

The active target is macOS. Read [rules.md](rules.md) and the relevant sections of
[the Rally design](docs/sega-rally-design.md) before changing execution, timing,
graphics or sound. The inherited design remains part of that contract. Phone
builds and device testing are deferred.

## Prepare a change

Create a focused branch from the revision you are working on:

```sh
git switch -c fix/describe-the-change
```

Use the [Mac setup and dependency pins](docs/getting-started.md), then the
[build and validation guide](docs/segarally-build-validation.md). Build into a
new ignored output directory so existing apps, recordings and saves survive.
Generated game C++ is never hand-edited; change the generator or verified seed
evidence instead. Preserve exact instruction guards, cycle charges, event order
and IRQ boundaries when optimizing. Keep enhancements off for baseline parity.

Run checks appropriate to the change. The dedicated Mac pipeline builds its
regression executables and runs CTest; repeat them for an existing pipeline build:

```sh
ctest --test-dir build/segarally-player/mac --output-on-failure
```

A manually configured host needs its test executables built first, as described
in the build guide. CPU/device changes also need a bounded comparison or replay
that could detect the relevant divergence. Renderer changes need matching saved
scenes and measured cost; audio changes need separate native PCM, output-stream
and listening evidence where applicable. A passing host suite does not certify
full-game or hardware fidelity. State checks that were not run explicitly.

Keep patches small and explain the trigger, resulting behavior, validation and
remaining limits. Record the source revision, input/replay identity, checkpoint
and measurement units. Every commit updates [HANDOFF.md](HANDOFF.md) with the
finding, next work and failed approaches worth preserving. Retain upstream
copyright/license notices and record dependency changes in
[THIRD_PARTY.md](THIRD_PARTY.md).

## Check a source release

After committing the intended changes, check that exact Git tree before preparing
public source artifacts:

```sh
python3 scripts/check_source_release.py --ref HEAD \
    --output build/source-release-check.json
```

Use a new report filename. Add `--archive /path/to/source.tar.gz` or
`--export-dir /path/to/extracted-source` to compare an artifact with the selected
tree. This gate checks source/export contents and records a file manifest; it
does not run the game, validate gameplay, or audit earlier Git history. Review
history separately before publishing it. An uncommitted working-tree change is
not part of `--ref HEAD`. A gate result must identify the actual checked revision.

## Public reports and patches

Include reproduction steps, build revision, Mac architecture/macOS version,
controller or audio-device details when relevant, and a concise diagnostic
excerpt. Check logs for personal paths or unrelated private information before
posting. Addresses, input descriptions, counts and hashes can identify a local
checkpoint without publishing game data.

Do not attach ROMs, imported images, generated game instructions, extracted
textures/samples, game-derived captures, emulator dumps or packaged apps to
issues, patches or releases. Keep them in ignored local storage. The eight
expressly reviewed [README screenshots](docs/images/README.md) are a narrow
documentation exception, pinned by exact hash; this is not permission to upload
additional captures or extracted assets. The original [app icon](assets/branding/README.md)
is also pinned by exact path and hash; derived platform icons stay in build output.
Do not use or
submit leaked game source, SDKs or internal documentation. Preserve the source
repository's original-code and public-reference boundaries in [rules.md](rules.md).

Useful next validation includes full-machine reference comparison with a recorded
MAME source/build identity, remaining course finishes, controller Settings/remapping
and neutral-resume behavior, and audio listening on additional devices. Distinguish
repeatability with the same executable, independent CPU comparison using shared
devices, and independent full-machine or PCB evidence. None substitutes for the
others; see [current status](docs/segarally-status.md).
