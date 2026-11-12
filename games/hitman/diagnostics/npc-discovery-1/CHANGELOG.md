# 0.1.27 / NPC Discovery

- Discover NPCs from the active mission's native actor list using template
  entity IDs and validate their local actor slots and construction records.
- Record eligible and rejected IDs for each room and mission load so they
  can be compared across PCs, including after a host restart.
- Keep NPC behavior local while testing discovery.
