# Main vehicle force and orientation stage

`update_main_front` closes the main callback's pre-balance region, original
ROM `[0x3521c,0x36760)`. The 5,444-byte region has SHA256
`09714c139a7f9519edc9ca782ebd48aaadd8f0c77c31ca30c6c240cb005d9b68`.
The implementation is an independently written typed value operation. It uses
the recovered contact, slot, polynomial, scalar and affine operations plus the
shared explicitly rounded vehicle arithmetic. No original instruction, mapped
address, device, processor or per-frame capture is a native runtime input.

The stage transforms the secondary four local points into both current and
candidate positions. Translation is the first term of each rounded affine
transform. This differs from the earlier contact-preparation stage, which adds
the origin after transforming an offset. Requested contact re-queries update
the existing result slots before their four response updates. The query mask
retains its original signed right-shift behavior.

Force is the ordered world-response sum. Moment uses a separately constructed
inverse orientation: negative third, negative first, negative middle rotations.
It does not transpose the rounded forward matrix. Pair sums and differences,
binary64 weighted terms, intermediate binary32 publications and later
corrections retain their explicit rounding boundaries. The motion normalization
uses the immutable coefficient table, including the original zero/subnormal
squared-length encoding. No epsilon or ordinary square-root normalization is
substituted.

One easy-to-miss caller detail is the scalar operation's adjacent argument
pairing. The six values form `({0,0,motion.y}, {0,motion.x,motion.z})`.
This is not the Euclidean length of the motion vector. An initial native
comparison interpreted those values as two consecutive triples; it first
diverged by ten binary32 units at frame 2330. Correcting the caller pairing
restored all recorded stage outputs. The arctangent dependency separately
preserves its recovered reduction and rounded binary32 publication.

All used finite coordinates/scalars are bounded by `2^20`; orientation and
steering magnitudes are bounded by eight. Constituent operations, coefficient
ranges, intermediate validity and final output bounds also apply. Unused
contact payloads remain raw. State, derived outputs and ordered sound requests
commit together, or remain unchanged. The sound event is an emitted request;
the function does not serialize the original sound queue.

## Evidence and limits

Capture 014's sealed original execution supplies 601 stage boundaries from
frames 2200–2800. All 65,509 compared state/derived words match, fingerprint
`38b159c6e376c5bc`. Nine passes perform actual re-queries: mask five occurs four
times, mask one three times, and mask four twice. This corpus emits no front
sound request. A separate original-data fixture checks the event transition,
duplicate suppression, zero force/moment case and four atomic failures,
including rejection after all slot calculations.

The final stage comparison and ASan/UBSan result are source-bound in
`build/ps2-native-main-front-live-008/result.json`. Record extraction, sealed
capture and private native asset hashes are checked. This is a comparison with
the complete original execution's observations, not a newly executed isolated
front-region interpreter corpus or an exhaustive proof of every branch.
The original front instructions and captures remain private under `build/`.

The stage comparison initializes each case independently. The separate whole
vehicle checker composes it with entry, contact, balance and final motion,
retaining native state between calls. Only that whole-callback evidence can
establish successive updates; neither proof establishes physical-console speed
or a completed race.
