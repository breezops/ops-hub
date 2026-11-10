# Four-player admission and room controller

## Latest: Protocol 2 preview runtime

The native plugin frame loop and dedicated preview launcher now drive PartyPreviewRuntime/PartyRoom/PartyAvatarPlayback. Party protocol 2 adds reliable native SceneLost interruption. The first distributable is movement/disguise only, with manual Freeform loading and no four-player world interactions. All 161 automated tests, native build/import checks, named-pipe launcher checks and disposable installer/rollback/removal checks pass. Native real-PC playtesting is still required. See FOUR-PLAYER-PREVIEW-1.md; earlier milestones below are historical.


Development-only implementation: core/PartyRoom. It is not connected to the installed HITMAN plugin or launcher. The verified installed baseline remains Foundation 4.1 and still supports two-player gameplay.

## Implemented and tested

PartyRoom now owns automatic admission over the same socket as PartyTransport. Clients send a bounded, versioned join request; the host checks matching protocol/mod/game values and returns a short-lived return-address cookie. The cookie uses Windows BCrypt HMAC-SHA256, binding the endpoint, host session, attempt nonce, version fields, name and reconnect credential. Proof is checked before allocating a slot. This checks return-path ownership; it is not Steam/account authentication or transport encryption.

The host assigns IDs 2â€“4 and per-connection incarnations. A guest becomes visible in the shared roster only after acknowledging the admitted transport. Duplicate proofs resend the existing assignment rather than rebuilding its connection. Pending joins expire, capacity is bounded, and incompatible builds/full parties receive explicit rejection. Joining an already loading/running mission is declined; reconnecting a known member is handled separately.

Roster snapshots are reliable and host-originated. Each includes revision, mission epoch, phase, and every connected member's ID/incarnation/name/availability/readiness/loading/completion state. Clients validate source, scope, size, unique IDs and their own assignment before replacing the roster.

Reconnect uses a host-issued resume credential retained in the room object. It can replace the UDP socket/address, retain the player ID and issue a new link incarnation, even if another lower-numbered slot is free. A fresh attempt nonce rejects old admission replies. A removed/reused slot invalidates the prior identity; credentials are in-memory and are not persistent account identities. Ordinary leave is acknowledged; loss still falls back to timeout cleanup. Host shutdown/migration is not a seamless failover feature.

The networked lifecycle now requires all present members to acknowledge availability/readiness before loading, all to acknowledge loading before play, and all to report completion before returning to the lobby. Pending joins also block starting. A disconnect during play interrupts the mission rather than converting the remaining completion reports into a victory. Replay selects a new epoch and resets readiness.

Gameplay payloads carry the mission epoch and are checked against current roster incarnations, including when queued events are later popped. Avatar/pose streams may relay after these checks. Interactions initially reach only the host; forwarding requires an explicit authority-adapter approval. Guests cannot publish host world-state or forge another player's identity. The low-level transport's automatic relay remains available for its isolated tests, but PartyRoom disables it so control/interaction validation occurs first.

## Native behavior deliberately not implied

ReportLoaded and ReportNativeCompletion are API inputs in these tests, not calls to load or extract inside HITMAN. Native adapters must report actual game transitions and validate gameplay action semantics. This controller does not spawn avatars, replace the current plugin Session, implement AI ownership, start contracts, trigger an exit, discover installed DLC/mission entitlement, or complete the four-player game loop by itself.

## Validation

All 135 core/network tests pass, zero failures, recorded in checks/four-player-room-tests.txt. Thirteen new room tests cover:

- BCrypt HMAC against an independently computed SHA256-HMAC vector.
- Three automatic joins converging on the same four-member roster, including idle connection survival.
- Full-room and mismatched-build rejection without disrupting admitted guests.
- Join/roster convergence with simulated loss, duplication and reordering.
- A captured return-address proof being rejected when replayed from another endpoint.
- Reconnect on a new endpoint preserving player 4 even while slot 2 is free, with a changed incarnation.
- All-player readiness/loading/completion and a replay epoch.
- Host approval of interactions and rejection of stale queued gameplay across replay.
- Disconnect interruption without false shared victory.
- Automatic joining over four actual Windows UDP sockets.
- A welcomed guest whose acknowledgement is in flight blocking mission start.
- Late join rejection during active play.
- An absent host timing out, fresh-nonce retry succeeding when the host appears, and closed-room action rejection.

The full development project builds. The installed Foundation 4.1 DLL was compared with its archived payload and remains unchanged. No new friend installer or live gameplay claim is issued for this core-only work.

## Next native integration

Separate the current single-partner avatar, rig, pose, knowledge and victim state into per-player records keyed by ID/incarnation. Update AI/speech/restart guards for three simultaneous remote stand-ins. These are real single-partner assumptions in SdkWorld, PoseHook's default target, and CrashGuards.

NPC authority must be coordinated once for the whole party. The current stand-ins are intentionally hidden from native NPC perception, so simply running every NPC's AI on the host would make native guest detection a separate unsolved problem. A practical continuation is host-authoritative ownership decisions with a single simulation owner per NPC, delegating native simulation where needed for the actual local Hitman. The host validates owner reports and publishes canonical world state. Do not instantiate three independent legacy two-player ownership algorithms.

After those native adapters are usable, connect this room status/action interface to the launcher's player array and four-player controls, then validate actual mission loading and extraction separately.
