# App artwork

`app-icon.png` is the original rally illustration used for the native Mac and
iPhone app icons. It is a 1024 x 1024 opaque RGB master, without text, sponsor
marks or an official game logo. It is not a game screenshot or extracted ROM
asset. Its SHA256 is
`021c23ada71d5fb229384d4555c4a541e2449106314b40c418f70271a0896226`.

Platform icon sizes, the Mac ICNS file and the iOS asset catalog are derived in
ignored build output. The source-release gate permits only this reviewed master
path and exact bytes; it does not permit arbitrary image files.

Mac packaging derives `AppIcon.icns`, copies it into the bundle's Resources and
sets `CFBundleIconFile` before signing. The iOS CMake target derives an
`AppIcon.xcassets` catalog for Xcode to compile before signing. Both paths use
the same master and Apple's `sips` tool; Mac conversion also uses `iconutil`.
Builds require a 1024 x 1024 PNG with no alpha channel. To change the artwork,
review the replacement and update the source gate's exact hash together.
