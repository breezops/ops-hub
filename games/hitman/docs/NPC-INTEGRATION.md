# Shared NPC integration — development stage

The intended persistent party and host-controlled mission flow is specified in
[SESSION-LOOP.md](SESSION-LOOP.md). Manual preview readiness is temporary; native
preparation and baseline checks must also support automatic admission.

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
Stop/Ready/Start and later successful retries cannot clear it. The pre-clear
callback begins a process-unique native retirement request; it does not clear
the latch. Only `ConfirmNpcSceneRetired` for that exact request may clear it.
Adapters without that verified native binding remain blocked. Confirmed stops
are not repeated every frame.

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

### NPC animation across maps and modes

Apply pose selection and playback to the NPC roster discovered and agreed
for the current mission. Use stable entity IDs for binding. Rank streams
using motion, guest distance and recent stream membership, regardless of
the NPC's name, story role or mission. Ritter and Ronnie are Freeform
regression cases for intermittent pose streams. The cause of their visible
stutter still needs an in-game check.

The native party adapter currently admits Freeform Training. As each new
map or mode is enabled, verify roster agreement and animator binding,
including actors that spawn during play. Compare patrol and crowded
alerts, with guests together and in separate areas, then verify clean
rebindings after restart. Use pose capture and guest animator-write logs
to distinguish slot contention from an animator that fails to apply a
received pose. Fix those paths in the shared adapter so new missions
inherit the correction.

### Controlled guest handoff

The original two-player session could let the guest's game run a pursuing
NPC. The party core now also supports an explicit host-requested handoff
with the agreed catalog and initial baseline receipts. The old owner
stops first, the new owner acknowledges its applied baseline, and the
host commits the new owner. Baseline retries retain the completed hold.
Busy actors remain with their current owner. This path lets a development
test request one handoff without enabling automatic proximity delegation.

The native adapter can now capture the release snapshot at the action
boundary after confirming the actor is held and its job has drained.
The core waits for that snapshot before preparing the receiving PC.
The standard build still selects host-only. A live handoff needs a verified
guest resume, a new running-job receipt, and a return to host control
after the NPC settles. The adapter marks native interactions busy when
it cannot prove the NPC is free. Keep that check until the native
interaction reader or a controlled test condition is verified.
The initial handoff rehearsal disabled NPC poses. The next candidate
streams poses for the host's nearby NPCs and for the guard while the
guest runs it. The host checks the sender and current ownership
generation before applying or relaying a guest frame. Each receiver
clears the old pose on an ownership change and drops frames from
previous generations. Native animation on the owner's own PC continues
through the game animator. Guest streaming is currently exercised with
the one-guard rehearsal. A larger delegated roster will need a shared
bandwidth budget.

The separate `COOP_NPC_GUEST_HANDOFF_TEST` build runs the agreed Freeform
catalog and waits for an explicit F8 press from the host of a two-PC
session. It selects an ambient non-target guard within eight metres and
uses the same actor for subsequent requests in that epoch. Wait for the
guest's running-job receipt before approaching the guard. After the
guard returns to ambient tension, F8 requests return to the host. Avoid
physical interactions, knockouts and kills throughout this rehearsal.
The diagnostic guard filter is scoped to this build because absence of
drag, dump or victim flags is insufficient to prove a guard is generally
free for transfer.

The 0.1.34 two-PC rehearsal selected Jordan Reeves at the boat stairs.
The guest confirmed a running native job for Reeves after the first F8,
and the host confirmed a new running job for the same actor after the
second F8. The testers saw Reeves react to the guest during guest
ownership while the adjacent guard stayed quiet. The guest log shows no
spotting, arrest, combat, or NPC tension transition during that interval.
Its recorded trespass events came after the return to host ownership.
This verifies the round trip and records the observed warning. A sustained
guest detection or pursuit run still needs evidence while the guest owns
the guard. Keep that ownership until the reaction and recovery are over.

The repeat run confirmed guest spotting, arrest, combat and pursuit while
Reeves was running on the guest. His tension returned to ambient before
the second F8, and the host confirmed its replacement job. The testers
saw Reeves slide on the host and other NPCs slide on the guest. These
views used remote transform updates with pose streaming disabled. The
rehearsal authorized only Reeves to run on the guest. Extending reactions
to nearby guards requires a general ownership policy and shared alert
handling.

In 0.1.35, both players saw smooth pursuit and recovery. The guest
captured Reeves's 231-bone pose, and the host confirmed writing it.
After the return to the host, the host resumed AI in about 200 ms but
its first pose capture came about four seconds later. The testers
reported a snap at the return. The next candidate prioritizes recently
returned actors for 1.5 seconds within the existing moving-pose range
and slot cap. This addresses the observed stream gap. The snap still
needs a live retest.

In the 0.1.36 retest the host resumed Reeves in 167 ms and captured the
first host pose 259 ms after F8. The tester saw an immediate head
direction reset without the previous snap.

### Automatic guard assignment

The first automatic build uses the existing explicit lease handoff in
a two-PC session. The host selects at most one calm, eligible guard
from the agreed roster when the guest is within 14 metres and is
at least three metres closer than the host. The guard remains with
the guest during warning, pursuit and recovery. After the guard has
been calm for two seconds, the host can take it back if the guest
moves outside 18 metres or the host becomes clearly closer.

The initial test uses the public side of the boat stairs. Keep the
host back and have the guest approach and trespass without F8. Move
the guest away after the guard recovers and compare the paired
native running-job logs. The automatic selection is map-independent,
while the private native rehearsal still runs in Freeform Training.

The 0.1.40 two-PC test passed without F8. Jordan Reeves started running
on the guest before the trespass and entered alert, arrest, and combat
on that PC. He returned to ambient, then the host resumed his AI at
the original post. Ronnie Aguilar was assigned only after Reeves
returned to the host.

