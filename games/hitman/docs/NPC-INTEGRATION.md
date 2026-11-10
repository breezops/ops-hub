# Shared NPC integration — development stage

The public player build remains OPS 0.1.3 / native Preview 4. It passed the
two-PC movement/turning tests; brief post-disguise gliding remains a known limit.
Its immutable package is preserved. This branch adds source and test integration,
not a new playable NPC release. The native caller still constructs the runtime
without an NPC adapter, so no NPC simulation callbacks execute in that mode.

## Reuse map

| Existing component | Reuse and required adaptation |
| --- | --- |
| Original ZHMModSDK co-op adapter | Keep game/entity access, scene tracking, stable NPC catalog and actor lookup. `ListNpcs` already excludes every remote player actor and ambiguous template IDs. |
| Original NPC snapshots/interpolation/poses | Keep transform/pose tools and codecs; route through one party coordinator rather than one Session per friend. |
| Original puppet helpers and CrashGuards | Keep actor knowledge and reasoning controls. Queued puppet requests and registry membership alone cannot acknowledge a completed AI-worker freeze. |
| Original knockout/death/clothing helpers | Reuse verified native operations after removing single-partner assumptions and validating actor context, reentrancy and event attribution. |
| Preserved PartyNpcAuthority / Replica / Sync | Restored with their tests. One coordinator covers 2–4 identities, epochs, owner leases, canonical life/clothing state and bounded queues. |
| Tested Preview 4 presentation | Preserve root-frame turning, disguise visibility, rig-count guards, incarnation checks and stand-in interaction protection. |
| Preserved PartyReplayClient | Restored for development tests, including the new orientation metadata. Labeled synthetic slots mirror avatar packets; they provide no native NPC AI or perception. |
| OPS HUB launcher/updater | Keep the existing package, checksum, install/removal and publication flow for future playable releases. No update is published by this branch. |

## Implemented in this stage

`PartyNpcOptions` defaults to HostOnly. NPCs begin with the host as owner, and
both proximity-driven and explicit guest lease transfers are refused in this
mode. NearestPlayer is explicit development opt-in; the existing release / prepare /
commit ownership protocol and tests are retained for later integration.

`PartyPreviewRuntime` accepts an optional `IPartyNpcWorld` adapter. With it,
HostWorld / Interaction packets feed the recovered NPC pipeline while Avatar /
Pose packets continue through the tested presentation path. Host registration
reuses `IGameWorld::ListNpcs`; zero/duplicate IDs are excluded, pooled capture is
retried, capacity overflow interrupts, and a registered ID becoming ambiguous
interrupts instead of silently updating a different actor.

Before reporting a loaded mission, every enabled peer must successfully call
`PrepareNpcScene`. Its contract requires holding local NPC simulation, including
future pooled actors, until individually resumed. The room's existing loaded
barrier therefore precedes the first host lease. The default implementation
returns false, so an unimplemented adapter cannot accidentally enable this mode.

Stop, leave, native scene loss, roster/incarnation loss and new epochs stop NPC
authority. A failed physical stop latches a reload requirement on that PC;
Stop/Ready/Start and later successful retries cannot clear it. Actual native
scene teardown clears the latch. Confirmed stops are not repeated every frame.

The recovered core preserves dead/unconscious and stripped-clothing progress,
rejects nonowner/stale reports, and heals dropped ordinary snapshots with
periodic host/source refreshes. This is not a skeletal NPC pose, inventory,
body-container or mission-objective codec.

## Validation and limits

The core suite now includes the restored ownership/network and replay tests,
host-only tests with two/four peers under loss/duplication, and runtime tests
for simultaneous avatar/NPC traffic, NPC preparation, Stop/Ready/Start, scene
loss, guest leave, late actors, catalog ambiguity, capacity and failed stops.
These use real room/codec/transport logic with modeled native callbacks. They
do not establish that an actual HITMAN AI worker has stopped.

Four seats in a replay test are four protocol identities, not four independent
game simulations. Two real PCs remain sufficient for most incremental engine
tests; later four-PC passes still need to cover simultaneous interactions,
perception and independent remote contexts.

## Next native milestones

1. Bind preparation/freeze to an observed native worker boundary, account for
   actors entering the scene later, and only acknowledge completed operations.
   The old void `SetNpcPuppet` intentionally queues work during scene startup.
2. Bind canonical capture/apply/resume using existing helpers with explicit
   party/NPC context. Do not invoke the legacy single-partner attribution path
   blindly, and do not mark a failed native operation complete.
3. Route guest interaction/damage claims through the host before irreversible
   local effects. Host-only simulation alone does not prevent a guest from
   killing its local puppet, and cannot undo that effect afterward.
4. Integrate perception for multiple players. Current stand-ins are deliberately
   unseen to native NPCs, so neither host authority nor proximity delegation
   automatically makes guards detect every player.
5. Enable a versioned, explicitly labeled Freeform NPC test only after those
   bindings are ready. First test host patrol movement, KO and death on two PCs;
   then guest actions, simultaneous actions and occasional four-PC checks.

Shared extraction/objectives remain a later layer. Keep changes in this branch
separate from the preserved playable movement build and retain upstream credit
and GPL source when future packages ship.
