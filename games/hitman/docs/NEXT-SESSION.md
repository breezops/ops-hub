# Next HITMAN work

## Workspace and preserved releases

Use M:\ChatGPT Projects. Repo ops-hub, branch codex/hitman-npc-authority,
draft PR7. Public OPS0.1.3/nativepreview.4 remains unchanged. Preserve
Foundation4.1, Hold-3 and every immutable diagnostic. Private packages are not
published to the site. Stray stays separate. No screen capture; keep SDK and
mod notifications/sounds OFF. Complete builds before asking user to launch.

## Validated native foundations

Hold-3 passed one local native hold/drain/eight-second hold/resume/new-job cycle.
Lifetime-1 passed observed old-job cuts over two native restarts. Lifetime-2
passed destruction-entry invalidation for99actors per restart. Lifetime-3
passed exact per-birth body returns: both cuts captured99identities and matched
99entries/99body returns, zero completion rejection/unknown actors, old observed
jobs0. Twelve paired healthy audits retained83NPCs across constructor marks
99->198->297. Final273entries/190returns/83jobs, loop6425. Full evidence in
research/checks/playtest-23-solo-lifetime-3 and OPSHUB-LAUNCHER.md. Game closed.
Do not repeat those unchanged restart-observation tests.

Pointer-only cuts include live identities and already-pending bodies, bind
unique book instance/prune epoch, and refuse lost/pruned evidence. Old returns
cannot complete a replacement at the same address. These are observations of
captured actors and native jobs, not all-producer closure, optional free or an
engine worker barrier. ConfirmNpcSceneRetired, pruning and guest readiness
remain unbound. SDK ClearScene dispatch remains Continue; do not wrap its
CallOriginal inside a plugin callback while the SDK detour-list lock is held.

## Current private candidate: Pool-1

OPS0.1.16/native preview.4.pool.1, diagnostics/pool-1. Check research journal for
completed SDK/package/delivery receipts before prompting. Uses existing retained
action bridge, NpcActionPhase::Boundary, mutation gate, identity/birth/slot/job
validation and actor-control entry. Adds per-actor fixed-scope ticket watches
(up to500immutable slots) to the existing hold arbiter. Legacy single-actor
mode is retained/tested and cannot mix with pool mode. Interference, owned hold,
retirement and exact scope/NPC/birth/lease are tracked separately for each actor.
Game calls pass through; only our marked call may be refused. Ticket resume
requires a current local-owner grant. No mutex spans yielding native code.
Destruction invalidates the matching watch. No address/lease/scope rebinding in
this prototype. It does not serialize every external native call or establish
all hold sources/cross-thread ordering.

The solo prototype selects TWO nearest eligible conscious non-target NPCs after
a fresh native restart. It binds/watches both before native control, validates
both identities each boundary, confirms owned native flags plus job returns and
cleared pointers for both, waits8seconds, grants local resume permission, resumes
both and confirms their new observed jobs. One cycle per process. F8 only.
Pre-existing holds/downed/target/untracked/stale actors excluded. Partial failure
stops further native control and requires quitting; never automatically resume
a game-owned hold. No whole-scene preparation or network NPC binding enabled.
Shutdown drains control before detaching actor destruction observation.

Both normal/absolute native configurations pass44CTest processes, including
independent owners/script collisions, recursive other-actor interference,
retired/address reuse/stale lease/scope/capacity refusal, owner-grant resume and
ticket-bound retained fiber cancellation/resume. The live result below validates the two-actor cycle in Freeform Training. Shared sources/headers finalized
before SDK build; no mixed-layout objects.

## Pool-1 live result

All four hooks attached. After one mission restart, F8 selected Terry Norfolk
and Joseph Harding. Both original jobs returned and both owned holds were
confirmed at 14:40:38.824. Resume was confirmed at 14:40:46.833, and both new
jobs were observed at 14:40:46.863. The final audit recorded 83 active jobs,
zero held NPCs and healthy identities. The game is closed. Evidence is saved
at research/checks/playtest-24-solo-pool-1/Host.log.

