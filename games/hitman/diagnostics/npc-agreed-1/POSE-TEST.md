# Nearby NPC animation and movement test

The agreed roster still uses host simulation. Guests blend subsequent
positions and facing over 100 ms. Initial baselines still require exact
native readback before the host resumes an NPC.

The host streams poses for up to 12 NPCs within 25 metres of a guest.
It selects the nearest NPCs across current guest positions and releases
unused animator watches. Guests clear pose overrides after stream
silence, on restart, and when leaving the room.

Compare a walking NPC from a distance and again after moving closer.
Check that position and facing stay smooth at both distances and that
nearby legs and body motion follow the host. Repeat after a host-led
restart. Use a fresh native build directory and verify a live launcher
status request before publishing the candidate.
