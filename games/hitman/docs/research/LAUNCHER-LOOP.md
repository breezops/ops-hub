# Next milestone: a complete two-player session loop

User priority, November 8, 2026: after the current body synchronization pass, build the dedicated launcher and a useful launch/play/extract/return loop, then continue gameplay polish. The ZHMModSDK menu remains a developer interface rather than the intended everyday player flow. Do not make exhaustive animation or mission coverage a prerequisite for beginning this milestone.

## Intended first player flow

1. Open the co-op launcher. Detect the Steam install, compatible game/mod version, and whether HITMAN is running. Installation/update/removal remains accessible from the same entry point.
2. Host or join a friend. Reuse the working direct connection first; the host's existing UDP 47047 configuration stays applicable. Show connection state, partner, matching versions, and reconnect state in plain language.
3. Choose a tested mission available on both PCs. Initially expose only Freeform Training; add missions after validation rather than displaying an unverified full campaign list.
4. Both players ready. The host selects the allowlisted contract and compatible mission setup; both games acknowledge the selection before loading. Show loading progress/status and recover clearly if one game fails to load.
5. Play together using the existing working movement, takedown, disguise and outfit-bag foundation. Keep co-op Windows notification sounds removed.
6. Complete the mission and extract. Bridge the real in-game exit/completion path so both games acknowledge completion; handle one player failing or disconnecting without silently calling it a shared victory.
7. Preserve the connected lobby while both games return to a selectable state. Offer replay, choose mission, or leave. Prove this loop twice without restarting HITMAN.

## Engineering boundaries

- The launcher needs an actual local bridge to the plugin for status and constrained commands; a UI that merely launches HITMAN does not establish this loop.
- Use the existing multiplayer transport and identity/version checks. Keep local launcher control local to the Windows user, with a bounded command schema and a mission allowlist.
- Loading and completion should be explicit states acknowledged by both peers. Mission epoch and contract identity scope all ready/load/exit/replay messages, preventing a delayed event from a previous run from affecting the next one.
- Real shared extraction is currently unresolved. Native exit metadata already exists in checks/playtest-02/native-exit-types.txt: the repository reference is indirect, through m_rRepositoryIdHolder. The native Trigger pin has now been matched by CRC32 to d5d636c1; its payload/conditions and IBoolCondition semantics still require inspection. A matching pin name alone does not establish a safe native exit call.
- Keep debug controls available for development. Friends should not need to use them for the normal tested loop.
- Public matchmaking, automatic updates, broader entitlement discovery, campaign progression, mission stories and all-map support remain later work.

## Acceptance for the first launcher milestone

Two Steam PCs at different locations can use the launcher to join, select Freeform Training, ready, load, finish/extract together, return to the same lobby and replay twice. Mismatched builds, a loading failure, cancellation and one disconnect produce understandable recoverable states. Install/update/remove remains usable without developer tools. No Windows co-op notification noise. An attractive launcher without the acknowledged two-game mission loop does not satisfy this milestone.

## Immediate body-pass boundary

The received interaction metadata establishes the native ZRagdollAttacherEntity fields and two pin hashes that match Attach/Detach. Next investigate the game's actual runtime linked entity, hand-bone names and victim-bone binding, then mirror the native attachment with cleanup on release, timeout, storage, disconnect and scene teardown. These actual bindings remain unknown; do not invent hand/limb names or enable native attachments using guessed data. Fresh animated dump poses were observed in the previous paired logs, but remote ragdoll/animation ownership is still unverified.

Once a bounded body pass has produced a usable result or an isolated known limitation, proceed to the launcher/extraction milestone and continue minor body polish alongside it. Outfit-bag exchange already passed the paired Foundation 3 test and should be preserved.

## Confirmed player-count requirement

The user confirmed four players total: one host and three guests. Foundation 4's launcher bridge and player-array schema are the first integration step; the current gameplay backend remains two-player. A tested LobbyRoster models four-seat admission and per-player ready/load/complete acknowledgements, with interrupted sessions and mission epochs. Networking and native avatar/authority routing must adopt that model before four-player play is claimed. The first live launcher-control test still uses the existing two-PC setup.
