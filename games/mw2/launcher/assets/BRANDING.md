# Launcher artwork and mission images

`launcher-brand.png` is the exact user-supplied image named
“MW2_ Four-Player Spec Ops Squad.png”. It is shown in Info and converted, without
cropping or replacing its content, into the app's multi-resolution Windows icon.
`make-icon.ps1` creates 16, 24, 32, 48, 64, 128 and 256 pixel PNG icon frames.
The executable embeds the icon for Explorer, the taskbar and Alt-Tab; the desktop
shortcut uses the packaged launcher.ico. No generative edits were applied.

The 23 original Spec Ops mission thumbnails are read from the user's local
`main/iw_02.iwd`, entries `images/so_*.iwi`, as mapped in missions.json. They are
decoded in memory and are NOT copied into the source repository or release ZIP.
Both the dropdown and briefing panel use the matching local image. Missing or
unsupported image data show a clear unavailable message rather than an unrelated
mission. The supported image variant is the current Steam build's IWi version 8
raw BGR24 format with a 32-byte header, validated dimensions and payload sizes.
Archive bounds and entry CRCs are verified with miniz before image decoding.

Overkill is still a concept mission and has no official MW2 image. It retains
the original custom concept artwork described in PROVENANCE.md.

Archive reader: miniz 3.1.2, https://github.com/richgel999/miniz/tree/3.1.2 .
The vendored source is unmodified; all nine downloaded source/license files were
verified against their upstream Git blob SHA-1s. `miniz_export.h` is the local
static-build export definition. The library's license is preserved in source
and included as MINIZ-LICENSE.txt in the launcher package. Compression and archive
writing APIs are compiled out; the launcher never modifies the IWD archive.
