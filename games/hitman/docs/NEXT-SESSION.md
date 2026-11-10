# Next HITMAN work

## Current checkpoint

Canonical workspace: M:\ChatGPT Projects. Public multiplayer remains OPS 0.1.3 / preview.4. Preserve it, Foundation 4.1 and all immutable private candidates.

Earlier private OPS 0.1.6 / native preview.4.action.2 established the solo action observer. Its first run passed 14 native action audits: 10 before and 4 after restart, all 83 NPCs matched with locked actor lists, no rejected/untracked IDs, no conflicting flags and healthy observation. The user knocked out a guard and changed outfit before restarting. Constructor watermark 99 -> 198, the same SDK thread, and a final native-loop count of 8,340. This is read-only validation; no NPC hold/resume or shared world control has been tested.

Action.1 failed in hook creation despite valid live sites, entry bytes, successful module pinning and initialized MinHook. Its exact return code was not saved. Action.2 waits for loaded Freeform and records precise failure stages; it installed successfully in the tested run. Do not attribute the original observer refusal to disk space. The user's disk-space concern described the earlier game crash. Storage has been moved/freed separately.

The source and preserved ZIP/checksum for action.2 are on M:. The user test folder is R:\Hitman COOP test\HITMAN-Coop-Action-2 and includes exact public 0.1.3 rollback. No site release was published. Do not repeat the same identity/read-only test without a regression or new uncertainty.

## Immediate engineering step — solo remains sufficient

OPS 0.1.9 / preview.4.action.5 passed its first complete native job-observation test. Both hooks installed with consistent allocation reports. Eighteen paired action/job audits (7 + 7 + 4 across constructor marks 99, 198, 297) stayed healthy, with all 83 NPCs matched, actor lists locked, zero identity rejections/conflicts and zero native job pointers without an observed active entry. The first restart was sampled at entered=returned=97, outstanding=0. The second restart's first sample already had one job: entered=199, returned=198, outstanding=1. Do not invent an observed zero barrier there. Each new mission settled back to 83 outstanding jobs; no sampled cumulative outstanding-work growth. Last counters 289 entered, 206 returned, 83 outstanding, loop calls 10,940.

During knockout/disguise, one native held flag was observed, with matching unit/actor flags and 82 active jobs. This is game/script-owned behavior: our controller must never resume an NPC merely because it is held. Saved full log/result: research/checks/playtest-17-solo-action-5, SHA 78fa806992b80736c84793f5474e1ae39589ba1f7b87eca2a5af711d0d889590. User finished and closed the game. No repeat of this completed observer test is needed.

1. Reuse the verified cancellation/cleanup/dispatch relationships, NpcActionPhase control primitive, birth tracking and mutation gate for a limited one-NPC local hold/resume binding. The retained observation callback currently holds its callback mutex plus a session try-lock while reading. Do not invoke a yielding/cancelling native control operation under either lock, and do not stack the legacy custom action-loop patch on the existing retained observer patch. Establish one retained entry bridge with an explicit validated control phase and safely detached client lifetime before enabling control.
2. Bind each selected operation to fresh actor identity, constructor birth, exact table slot and scene/scope/lease. Preserve explicit unknown coverage for pre-attachment jobs and reused unit addresses. Observed job returns across restart are useful evidence but not an engine physical-retirement acknowledgement. SDK pre-clear remains insufficient; never acknowledge multiplayer NPC readiness from it.
3. Distinguish controller-owned holds from game/script-owned holds. Refuse an already-held actor. Confirm native job completion/cancellation and agreed native flags before acknowledging hold; resume only the same hold owned by the controller and the same still-valid actor generation. Scene clear, actor replacement, mismatched flags or untracked work must refuse safely.
4. Prepare a separately versioned one-NPC solo hold/resume candidate with the exact public 0.1.3 rollback only after the boundary/lifetime/refusal fixtures pass. Verify native job return and patrol resumption, not only a Boolean or empty pointers. Two/four PCs are unnecessary for this step.

The original intermittent MinHook allocation refusal is not proven fixed by one successful Action-5 run. Keep exact allocation tracing; confirm attachment before every new live control test. Fix the cosmetic AllocationPath classification: a successful creation using an existing near page/table currently says no-near-code-candidate because it allocated nothing. Only classify that as a failed search when actual status is MH_ERROR_MEMORY_ALLOC. No reinstall is needed solely to repeat that diagnostic label.

Action-4 is withdrawn: it mixed shared report layouts during concurrent edits/build. Its allocation telemetry is invalid. Action-5 clean build froze finalized headers before compilation and added sizeof-based LayoutTag linker guards. The matching-layout fixture links/runs; stale 80-byte layout is rejected. All 20 native processes, SDK ABI, package installer/rollback/updater staging and archive/payload/vendor integrity checks passed. Preserve all immutable artifacts and public OPS 0.1.3. No control or physical lifetime proof has been claimed.

## First shared-NPC test â€” two PCs

After local control/lifecycle bindings are verified, connect host-authority capture and guest preparation/apply through the existing party/core. Begin with a restricted Freeform test of host patrol movement, knockout and death replication. Verify guest-native behavior and interaction protection before admitting guest actions. Existing stand-ins are deliberately unseen by NPCs; host authority alone does not solve multi-player perception.

Then add guest actions, simultaneous interactions and restart/rejoin recovery. Keep two PCs as the routine development setup. Use three/four for roster, fan-out, seat reuse and disconnect/late-join checks after the same core is stable. No shared extraction/objective system is required merely to label and run the initial restricted NPC test, but do not call that full co-op gameplay.

## Product direction

Host-controlled persistent party, native mission selection/restart and automatic admission remain the target; manual Ready/Start is temporary. Prefer native menus and use launcher mission controls as a fallback. Shared objectives/extraction and the complete mission loop remain later bindings.

The optional pause rule is unanimous explicit pause by admitted mission players, resuming on any unpause while leaving other menus open. Focus loss, tutorials and map screens are not votes. Keep this separate from native NPC holds, and flag any large UI/engine detour before expanding it.

Finish compilation before launching the game. Use no screen capture. The already-built coop_npc_site_inspector --pid mode has now passed against the running game and is available for future read-only diagnosis.