## Direction after local batch validation

The private runtime now supports a fixed catalog of stable NPC entity IDs. In
this mode every peer must confirm that those exact actors are held and drained
before the room starts. Only the host runs their simulation. Other NPCs keep
their local behavior. Stopping during loading cancels preparation even before
any commit arrives. Reuse requires confirmed native retirement. Private packages
using this mode need a distinct room build ID and identical catalogs on all PCs.
All 207 core tests passed, including two- and four-peer catalog admission,
host-only updates, loading cancellation, missing actors and out-of-catalog
commits. The Pool-enabled SDK plugin compiled with these changes.

The Pool-1 selection path now exercises the exact-ID binding helper. It resolves
and checks all requested actors at the retained action boundary before opening
the mutation scope and registering watches. Its selected-actor log now records
entity IDs alongside labels and births. The existing live Pool-1 log predates
this change and does not contain the two entity IDs.

Catalog-1 connects native preparation, acknowledged freeze, baseline apply and
resume through the retained action boundary. In Freeform Training each PC
resolves the two named actors after a fresh native restart. The existing room
handshake checks the private build and exact stable IDs. The host resumes its
observed jobs. The guest verifies held, drained actors and transform readbacks.
Pending commits advance through freeze, apply and resume receipts without
restarting earlier steps. Catalog-1 tests living position and facing with two
players. See `diagnostics/catalog-1/START-HERE.txt`. Whole-scene admission,
retirement recovery, canonical life/interaction and multiplayer NPC perception
remain follow-up work. Existing stand-ins are deliberately unseen by NPCs.
Two PCs are routine; use3/4for roster/fan-out/seat reuse/disconnect checks.

Catalog-1 is staged for the first live test at
`R:\Hitman COOP test\HITMAN-Coop-Catalog-1`. OPS 0.1.17 passes 209 core tests,
the SDK ABI check, synthetic installation and exact OPS 0.1.3 rollback, and
archive and delivered payload verification. ZIP SHA-256
`ec1f74151fd35826f8fb84adadb0dd717834829e9e47d53d6ccfacd3bb16a55d`.
OPS HUB now serves 0.1.17 through the launcher update channel.

## Catalog-1 live result

Both PCs matched Terry Norfolk (`c174e1bfd2f4a861`) and Joseph Harding
(`c827956846e4ffa1`) and confirmed both initial holds and the same starting
positions. The host resumed both native jobs. Later audits still showed both
actors held and drained on the guest while the host ran their simulation.
The guest saw Terry turn toward the host. Terry ended his phone idle on the
host, while the guest still saw the phone pose and prop. Longer walking paths
and Joseph's movement were not exercised. The logs record the stand-in's
perceptibility guard on both PCs. The guest could not elicit the host's reaction.
Both logs are preserved locally in `research/checks/playtest-25-two-pc-catalog-1`.

### Reuse from the original two-player tests

The original test ran upstream 0.13.94 beta, protocol 16. Foundation 1–3
extended that same two-player session. The beta host log records live NPC pose
traffic, ownership changes in both directions and replicated alert floors. The
existing `WatchNpcPoses`, `GetNpcPose`, `ShowNpcPose` and `ClearNpcPose` paths use
the same `PoseHook` for animated NPCs. The first Catalog follow-up can reuse that
capture and playback with the packed `MsgNpcPose` codec and the existing pose
buffer. Start with full poses for the two catalog actors. A full 256-bone frame
uses 1,055 bytes before the party envelope, within the current payload budget.
Bind reads and writes to each catalog ticket's actor birth and current animator.
Accept the current owner's frames within the current room epoch and clear the
animator override on scope stop, actor retirement or a stale stream. Verify
guest body and hand motion during Terry's phone idle transition. Check phone
visibility independently. The legacy held-item path scans Agent 47's inventory
and supplies a repository item to the partner avatar. Terry's scripted prop
needs a native binding if it persists after pose playback.