The 0.1.41 build keeps one slot per guest, including during handoff
and return. Each calm eligible guard is considered for the closest
guest. If that guest already has a guard, the host keeps the candidate.
With three or four PCs, spread guests across different guards and
check that one guard returning does not interrupt another pursuit.
The simulated four-player test covers three separate assignments,
competition for the same guard, and independent recovery. Native
simultaneous encounters still require a multiplayer test.

The 0.1.43 coverage test follows the legacy Session ownership policy's
proximity and active-pursuit principles. The party adapter can assign
up to three eligible guards per guest and starts at most one new
assignment per guest on each policy update. It counts transfers
against that cap until the host confirms the return. The two-player
simulation checks that two calm guards return as the guest moves away,
new guards take the freed slots, and a pursuing guard stays with the
guest throughout. The live Freeform run should use normal movement
through separate areas, then check patrols, aggro and recovery after a
host restart. Fold the 0.1.42 weapon check into the same run.

This stage still assigns only eligible guards using player proximity.
The original mod also distributes other NPCs, uses native contact and
pursuit events to influence assignment, and relays alert floors. Those
are separate gaps in party gameplay. In particular, a guard owned by
the host still cannot natively see a guest stand-in when the two
players travel together. Test that case explicitly as perception
improves. Mission-wide alert propagation, shared interactions and
objectives remain separate steps toward the full mission loop.

The 0.1.43 two-PC logs confirmed three guest-owned guards progressed
from warning into combat and back to ambient. Two returned to host
ownership about four seconds after reaching ambient. One returned
around 33 metres from its initial position. The logs cannot establish
its intended post at that moment. The 0.1.44 policy waits until the
guard has been calm and its movement settled for two seconds before
automatic handoff, so the original owner's navigation can continue.
The recovery regression test keeps an ambient guard moving past the
old transfer deadline, then confirms handoff after it settles.

### Drawn weapon visibility

The same test reported invisible drawn weapons on remote players and
guards. The 0.1.35 party animation carried bones and transforms. It
did not carry equipment identity or drawn state. The original mod
already identifies the local player's held item by repository ID and
attaches a native presentation copy to its legacy stand-in using the
inventory handler. Reuse that path with a slot per party avatar and
reject callbacks from a replaced actor or retired mission.

For guards, first verify the owner-side inventory handler's main-weapon
and unholstered fields during draw, holster, and ownership transfer.
Prefer showing the replica's matching native item when present.
Scope the visual state to the current actor and ownership generation.
Verify draw and holster on both PCs before connecting the separate
gameplay paths for shots, damage, pickups and inventory.

The 0.1.36 retest confirmed matching main weapons on both PCs. The
guest saw Reeves draw his gun, and the host saw empty hands. Both logs
kept the actor's unholstered flag at zero. The follow-up probe reads
visibility on the actual weapon geometry and the item's native
visibility switch, in addition to the inventory and actor fields.

In 0.1.40, Reeves's weapon visibility switch followed the visible
draw and holster on the guest. The host had the same main weapon
and its switch stayed off while Reeves was a replica. The geometry
flag was on throughout, and the unholstered flag stayed off. Use the
owner's visibility switch as the next candidate signal, and match
the replica's weapon identity before applying it.

The 0.1.42 candidate carries the main item's full repository ID and
visibility switch in the existing authoritative NPC snapshots. The
action boundary applies the switch only to a matching weapon on a
held guard. The owner retains native weapon handling. The network
test covers draw, holster, return and rejection of the former owner's
reports. In the two-PC test, check whether the gun appears in the
right hand and clears on recovery. Check again after a native restart.
If the weapon is still invisible or appears detached, report what
each player sees and save both logs.

During the 0.1.43 run, the remote PC applied both visibility values
and read them back for the matching guard item. The geometry flag
was already on. The other player still saw empty hands. The next
build logs the distance from the item to its actor to distinguish a
weapon remaining at its spawn from one following the actor while
invisible. This diagnostic does not yet reconstruct weapon attachment.

The same run repeatedly attached and recreated player item copies,
sometimes alternating repository IDs about a quarter-second apart.
The party sender now requires two matching samples before changing
what the receiver sees. This filters isolated sample errors and adds
one sampling interval to intentional changes. Rifle grip and
attachment alignment remain open.

The player-weapon candidate sends the held item's repository ID with
the avatar frame. Empty frames clear the held item. Playback rejects
old timestamps and stale party identities before changing the avatar.
The native adapter requests a presentation item for that seat and
attaches it through the original inventory finalizer. Holster, hide
and scene changes invalidate outstanding requests. The callback checks
the requested item entity before touching the avatar and disables pickup
on the visual copy. Test draw, holster, swap, and a mission restart with
two PCs before expanding this to back-slot equipment or guards.

The 0.1.37 two-PC test passed draw, aim, and holster for both players.
Each log confirmed attachment of the same pistol to the other player's
avatar. The testers saw the pistol move between the front and back of
the character while remaining held. The mod follows the streamed hand
pose and uses a fixed right-hand attachment. Record this as likely native
concealment motion. Weapon attachment after a mission restart remains
unverified in 0.1.37. The 0.1.38 test then confirmed both players could
draw and holster after a host-led restart, with a pistol attachment
logged on each PC in the new mission epoch.

### Order for the next playtests

1. Retest the returning guard's pose and keep the successful chase as a
   baseline. The same run can compare the native guard's main weapon and
   drawn state against its held copy. The rehearsal logs both values
   when they change on either PC.
2. Verify player weapon visibility using the original stand-in attachment
   path. Use the guard readback to decide whether replicas can show
   their existing weapon or need a presentation copy.
3. Check Escape and tutorial interruptions separately using the same
   build. The rehearsal logs menu requests and the native `UI_Menus`
   hold while the party tick runs. Use that evidence for a small native
   menu experiment before wiring unanimous pause. Keep NPC handoff and
   retirement independent of menu and pause state.

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

### Deferred action observation

`VerifiedNpcActionJob` follows the existing units-table caller to a second call
using the same table, checks the action loop's unit stride, resolves its job
descriptor's function pointer and requires a unique matching job body. A
read-only signature check against the installed 3.290.1 executable confirmed
those relationships. No fixed executable address is used by the runtime resolver.

