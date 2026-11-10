# Next HITMAN work

## Current checkpoint

Canonical workspace: M:\ChatGPT Projects. Public multiplayer remains OPS 0.1.3 / preview.4. Preserve it, Foundation 4.1 and all immutable private candidates.

Private OPS 0.1.6 / native preview.4.action.2 is the latest tested solo observer. Its first run passed 14 native action audits: 10 before and 4 after restart, all 83 NPCs matched with locked actor lists, no rejected/untracked IDs, no conflicting flags and healthy observation. The user knocked out a guard and changed outfit before restarting. Constructor watermark 99 -> 198, the same SDK thread, and a final native-loop count of 8,340. This is read-only validation; no NPC hold/resume or shared world control has been tested.

Action.1 failed in hook creation despite valid live sites, entry bytes, successful module pinning and initialized MinHook. Its exact return code was not saved. Action.2 waits for loaded Freeform and records precise failure stages; it installed successfully in the tested run. Do not attribute the original observer refusal to disk space. The user's disk-space concern described the earlier game crash. Storage has been moved/freed separately.

The source and preserved ZIP/checksum for action.2 are on M:. The user test folder is R:\Hitman COOP test\HITMAN-Coop-Action-2 and includes exact public 0.1.3 rollback. No site release was published. Do not repeat the same identity/read-only test without a regression or new uncertainty.

## Immediate engineering step — solo remains sufficient

1. Reuse the existing NpcActionProbe, verified cancellation/cleanup/dispatch relationships, NpcActionPhase control primitive, birth tracking and mutation gate. Bind native job-entry/return observation with thread-aware installation and process-lifetime retention. No thread-owned lock may span the original yielding native job.
2. Account for jobs already running before probe installation. A late probe's zero counter is not proof of zero work. Consider a fresh mission generation after probe installation, with explicit constructor/entry coverage for admitted actors; do not assume old jobs retired because the SDK's pre-clear callback fired.
3. Require fresh identity/birth/scope/lease, scheduled native slot membership and the observed locked callback context before control. Preserve conservative checks for pending jobs, native hold flags and script-owned holds.
4. Prepare a separately versioned, limited one-NPC local hold/resume test with exact rollback. Confirm cancellation/completion and resume behavior, not just a successful Boolean return or empty task pointers. Single-player testing is enough for this step; do not require a friend yet.

## First shared-NPC test — two PCs

After local control/lifecycle bindings are verified, connect host-authority capture and guest preparation/apply through the existing party/core. Begin with a restricted Freeform test of host patrol movement, knockout and death replication. Verify guest-native behavior and interaction protection before admitting guest actions. Existing stand-ins are deliberately unseen by NPCs; host authority alone does not solve multi-player perception.

Then add guest actions, simultaneous interactions and restart/rejoin recovery. Keep two PCs as the routine development setup. Use three/four for roster, fan-out, seat reuse and disconnect/late-join checks after the same core is stable. No shared extraction/objective system is required merely to label and run the initial restricted NPC test, but do not call that full co-op gameplay.

## Product direction

Host-controlled persistent party, native mission selection/restart and automatic admission remain the target; manual Ready/Start is temporary. Prefer native menus and use launcher mission controls as a fallback. Shared objectives/extraction and the complete mission loop remain later bindings.

The optional pause rule is unanimous explicit pause by admitted mission players, resuming on any unpause while leaving other menus open. Focus loss, tutorials and map screens are not votes. Keep this separate from native NPC holds, and flag any large UI/engine detour before expanding it.

Finish compilation before launching the game. Use no screen capture. The already-built coop_npc_site_inspector --pid mode has now passed against the running game and is available for future read-only diagnosis.
