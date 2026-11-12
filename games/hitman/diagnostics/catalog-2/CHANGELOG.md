# Catalog-2 · OPS 0.1.18

- Streams skeletal poses for the two living Freeform Training catalog NPCs.
  Reuses the original two-player animator capture, packed poses and playback.
- Verifies that a pose comes from the host in the current mission and targets
  a guest actor whose native baseline has been applied. Clears stale pose
  overrides and invalidates matching bindings on actor destruction.
- Uses full, independently decodable frames for this two-actor test. Gesture
  playback and phone visibility are separate observations in the live check.