`NpcActionProbe` provides an optional observation hook at that job entry. It
replays four whole instructions (16 bytes), preserves the native unit argument,
and counts a return only after the original job function returns. A short mutex
protects fixed-size bookkeeping; no thread-owned lock or TLS spans native code.
Counts therefore remain outstanding when a custom stack runs other work. If a
stack is abandoned without returning, its count remains outstanding. Capacity
or tracking faults latch unhealthy status; the observer never skips native work.
Removal refuses observed outstanding calls or an unexpectedly modified patch.
Installation/removal still require external native quiescence, and retained
trampoline memory is not proof that unloading the plugin is safe.

Eight new fixtures cover the job trampoline and result, an original call still
in flight, reentrant diagnostics, concurrent independent units, nested work on a
custom execution stack, tampered hooks and invalid/ambiguous resolver inputs.
The existing alternate-stack fixture is shared with the damage-admission tests.
These fixtures do not establish real-game cancellation semantics.

The observer is **not installed by the movement preview**. An empty observed
count alone is not a freeze acknowledgement: new scheduling must also be held,
other action entrances accounted for, actor lifetime pinned and native teardown
completed. Read-only inspection finds a separate cancellation path that requests
job state 2, resumes a suspended job and then cleans up its action/descriptor.
The scheduler also uses state 3 for terminal cleanup. Do not treat a state-byte
write as a completed cancellation or call that native path from an arbitrary
callback. The next binding must run at a verified engine lifecycle point and
confirm original-call return before replacing native state. This work remains
independent of the temporary manual preview Ready/Start controls.

### Cancellation binding and scene recovery

`VerifiedNpcActionSites` resolves the action job together with cancellation,
cleanup and dispatch. It checks the pending-job load, suspended/cancel-request/
terminal state instructions, the cancellation routine's cleanup tail and two
action-loop calls to the same dispatcher. Unknown state transitions, mismatched
targets, out-of-text targets or ambiguous entry patterns reject the entire
bundle. Read-only checks matched this bundle in the installed 3.290.1 executable.
This resolves addresses; it does not invoke cancellation or establish a safe
native lifecycle point.

The SDK's existing `SceneCleared` notification is an early callback. The runtime
now stops/discards its old replica there and calls
`BeginNpcSceneRetirement(token)` before the engine tears the scene down. It
preserves any stop fault and blocks preparation, Ready and presentation while
the native retirement request is pending. The token is unique within this
process across runtime instances, including successive rooms/clear requests.
`ConfirmNpcSceneRetired(token)` defaults to false and is polled on subsequent
ticks. An adapter must retain retirement evidence across its ordinary pre-clear
data reset and distinguish the retired scene from a newly loaded one. Neither
`IsInScene=false` nor a previously completed token can authorize recovery.
Polling does not retry operations against the old scene's NPCs. Movement-only
mode never requests NPC retirement and retains its existing behavior.

Four additional core regressions cover early-clear blocking and stale receipts,
token reuse across runtime instances, movement-only behavior and avoiding native
operations on retired actors. The existing failed-stop recovery test now requires
an explicit teardown receipt. Two additional native fixtures cover the complete
cancellation bundle and changed cancellation semantics/targets. Total validation
is 251 tests: 203 core, 10 pose, 6 guards and 32 native NPC fixtures.

The engine side of retirement confirmation is still unbound. To implement it,
hold new scheduling, perform cancellation at a validated engine boundary,
observe original action return and native teardown completion, and then retire
the matching actor/scope bindings. A pre-clear callback must not manufacture that
receipt. Native NPC mode remains disabled until these operations are verified.

### Native per-actor hold/resume primitive

`VerifiedNpcBehaviorSites` extends the cancellation bundle with the game's
existing actor/bool function. It checks the actor flag/runtime-ID offsets, unit
stride, live-unit check, unit hold-bit update and call to the same cancellation
routine, all against the same units table. It leaves the SDK's unknown flag name
unchanged. Read-only inspection matched this path in installed 3.290.1. Resolve
the bundle before installing the job probe, whose patch changes the job prefix.

`NpcActionPhase` optionally wraps the action output loop. A callback receives a
noncopyable permission valid on that native stack only, before original action
scheduling. Installation must occur on the explicitly identified SDK game thread;
an unexpected execution thread latches unhealthy status before any callback.
`SetHeld` requires an exact scope/birth/lease pin, a supplied fresh
native identity/lifetime validator, membership in the current enabled-ID span,
matching runtime ID, unit actor pointer and consistent actor/unit flags. It invokes the existing
native actor function and verifies identity and state again afterward. Mutating
operations and input spans are bounded to the SDK's 500 native actor slots;
the old reasoning guard's larger address-comparison bound is not a memory-access
bound for this helper. A hold
completes only when both flags agree, the native action/job references are clear
and the matching job probe has no outstanding call for that unit. A native return
without those observations remains pending. Resume also requires local ownership
and a drained operation; callers must separately prove canonical baseline apply
and authority over any pre-existing native/script hold.

`NpcMutationGate::Pin` protects binding metadata through the whole native call
without a thread-owned lock or granting damage permission. No such lock or TLS
spans cancellation or a native stack switch. The output-loop hook always calls
the original loop with the original span, including after a callback fault.
Unhealthy status latches and refuses reinstallation; removal refuses observed
in-flight phase/action calls or a modified patch. Externally established native
quiescence and callback lifetime are still required for install/remove/unload.

Seven isolated phase fixtures exercise the actual hook/trampoline and modeled
actor function: identity refusal, changed native identity after return, partial
state, local-owner resume, alternate-stack permission refusal, fault handling
and a suspended job. The latter uses a Windows fiber to suspend the original job,
then resumes it through modeled cancellation and confirms it returned before
reporting completion. The existing alternate-stack fixture separately covers
the game's custom-context property. Two additional resolver tests reject changed
actor layouts/flags, unit tables, cancellation targets and ambiguous code. The
full suite is 260 tests: 203 core, 10 pose, 6 guards, 34 native NPC and 7 phase
fixtures. SDK-free CTest now runs four executables; isolated processes preserve
the probes' one-native-entry lifetime invariant.