The beta let either local Agent 47 affect NPCs. The host assigned a calm NPC to
the nearer player, used contact and chase exceptions, and kept alert NPCs with
their current simulator. The avatar stand-ins had the same unseen guard in the
first beta test. The existing alert-floor path capped mirrored tension, allowing
one PC to show suspicion while the other was caught. Use this as the precedent
for the party's nearest-player policy. The party already models acknowledged
release, prepare and commit. Catalog-1's native watches are fixed to one lease,
and the live runtime still needs validated player-position input and native
handoff support. A safe handoff should be verified with the guest's real local
47 before tackling simultaneous perception of multiple players.

Foundation 1 added corpse-state and drag fixes. Foundation 2's native nude-bit
scan verified that the first outfit pickup disables taking the corpse's original
outfit again. Its transform-only drag stream made the body follow, with an offset
and a jump at the end of the locker animation. Foundation 3's paired logs verify
native outfit-bag creation and pickup in both directions. Its drag tuning still
caught on doorways. Preserve the native body, clothing and bag routines as
building blocks and route later interactions through the party's scoped NPC
authority. The first beta run confirmed host restart following on both PCs and
recorded that extraction completed only for the host.

## Catalog-2 pose test

Catalog-2 reuses the legacy packed `MsgNpcPose` codec, pose buffer and animator
hook for Terry and Joseph. The host sends independent full frames at up to
12 Hz per actor. A guest accepts only host frames for its installed living
catalog actors in the current epoch. The adapter binds exact actor births and
animators, requires matching bone counts, and clears stale overrides and
bindings before a matching actor's destructor body runs. The separate build
identity prevents mixed Catalog-1 and Catalog-2 rooms. Guest-triggered NPC
attention is outside this host-authority test. The first live pass should
compare Terry's turn, arm and hand motion, and the phone prop itself.

### Catalog-2 live result

The host captured 231-bone poses for both actors. The guest animator reported
matching 231-bone native and streamed poses for both. The guest saw Terry drop
his phone and turn in place to track the host. The turn looked choppy on the
guest. During the later interaction window the guest game reported 77–92 FPS.
Catalog-2 caps skeletal snapshots at 12 Hz and applies canonical root transforms
from the separate 10 Hz report path. The logs confirm matching rigs and initial
capture, with no per-actor packet cadence or loss measurements yet. Keep both
logs under `research/checks/playtest-26-two-pc-catalog-2`.

Measure per-actor pose and transform delivery cadence and smooth the guest root
and skeletal playback together. Retest Terry's standing turn first. For a
walking check, use an accessible naturally walking NPC in a future fixed
catalog. The pair could reach Terry beside the car, but he stayed in place and
the mechanic blocked their route to the walkers. Avoid requiring a knockout
for the movement test. The phone transition passed as an observed visual result.

Catalog-3 pairs each position, facing and skeletal sample in one unreliable
presentation frame and interpolates it at the guest. The host samples up to
24 Hz per catalog actor. The native guest preserves the canonical transform as
a fallback and applies the interpolated transform at the action boundary. The
walking slot uses Ronnie Aguilar, Ritter's bodyguard. A marker follows that
catalog actor, so testers can use Instinct from the dock while leaving both
catalog NPCs alive and dressed. Live resolution, sight range, and movement
quality await a two-PC test.

Catalog-3's first host attempt loaded and restarted Freeform correctly. The
native constructor mark advanced from 99 to 198 and the action observer remained
healthy. Room creation failed during catalog lookup. The original beta guest
log records the native spelling `Ronnie  Aguilar` with two spaces. Catalog-3.1
accepts that exact native name and the display spelling in both the room lookup
and walking marker. The launcher now reports the failed check directly.

### Catalog-3.1 live result

