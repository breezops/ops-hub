# Nearby NPC animation and movement test

The agreed roster still uses host simulation. Guests blend subsequent
positions and facing over 100 ms. Initial baselines still require exact
native readback before the host resumes an NPC.

The host streams up to 12 poses. It gives recently moving NPCs within
40 metres of a guest priority, then fills any free slots with nearby idle
NPCs within 25 metres. The guest plays the host's sampled transform and
pose together while a stream is active and blends back to regular position
updates when the stream ends. Guest logs record the first actual animator
write for each binding.

In the 0.1.32 test, both PCs synchronized 83 actors before and after
restart. The host captured and the guest received matching 231-bone poses.
Ronnie received a pose briefly before restart and had no pose entry after
restart. The tester observed walking guards gliding while watching the
side of the boat through a crowd.

## When a player gets caught

The original two-player session exchanged NPC alert levels and used them
to keep the other copy of the guards alert. It capped the mirrored
tension below hunting and fighting, so the partner's actions did not
automatically make the guards attack the local player. When a guard
chased the guest, the host could give that NPC to the guest's game to
simulate. Otherwise ownership generally followed proximity, with
exceptions for active contact and higher tension. This matches the early
test where one player was caught and the other stayed suspicious.
The retained [two-player session](../../core/src/Session.cpp) and
[native adapter](../../plugin/Src/SdkWorld.cpp) contain the reference
implementations for ownership and alert floors.

The current party adapter runs the NPCs on the host and replicates living
NPC transforms to guests. If the host triggers a panic, running guards
and civilians enter the same moving pose pool as patrolling NPCs. When
more than 12 moving NPCs qualify, the closest to any guest fill the shared
pool. The others still receive host position and facing and may glide.
Guest detection, pursuit attribution, knockouts and deaths need native
party integration. In particular, a guest getting caught cannot be
validated by watching host-driven patrol motion, and a knockout or death
can take an NPC outside the current transform-only scope.

For combat, reuse the original alert-floor and NPC control primitives
through the party coordinator. Track shared alert state and the player
who caused it so a pursuing guard reacts to the correct player. First
verify host and guest triggers in separate two-PC runs. Compare which
guards pursue, which civilians flee, what the uninvolved player sees,
how actors return to patrol, and whether restart clears the state. Then
verify simultaneous triggers with four PCs, including separated guests
competing for pose slots. Record any sliding during the crowd burst
separately from differences in who gets detected or pursued.

The original pose sender also staggered updates by distance, sent
changed bones between periodic full poses, and shrank its pose budget
under congestion. Those mechanisms are candidates for expanding crowd
coverage after validating the current pose writer.

## Alert readback test

The candidate reuses the original `CrashGuards::TensionOf` reader and
200 ms alert cadence. Each PC logs changes above ambient and returns to
ambient for its validated NPC bindings. The host samples running actors
and guests sample held, drained actors. Log lines include role, stable
entity ID, and local actor birth. `Tension probe` reports how many
eligible actors returned a known level on the first sample. Restart
clears the sample history. The original awareness-event parser also
logs local spotted, trespass, arrest and combat events with the local
seat and mission epoch. This gives a second timestamped record to
compare with the per-NPC tension changes.

In a two-PC run, let the first player visibly trespass and draw a guard,
then break line of sight and wait for the search to settle. Restart in
game and repeat with the other player taking the same route. Keep the
observer out of sight, and avoid knockouts or kills during this test.
Compare the NPCs' visible reactions and the paired `npc-alert` logs.
Levels 3–5 cover searching and alerted states, and 6–8 cover hunting,
arrest and combat. These levels describe the NPC's state. Identifying
the actual player being pursued requires a separate perception or
interaction readback. The probe performs no alert writes or ownership
transfers.

## Patrol retest

Compare a walking NPC from a distance and again after moving closer.
Check that position and facing stay smooth at both distances and that
nearby legs and body motion follow the host. Repeat after a host-led
restart. Use a fresh native build directory and verify a live launcher
status request before publishing the candidate.
