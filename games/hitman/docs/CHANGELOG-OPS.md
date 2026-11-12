# OPS HUB HITMAN co-op changelog

## 0.1.43 — Nearby guard coverage

- Let each guest's game run up to three nearby eligible guards using
  the same host-confirmed NPC handoff and synchronized presentation.
- Keep a guard with its current player through pursuit and recovery.
  Return calm guards independently as the guest moves to another area.
- Use ordinary Freeform traversal for the two-player test. Check
  guard patrols, reactions, recovery and behavior after a restart.

## 0.1.42 — Guard weapon visibility test

- Send the guard's native main-weapon visibility switch from the
  current NPC owner to held replicas.
- Apply the switch only to a matching native weapon repository ID.
- Exercise draw, holster and ownership return with two players in
  Freeform Training.

## 0.1.41 — Independent guest guard handoffs

- Allow one nearby guard per guest in a Freeform Training party of up
  to four players.
- Prefer the nearest eligible guest and keep an active encounter with
  its current owner through recovery.
- Return calm guards independently while other encounters continue.

## 0.1.40 — Automatic guard handoff test

- Automatically assign one calm nearby guard to the guest when they
  approach ahead of the host in a two-PC party.
- Keep that guard with the guest through an alert and allow return
  to the host after recovery and separation.
- Preserve shared pause and suppress new handoffs while paused.
- The Freeform Training rehearsal uses no F8 step.

## 0.1.39 — Shared pause and waiting overlay

- Pause the party when an admitted player opens Escape or a blocking
  tutorial. Other PCs show the original waiting overlay.
- Resume after the last blocking menu closes. Clear pause state on
  disconnect and mission transitions.
- Block automatic focus-loss Escape menus during a party mission.
- Exclude deliberate pause time from NPC watchdogs and handoff deadlines.
- Remove the one-shot F9 diagnostic.

## 0.1.38 — Native Escape menu rehearsal

- Add a one-shot F9 test that releases the native menu pause request
  after HITMAN accepts Escape in a live party.
- Keep the player weapon draw and holster checks after mission restart.
- Keep guard weapon geometry diagnostics for a chase on the guest.
- F9 tests the local menu only. The unanimous party pause policy
  remains a separate follow-up.

## 0.1.37 — Remote player weapon visibility test

- Send the player's held item with avatar movement and attach a visual
  copy to the matching remote player.
- Clear held items when holstered and reject stale updates and item
  callbacks during player or scene changes.
- Extend the guard weapon probe to record the existing model's
  visibility. Guard weapon visibility and pause remain diagnostic.

## 0.1.36 — Guard return, weapon and pause observations

- Give recently returned NPCs brief animation priority within the
  existing moving range and pose budget.
- Log whether a guard's main weapon is available and drawn on each PC.
- Log native menu requests and the local menu pause hold during play.
- The two-PC Freeform rehearsal uses the existing F8 guard handoff.
  Weapon and pause entries in this release are diagnostic.

## 0.1.35 — NPC animation during guard handoff

- Restore nearby NPC pose streaming for the host's actors.
- Stream the rehearsal guard's pose from the guest while the guest
  runs its AI. Validate the sender and ownership generation on the host.
- Drop outdated poses at handoff and resume the current owner's stream.
- The two-PC Freeform Training rehearsal still uses F8 for a single guard.

## 0.1.33 — NPC patrol and alert test

- Prioritize moving NPCs within 40 metres of a guest for the 12 shared
  animation slots. Fill spare slots with idle NPCs within 25 metres.
- Play the host's sampled NPC position and pose together. Blend back to
  regular position updates when the pose stream ends.
- Log the guest's first actual pose write per animator binding and record
  native NPC tension changes and local detection events on both PCs.
- Shared NPC behavior is still host-driven. This release gathers evidence
  for guest detection and pursuit. Keep knockouts and kills out of this test.
- Native build `0.13.94-four-player.preview.4.npc-agreed.4`, HITMAN 3.290.1,
  party protocol 2. Both PCs need the matching release.

## 0.1.3 — 2026-11-09

- Carry the animation world orientation alongside each player's bone pose.
- Convert the skeleton root into the receiving animation frame, preserving
  body facing independently of travel direction during crouched turns/strafe.
- Interpolate that orientation with the pose; retain the existing presentation
  when either frame is unavailable or invalid.
- Clear captured frames on source rebind/scene change; keep outfit rig guards.
- Add low-rate frame-conversion counters to logs for the next live test.
- Native build is 0.13.94-four-player.preview.4. All players must update;
  the party envelope remains protocol 2, with an optional pose extension behind
  the existing exact native-build check.

Preview 3 passed the user's two-PC repeated-disguise test with no invisibility;
movement and Stop/Ready/Start passed. This new turning correction has automated
coverage, but its visual result in HITMAN still needs a two-PC playtest.

## 0.1.2 — 2026-11-09

- Size the launcher within the Windows work area instead of forcing an 820-pixel minimum height.
- Add vertical scrolling and a compact stacked layout so controls stay reachable at high display scaling.
- Remove the mod's forced blackout for already-visible party avatars during outfit rematching.
- Pause old-layout pose writes before changing outfits and rematch as soon as native rig data is available.
- Skip stale pose maps when an animation worker sees a changed bone count.
- Preserve initial spawn and Stop/Leave visibility gates and independent player state.
- Bump the native build to 0.13.94-four-player.preview.3; every player must update. Party protocol remains 2.

The prior two-PC test passed movement, repeated vaulting beside teammates,
disguise synchronization and session restarts. This new outfit presentation
change still needs live testing: engine-level geometry streaming may cause a
brief gap or pose change even though the mod no longer forces a blackout.

## 0.1.1 — 2026-11-09

- Fix Install / repair and in-launcher updates rejecting their own running launcher.
- Continue blocking changes while HITMAN or a destination game-folder launcher runs.
- Capture setup output without a flashing console; display the actual error in the launcher.
- Save setup logs and add an Open setup log button for successful and failed runs.
- Use Windows PowerShell's own module paths even when inherited from PowerShell 7.
- Native Preview 2 gameplay code and party protocol remain unchanged.

## 0.1.0 — 2026-11-09

- Import HITMAN native/core source, development notes and experimental NPC source patch into games/hitman.
- Redesign the WPF launcher using the supplied monochrome/crimson design brief.
- Preserve host/join/leave/ready/start/stop/reconnect and manual Freeform Training setup.
- Add install/repair, tracked removal, and a separate HITMAN check/download/verify/install/restart updater.
- Reuse native Preview 2 (0.13.94-four-player.preview.2, game 3.290.1, party protocol 2).
- Keep notification sounds and upstream automatic updates disabled.
- Add standalone core/launcher source checks and a versioned package/site-manifest assembler.

Experimental limits: movement, poses and disguises only, up to four slots. NPCs,
bodies, items, objectives and extraction are separate on each PC. Preview 2's
vault/takedown protection still needs a live regression test. The public update
channel needs Breezops to publish the initial manifest and package before online
updates can be verified. No game files, saves, player logs or credentials ship.
