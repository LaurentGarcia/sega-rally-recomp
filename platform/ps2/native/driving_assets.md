# Native driving assets

`DrivingAssets` owns ordinary native course, selection, balance/input, motion
and sound-command values. Its views feed recovered operations directly, without
ROM addressing, devices, processors or allocation during a query. Loading assets
does not initialize a vehicle, connect the main callback, establish its cadence,
or make the static geometry study a driving game.

The owner supplies four complete verified images to
`platform/ps2/tools/export_driving_assets.py`. Their size and SHA256 identities
are checked before extraction. Output and provenance sidecar must be new files
below ignored `build/`, without symlink components. No original bytes, converted
package, disassembly or captured motion belong in source control.

## Explicit contents and provenance

One package contains one of eight descriptor selections. The program descriptor
at ROM `0x3dac0 + 12*selector` identifies both the selection geometry and contact
polygons. The two independently decoded segment counts must agree. Selection
vertices retain X/Z and corner indices; contact polygons retain all 17 words,
including untouched Y/coefficient payloads. The 8,192 normal coefficient pairs
use the verified table image at word offset `0xc000`; slope sign is removed,
matching the original reference table interface.

The selection header's fourth word identifies one X/Z guidance pair per
segment in the verified main-data image. The original surrounding update reads
row `max(0, previous_signed_segment)` and publishes `{x,0,z}`. All 601 entry
observations in capture014 match that immutable lookup. The native callback
therefore derives this direction from its own previous segment, without a
captured per-frame direction input. No physical unit or camera role is inferred.

The additional immutable values are:

- Ten 13-word parameter records from the pointer table at `0x377a0`, plus the
  first four local contact points at `0x33e30`. These parameter records belong
  to the balance helper; they are not the object's contact scalars at +0x50,
  +0x54 and +0x58. The integration test keeps those scalars explicit.
- The shaping, cabinet, ratio, gear, power, sound-parameter and occasional-command
  values mapped to `MainBalanceAssets`. Source ranges are `0x37680..0x37694`,
  `0x39230..0x39538`, `0x3a254`, `0x3a264..0x3a278`, `0x3a300..0x3a31c`,
  `0x3dd10..0x3ddb0` and `0x3ae30..0x3ae50`.
- Two 82-float power curves beginning at `0x39f20` and `0x3a0c0`.
  Initializer stores at `0x3a2e0` / `0x3a2f4` select them when ordinary value
  `0x2139d0` is zero / nonzero. No car identity is inferred. The supported
  rate bound requires indices 0 through 80 and successor 81. The 104-float
  spacing between starts does **not** prove a 104-element curve; earlier private
  transport experiments included that larger extent and are superseded.
- Two 32-word motion command lists at `0x34e40` and `0x34ec0`, with scalar
  `0x34e3c`. Secondary local points are extracted from twelve immediate operands
  within `0x352d4..0x35410`. Every operand is checked as an `LDA` followed by a
  matching register store to the geometry input address, in the exact verified
  program image. They are not inferred from the static mesh's captured matrices.
- 189 three-byte sound encoding rows beginning at `0x24ce0`. Supported direct
  commands are at most 181; initialized parameterized commands reach 188;
  other referenced immutable lists are lower. This proves the required extent
  through index 188, not an arbitrary 256-command namespace. Callers must reject
  commands outside the supplied span. Row interpretation and queue behavior
  remain separate from this loader.

`balance_assets(0)` and `balance_assets(1)` return stable immutable values with
the corresponding power-curve span. The caller explicitly chooses a variant.
`motion_assets()`, `sound_commands()` and `course_directions()` expose the remaining values. Asset
transport does not certify the complete balance helper's behavior or cover
mutable read-before-write state discovered by whole-callback tracing.

## Ownership, bounds and wire format

The version 3 package uses a 96-byte little-endian header, exact counts and
payload length, reserved-zero fields and reflected IEEE CRC32. The header and
implementation describe the field order. Versions 1 and 2 private experiments are
rejected; there is no heuristic conversion. No file-supplied offsets are used.

Limits are 65,536 selection vertices, 4,096 segments, 65,535 polygons, 1..255
polygons per segment, 8,192 normal pairs and at most 4,096 sound rows. Segment
ranges must partition the polygon pool. X/Z and local points are finite within
the recovered bound; normal coefficients obey the validated table domain.
Other arithmetic asset scalars must be finite. Raw copied payload words remain
raw; each recovered operation retains its own narrower domain validation.

The file loader uses bounded streaming reads and rejects trailing bytes. Decode
builds a replacement privately and commits only after complete validation.
Allocation or validation failure leaves existing views unchanged. Successful
replacement expires old views; moving the owner preserves their storage while
transferring ownership. Caller operations never own or free the returned spans.
File loading temporarily holds the encoded payload and decoded replacement,
plus any prior live package; callers should release obsolete packages when
switching courses if that peak matters.

For the actual eight selections, packages occupy 74,491–415,535 bytes and host
owned storage occupies 76,767–420,131 bytes. Selection 0 is 285,587 encoded bytes
and 290,183 owned bytes on the validation host. EE structure sizes may differ;
`storage_bytes()` reports the current build's actual vector capacities and
owner object size. These are memory measurements, not throughput claims.

## Validation and limits

Six portable tests pass, including 120 original native ownership/domain checks,
failure at each allocation, corrupted/truncated/trailing files, output path
confinement, complete typed-payload identity and simultaneous maximum counts.
ASan/UBSan also runs the original fixture and the owner's decoded selection 0.
No separate asset-loader hardware fixture is claimed; whole native vehicle execution is validated separately.

The source-bound report is
`build/ps2-driving-assets-live-004/result.json`. All eight independently exported
packages are decoded and every typed payload value is traversed in canonical
wire order for comparison. The loaded selection 0 then drives the existing
joined candidate/contact operation across all 26 sealed boundaries from capture
013: 104 contacts, nine active steps and 1,196 output words match, fingerprint
`9a47fb54b88fea84`. This substitutes native owned asset views for the offline
image decoder; it does not turn those separate boundary inputs into a continuous
vehicle simulation. Balance/power/motion assets have exact transport evidence;
their full-update behavioral comparison is a separate task.

The first integration attempt incorrectly treated the 13-word balance parameter
records as contact scalars. Its private comparison rejected the mismatch. The
final interface and checker preserve the proven distinction instead of silently
substituting matching-looking values.
