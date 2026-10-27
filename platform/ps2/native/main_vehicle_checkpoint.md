# Native developer vehicle checkpoint

`MainVehicleCheckpoint` stores one initial typed vehicle state for a native
developer driver. It is not a recovered race initializer, save-game product
format, or frame-by-frame replay. After loading, the driver advances that state
with its own controls. Private exports are written only after the whole native
sequence comparison passes; exported game-derived values stay under ignored
`build/`.

Version 1 is exactly 752 bytes: a 32-byte header and 180 little-endian words.
The header contains magic `RLYCAR1\0`, version, header size, word count, payload
CRC32, course selector and native asset payload CRC32. The application must
match the last two fields against its loaded `DrivingAssets`. Course guidance
comes from the immutable native course table and prior segment state; it is not
stored as a changing external direction.

The field visitor declares the wire order independently of C++ layout.
Setup contributes five words, initial cabinet/game-flow inputs fourteen, and
persistent state 161. Every value occupies one word. Narrow signed integers
are sign-extended, narrow unsigned integers zero-extended and booleans exactly
zero or one. Float words preserve signed zeros, subnormals and unused raw
payloads. The game operation validates numerical inputs when they are used;
successful storage decoding alone does not certify a playable state.

Decode rejects wrong magic/version/count/size, checksum mismatch, trailing
bytes and noncanonical narrow encodings without changing the previous output.
Encode also uses a bounded temporary before publication. File loading reads
only the fixed buffer plus one trailing-byte check. No heap allocation,
processor state, original pointer or address map is part of the codec.

Four portable tests include an independently authored Python `struct`/`zlib`
file, malformed files with repaired CRC, raw payload round trips and atomic
rejections. A separate coverage check compares the serialized state fields
with the offline original-boundary adapter's complete 161-field set, so a
self-consistent encoder/decoder cannot silently omit a persistent field.
