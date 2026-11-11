# Intended co-op session loop

Product requirements agreed with the user on 2026-11-09. These describe the
target behavior; the public movement preview does not implement this loop yet.

## Player flow

1. Host creates a persistent party; up to three guests join once. The launcher
   can launch HITMAN and display connection/loading status.
2. Host chooses a supported mission. Prefer observing the host's native game
   selection and launching that contract on guests through verified engine APIs.
   If native selection cannot be integrated reliably, the host selects it in
   the launcher, which requests the same native contract launch on every PC.
3. Guests automatically load and join the current mission. There is no player
   Ready button or separate Start Preview step. Once the local scene, avatar and
   required world baseline are valid, that guest is admitted automatically.
4. Host uses the game's Restart Mission option. The party stays connected,
   every guest restarts once, and synchronization resumes after loading.
5. At mission completion the host chooses replay or another supported mission.
   Reuse the native results/menu actions where verified; otherwise expose those
   choices in the launcher. Guests follow without reconnecting or re-readying.

Keep ordinary game menus and interactions wherever possible. A custom in-game
mission menu is not a prerequisite. Launcher Stop/Ready/Start controls are
development tools, not the intended session lifecycle.

## Session rules

- Only the host selects, restarts or advances the party mission. A guest leaving
  or navigating away does not restart the host. Confirm what guest-native restart
  should do; initially intercept it or treat it as a local rejoin, never broadcast
  it as a party restart.
- Joining a running mission means automatic load and baseline admission, not
  immediate simulation against an unprepared scene. Existing players keep playing
  where supported. If late join is not yet safe, show that limit and admit the
  guest automatically at the next supported transition; do not call that full
  mid-mission join support.
- Background checks replace manual readiness: matching builds, owned/installed
  content, exact contract/variant and supported configuration, native load,
  NPC preparation and canonical state installation. Show actionable failures.
  The launcher must not claim content ownership from a map name alone.
- Keep party identity separate from mission lifetime. A host transition creates
  a fresh mission generation; old packets, actors, receipts and delayed commands
  cannot affect the next mission. Restart forwarding must suppress echo loops
  and duplicate native restarts.
- Slow or disconnected guests do not require the host to click Ready again.
  Recover automatically when possible; show explicit retry/leave recovery when
  native teardown or state installation cannot be confirmed. Host loss ends or
  suspends the session explicitly; host migration is not promised.
- Host mission completion is authoritative for the shared session. Distinguish
  a genuine success/failure from RestartLevel's contract-failure event. Shared
  extraction, results and progression need their own verified implementation;
  guest vanilla menus/results alone are not completion proof.

## Preferred shared pause policy

Pause the shared mission only while every connected, admitted player currently
in that mission has explicitly opened their pause menu. For two players this
requires both; for four it requires all four. Resume when any one player closes
their pause menu. Other players can keep their own menus open while play resumes.
One player's open menu therefore leaves the shared mission running.

Keep each player's native menu under their own control. Do not force a pause
overlay onto a player because somebody else opened a menu. Focus loss, tutorial
popups, map/inventory screens and technical loading/recovery states are not
automatic party-pause votes.

The host coordinates pause from current mission/roster identities. Count guests
only after admission, remove departed players, and recompute on joins, departures
or reconnects. Old-epoch, old-incarnation and stale votes cannot pause a new
mission. The game-update/network path must keep running while shared simulation
is paused so an unpause can always reach the host and other PCs.

Keep this separate from NPC authority holds, preparation and retirement. A
player unpausing does not release guest NPC simulation or validate a pending
world baseline. This policy is a product requirement; it is not implemented by
the current movement preview or the experimental NPC action primitive.

### Fallback if the native Escape menu requires a simulation pause

Use the original two-player behavior as the reference. A native Escape
menu or blocking tutorial on any admitted PC pauses the party. The
player who opened the menu or received the tutorial keeps their native
screen. The other players see a waiting overlay and remain in their
current screen. Resume when all admitted players have dismissed the
menus or tutorials that caused the pause. Clear a departed player's
hold and recompute after a restart or reconnect. Keep networking and
menu input available while the simulation is paused.

