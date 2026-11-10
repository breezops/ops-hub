# HITMAN two-player co-op: investigation and proposed foundation

Research date: November 8, 2026. This document records the initial investigation. Current progress: the original beta was installed and tested on two real PCs; paired findings are under checks/playtest-01, and the first compiled development update is described in FOUNDATION-1.md.

Local validation completed: 81 stable-core tests, 92 beta-core tests, and 13 relay-logic tests on each snapshot passed. See VALIDATION.md for scope and raw outputs. These results do not certify in-game behavior.

## Intended experience

Two Windows PCs running Steam copies. Steam Offline Mode is a desired supported configuration, while the PCs remain connected to the internet. Players connect from different locations; the host can forward ports.

Desired flow: Host session -> friend joins -> show missions both players can launch and have installed -> choose a tested mission -> both ready -> both load -> two visible assassins play together -> both complete/extract -> return to the session.

Recommendation: evaluate and, if necessary, extend Jordan Rodrigues's HITMAN Co-op beta. Keep the stable release as a comparison build. Do not start by replacing the networking or writing a new multiplayer implementation.

## What exists and what it establishes

| Project | Evidence | Fit |
| --- | --- | --- |
| HITMAN Co-op, JordanRodrigues021 | Stable 0.13.2; beta 0.13.94. Source implements custom UDP sessions, partner avatar, NPC ownership, deaths/knockouts, items/doors, pause/restart, status and diagnostics. | Closest match. Candidate foundation, pending real-player tests. |
| ZHMModSDK / Hitmen, OrfeasZ | Community runtime SDK; Hitmen has direct-IP GameNetworkingSockets code and a second Hitman entity experiment. Current source includes commented-out position/NPC send loops and unfinished sections. | Engine reference and dependencies; weaker starting point for a usable co-op build. |
| Ghost Mode / LocalGhost / HITMAPS | Revives the existing competitive Ghost Mode path and its matchmaking/invite infrastructure. | Useful historical reference, but does not establish shared-world assassination co-op. |
| Peacock | Local replacement for HITMAN's service layer. Co-op README says it is not required and its combination with the mod was untested. | Consider later if local mission/profile services are needed; it does not supply shared simulation by itself. |

The beta's November 3 release notes describe Freeform Training tests with a test bot and explicitly say two real PCs were not yet tested in that release's validation. Reported tests and features are not our certification of the mod.

## Important source findings

1. **Steam-independent transport.** `core/src/Socket.cpp` uses Windows UDP sockets. Default direct port is UDP 47047. Optional Python relay uses UDP 47100; the README says no public relay is built in. Direct IP is the simplest first path for this user.
2. **AI authority is distributed.** The host assigns which PC simulates each NPC, generally by proximity with hysteresis and exceptions for active pursuit. On the other PC that NPC is a streamed puppet. This is not one complete host simulation that natively sees two players.
3. **Partner avatar is a stand-in.** The README and engine code describe a remote avatar excluded from normal local AI targeting/damage. Need to test a guard noticing player B while player A owns that guard, especially when both players stand together. This can be a fundamental gameplay limitation.
4. **Scene matching is not full mission matching.** `SceneDesc` compares scene plus mission bricks, with CPU-specific bricks excluded. `OnLoadScene` can redirect within the same location and mount scene resources. That does not establish matching contract IDs, targets, scripted objectives, loadouts, or full ownership checks.
5. **Ownership must be checked per mission.** The source assumes starting one mission at a location means other missions there are owned. We should not inherit that assumption for a mission browser. Resource files being present also does not prove permission to play a mission.
6. **Completion is currently local.** In stable and beta, `SdkWorld::OnPeerMissionEvent` displays notifications for `ContractEnd` and `ContractFailed`. `OnTeamEvent` handles ready checks, signals, countdowns and related events; it does not implement a full shared completion state machine. Kill synchronization intentionally preserves target-death events for local objectives, but mission completion needs real testing.
7. **Version gates should be stricter for us.** Stable's session configuration allows game-version mismatches by default. Our first test will require identical game build, mod build, difficulty, mission and relevant settings.
8. **Beta improves relevant weak spots.** The beta adds body positioning/drag handling, more nearby NPC animation synchronization, out-of-order packet handling and better diagnostics. These make it the preferred experimental candidate, not a proven reliable release.

## Steam Offline Mode and connection setup

There are three independent things: Steam login status, HITMAN's connection to IOI services, and the connection between our two PCs. Steam can be offline while Windows networking remains connected. The mod's custom UDP transport does not require Steam matchmaking. Actual offline game startup and mission availability still need validation on both installed copies.

