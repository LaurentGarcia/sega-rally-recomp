# PlayStation 2

The current target is a [native game rewrite](../../docs/ps2-native-rewrite.md)
using the open PS2SDK, direct GS/DMA rendering and native input/audio services.
Its application still shows an original platform diagnostic or the optional
static car study. The main vehicle callback and driving loop are not connected;
the static preview does not establish native gameplay performance.

Shared services and the independent `engine-demo` target are documented in
[PS2 engine architecture](../../docs/ps2-engine-architecture.md). The build checks
dependency direction between hardware services, game rules, captured-scene
adapters and offline recovery. See [PS2 development](../../docs/ps2.md) for build
commands and current hardware limits.

The recovered main-callback components now include segment selection and a
joined candidate-preparation/contact operation. The latter matches 26 sampled
original calls, including nine active responses, using private course data.
A separate geometry tail updates two triples and publishes its selected index;
earlier filtering and event dependencies in that larger helper remain open.
Each has an explicit bounded native contract and an offline original-program
comparison. Their scopes and evidence are listed in
the [main vehicle dependency map](native/main_vehicle_recovery.md); they are
not a substitute for the unrecovered callback work.

`build_ps2.py --help` lists the native application, original engine sample,
hardware fixtures and separate arcade-reference probe workflows. None is a
finished PS2 player release. All original game data, generated reference source,
module arrays, captures and ELFs belong under ignored `build/`.