Reuse the named game-loop pause request and the original waiting
overlay. Track the source of each hold so focus loss and non-blocking
menus do not become party-wide pauses. A tutorial can keep the party
paused while it is being read, even when nobody opened Escape.

## Reuse and implementation order

### Bounded native pause investigation

In the 0.1.35 two-PC rehearsal, players reported that Escape and tutorial
popups froze gameplay on the PC that opened them. The party runtime has
no shared pause vote. When the host's simulation stops, its NPCs can also
appear frozen to guests because there are no new simulated positions to
display. Test that separately from whether the guest's own player can
still move. Include a guest-owned guard in the check so the two
simulation owners can be observed independently.

In 0.1.36, the guest reported that NPCs appeared to keep moving while
the host's Escape menu was open, and the host saw their local background
freeze. Only the guest log captured menu transitions during play. It
recorded the native `UI_Menus` hold for both a splash hint (request 14)
and Escape (request 1), while the always-update callback kept running.
This confirms the hold is shared by multiple menu types and leaves the
host's simulation state during Escape unverified in this run.

Use the existing `GamePause` named-request binding for shared pause rather than
freezing individual NPCs. It already locates the native request list, adds a
mod-owned request and releases that request through the SDK. The plugin's
`eUpdateAlways` path is intended to keep networking alive while paused. These
are reusable mechanisms, not proof of the unanimous policy in-game.

The remaining native question is whether the local pause menu can stay open
with its simulation-pause request suppressed. Verify that separately before
building party pause coordination. `UI_Menus` covers other menus too; its
presence alone is not an explicit pause-menu vote. Likewise, `m_LastMenu` is
recorded before `TryOpenMenu` succeeds and can be stale, so it is not sufficient
as the authoritative vote. Identify accepted menu open/close state, preserve
menu input blocking, and keep unrelated loading/script pause requests intact.

The F9 rehearsal tests one Escape menu at a time. In a playing party,
open Escape while HITMAN is focused, then press F9 once with that menu
still visible. The build checks that the native open succeeded and
releases the existing `UI_Menus` hold once through the SDK. Watch for
menu closure, NPC motion, and whether menu input still works. Repeat
with the other PC opening its menu. The probe does not act on tutorial
popups or replace the game's other pause requests. A successful
release in the log verifies only the request was removed. The two-PC
observation verifies whether the menu and simulation can coexist.

If that narrow native binding is verified, route current, admitted-player votes
through the host and drive one dedicated party pause request. Validate one-menu
open (world runs), all menus open (world stops), any menu closed (world resumes),
then departure/reconnect and restart. Do not use the legacy partner-pause path,
whose rule pauses for any one partner, as the party coordinator.

If keeping menus open requires substantial UI replacement or broad engine
patches, use the fallback above. The native
NPC hold/resume work below remains necessary for shared world authority and
must not be presented as work required merely to pause the game.

The legacy backend already has native RestartLevel discovery, deferred restart
execution, restart-event observation and echo suppression. Reuse those helpers
with host-only party commands rather than creating one legacy Session per guest.
The party core already exposes SelectMission, BeginLoad, ReportLoaded,
ReportNativeCompletion and ReturnToLobby. These are coordination primitives,
not native mission-launch bindings. The preview currently restricts itself to
manually loaded Freeform Training and manual Ready/Start.

1. Finish native NPC preparation/apply/lifecycle bindings without coupling them
   to manual launcher readiness. The automatic coordinator must call the same
   verified preparation and baseline checks.
2. Add the persistent-party lifecycle and host-only native restart forwarding.
   Validate restart twice in succession with two PCs and stale/duplicate commands
   in core tests. Retain the preview workflow until the replacement is verified.
3. Bind native contract launch and local content validation, starting with
   Freeform Training. Test joining from the main menu, then automatic restart and
   joining an already running mission with a current world baseline.
4. Bind host completion/replay and one additional supported mission. Demonstrate
   a complete A -> completion -> replay -> B loop with two PCs before expanding
   the mission list. Use launcher mission selection as the fallback host control.
5. Check three/four-player late joins, staggered loads, restarts, departures and
   reconnects. Two PCs remain the normal incremental engine test setup.

Do not remove the native loading barrier merely to remove the Ready button.
Make readiness automatic, preserve the proven playable release, and label each
new native capability by what has actually been tested.