First finish installation and launch the game once normally on each PC, load the proposed mission, then exit and verify it starts and loads that mission with Steam in Offline Mode. This follows Valve's initial-launch guidance. Treat IOI-server-offline gameplay separately; persistent mastery, unlocks and online modes are outside our first acceptance target.

Host router rule:

| Field | Value |
| --- | --- |
| Protocol | UDP |
| External port | 47047 |
| Internal destination | Host PC's current LAN IPv4 address |
| Internal port | 47047 |

Reserve that LAN address in the router so the rule continues pointing to the host. Allow inbound UDP 47047 through Windows Firewall for the game/mod. Leave the firewall enabled. The friend joins the host's public IPv4 address in the co-op UI; the guest needs no forwarded port. Public IPv4/CGNAT reachability must be checked if connection fails. No TCP or UDP 47100 forward is needed for this direct path. UDP 47100 is only for hosting the optional relay.

## First mission: ICA Facility, Freeform Training

Choose the replayable Freeform Training mission, rather than the initial guided tutorial. It has a small reproducible sandbox, a target, guards, disguises, items and extraction. The upstream beta's test harness already uses it, giving us a useful comparison.

Both players must confirm that exact mission is independently playable. Use identical difficulty, default starting location, matching basic loadouts, no other gameplay mods, and a fresh mission rather than a save. Both initially launch it manually; automatic lobby-driven loading comes after gameplay passes.

Initial build policy: record game executable version, loader version, mod version, protocol and hashes on both PCs. Evaluate beta 0.13.94 if it supports the installed game. Stable 0.13.2 targets 3.290.1; beta notes list 3.290.x and 3.280.x builds. Do not downgrade or patch blindly if the newly installed game differs. Pin the selected test build and turn off its automatic updates during a test series.

See TEST-MATRIX.md for the exact first-session procedure.

## What to build after the test

### Milestone 0: reproducible baseline

Record versions and settings; get direct UDP working; collect logs from both PCs; demonstrate a complete Freeform Training run. Run the guard-awareness test near the beginning. If player B can act invisibly beside player A, decide whether we can repair the authority model before spending time on the lobby.

### Milestone 1: dependable shared mission

Fix the observed blockers in priority order: player detection/damage, target and objective agreement, kills/knockouts/bodies, items and disguises, extraction, restart and teardown. Define initial policy: if either player dies, the team retries; both players must reach extraction before a team victory. This is our proposed behavior, not a claim about upstream.

Keep the successful two-PC connection alive while returning to a waiting state. If an authority repair requires true engine support for two perceived players, prototype that separately before committing to a broad map rollout. Do not silently call a visual stand-in fully functional co-op.

### Milestone 2: lobby and verified mission list

Add a two-slot lobby with Host/Join by IP, connection status, matching-build checks, selected mission, ready state and clear reasons a mission is unavailable.

Each client reports a locally verified catalog. A mission is selectable only when it is on our tested allowlist, both clients can legitimately launch it, its required packages are installed, and both sides agree on the exact mission/contract variant and difficulty. Cosmetic name or location alone is insufficient. Until a reliable entitlement/catalog API is identified, use a small explicit allowlist with independent native-menu launch verification; do not advertise an automatic ownership detector.

Proposed launch handshake: host proposes exact mission settings -> each client validates and acknowledges -> both ready -> each invokes normal game mission startup -> each reports loaded identity -> compare -> hold simulation until both are ready -> start. If validation or loading fails, keep the session and show the reason. Preserve each machine's CPU-dependent simulation resources when comparing mission content.

Start with manual native-menu loading and explicit ready confirmation. Automate launch only once the proper contract/start API is understood. Scene loading alone can place players on the same map with different objectives.

### Milestone 3: wider mission support

Expand one mission at a time, retaining a compatibility record per exact variant. Suggested next candidates are Hawke's Bay for another small environment and Paris for two targets plus crowd/streaming pressure, if both players own and can launch those missions. They are proposed test candidates, not confirmed compatible missions.

Campaign assassination levels can be evaluated individually. Synchronized mission-story opportunities, scripted solo interactions, cutscenes, campaign transitions and persistent progression require separate validation. Freelancer, shared saves, arbitrary user contracts, joining mid-mission and more than two players are later scope.

## Decision after our first session

Continue from upstream if two players can see each other, guards react usefully to either player, kills/objectives agree, and a mission can be completed and restarted repeatedly. Repair isolated failures in a small credited development branch. If the authority model prevents essential stealth behavior, prototype that missing capability before building the lobby or promising general compatibility.

