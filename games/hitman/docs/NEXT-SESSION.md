# Next HITMAN session

## Saved state

- Canonical workspace: M:\ChatGPT Projects. The old C: root is a compatibility junction. All 152,933 original files passed SHA-256 verification; the approved C: duplicate was removed.
- Public tested multiplayer build: OPS 0.1.3 / native preview.4. Keep it and Foundation 4.1 preserved.
- Installed private diagnostic: OPS 0.1.5 / native preview.4.action.1. The Action-1 folder includes an exact 0.1.3 rollback. No new release or site publication is needed for resuming diagnosis.
- Movement, crouch/turning, vaulting, disguise visibility, and stand-in interaction protection have multiplayer playtest coverage. NPCs, items, objectives and extraction are still separate on the public preview.
- The solo identity test passed 83 NPCs before and after restart. The subsequent action observer reported unavailable and produced no action audits. Do not call the native action boundary or hold/resume validated.
- The read-only site inspector builds and resolves all required sites against the installed executable as data. Its --pid mode is compiled but still needs live verification. It never patches game memory or captures the screen.
- The last live attempt ended before memory inspection. The crash stack was in NVIDIA NGX/DLSS-related code, and a concurrent build failed with a paging-file/commit error. Storage has now been freed; the exact observer installation failure remains unknown.

## First: short solo diagnosis

1. Confirm C: still has room. Finish all compilation before launching HITMAN; do not compile during a live test.
2. Reopen the existing Action-1 launcher, launch HITMAN, and load Freeform Training alone. Do not host, join, ready or start a preview. No reinstall is needed unless the installed build changed.
3. Read the startup log immediately. If the observer is unavailable, investigate before asking for another complete knockout/restart sequence. The brief startup check is enough to identify that failure.
4. If needed, use the already-built tools/native-tests target coop_npc_site_inspector with --pid and the current HITMAN3 process ID. Compare runtime resolver results with the passing offline results. Only read the game; no screen capture or process-memory writes.
5. If the resolver passes but installation still refuses, add exact observer-stage/MinHook error reporting. Version any new candidate separately; never overwrite delivered action.1 or preserved packages. Do not weaken signature/identity checks just to enable the observer.

## Then: solo boundary and control checks

- Once the observer installs, check actual scheduled IDs, locked actor lists, fresh births/identities, pointer relationships and healthy observer state in Freeform. Zero rejected identities/conflicting records is the initial goal; do not assume all enabled NPCs are always scheduled.
- Repeat the native mission restart and confirm fresh identities, valid callbacks and clean teardown behavior. Apparent drained pointers alone are not worker completion or retirement proof.
- After those observations pass, complete the native control/lifetime bindings needed for a deliberately limited one-NPC hold/resume test. Confirm both native flags and completion of pending actions before acknowledging success. This is independent of the optional shared pause feature.
- Single-player testing is useful for all of these native steps. No friend is required merely to validate actor identity, native callback context, or a controlled local transition.

## First shared-NPC test: two PCs

Reuse the existing party/core and native helpers. Start with a clearly limited Freeform host-authority test: host patrol movement, knockout and death should replicate on the observing guest. Confirm guest-native behavior and interaction protection before allowing guest NPC actions. Keep unsupported guest actions out of that first test; do not describe it as full shared gameplay.

After that baseline works, add guest actions and simultaneous interactions, then native restart/rejoin recovery. Two PCs remain the normal development setup. Use three/four players for roster, seat reuse, late join, disconnect and fan-out checks once the same core is stable.

## Product direction to preserve

The host should eventually control a persistent party and native mission changes/restarts, with automatic admission and no permanent manual Ready/Start workflow. Prefer native game menus and use launcher mission controls as a fallback. Shared NPC work remains the immediate priority; shared objectives/extraction and the complete mission loop are later bindings.

The optional pause rule remains: pause simulation only while all admitted mission players explicitly pause; resume when any player unpauses, keep others' own menus open, and do not count focus loss/tutorial/map screens. Investigate native menu pause requests as a bounded task, separate from NPC authority holds.

## Shutdown checkpoint

HITMAN, the Action-1 launcher, native build/test tools and migration jobs were checked stopped before shutdown. No work is scheduled to run overnight. Pending live diagnosis resumes only when the user is back.