This primitive is **not installed by the playable preview**, and no production
SDK validation callback or NPC world adapter calls it yet. The action loop is a
candidate engine boundary; the live actor-list/lifetime conditions and native
function's animation-controller effects still need validation. Pool admission,
external script hold arbitration, irreversible damage/interaction paths, native
contract acquisition, canonical apply and actual scene-retirement confirmation
remain required before native shared-NPC mode can be enabled. An operation's
completed result is not by itself a scene preparation or retirement receipt.

### Construction generations and the first identity check

The development mod now observes the SDK's existing `ZActor_ZActor` constructor
hook on 3.290.1. It records the raw address and a monotonic local generation,
then returns Continue so the original SDK detour chain and constructor run
normally. It does not inspect the unfinished actor, call native controls or take
the session mutex. `NpcActorBirths` has its own short metadata lock, a bounded
registry and exception containment. Missing observations never invent a birth;
address reuse invalidates the previous generation, and a capacity/tracking fault
invalidates identity proof rather than allowing an untracked actor.

Construction entry is not completion or lifetime proof. Fresh SDK validation
checks the observed game thread, Freeform scene, unique activated-list template
ID, enabled/activated flags, runtime slot, native entity reference and GetID.
The action-boundary validator additionally checks the actor-manager list-lock
flag, current scope/birth/lease and construction generation. Scoped capture and
life-receipt binding reject mismatched generations. Event collision counting
still includes unmanaged actors, but a stale construction cannot supply a life
receipt. The real native lifetime/actor-list contract still needs live validation;
none of these checks automatically installs the action-phase mutation hook.

The construction registry survives the early scene-clear callback. Its explicit
pruning operation requires a later verified retirement barrier, retains current
survivors and all constructions after a watermark, and never resets generation
numbers. No production retirement callback prunes it yet. Six new fixtures cover
unknown/reused addresses, surviving and newer constructions during retirement,
capacity reclamation, latched faults and concurrent constructor events. The full
suite is now 266 tests: 203 core, 10 pose, 6 guards, 40 native NPC and 7 phase.

A private, separately identified OPS 0.1.4 candidate logs read-only identity
audits in Freeform Training. It leaves shared NPC control off and needs only one
PC. It is not the public update channel. See
[the solo guide](../diagnostics/identity-1/START-HERE.txt) and
[reproduction instructions](../diagnostics/identity-1/BUILD-CANDIDATE.md).
The candidate includes a verified copy of the immutable OPS 0.1.3 ZIP and a
restoration shortcut. Its install/restore path is tested in a synthetic game,
and the actual archive passes the launcher's staging/checksum checks. A live
identity audit is still required; modeled tests do not establish constructor
coverage, runtime-slot relationships or safe control behavior in HITMAN.

The user's first solo identity run on 3.290.1 passed ten audits: seven before
restart and three afterward. Every audit reported 83 enabled, unique and
admissible NPCs, zero untracked/rejected/duplicate identities, healthy tracking
and the same SDK update thread. The constructor watermark advanced from 99 to
198 across restart; an outfit-discard event also occurred during the run.
This supports the observed constructor coverage and runtime-slot relationships
for this Freeform run. It does not validate other missions, multiple PCs, the
action-loop invocation context, hold/resume side effects or physical teardown.

The next observation path now uses the existing action-loop boundary in an
explicit read-only configuration. `NpcActionPhase::Config::allowControl=false`
requires a fresh observation validator, does not require a job probe, and makes
every `SetHeld` request refuse. `Boundary::Observe` verifies the current game
thread/stack, scheduled runtime ID, fresh stable ID/birth and native table actor
pointer. It validates identity again after reading. The SDK validator requires
the actor-list lock and healthy constructor tracking without enabling a mutation
scope. No preview automatically installs this observer yet.

`NpcBehaviorReadback` copies the supported actor/unit flags and action/task/job
pointers through checked reads. It reports conflicting flags rather than fixing
them. A missing pointer or apparently drained record is an observation only,
not proof that native workers are finished or that an actor can be retired.

Read-only inspection of the 3.290.1 AI update established why neighboring SDK
frame callbacks are insufficient: the caller sets `m_bLockActorLists` before
updating behavior, invokes both verified reasoning/action loops while locked,
then clears the flag before returning. Registering at the next SDK priority
does not execute within that window. An experimental adjacent-priority sampler
was discarded before packaging or playtesting. Installation of the actual
action-loop observer still requires an established native-quiescent point;
neither a priority name nor this binary inspection alone establishes that.

Six additional fixtures cover read-only native memory inspection and actual
observer-hook execution: invalid/unreadable memory, runtime/pointer mismatch,
conflicting flags, identity changes during observation, execution-stack and
scheduled-ID checks, original-loop execution, and refusal of actor control.
The suite now contains 272 tests: 203 core, 10 pose, 6 guards, 44 native NPC and
9 phase tests. These are modeled checks, not live observer validation.

### Process-lifetime live observer

`NpcActionObserver` supplies the next live read-only hookup separately from the
control primitive. It uses a private static copy of the same MinHook library
used by ZHMModSDK (pinned v1.3.4 source and license under `third_party/minhook`).
MinHook handles entry patching and thread instruction-pointer relocation; this
does not establish a native worker-drain or actor-mutation barrier. The observer
does not install an action-job probe or enable any NPC mutation scope.

Installation is attempted once on the already observed SDK game-update thread,
only for game 3.290.1 with the complete verified site bundle. Before enabling
the hook, a valid original trampoline and immutable configuration are published.
The hook validates the native table and bounded, unique scheduled runtime IDs;
wrong threads, malformed spans and callback exceptions disable inspection while
the original native loop keeps executing. The view checks stack/thread identity
and revalidates actor birth/SDK identity around each checked native read.