The near-term deliverable is one repeatably playable mission on two real PCs, with Steam Offline Mode verified, followed by a lobby showing only missions that have passed the same process.

## Evidence and local copies

- Co-op repository: https://github.com/JordanRodrigues021/hitman-coop
- Stable release: https://github.com/JordanRodrigues021/hitman-coop/releases/tag/v0.13.2
- Beta release: https://github.com/JordanRodrigues021/hitman-coop/releases/tag/v0.13.94-beta
- Beta documentation: https://github.com/JordanRodrigues021/hitman-coop/blob/beta/BETA.md
- SDK: https://github.com/OrfeasZ/ZHMModSDK
- Ghost Mode: https://github.com/hitmaps/ghost-mode
- LocalGhost / Peacock: https://thepeacockproject.org/wiki/ghost-mode/
- Valve Offline Mode guidance: https://help.steampowered.com/en/faqs/view/0E18-319B-E34B-B2C8

Inspected co-op main snapshot: `7f7ecdb0b9394f85fdaa1ba791f0d6af8ca1b916`.
Inspected co-op beta snapshot: `1a9572cac9358d96e5bde00137722ac7e6c0f9e3`.
Inspected SDK snapshot: `9043debf7b7e90c35a3e71f5f264f133c11a7168`.

Source copies are under `upstream/`. The SDK clone has Windows long-path checkout failures in third-party RakNet sample files; the inspected Hitmen source was available. The beta is a source archive extracted from the fetched commit, not a separate Git checkout. Co-op's repository license is GPL-3.0; preserve its license and credits in derivative work.

## Latest development status

Foundation 3 is compiled and installed on the host; friend package is deliverables/HITMAN-Coop-Foundation-3.zip. See FOUNDATION-3.md and checks/foundation-3-validation.txt. All 105 automated tests pass. The latest paired Foundation 2 findings are in checks/playtest-03: native nude-bit scanning fixed the reported guest-first corpse-disguise case; transform-only dragging reached the host's physical body; discarded outfit bags and locker animation remained missing/incomplete. Foundation 3 adds native bag replication, bounded target-velocity drag correction, permanent removal of Windows notification sounds and separate dump diagnostics. Those new native behaviors await the next paired test. The native locker animation and shared-exit binding are not solved. The intended dedicated launcher remains Host / Join / tested mission / ready / launch, with the SDK menu retained for development.

## Required player count and launcher foundation

User confirmed FOUR players total (host + three guests). This is now a product requirement. The current playable upstream-derived backend still handles only two. Foundation 4 adds a real local launcher/plugin bridge, standalone WPF controls, a player-array status schema, and a tested four-seat lifecycle model. It does not yet expand gameplay transport, per-player avatars or NPC authority. See FOUNDATION-4.md and checks/foundation-4-validation.txt. Automatic mission loading, shared native extraction and the complete persistent-lobby loop remain unfinished. The next live test validates launcher controls using the existing two-PC setup before the networking refactor. Do not advertise four-player gameplay based on the standalone roster tests.

Foundation 4.1 subsequently passed all three launcher checks on both PCs by user report, corroborated by the host log (checks/playtest-06). It is the verified installed baseline. The development-only PartyTransport refactor now passes nine additional tests, including actual host-plus-three-guests UDP and loss/reordering/slot replacement, for 122 passing tests total. See FOUR-PLAYER-TRANSPORT.md for completed scope and remaining native/admission/authority work. It has not been installed or represented as playable four-player support.

## Four-player admission milestone

The development-only PartyRoom controller now performs automatic version-checked admission, endpoint-bound cookie challenges, host-assigned IDs, reliable roster snapshots, acknowledged joins, resumable identities/new-endpoint reconnects, leave/timeout cleanup, all-player readiness/loading/completion and replay epochs. Host approval gates interactions before forwarding. All 135 core/network tests pass, including four actual UDP sockets. See FOUR-PLAYER-ROOM.md and checks/four-player-room-tests.txt. It is not yet wired into the native launcher/gameplay backend; installed Foundation 4.1 remains unchanged. Next is per-player native avatar/rig/knowledge/victim state and global NPC ownership coordination. Native stand-in invisibility means purely host-run AI cannot simply be assumed to detect guests correctly.

## Preserved 4.1 and isolated native migration

