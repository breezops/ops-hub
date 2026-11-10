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
