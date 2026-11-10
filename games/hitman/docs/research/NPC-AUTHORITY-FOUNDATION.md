# Shared NPC authority foundation (development only)

The working movement preview is frozen separately. Its release ZIP and corresponding source are preserved under preserved/Four-Player-Preview-1-Two-PC-Tested-2026-11-09 and R:/Hitman-Coop-Backups/Four-Player-Preview-1-Two-PC-Tested-2026-11-09. The installed DLL was verified against that release. User-verified scope is two real Steam PCs, smooth movement/crouch/vault, start barrier and remote disguises. Four real PCs and reconnect/restart are not yet verified.

NPC work lives only in dev/HitmanCoop-npc-foundation, build label 0.13.94-four-player.npc-dev.1. No NPC update installer is issued. PartyNpcSync is NOT instantiated by the native preview runtime, and the installed movement preview remains unchanged.

## Implemented core

PartyNpcAuthority is one host coordinator for the entire party, not three independent two-player ownership systems. The host registers stable NPC IDs from its catalog, then validates all owner reports against room session, mission epoch, NPC ID, connection incarnation, lease generation and report sequence. Unknown IDs, nonowners, old epochs/generations/incarnations and nonmonotonic reports cannot change canonical state. Catalog, queues and decoder inputs are bounded; overflow explicitly fails/interupts rather than silently dropping ownership controls.

Ownership transfer releases the old owner, waits for native freeze and final-state capture acknowledgement, prepares the next owner with the host's canonical baseline while it remains frozen, and only then commits the new lease. A missing acknowledgement suspends authority for that NPC; there is no timeout-based second grant. Prepared/commit failures are retried without acknowledging an unperformed native operation. Actors that appear late retain their newest pending baseline, including death rather than the original alive catalog entry.

An owner that is already fighting, in an interaction, or down can decline before being frozen. That keeps its current simulation/interaction intact. Handoff attempt generations are never reused after declines. If engagement begins during the actual release, the new generation returns to the original owner. Calm-NPC proximity policy considers up to four fresh validated positions, a two-second hold and five-metre hysteresis. These are configurable policy defaults, not verified gameplay tuning.

Canonical state currently includes transform, alive/unconscious/dead, stripped-clothing flag and engagement/interaction flags. Death and stripped clothing cannot be undone by an older snapshot. Wake-up/respawn require separate explicit transitions and are not implemented. Body drag/container state, outfit bags, inventory, NPC skeletal pose, takedown animation, actor birth/respawn, damage attribution and interaction claims are not included in this state codec.

PartyNpcReplica has explicit freeze/apply/resume/capture callbacks. It never starts a newly assigned simulator until those native operations succeed and the host commit arrives. Old commits cannot cross an observed release fence. Refreshing an existing lease does not reset report sequence or restart AI. Stop distinguishes an actual confirmed native freeze from an unsuccessful attempt; stopping report authority does not falsely prove that physical AI has stopped.

PartyNpcSync binds the core to the real PartyRoom channels. Guest reports/acknowledgements reach only the host through Interaction; the host publishes canonical HostWorld commands. Raw guest reports are not approved for direct forwarding. Lifecycle controls and critical death/clothing states use reliable delivery; ordinary transform updates use sequenced snapshots plus periodic source refresh and host baselines to heal dropped final updates. NPC scope ends when the room leaves Playing, membership/incarnation changes, scene is lost, or the host stops the NPC controller. Restarting the same stopped scope is declined; a new mission epoch is required.

## Validation

The final full suite passes 178 core/network/runtime tests, plus six direct pose-hook tests and four generated speech/AI-guard tests: 188 total. The complete native development project builds, and the DLL passes SDK 4.2.1 / ABI 1 and all 55 import checks. These are automated/model and build results, not native NPC playtesting.

Twenty-three NPC tests include per-step assertions of at most one running simulator per NPC across four fake worlds; two- and four-peer use of the same real PartyRoom/codec/state pipeline; guest-to-guest transfer; final release-state capture; native freeze/apply/resume failure; actor pooling; knockout/death/clothing monotonicity; wrong envelope/channel/identity/state rejection; delayed commits and old report sequences; busy declines and burned attempt IDs; combat races; bounded queue failure; host shutdown; and full partitions. Eight deterministic runs converge through 22% loss, 30% duplication and reordering. Codec fuzz/truncation/trailing-data checks also pass.

Evidence: checks/npc-foundation-all-tests.txt, npc-foundation-pose-tests.txt, npc-foundation-guard-tests.txt, npc-foundation-build.txt and npc-foundation-dll-checks.txt.

## Native work still required

The existing native SetNpcPuppet API is void and defers changes during the first three seconds of a scene. It cannot by itself prove that an AI worker has stopped. The new freeze acknowledgement must be based on actual applied native state/worker synchronization, including NPCs not yet pooled or currently interacting. Applying knockout/death and a canonical baseline must be idempotent and must use the correct per-player actor context; do not blindly call the legacy single-partner helpers.

Native input/damage/takedown arbitration must prevent an unassigned local player from irreversibly killing a puppet before ownership is resolved. Owner reports alone cannot undo that native action. Full AI knowledge/search/perception state is not transferred here. Remote stand-ins are still unseen to native NPC perception; delegating simulation by proximity does NOT solve detection of every nearby real player. Those are actual engine integration requirements, not claims covered by the tests.

Once these bindings are implemented, two real PCs are sufficient for initial native ownership, perception, KO/death, restart and error testing. Use occasional four-person sessions for simultaneous interactions and three remote avatar/AI contexts. Keep the preserved Preview 1 available throughout.