The containing module is pinned until process exit. `Stop`, called outside the
client callback before the SDK world is destroyed, waits for the short read-only
callback and detaches its context. It does not wait for an original native loop
or yielding job. The entry then passes through to the retained original; no
trampoline removal, uninitialization or executable-memory freeing is attempted.
This diagnostic requires restarting HITMAN for reload/update; it does not claim
general SDK hot-reload safety. No thread-owned lock spans original scheduling.

Seven isolated process fixtures exercise the actual MinHook path: identity and
stack refusal, native pass-through after stop, wrong-thread and malformed-span
refusal, callback faults, detachment during callback/native execution and entry
patching while a synthetic native function is already executing. Together with
the earlier suites this is 279 tests. Normal and action-diagnostic SDK builds
compile. Actual in-game invocation and record relationships remain unverified
until the user completes the solo action.1 test.

The private candidate is OPS 0.1.5 / native
`0.13.94-four-player.preview.4.action.1`; its recipe and guide are under
`diagnostics/action-1`. It logs `[npc-act]` audits at most twelve times per
Freeform scene, retains identity audits and includes the verified original
OPS 0.1.3 rollback. It changes no NPC control, scheduling, mission loop or
pause policy. The public updater channel remains untouched.

The next live action.1 investigation resolved the complete site bundle in the
running game and confirmed the exact action-loop entry bytes. Read-only state
inspection of that same, hash-matched diagnostic DLL established that module
pinning and hook-library initialization succeeded, but no hook or original
trampoline was created. The refusal is therefore within `MH_CreateHook`; the
original build did not preserve its return code. This does not validate action
callbacks, actor-list locking at that boundary, or any native control operation.
The user clarified that their disk-space concern described the game crash,
not the observer refusal; these are separate findings.

Private action.2 (OPS 0.1.6) defers the single attempt until Freeform is loaded
and records the exact installation stage and MinHook status. The adjacent Win32
last-error value is only a snapshot and may be stale. Delayed installation is
a hypothesis to test, not a demonstrated fix. A new refusal fixture checks that
invalid configuration/prologue reporting leaves installation unarmed and that
a subsequent valid entry can still be observed. Delivered earlier candidates
and the public channel remain immutable. Start with the short guide under
`diagnostics/action-2`; do not repeat a full mission test before checking startup.

The first solo action.2 run passed: installation returned MinHook status 0 and
fourteen action audits (ten before restart, four after) each matched all 83
scheduled/enabled NPCs with actor lists locked, zero rejected/untracked IDs or
conflicting flags, and healthy observation. The constructor watermark advanced
99 -> 198 and the callback stayed on the same SDK update thread. The user
performed a knockout and disguise change before restarting. Job-pointer counts
changed from 83 to 82 during that sequence and returned to 83 after restart;
that is observation, not a completed-job or freeze acknowledgement. The final
audit reported 8,340 native-loop invocations. No observer fault/error appeared.

This validates the read-only callback/identity path for that run across restart.
Delayed installation succeeded there; action.1's precise creation error remains
unknown. Native job-entry/return coverage, cancellation, hold/resume, physical
retirement, and multiplayer NPC authority still need their own validation.

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

### Retained job observation (private action.3)

`NpcActionProbe::InstallRetained` reuses the existing fixed-capacity entry/return
bookkeeping with thread-aware MinHook entry patching. It verifies the complete
16-byte job prefix, pins its executable module and publishes the original
continuation before enabling the entry. Its bridge has no SDK world context,
and no lock or TLS ownership spans the native job. Hook/library/trampoline
removal is deliberately refused in this mode even when observed counts are zero.
The externally-quiesced custom installation remains available to existing
fixtures/control primitives, with explicit refusal to switch installation modes.

Both job and action sites resolve before either is patched. Only private action
diagnostic builds install the job probe after Freeform loads; public builds keep
both diagnostic options off. Job installation stage/status is logged. Paired
action-boundary audits log entries/returns/outstanding, units with observed calls
active, native job pointers without observed entries, and actor births after the
attachment watermark. These are observations, not authority readiness receipts.

The late-attachment fixture deliberately starts a native call before attaching:
the call keeps running while entered/returned/outstanding are all zero. Its
eventual return cannot become an observed completion. This prevents treating
zero counters as drain proof. Further isolated fixtures exercise nested custom
stacks, simultaneous jobs, reentrant inspection, retained removal refusal,
invalid/mismatched entries and tampered patches. All 16 native test processes
pass; the private SDK build and SDK ABI check also pass. No real-game action.3
run has been claimed. The next solo test checks job behavior across two native
restarts, including a knockout/disguise between them.

Unit addresses belong to reusable table slots. An active entry at an address
does not bind that entry to the current actor birth, and the attachment
constructor mark alone does not establish worker coverage or physical lifetime.
Outstanding records are never discarded on scene pre-clear. Local hold/resume
still needs this evidence reconciled with fresh identity, native scheduling,
script-owned holds, cancellation and actual actor retirement before control.

The first live action.3 attempt did not attach either observer. Both creation
calls returned MinHook status 9 (`MH_ERROR_MEMORY_ALLOC`), while site resolution
succeeded and the saved Win32 last-error snapshots were zero. There were no
job or action audits. The user completed both restarts: fifteen identity audits
remained healthy for all 83 NPCs, with constructor marks 99 -> 198 -> 297.
This is an attachment failure, not evidence about native job completion.

At hook creation the pinned MinHook source can report this status from failed
near-buffer allocation or failed hook-table heap allocation. The current log
does not distinguish those paths. Diagnose actual allocator failures/usable
address ranges before changing hook installation or requesting another full
mission test. One earlier action.2 success does not establish reliable delayed
attachment; the next live check should confirm attachment before actions.

### Creation allocation diagnostics (private action.4)

