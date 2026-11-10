# First two-PC co-op test

Status: not run in the game. Mission: ICA Facility / Freeform Training. Candidate build: upstream beta 0.13.94, subject to installed game compatibility.

Record once per session: date, PC A/B game version, loader/mod version, protocol, file hashes, difficulty, mission/contract identity if available, loadouts, starting location, Steam online/offline status, IOI connection status, host, connection method, round-trip latency, and graphics/simulation settings. Keep both PCs' logs with matching run IDs.

| Step | Action | Required observation |
| --- | --- | --- |
| 1 | Launch unmodified on both PCs, open Freeform Training, then repeat with Steam Offline Mode | Both can independently start the exact mission. Keep networking connected. |
| 2 | After selecting a matching build, host/join using direct IP and UDP 47047 | Session connects; no mismatched build or mission; both logs identify the session. |
| 3 | Stand together; walk, crouch, aim and climb separately | Partner visible; usable movement/animations; no duplicate local player controls. |
| 4 | While side by side, A then B openly performs an illegal action in front of the same guard; repeat with B closer, then A closer | Guard notices either actual player appropriately. Record immunity, frozen AI, wrong victim or disappearing alerts. This is an early stop/go gate. |
| 5 | Repeat detection with players separated and swap host/client | Detection does not depend on being host or accidental ownership proximity. |
| 6 | A knocks out a guard; B performs another knockout. Hide one body and leave another visible | Consciousness, discoverability and body location agree; AI finds the visible body, not the hidden body. |
| 7 | Drag a body across a doorway; drop it; take its disguise | Other PC sees usable body placement and the correct outfit change, without a crash. |
| 8 | Pick up/drop/throw the same scene item; open/close a door | No duplicate pickup, persistent invisible item, or incompatible door state. |
| 9 | Have the guest kill the target; repeat on a fresh run with host killing it | Both objective lists update. Record discrepancies in kill method/scoring separately. |
| 10 | Test any required non-kill objective available in a later candidate mission | Both objective states update; avoid claiming generic objective sync from kill sync alone. |
| 11 | Approach extraction together, then attempt staggered exits | Document existing behavior. Proposed finished behavior is team extraction only after both are ready. |
| 12 | Die as guest, then as host, in separate runs | Document existing failure behavior; desired team retry must not leave a frozen or immortal partner. |
| 13 | Restart three times, return to menu, reload, and swap host | No teardown crash, stale NPCs, duplicate avatars or permanently held pause. |
| 14 | Briefly interrupt the connection during a disposable run | Clear disconnect state; no continuing invalid shared victory. Reconnect behavior documented. |
| 15 | Finish three clean full runs, including one with roles swapped | Same outcomes on both PCs; retain both logs and recordings of any discrepancies. |

Core and relay tests are supporting evidence only. A test bot cannot certify two real players' AI awareness, damage, objective state, extraction or internet behavior.

Classify each issue as: crash; gameplay blocker; recoverable desync; cosmetic. Address crashes and gameplay blockers before expanding the mission list. Never mark an untested mission compatible merely because the mod accepts its scene.