At the user's request, the exact Foundation 4.1 installer and archived source are now preserved in verified read-only archives under preserved/Foundation-4.1-Verified-2026-11-08 and R:/Hitman-Coop-Backups/Foundation-4.1-Verified-2026-11-08, with restore instructions. Native four-player changes now live separately in dev/HitmanCoop-four-player (four-player.dev.1), not the baseline source or installed game. PoseHook has per-player/incarnation rig/pose contexts with three remote targets; six direct hook tests and all 135 core/network tests pass. See FOUR-PLAYER-NATIVE.md. SdkWorld avatars and native AI/speech/interaction authority are not yet migrated; this is not a playable four-player build.

## Native guard and avatar state progress

The isolated four-player development copy now has per-player/incarnation speech, AI knowledge and reasoning guards, plus RemoteAvatarState holding 56 previously scattered avatar-specific fields. Four new guard tests execute the generated speech stub against synthetic actors and validate cleanup/reconnect/warmup behavior. The full project builds, 145 automated tests pass, and DLL ABI/import checks pass. This remains development-only: native methods still target the legacy record, and three-avatar spawning/room-stream wiring is next. See FOUR-PLAYER-NATIVE.md. Installed and archived Foundation 4.1 remain unchanged.

## Native avatar adapter and playback milestone

Twelve native character methods are parameterized by RemoteAvatarState, preserving legacy wrappers. The development SdkWorld now implements a roster/incarnation/epoch-scoped IPartyAvatarWorld with independent native pose/AI guards, rig watching, actor reuse and scene cleanup. PartyAvatarPlayback decodes bounded typed streams and buffers each remote player's movement/poses independently, clearing stale identities, replay epochs and changed skeleton poses. All 156 automated tests pass; the full native build and SDK ABI/import checks pass. These adapters are not yet driven by a PartyRoom instance in the plugin frame loop or the launcher. That runtime integration, actual local capture/send and native scene-transition reporting are next before any friend installer or in-game four-player claim. See the current milestone at the top of FOUR-PLAYER-NATIVE.md. Installed Foundation 4.1 and both frozen backups remain protected.

## Four-player movement preview 1 runtime connected

PartyPreviewRuntime now owns the room/playback in the native game frame loop, captures/sends local movement/poses and reliable outfit/skeleton metadata, reports loaded only from native Freeform/local-player availability, and interrupts on actual scene clear/loss using party protocol 2. A dedicated build-matched preview launcher controls real Host/Join/Ready/Start/Stop/Reconnect and shows the actual four-seat roster. The legacy backend is mutually excluded. All 161 automated tests, the full native build/ABI checks, native named-pipe launcher harness and disposable setup/upgrade/4.1-restore/removal tests pass. A friend package with its own exact 4.1 rollback is prepared under preview-1 and deliverables. Native gameplay remains untested; first use two real PCs, then four. This backend has movement/disguises only; NPC/body/item/objective/extraction authority is still future work. Installed Foundation 4.1 remains untouched. See FOUR-PLAYER-PREVIEW-1.md.

## Preserved live preview; isolated shared-NPC core and fewer-person testing

User confirmed the first real two-PC Preview 1 movement/start/disguise test. Its exact installer/source are now verified and preserved on C: and R: under Four-Player-Preview-1-Two-PC-Tested-2026-11-09. Current installed preview matches the archive. New work lives in dev/HitmanCoop-npc-foundation with a distinct development label.

A host-coordinated four-peer NPC authority/replica protocol and PartyRoom transport bridge now pass 23 new core tests, including no overlapping fake simulators, calm proximity delegation, confirmed native handoff callbacks, busy declines, actor pooling, generation fencing, canonical KO/death/clothing state, loss/reordering and partition shutdown. Full suite is 188 tests, native build and ABI/import checks pass. Native NPC binding/perception/damage arbitration remain unimplemented and are explicitly not installed. See NPC-AUTHORITY-FOUNDATION.md.

An optional standalone Replay Lab targets frozen Preview 1 and adds two visibly synthetic avatar slots, so two real people can exercise a four-seat roster and three rendered remote contexts. Its four model tests and built EXE/four-localhost-UDP integration test pass. It does not run extra games or simulate NPC interactions. See TESTING-STRATEGY.md and deliverables/HITMAN-Coop-Replay-Lab-1.zip. Four real PCs remain a short integration/performance check, not a prerequisite for routine development.


2026-11-09: Three-human Preview 1 vault/takedown defect isolated. Preview 2 develops native picker protection and damage fault/mission-reload handling separately from NPC authority. See FOUR-PLAYER-PREVIEW-2.md. Frozen Preview 1 and Foundation 4.1 unchanged.
