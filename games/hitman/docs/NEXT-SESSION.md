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

Product target remains4players, host-controlled persistent lobby and native
mission/restart following, automatic admission rather than manual Ready/Start,
then shared objectives/extraction. Unanimous explicit pause is optional and
separate; focus loss/tutorials/map are not votes. Avoid a major pause detour.