`native/MinHookDiagnostics` traces actual VirtualAlloc/VirtualQuery and heap
allocation calls during this instance's synchronous hook creation. A forced
include on each vendor C source redirects the API names; the vendor's fourteen
pinned source/header/license files remain byte-identical. Per-source flags are
required because Visual Studio cannot reliably express mixed-target C/C++
language-specific flags at target level. The wrapper implementation compiles
separately without aliases, and never redirects other modules' imports.

Fixed per-thread capture counters do not allocate or lock. They survive the
installation refusal report and record immediate VirtualAlloc/VirtualQuery
errors. Heap failures carry counts rather than an invented Win32 error, since
those APIs do not guarantee one. No TLS capture spans native action jobs: hook
creation is synchronous, and normal job execution uses its existing bookkeeping.
On error 9 only, a separate bounded, read-only query walk counts free bytes,
largest free extent and aligned 4K candidate starts within the pinned allocator's
1GB range. Candidate counts are observations, not successful allocations.
The logging includes no process addresses and the allocator policy is unchanged.

Four isolated fixtures verify VirtualAlloc refusal, hook-table HeapAlloc refusal,
no-candidate refusal, successful creation/trampoline execution after each fault
is removed, and concurrent independent report capture/reset. All twenty native
CTest processes pass. Fault injection is compiled only into the SDK-free fixture
library; it is absent from the player build. This is diagnostic evidence for the
next startup check, not a claimed fix for the live intermittent allocation error.

The Action-4 live startup reported successful attachment and eight job/action
audits (last 99 entries, 16 returns, 83 outstanding), but the diagnostic artifact
was invalid. A report header was expanded while SDK compilation was already
running. Saved object disassembly confirmed the old Installation implementation
copied 96 bytes while its final caller expected 128. The resulting extra fields
were invalid; a misplaced logging condition printed scan data on success too.
The user quit normally. Discard this allocation telemetry and do not promote
that build's observations to verified job or actor-lifetime proof.

Action-5 fixes the condition and encodes sizeof(Report)/sizeof(InstallReport)
into Create and both Installation function signatures through LayoutTag types.
An old caller/callee layout now requests a different linker symbol rather than
silently exchanging incompatible data. The matching-layout fixture links and
runs; a deliberately stale 80-byte report fails at the tagged linker symbol.
All twenty native test processes pass. The replacement SDK build uses a fresh
empty directory after shared source/header edits are finalized and frozen.
This prevents reuse of the mismatched intermediates; full game startup remains
required before claiming the replacement diagnostic works live.

Action-5's complete solo run now passes native job observation across two
restarts. Eighteen paired action/job audits stayed healthy for all 83 actors,
with no identity rejection, conflicting flags or job pointer lacking an observed
entry. Constructor marks advanced 99 -> 198 -> 297. At the first restart the
sampled counters were 97 entered / 97 returned / 0 outstanding. At the second,
the first sample was already 199 / 198 / 1: do not claim an observed zero barrier.
Steady state returned to 83 outstanding calls in each fresh mission, without
sampled cumulative growth. Final counts were 289 / 206 / 83 and 10,940 loop calls.

During knockout/disguise one actor/unit native held flag was observed, with 82
outstanding jobs. The game can own such a hold; future control must refuse an
already-held actor and resume only the exact hold and actor generation it owns.
Original-call returns do not substitute for physical actor retirement or make
the SDK pre-clear event a multiplayer readiness acknowledgement. Keep control
disabled until a validated control phase can run without holding the observer's
callback/session locks across native cancellation, with identity/birth/slot and
hold-ownership checks. Do not stack the legacy custom phase patch on this hook.

Creation allocator reports are consistent in this run. The action hook reused
existing buffer/table storage successfully without a new allocation. Its current
no-near-code-candidate heuristic label is misleading on success; only infer a
failed search with actual status 9. This does not require repeating the passed
job test. The original intermittent allocation refusal remains unproven fixed.

### Private Hold-1 local control candidate

The retained action bridge now optionally reuses NpcActionPhase::Boundary for a
control callback. The existing observation callback remains short and locked;
control is admitted under that lock with an atomic in-flight context reference,
then executes after the lock is released. Stop deactivates admission and drains
both callback kinds before client teardown. Neither callback may call Stop.
The private SDK callback releases its session try-lock and refuses reentry from
OnFrameUpdate, whose outer session lock would otherwise survive the native call.
Boundary validation still requires the same stack/thread, scheduled slot,
fresh identity/birth, mutation ticket and healthy matching job probe.

NpcHoldArbiter installs a separate pinned pass-through observer at the verified
actor-control setter. It watches one actor per process. A consumed exact-stack
marker identifies the controller's single entry; external/recursive setter calls
are preserved and permanently refuse subsequent controller operations. This
does not prove complete coverage of direct flag writers or cross-thread order.
The bridge and original native code remain alive until process exit.

COOP_NPC_HOLD_DIAGNOSTICS defaults OFF. Its private solo F8 cycle selects a
nearby conscious guard only after a native restart supplies a fresh birth and
the selected unit has one observed job. It refuses pre-existing holds, confirms
native job return/cleanup before its eight-second hold, resumes only its own
hold and requires a new job afterward. Scene clearing invalidates diagnostic
admission first and never attempts native resume during teardown. A fault stops
further control; it never falsely acknowledges a pending operation complete.

All 28 native test processes pass, including retained Windows-fiber cancellation,
wrong stack/identity, shutdown during control, external and recursive holds, and
integrated cancellation/ownership/script-collision refusal. Hold-1 is a first
local in-game diagnostic, not a guest preparation or physical-retirement binding.

Hold-1's first live attempt attached all three hooks and received two F8 presses,
but selected no NPC. No native hold/resume cycle ran. The user was beside the
stationary entrance NPC working over oil. A civilian/mechanic outfit role may
have failed the guard-only filter, but the old log did not identify the stage.
Hold-2 accepts conscious non-target native NPCs regardless of outfit role and
logs every selection rejection stage plus nearest eligible distance. Identity,
fresh birth, scheduled slot, observed-job and hold-ownership checks are unchanged.
The existing standing NPC is sufficient; walking visuals are not required to
verify native job completion/resume. No knockout or further map access is needed.