Both PCs matched Ronnie Aguilar (`07ac2b57aa2deb71`) and Terry Norfolk
(`c174e1bfd2f4a861`), received identical initial positions, and confirmed
matching 231-bone guest rigs. The host resumed both native jobs. The last
sampled native audits still showed two held and drained actors on the guest
and zero held on the host. The pair saw Ronnie walk at nearly the same
position on both screens and reported a much smoother Terry turn on the guest.
The logs confirm setup and playback, while the walking agreement comes from
the players' observation. Both logs are preserved locally in
`research/checks/playtest-28-two-pc-catalog-3.1`.

Kalvin Ritter was in different places while Ronnie followed him. Catalog-3.1
synchronizes Ronnie and Terry, and Ritter still runs locally on each PC. Use
that observation to validate discovery of related actors, including living
mission targets, when generic admission is ready.

## Mission-wide NPC discovery and attention

Keep Catalog-3.1 as the live reference for root and skeletal playback. General
support should discover actors from the mission's native actor list. Reuse the
original beta's template entity IDs, scoped to the current mission and room
epoch, with the newer per-PC actor birth and table-slot checks. A name or
position can label a diagnostic, while matching uses an unambiguous ID and
an exact native binding. Compare eligible IDs across PCs before admitting an
actor. Treat ambiguous or missing IDs explicitly. Recheck pooled actors when
they activate and invalidate a binding before retiring its native instance.
Test that lifecycle in Freeform and then on a different map or mode using the
same discovery logic. Target damage and objective completion need their own
authoritative transitions before living target support can cover those actions.

For guest attention, reuse the beta's proximity ownership behavior. The host
coordinates one owner per NPC and uses the current owner's local Agent 47 for
native perception. The party runtime now feeds validated avatar positions to
the existing nearest-player policy. The native Catalog build still uses host
ownership. Next native work is safe lease transfer and a verified activity
check so a guard already investigating, fighting or performing an interaction
can keep its simulation. Same-guard perception of multiple nearby players
remains a separate check. Use the paired original beta and Catalog logs for
behavior comparisons and scale detailed pose streaming by player proximity.
For escorts and shared scripted behaviors, verify that dependent actors remain
under coherent authority during the behavior. Ronnie and Ritter are the first
acceptance example for that rule, with identities resolved by the generic path.

The first NPC discovery build adds room-scoped scans to Loop-6. Compare
the complete ID sets on two PCs before and after a restart. Use that
evidence to select which native actor bindings can be admitted to shared
simulation. Keep the scan independent of NPC names.

### Gameplay parity and upstream reuse

Use Jordan Rodrigues's original 0.13.94 beta and the paired Foundation tests as
the gameplay reference. Preserve the native actor, inventory, item, door,
disguise, body and event paths that those builds exercised. Route changes through
the party's mission epoch and host validation. Track the initiating player and
the exact native entity, and apply each accepted world change once. A scene
restart, stale actor birth or changed owner must invalidate old actions.

- **Attention and disguises.** Reuse proximity ownership, native alert reads,
  capped alert mirroring, corpse clothing state and the native outfit-bag path.
  Test each player drawing attention, then subduing a guard, taking the original
  disguise once and using the discarded outfit bag across PCs.
- **Items and environment.** Reuse the beta's stable item identity, pickup,
  placement and door paths. Test both directions for an ordinary pickup, dropped
  tool and door. Then test a native tool-to-object interaction and an assassination
  with a scripted or accident result. Confirm that the changed object and NPC
  state agree across PCs. The generic item pickup path does not establish
  replication for every scripted interaction.
- **Kills and challenges.** Preserve the native `Kill`, `Pacify` and
  `ChallengeCompleted` observations and the original mod's mirror-event
  suppression. The original beta logs show real local game challenge events.
  The `Coop_Challenge` message carries a display title to the partner's
  notification, while HITMAN evaluates actual challenge conditions locally.
  Verify the proper target, tool, outcome and actor credit through native game
  events. Check party notification, objective state and profile unlock on each
  PC as distinct outcomes. Keep method and source attribution through the
  accepted action so a mirrored death does not manufacture duplicate credit.
