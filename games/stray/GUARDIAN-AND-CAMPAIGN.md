# Guardian first, then campaign coverage

The original Unreal project/source is unavailable. Reflected APIs and shipped
Blueprint behavior can be inspected, but finding an actor is not proof it syncs.

## Guardian completion gate

1. Host starts Guardian dialogue; both reach the same native dialogue/line.
2. Joining player starts it; same outcome, without duplicate interactions.
3. Either player reveals typing text without skipping a line on the other PC.
4. Either player advances; both reach the same line exactly once.
5. Item prompts and their choices produce the same outcome.
6. Both exit dialogue, regain control and retain matching quest progression.
7. First-meeting cinematic, skip and post-scene state pass from both initiators.
8. Repeat after checkpoint reload and delayed peer input.

Use isolated UserDir profiles, chapter 4 loading, and the guarded
`harness guardian-stage` command to place a test cat near Guardian without
changing quest flags. This tests a dialogue fixture, not the natural first-meet
cutscene or a full campaign. Do not force dialog internals or edit quest flags
to manufacture a passing result. Setup may resume the initial test menu once;
ordinary sync never dismisses menus in a polling loop.

## Campaign audit

Inventory each loaded chapter's talkables, dialogue classes, cinematic actors,
checkpoint requirements and exceptional story scripts. Tag coverage as discovered,
entry tested, advance tested, completion tested and checkpoint tested. Native
shared hooks should cover common paths; exceptional quest scripts need specific
fixtures. Select representative scenes with different interaction styles instead
of asking testers to replay every connecting section. Retain an explicit list of
untested scenes; an inventory is not a claim of full campaign compatibility.

## Four-player direction

Current transport remains two-player. Shared scene sessions should eventually
use stable participant IDs, a host-ordered action stream, occurrence IDs and
readiness/completion acknowledgements from every connected player. Avoid baking
host/client pairs into the campaign coverage records. Four-player support must
include late/slow/disconnected participants and quest authority, not just four
visible cats. Do not advertise four players until transport and scene barriers
are implemented and tested.

## Evidence so far

The isolated guardian-proximity-audit recorded joining-player-triggered Guardian
dialogue running on both peers with native cat bindings and line index 1. The
runner ended before text advancement/completion verification. No native input,
first-meeting cinematic, shared quest outcome or full Guardian pass is claimed.
34 loaded Slums sequence actors were inventoried as discovered_not_verified in
CoopMod/reports/campaign-scene-inventory.json in the development workspace.

Repeat fixture guardian-dialogue-progression also reached dialogue entry, but
line-advance verification failed. Both later reported Dial_b12_ask_the_guardian
using backpack externalTalkable components with different generated actor names
on each PC. A stable local-component/dialogue identity and explicit conversation
transition handling are needed; raw generated object paths cannot identify this
interaction across peers. The test now rejects dialogue-class changes and refuses
to advance if both text-ready states are not confirmed.

The chapter sweep previously could issue up to ten setup resume commands. It is
now limited to one, after a minimum chapter setup delay. Gameplay pause recovery
remains disabled; startup focus/menu behavior still requires a native fix.

## Current repair candidate

The loaded Slums SDK identified actual authored entry points: Guardian uses
LogicTrigger and SeqPlayed, the alarm uses LogicTrigger/AlarmIsOn, and the broom
robot uses LogicTrigger/SeqDone. StartSequence is not the actual cutscene cue.
The candidate observes these markers, emits them in broom/alarm/Guardian order,
and replays only those explicitly allowed native scene graphs. Already-applied
markers do not restart a scene. Guardian's confirmed entry guard is mirrored
before invoking its graph to prevent the local proximity trigger re-entering it.

Backpack externalTalkable actor numbers differ between processes. A scoped alias
now resolves the local backpack only for the matching native dialogue. Shared
B12 help uses its native input handler. Item choices carry a dialogue-scoped item
ID and find the receiver's own inventory item; no inventory item is invented.

The first natural approach test proved the broom marker mismatch: source true,
receiver false. The source left Guardian's cinematic while the receiver retained
cinematic mode at the same end position. This is evidence for the missing cue /
entry guard repair, not proof the repair has passed. Manually unwinding one
extra cinematic entry cleared the receiver's cinematic flag but did not restore
normal cat locomotion, so this is not used as a production recovery strategy.

The guardian-ordered-chain fixture is a fresh natural approach test with the new
cue order and entry guard. No teleport or forced Guardian start is used. It must
still pass exit/control, dialogue, help, postcard choice and checkpoint checks
before publication. 61 automated tests pass, including differing actor-number
aliases and duplicate-entry/echo regression. Source-only candidate; no new
launcher release or production Stray update has been published.
