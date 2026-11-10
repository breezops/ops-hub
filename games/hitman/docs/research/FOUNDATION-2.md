# Foundation 2 — body streaming and outfit state

Version: 0.13.94-foundation.2. Protocol 18: both players must update. Target: HITMAN 3.290.1, ZHMModSDK 4.2.1 ABI 1.

## Changes and limits

- Downed NPCs send their transforms without requiring animation bones. Their packets carry an empty skeleton rather than a cached standing pose. Standing takedowns still require a valid skeleton.
- Explicit drag/dump state is included in the victim stream, so the receiver starts following a drag without waiting for 0.75 m of movement. A locally dragged body can be selected even when it was the remote interaction target. Existing native ragdoll impulse movement remains in use; physical following is not yet verified in game.
- Body clothing observation now considers the native nude bit as well as outfit availability. Matching availability alone no longer skips applying the naked appearance on receipt.
- The native NPC outfit hook observes the completed engine call. A native nude call can announce the exact actor even before its down-state event is processed. It does not infer a source corpse from the player's Disguise event, which also occurs for floor bundles.
- Logs identify selected bodies, missing standing poses, outgoing transforms every two seconds, received victim streams, native outfit calls, nearby actor states at Disguise events, and changes in drag/dump/nude/cloth flags. Diagnostic flag bits: drag=1, dump=2, nude=4, cloth=8.
- Shared extraction is unchanged and still declines to fire without a verified native binding. No further extraction test is needed for this build.

These are experimental code changes. Passing automated tests does not establish that the native clothing flags or ragdoll movement behave correctly in the live game.

## Installation

Close HITMAN. Extract the entire new ZIP into a new folder. Open START-HERE.cmd and choose 1 to install or update. Names and preferences are retained. Choose 2 in that same menu to remove co-op. Developer tools are not needed on either player's PC. Automatic mod updates stay disabled so the two players remain on the same build.

## Short field test

1. Both players confirm 0.13.94-foundation.2 / protocol 18 in the co-op menu. Load fresh Freeform Training and connect as before.
2. Guest knocks out a fresh guard and takes its disguise. Host checks that this exact corpse looks stripped and no longer offers its original outfit. A separate clothing bundle on the ground is a different object and can legitimately offer an outfit.
3. Guest drags that body about five metres, stops and releases it. Host checks actual corpse movement and its final position, then tries dragging the same body back. Store it only after checking those positions.
4. If practical, repeat the disguise check on a fresh guard with the host taking the outfit first. If the first pickup still fails, stop and preserve both logs rather than taking the same outfit a second time and obscuring the first transition.
5. Copy each player's Retail/mods/HitmanCoop.log before relaunching. Report whose action failed and whether the animation, corpse appearance, position or pickup prompt differed.

## Validation

The C++ build, 100 core/session/network tests (including transform-only dragging, release and reverse-direction takeover), SDK import/export validation, package hashes and installer upgrade/removal checks are recorded in checks/foundation-2-validation.txt. Live two-player verification remains pending.

Source is included in the package. Original Jordan Rodrigues and ZHMModSDK credits and licenses are retained. This is an experimental community mod, not an official IO Interactive product.
