# OPS 0.1.11 / hold.2 - entrance NPC selection

- Allow the nearest eligible conscious non-target NPC, including civilians and
  mechanics. Hold-1's guard-only outfit filter could exclude the entrance NPC
  the user was standing beside; its log did not establish the exact rejection.
- Record counts for every selection stage: invalid/down/target/stale identity,
  observation/hold/job coverage, spatial availability and distance. Log the
  nearest otherwise-eligible distance and whether any candidate is within 8m.
- Preserve fresh restart/birth, scheduled identity/slot, tracked-job, native
  completion and hold-ownership requirements. No native control logic changed.
- Keep the eight-second owned hold/resume cycle and exact tested 0.1.3 rollback.

Hold-1 received two F8 requests but selected no actor. No native hold/resume
cycle was validated. Hold-2 is a separately versioned private retry; public
downloads and all preserved candidates remain unchanged.
