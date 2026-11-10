# Foundation 1 — first development build

Version: `0.13.94-foundation.1`. Protocol: **17**, deliberately incompatible with the original beta's 16. Both players must update together. Target: Windows Steam/Epic HITMAN 3.290.1 and ZHMModSDK 4.2.1 ABI 1.

## Changes

- A body knocked out by the partner is now eligible for a locally initiated drag/dump stream. Previously the blanket mirrored-knockout exclusion prevented this interaction being sent back.
- A body's first observed state being stripped now announces that state, instead of waiting for a later transition that will never happen.
- Receiving an already visually matching stripped body now still records the state and disables taking its outfit again. The pickup block is maintained during subsequent body scans. Taking the original outfit stays irreversible until the scene resets.
- Added a bounded, same-mission native-exit request using the actual exit repository ID and the player's position. It rejects old mission epochs and only attempts the matching enabled native exit when the player is within five metres and the target is dead. It is restricted to the current Freeform Training scene. It never teleports players or synthesizes ContractEnd.
- Native exit support is discovered from engine reflection, not guessed function addresses. Local exit-type metadata and candidate diagnostics are written to the log and `HitmanCoop.log.exit-types.txt`. If an enabled native exit cannot be identified, no automatic exit action is taken. The native binding is **not yet verified in a running game**; this is an experimental bridge and diagnostic capability, not a certified shared-extraction fix.
- Failed physical body moves now log ragdoll availability, body identity, offset and velocity. The engine's impulse-based relocation and existing fallback are otherwise retained. The drag-stream fix does not establish that every ragdoll movement failure is solved.
- Installer upgrades preserve the first pre-mod backups rather than backing up old co-op payloads as originals. Removal restores each original file once and avoids resurrecting an old co-op DLL. This also handles old installer manifests containing such backups.

## Validation

- Built the actual x64 DLL with Microsoft C++ 2022 (19.44), Windows SDK 10.0.19041 and the checksum-pinned developer package for SDK 4.2.1.
- **97 core tests passed**, including new first-observation clothing, replay/stale-scene extraction, proximity and invalid-request cases. Existing session tests include lossy links and real local UDP loopback.
- DLL export/import/ABI checks passed: SDK 4.2.1 ABI 1, all 53 SDK imports available, and no extra Visual C++ runtime dependency.
- Isolated actual old-beta -> changed new DLL -> removal lifecycle passed; no old co-op DLL, loader or added settings reappeared after removal.
- The host was updated after repairing the disconnected-Steam-library detection issue. Its installed DLL hash matches the development package. Guest installation and new real-player behavior remain pending.

## Install and focused field test

Close HITMAN on both PCs. Extract the new package into a new folder, then run UPDATE.cmd from that folder on each PC. Keep the earlier beta ZIP for a deliberate rollback. Do not combine protocol 16 and 17 clients.

1. Confirm the new version in CO-OP > About; connect in Freeform Training as before.
2. Player B knocks out one guard. Player A takes its disguise, and B checks that the same corpse cannot supply the original outfit again. Distinguish the corpse from any clothing bundle A leaves on the floor.
3. Player A drags the body B knocked out for about five metres, stops, then stores it. Repeat with players reversed. Report animation, actual body position, storage, and whether the observer can still interact with it.
4. Kill the target, confirm both objective lists, and stand together at the same exit. Have the guest activate the exit first. Check whether the other game produces its own mission-end screen. Repeat with the host first on another run.
5. Preserve both logs and the host's `HitmanCoop.log.exit-types.txt`. If extraction remains unsupported, these diagnostics identify the actual native pin/property binding we need rather than asking players to repeat a broad test.

## Direction after this build

First verify/fix the native exit binding and residual body physics using this focused run. Then finish dropped-item transfer, death/retry and repeated-restart checks. A lobby should initially expose only the exact Freeform Training contract, with matching-build checks, ready state and normal mission loading. Expand the allowlist after individual missions pass these same tests. Automatic entitlement discovery, synchronized mission-story scripts and persistent campaign progression remain later work.

## Source and tooling

Development source: `dev/HitmanCoop`. It is a copy of upstream beta commit `1a9572cac9358d96e5bde00137722ac7e6c0f9e3`, with the changes above. Upstream copies and original test installers are retained.

Microsoft Visual Studio Build Tools 2022 was installed at `C:\BuildTools2022` because the prior compiler could not build the C++23 plugin. Dependencies were downloaded using upstream's pinned SHA-256 checks. Screen capture was not used during development.
