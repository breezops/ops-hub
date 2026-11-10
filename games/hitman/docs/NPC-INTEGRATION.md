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

Native bridge progress on this branch:

- `CapturePartyNpc` reads the existing stable-ID actor catalog through the SDK,
  verifies the entity's current ID and excludes player stand-ins. It reuses life,
  clothing and transform classification. Unknown tension/interaction remains
  busy; a legacy cached pacify marker is not an independent completion receipt.
- The knockout helper now accepts an explicit source entity. Legacy event
  attribution stays in its legacy wrapper. Its native Boolean return is
  preserved and is not treated as verified baseline completion.
- Read-only binary inspection of 3.290.1 identifies the existing unique table
  caller, synchronous reasoning loop and direct per-unit reasoning call.
  `VerifiedNpcReasoningLoop` resolves those relationships rather than using a
  hardcoded runtime address.
- `NpcReasoningFence` wraps that loop with a nonblocking exclusive hold and
  shared native-call guard. Its checked prologue/trampoline is tested against
  generated x64 code, including in-flight refusal, ABI argument preservation,
  restoration, reentrant holds and blocking the next call while held.
- Existing puppet-list changes defer publication while reasoning is active;
  `TrySetNpcReasoningHeld` succeeds only after the guarded list actually contains
  or removes the NPC's unit. Registered player actors cannot be controlled by
  that NPC API. The movement frame flushes deferred publications if any exist.

The fence is **not installed by the native preview**. Installation/removal
require native-quiescent points; trampoline memory is retained deliberately for
already-fetched jumps. A reasoning-only hold does not stop actor state updates,
animation, physics or guest damage. It must not acknowledge `PrepareNpcScene`
or `FreezeNpc` until those additional paths are bound. The SDK labels several
distinct actor update phases; their names alone are not synchronization proof.

Native fixtures can now build with `cmake -S tools/native-tests -B
_build-native-fixtures` and run through CTest without an SDK or game. CI executes
the production pose hook, guard stubs and reasoning-fence/registry code against
synthetic buffers and executable fixtures; it never patches HITMAN.

Additional native ingress and receipt bindings:

- `NpcMutationGate` binds a local scope, stable NPC ID, native actor birth and
  owner lease. New bindings start blocked. Old scope/birth/lease grants cannot
  authorize replacements, and bounded tombstones reject retired births.
- The existing hit detour uses an admission hold through the original native
  call while a scope is active. Role changes/retirement retry while a hit is in
  flight. Exact-actor canonical permits are keyed to the current native stack;
  they do not enable picking or another thread/fiber stack's damage.
- The existing picker stub scans bounded atomic NPC slots as well as the player
  slots. It preserves the original normal-path comparison flags and supports
  holes. The default movement preview has no NPC scope or bound slots.
- Only managed native hit/picker paths are covered. Unknown/unadmitted actors,
  direct script kills, environmental effects and interactions already running
  still need handling. Active NPC mode owns hit dispatch through CallOriginal;
  coexistence with additional third-party hit detours has not been validated.
- Native 3.290.1 ActorId event producers call GetID and hash the returned native
  entity reference: low32 * 0x13285CD7 - high32 * 0x61C88647, wrapping uint32.
  This is a local native identity, not the cross-PC stable NPC ID. The event
  decoder accepts exact integer-valued telemetry decimals and rejects truncation.
- `NpcLifeReceipts` requires this PC's native ContractSessionId and a verified
  actor ticket. Hash collisions/recycled event identities cannot select an
  actor, late Pacify cannot reverse Kill, and same-contract room epoch changes
  preserve physical life facts only after fresh actor tickets are bound.
- The native achievement callback queues bounded receipt data under the session
  lock; the frame resolves native actor identities on the game thread. It counts
  all activated actors for collision checks, including actors absent from the
  shared catalog. Overflow interrupts rather than silently discarding facts.
- Party capture requires an actual down state plus a confirmed receipt, or a
  native IsPacified result, for ambiguous dead/KO cases. It does not use the
  legacy optimistic pacify cache as completion proof. The future adapter must
  obtain the local native contract session before acknowledging readiness.

Read-only inspection also finds a separate action-output loop that schedules
deferred/fiber work; the synchronous reasoning fence does not cover it. Its
completion/cancellation boundary must be verified before native NPC preparation
can succeed. In addition, the current scene-clear hook runs before the engine's
actual teardown: it is not itself a post-destruction worker fence. Bind scope
retirement and fault recovery to a proven lifecycle boundary before enabling
the adapter. Existing animation and client body/ragdoll presentation can remain
non-authoritative; the required hold covers decision/action and damage authority.

The inspected custom context switch saves/restores the TIB stack base/limit and
switches rsp. A thread-local override alone can therefore be visible from a
different execution stack on the same thread. Windows [fiber-local storage](https://learn.microsoft.com/en-us/windows/win32/procthread/fibers)
is documented for Windows fiber switching; the custom switch does not establish
that FLS follows it. The ingress gate instead uses a bounded stack-keyed permit
registry and an atomic reader/writer admission counter. No thread-owned lock
spans the original hit call. A generated alternate-stack fixture swaps TIB bounds
and rsp while retaining the same thread-local marker; it verifies permit
isolation, nested canonical updates and refusal to mutate admission while a
native call on another stack is still active. Do not apply the synchronous
reasoning-loop lock unchanged to deferred/fiber action jobs.

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