### Confirmed near-allocation exhaustion and private absolute backend

Hold-2's next startup could not create the job/action hooks (status 9); the
arbiter was not attempted. Telemetry showed no VirtualAlloc or heap allocation
attempts. Read-only VirtualQueryEx scans confirmed zero aligned 4K candidates
within both 1GiB and nearly 2GiB of all three verified entries, despite 1,212,416
free bytes in holes whose largest extent was 61,440 bytes. The existing native
inspector resolved all relationships and live prefixes matched. Increasing the
search range would not have supplied a usable reservation in this process.

Hold-3 opts into the documented generated MinHook derivative described in
third_party/minhook/PROVENANCE.md. It allocates private executable pages anywhere
and uses fourteen-byte register-preserving absolute entries. It keeps upstream
thread suspension/IP mapping and copies complete decoded instructions. Only
verified relocation-free entry prefixes are admitted; unknown prefixes, relative
or RIP-dependent stolen instructions and short/hot-patch entries are refused.
Both original pinned sources and generated patch tooling are included for
reproduction; the compiled absolute sources are explicitly a local derivative.

Tests reserve a four-GiB arena around an entry, require the trampoline to be
more than two GiB away, execute the detour/original and verify restoration.
Existing cancellation, ownership, shutdown and concurrent installation tests
also pass with this backend. Normal near-relay builds remain the default.
This removes the near-page dependency in the tested backend; Hold-3 live startup
and the one-NPC cycle still need their own evidence.

