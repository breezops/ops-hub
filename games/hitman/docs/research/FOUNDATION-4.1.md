# Foundation 4.1 — launcher field-test corrections

Version 0.13.94-foundation.4.1; gameplay protocol 19; launcher schema 1. Current gameplay limit remains two players; product target is four.

## Changes

- Recognizes the exact Freeform Training scene path without case sensitivity. The actual game log uses assembly:/_PRO/Scenes/Missions/TheFacility/_Scene_Mission_Polarbear_Module_002_B.entity. Foundation 4 compared a lowercase substring against that path, incorrectly marking the loaded mission unsupported and disabling host Replay. Recognition and the replay execution guard now share one tested exact, case-insensitive allowlist function.
- The launcher shows separate incoming request, waiting for a reply, friend acknowledged, own reply sent and expired states. The original check expires after 60 seconds; the prior launcher switched labels after only 30 seconds and provided no acknowledgement display. The display now uses the actual 60-second window. It prevents repeatedly starting the same outgoing check while waiting, and resets readiness on disconnect or a changed mission epoch.
- Adds a focus-loss pause-menu guard while the local launcher is actively polling the game. It leaves native focus handling intact, blocks an unfocused request to open the pause menu, and permits focused Esc and other menus. Intentional shared pause remains enabled. This guard targets the observed Alt-Tab behavior but requires a live check: if the engine also creates a separate focus-loss pause outside TryOpenMenu, further binding will be needed. A log entry identifies when this guard blocks a menu request.
- Keeps the working outfit bags, notification silence, existing two-player gameplay and launcher controls.

## Install

Close both HITMAN and the old launcher on each PC. Extract the entire new ZIP into a new folder, open START-HERE.cmd and choose 1. Then open the new HITMAN-Coop-Launcher.exe (or choose 3). Both the DLL and launcher must be updated together. Do not install over a running game.

## Short check

1. Launch through the new launcher, host/join, then load Freeform Training on both PCs. The host's Replay button should become available after both games synchronize.
2. Alt-Tab to the launcher. Check that this does not open the pause menu or hold the other game. Separately, focused Esc should retain the deliberate shared-pause behavior.
3. Host clicks Ready check. Guest replies using I'm ready within 60 seconds. Host should see that the friend replied ready; guest should see that their ready reply was sent.
4. Host tries Replay / restart once after resuming any deliberate pause. Preserve both logs if a step fails; do not repeatedly re-send ready checks while reading the chat.

No repeat body/bag test is needed for this launcher patch. Four-player networking, automatic contract launch, shared extraction/return-to-lobby and body attachment remain unfinished.

## Evidence and validation scope

The Foundation 4 host log is preserved at checks/playtest-05/host-before-4.1.log. It confirms the uppercase mission path, the first ready request at 19:17:25 and a later incoming request at 19:19:16, after the first request's 60-second response window. This supports the observed reversed request direction without treating the queued footer as an acknowledgement.

Automated cases cover actual scene-path casing and rejecting other mission paths, distinct ready/expiry states and focus-pause policy. Native focus-loss behavior and live replay/ready UI still require the short field check. Detailed validation is recorded in checks/foundation-4.1-validation.txt.
