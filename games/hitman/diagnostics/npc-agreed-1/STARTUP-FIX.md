# Shared NPC startup fix (OPS 0.1.31)

The 0.1.30 DLL mixed object files from before and after a plugin layout
change. The launcher status handler read the wrong member offset and
crashed HITMAN at startup. Version 0.1.31 uses a clean build and links the
status handler against a symbol keyed to the plugin and world sizes.

The test covers living NPC movement in Freeform Training and a host-led
mission restart. Use the launcher from the extracted download folder or
the launcher's installed update cache.
