# Four-player transport refactor — first isolated module

Status: development-only, not installed or exposed by the launcher. The accepted playable baseline remains Foundation 4.1, supporting two players. User acceptance and host log are saved in checks/playtest-06/RESULTS.md.

## Implemented

core/PartyTransport now maintains three independent guest connections behind one host datagram socket. Host is player 1; admitted guests occupy IDs 2–4. It reuses the existing reliable UDP Connection implementation. The host stamps and verifies sender identity, receives guest actions locally, and forwards them to the other guests without echoing them to the originating guest. Host-originated world-state traffic has a host-only channel; guest attempts to use that channel or claim another player ID are rejected.

Each datagram has an experimental H4P1 envelope with a party-session identity and a per-link incarnation. These are checked before Connection processes sequence numbers or acknowledgements. Reusing a slot with a new incarnation therefore rejects delayed packets from its prior guest without poisoning the replacement connection's reliable sequence state. Application deliveries also carry source incarnation so the future native player registry can reject already-forwarded stale source data.

Only explicit Admit calls can allocate a guest slot. Unknown endpoints cannot allocate state. Capacity is bounded at three guests; duplicate endpoint/incarnation admission is idempotent. Incarnations must be nonzero and unique among active guests. These values are not generated or negotiated by this module, and they are not a substitute for an admission/authentication design.

The host reports removed guest IDs/incarnations for future roster cleanup. A timed-out guest can be removed while the other guest links continue. Client host-timeout, connection overflow, bounded receive processing, payload limits and explicit application-queue exhaustion prevent silent acceptance of an unusable link or unbounded queue growth.

## Reliability issue found and fixed

The initial stress run lost one guest's first reliable batch. The reused Connection packet header defaults its acknowledgement to zero before receiving any packet. A four-player host may send that guest another player's update before it receives the guest's first packet. If that first guest packet is lost, an initial ACK=0 could incorrectly acknowledge the guest's sequence-zero packet.

The new transport starts each connection at sequence 1, leaving pre-receive ACK=0 referring to no sent packet. A dedicated regression drops the guest's first packet, lets the host send first, and verifies that the guest action is still retransmitted and delivered. The existing installed protocol-19 session implementation was not modified.

## Validation completed

122 tests pass, zero failures, recorded in checks/four-player-transport-tests.txt. Nine new transport tests establish:

- One host plus three guest sockets exchange source-identified updates; no local echo.
- Capacity and duplicate identity/endpoint rules.
- Reliable actions arrive exactly once under 22% simulated packet loss, 35% duplication and jitter/reordering.
- Slot replacement rejects the prior link incarnation before reliable sequence processing.
- Guest identity and host-world-state forgery are rejected.
- Actual Windows UDP loopback with one host and three independent guest sockets.
- One guest timeout preserves the other two guest links and reports the removed identity.
- A host sending first cannot falsely acknowledge an undelivered initial action.
- An unconsumed application queue reaching its bound produces explicit failure instead of silent reliable-state loss.

Existing two-player/network/launcher/lifecycle tests remain in the same passing suite. No change was installed into the game, no new port forwarding was requested, and no screen capture was used.

## Required integrations before four-player play

1. Version/build-checked join admission and exchange of party/link identities. Wire this to the launcher and roster rather than calling Admit with test-provided identities.
2. Roster dissemination, source-incarnation filtering and disconnect handling on every receiver. Already-forwarded old-source packets require application-level roster filtering too.
3. Per-player avatar/pose/victim state and native stand-ins. SdkWorld and CrashGuards currently retain single-partner assumptions.
4. Host-authoritative NPC ownership/state coordination plus validated client interactions; native simulation may need delegation for guest perception. Do not run three independent copies of the existing distributed two-player ownership algorithm. See FOUR-PLAYER-ROOM.md for the native-perception constraint discovered in the current stand-in design.
5. Mission epoch validation for gameplay payloads, all-player readiness/loading, native completion acknowledgements, and the persistent launcher mission loop.

The routing module is opaque to gameplay payloads. It does not yet validate mission epochs or action semantics, discover peers, render three native remote players, synchronize four-player NPC interactions, implement extraction, or establish internet four-player performance. Passing its four-socket tests is not certification of four-player HITMAN gameplay.

The subsequent PartyRoom module adds automatic admission, roster/reconnect handling, mission epochs and lifecycle coordination above this transport. Those additions pass 13 more tests (135 total). They remain separate from native avatars and the installed two-player plugin. See FOUR-PLAYER-ROOM.md.