- **Mission loop.** Reuse the native restart and `ContractEnd` observations.
  Require both games to agree on completed objectives and the shared exit
  before marking the party complete. Verify restart and a second contract from
  the same lobby after the native actor lifecycle supports it.

Repeat the same checks on another supported map after Freeform. Record the
native behavior that needs a mode-specific adapter when one of those checks
fails. Keep the original tested packages as references for regressions.

Product target remains4players, host-controlled persistent lobby and native
mission/restart following, automatic admission rather than manual Ready/Start,
then shared objectives/extraction. Unanimous explicit pause is optional and
separate; focus loss/tutorials/map are not votes. Avoid a major pause detour.

## Core loop first

1. **Keep the party through a native restart.** The host's in-game restart
   advances the room's mission epoch and keeps the session and seats. Each
   guest requests the original mod's deferred native restart once. The room
   waits for the scene clear and the returning local player on every PC before
   resuming. A timeout or interruption cancels queued work. `Loop-1` exercises
   this path in Freeform Training using the movement adapter. Its build flag is
   `COOP_PARTY_NATIVE_RESTART`, with native version `0.13.94-four-player.preview.4.loop.1`.
   The catalog NPC adapter still needs confirmed native retirement before it
   can safely follow this lifecycle. Loop-1 passed two consecutive native
   restarts in the same room on two PCs. Both logs recorded scene clear and
   matching mission epochs. A live check with four seats remains.
2. **Launch or change a mission as host.** Add a host mission choice backed by
   verified native contract identity and local availability. Carry exact
   contract and mode through the same load barrier. Guests follow the host's
   native load, and loaded receipts replace manual readiness. Validate a
   restart and a switch to a second supported mission in the same room.
   The local Loop-2 diagnostic starts this barrier when the host enters
   Freeform from a main-menu room. Guests still select Freeform manually.
   The solo trace records the contract ID and the caller of the normal
   native scene load so the guest launcher can be implemented against the
   actual contract startup path.
   The solo run confirmed the Freeform contract ID and tutorial mode.
   Its scene trace starts downstream of the suspected native contract
   entry. Verify the contract caller and its callback lifetime before
   invoking it from a guest. See the Loop-2 diagnostic build notes.
   Loop-3 confirmed that the native Freeform menu action uses
   `ZPlanningPageControllerEntity` and supplies two callbacks. Resolve how
   the main menu creates and populates that controller before invoking the
   action from a guest. See the Loop-3 diagnostic build notes.
   Loop-4 tests a second native entry through the contract-start entity used
   by the shipped tutorial. Verify it from the main menu in a solo run before
   binding it to a guest's room load request.
   Loop-4 dispatched the entity input and showed a loading indicator, but
   stayed in the main menu. Native scene and contract fields were still
   empty. Compare with the shipped prologue menu's ID-based
   `start-contract` action before wiring guest launch.
   The entity's public input waits for a contract-end event before it
   calls the launch routine. Loop-5 tests that same native continuation
   in a fresh main menu with an empty contract context.
   Loop-5 loaded the exact Freeform contract and reached playable state in
   about three seconds. Loop-6 uses that path for guests and checks the
   exact native contract and local avatar at the room's load barrier.
   Loop-6 passed live two-PC and three-PC main-menu launches and
   same-room restarts. The successful three-PC retry confirmed the exact
   contract on initial load and restart in all three logs.
3. **Complete and replay.** Bind shared objectives and native exit events.
   Collect actual local `ContractEnd` receipts before presenting party success.
   Return the party to mission choice with the same seats and allow replay.
4. **Integrate gameplay in groups.** Bring in general NPC discovery and safe
   authority handoffs, then takedowns, disguises and bodies, then items, doors
   and scripted interactions. Use the two-PC baseline routinely and four PCs
   for concurrent actions, ownership transitions and room lifecycle checks.