Windows reservation alignment references: [VirtualAlloc](https://learn.microsoft.com/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc)
and [VirtualAlloc2](https://learn.microsoft.com/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).


## Observed invocation cuts across native restarts (Lifetime-1)

The retained job probe now keeps one bounded record per observed invocation,
with a monotonic serial captured on that native stack. Returns clear that exact
slot/serial, not a unit-address aggregate. A cut captured under the record mutex
counts only still-active serials at or before the cut; later invocations,
including those reusing the unit address or freed record slot, cannot complete
an older call. Overflow/capacity loss makes observation unhealthy. A zero cut
covers only observed entries, never pre-attachment jobs or unobserved producers.

The opt-in COOP_NPC_LIFETIME_DIAGNOSTICS (default OFF, action diagnostics required)
captures pointer-free cuts at the existing pre-clear notification. Its bounded
32-entry book survives ordinary world clears, uses process-unique tokens across
instances, retains old records and reports capacity exhaustion without eviction.
Subsequent SDK frames report changed old-work counts separately from the global
active job count. It does not dereference old actor/unit pointers, alter native
scheduling, prune birth records or implement ConfirmNpcSceneRetired.

The SDK4.2.1 Hook<void>::Call implementation holds its shared detour-list SRW lock
while invoking a plugin detour; it releases it before the normal native call.
Calling CallOriginal inside our ClearScene detour would extend that SDK lock
through native teardown and bypass subsequent detours. This candidate keeps
Continue and the original SDK dispatch unchanged; native-return and physical
retirement remain UNKNOWN. No new teardown hook or native control is enabled.
The later binding still needs a verified lifetime barrier, not merely zero
observed jobs or a completed scene load. Hold-3 remains separately preserved.


## Lifetime-2: destruction-entry identity invalidation

Offline inspection of installed3.290.1 links the SDK-known constructor's primary
vtable assignment (anchor RVA1481ae) to vtable1d680c8, scalar deleting wrapper
148900 and body destructor148940. The wrapper calls the body before its optional
size0x1440 allocation free. The body's entry restores the same class vtable.
NpcActorLifetimeSites verifies the complete constructor dataflow prefix, unique
anchor, section bounds/alignment, wrapper call/delete-size/restore pattern,
body prefix and same-vtable relationship. Mismatched, ambiguous, truncated or
out-of-section chains reject. The inspector maps executable bytes as DATA and
translates stored PE vtable pointers explicitly; it does not run game code.
The actor-manager clear routine at16a630 clears several containers and delegates;
that alone has not established the required worker/lifetime barrier.

The opt-in COOP_NPC_DESTRUCTION_DIAGNOSTICS (default OFF, requires lifetime and
identity diagnostics) attaches a separate pinned pass-through body observer.
A short pointer callback marks the current birth invalid BEFORE native body
execution. Find returns zero for that tombstone. A later observed construction
at the same address issues a new monotonic birth; unknown destruction entries
never invent a stamp. Tombstones do not reclaim capacity without an independently
verified prune barrier. The callback/book locks are released before native code;
Stop drains only pointer callbacks, detaches context and retains module/bridge/
trampoline through native returns. It does not wait on or claim native teardown.
Counters distinguish body entry/return, birth invalidation and unknown actors.

Both hook configurations pass35native CTest processes, including four new
retained destruction cases: invalidation before body and address reuse, client
destruction after Stop while original body remains pending, nested native bodies,
and bad-entry/unknown-actor refusals. Additional resolver and birth-book cases run
inside the native suite. The absolute backend admits the exact21-byte destructor
prefix (three whole relocation-free stores form its stolen15-byte span).
No SDK ClearScene wrapper or new native control is enabled. Body return precedes
optional allocation free; an entry stamp is not a lifetime pin against native
concurrent destruction. No ConfirmNpcSceneRetired, birth pruning or guest NPC
readiness binding is enabled from these observations.


## Lifetime-3: exact birth body-return receipts and pre-clear membership

Destruction entry now records a bounded receipt keyed by exact constructor
birth, with the pointer retained only as an opaque comparison value. The bridge
carries that birth across the native body, then admits a short completion
callback under the same callback mutex. Stop clears both callbacks before client
destruction; no client pointer or thread-owned lock spans native execution.
A body completing after Stop leaves its receipt pending rather than accessing a
freed client. Stats return types use sizeof-based linker tags after this change.

A typed actor cut captures live identities AND already-entered bodies that have
not returned. A reused address can therefore contain both an old pending body
and a new live birth in the same cut. Each return completes only its exact
birth/pointer receipt; it cannot complete the replacement. Opaque cuts bind a
unique book-instance ID and prune epoch, so another/reconstructed book or lost
history cannot satisfy an old cut. Pruning remains externally gated and unused
in production; its invocation invalidates old cuts even at the same birth
watermark. History exhaustion fails observation closed rather than recycling
pending/returned proof. No actor memory is read by these receipt operations.

The existing pre-clear diagnostic keeps an actor cut beside its job entry cut,
reporting captured/destruction-entered/body-returned per identity. The two cuts
are sampled separately and are NOT a producer closure or atomic scene fence.
Actors born after the cut are not included. Pre-attachment work and complete
native worker coverage remain unknown. Body return precedes optional free; the
reports do not implement ConfirmNpcSceneRetired or PrepareNpcScene, prune births,
restore a failed guest or enable shared NPC readiness.

Both native hook configurations pass37CTest processes. Added process cases
exercise a native body constructing a replacement before the old completion and
scene cuts containing overlapping generations. Birth-book fixtures additionally
cover already-pending bodies, wrong/duplicate returns, book address reuse,
pruned evidence, survivor cuts and bounded history refusal. Stop-during-native
now includes a registered completion callback and a destroyed client. SDK and
package checks/receipts belong to the separately versioned private Lifetime-3.


## Pool-1: two-NPC local control prototype

The existing native hold arbiter now has separate legacy and ticket-bound pool
modes. Fixed process-lifetime slots publish immutable scope/NPC/birth/lease
metadata through the actor pointer. Native calls observe atomic per-actor
interference, owned state and retirement. The exact-stack one-shot marker also
binds the current record, so recursively controlling another watched actor is
external for that actor. Every game call passes through. Our own marked call
can be suppressed if retirement/interference was observed since its precheck.
Ticket operations pin the current mutation binding; resume additionally requires
local-owner permission. Pool records never evict/rebind addresses, leases or
scopes. Native destructor ingress retires the exact matching birth watch.
This does not close all native hold writers or arbitrary cross-thread races.

NpcActionPhase::SetHeld reuses this ticket path, with the legacy mode preserved.
The private COOP_NPC_POOL_DIAGNOSTICS flag requires hold/destruction diagnostics
and defaults OFF. A solo F8 cycle selects two nearest fresh eligible non-target
NPCs, binds/watches both, confirms each owned native hold/job drainage, waits8s,
resumes each with a local-owner grant and confirms two new jobs. It does not
change the native scheduling span, skip the original loop, admit network actors
or implement whole-scene preparation. Partial failure stops rather than resuming
an uncertain hold; this fixed-scope prototype requires a new process to retry.
Control callbacks drain before destruction observation is stopped at shutdown.

Normal and absolute native suites each pass44processes: six new pool-arbiter
cases cover independent ownership/collision, nested other-actor script calls,
retired/reused/stale bindings, capacity, owner-grant resume and recursive same-
actor interference; a retained native fiber case validates ticket-bound cancel/
resume. Existing legacy/control/lifetime cases remain passing. SDK live batch
behavior is a separately versioned Pool-1 test; see its recipe and journal.
## Retiring native binding scopes

The development hold arbiter can now release a pool's metadata after the
mutation scope is revoked, every watched actor has a matching constructor
birth/destructor entry/body-return receipt, and controller calls have finished.
All metadata lookups use a short mutex. A controller call pins its selected
record with the existing busy flag until its final read, including across
native continuations. The mutex is released before native execution.

Scope release clears only the mod's records. A new scope can watch new
constructor births at the same actor addresses. Late receipts for a previous
birth cannot retire its replacement. Game/script hold calls continue through
the original function. The legacy single-actor mode keeps its existing behavior.

The native world forwards verified destructor returns to the arbiter and
retries pre-clear scope revocation on later frames when pinned calls prevent
the first attempt. The private [NPC rebind diagnostic](../diagnostics/npc-rebind-1/BUILD-CANDIDATE.md)
watches the entire active catalog from the original mod's `ListNpcs` on each
Freeform load. It validates fresh identity and action-boundary membership,
grants local interaction ownership, and issues no native hold/resume calls.
It also carries the return-callback sampling refinement from the preceding
solo test.

Native fixtures cover exact receipt requirements, stale receipts at reused
addresses, three reloads of 500 bindings, and a controller continuation
overlapping teardown. Multiplayer NPC startup still requires its native
scene preparation, baseline application, and simulation-ownership wiring.

The `npc-party-bind.1` development profile also enables
`COOP_NPC_PARTY_BINDING_DIAGNOSTICS` with the restart binding and party NPC
discovery diagnostics. Each PC admits its initial watched catalog from
the current room's agreed NPC IDs after the room enters Playing. Admission
still validates each local actor at the native action boundary. A change
in room, epoch, seat incarnation, or loss of an admitted ID revokes the
local mutation scope. Scene clear drops the agreement and the next
mission uses fresh reports and constructor stamps.

This profile retains local NPC simulation and interaction ownership.
The four-player roster regression covers paged agreement, immediate
invalidation at host restart, and a different agreed set in the next
epoch. Connecting host simulation requires confirming that remote
simulators are stopped and can apply the host's baseline.

The host-only startup handshake can now require a baseline receipt from
each guest before the host resumes that NPC. A guest sends its receipt
after the native adapter confirms both freeze and baseline application.
The host checks the room epoch, seat incarnation, initial lease, revision,
and exact baseline. The existing fixed catalog development adapter enables
this mode and uses a distinct room build key. The agreed mission roster
runtime can now prepare a catalog after agreement in Playing and use the
same receipt handshake. It keeps the prepared set for the mission epoch
while additional IDs appear and stops if a prepared ID leaves the
agreement. Restart requires fresh agreement and the adapter's retirement
receipt. The current agreed-roster DLL uses local simulation while the
native preparation adapter is extended to accept that catalog.
