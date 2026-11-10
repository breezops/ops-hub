# Foundation 3 — outfit bags, drag tracking, silent notifications

Version 0.13.94-foundation.3, protocol 19. Both players must update. Target: HITMAN 3.290.1 with ZHMModSDK 4.2.1 ABI 1.

## Changes

- Native outfit bags created when a player changes disguise are captured with their actual outfit GUID, charset, variation, modifier flag and placement. The partner creates a real native outfit bag with those values. This is intended to support its regular pickup prompt, not just its appearance.
- Bag IDs distinguish host and guest creations. Spawn/removal messages include mission epoch and scene, reject invalid creation arguments, and suppress duplicate spawns and resurrection after consumption. Native entity deletion is queued by raw identity for processing on the game thread, so the engine's deletion workers never wait for the session lock. Scene teardown forgets references; disconnect removes peer copies while preserving local bags for reconnection.
- Drag tracking adds the streamed target's measured velocity to position correction. This should reduce the trailing offset during movement. Speed and impulse limits remain bounded, and feed-forward stops on release or a stream timeout. Discontinuous/stale target samples do not become chase velocities. This remains a native-physics experiment until the next paired test.
- Drag and container-dump states have separate wire flags. Dump diagnostics record animation-pose freshness and root/pelvis offsets. On the first dump, native type metadata is written to HitmanCoop.log.interaction-types.txt for the container/attachment binding investigation.
- The Windows MessageBeep call is removed. Legacy settings cannot enable that sound. Co-op popups default off, and the installer sets sounds/show_notifications false on upgrades. The game's own normal audio remains controlled by the game.

## Remaining limitations

The locker animation is not fixed in this build. Previous paired logs show that the sender's actor root stays stationary while its native dump animation plays, then the final storage message arrives. Repeating root transforms cannot reproduce that animated path. This build distinguishes dumping and collects the missing native animation/container metadata; it does not guess a trajectory, hide the corpse early or claim synchronized container animation.

Outfit-bag creation, native pickup/deletion behavior, drag alignment and reconnect/restart cleanup require live validation. The lifecycle guards do not arbitrate simultaneous pickups before either player receives the other's message. Bags for an outfit absent from the local loaded outfit kits are logged and declined. Shared extraction remains guarded and unverified. The dedicated launcher is still planned, not implemented.

## Install

Close HITMAN. Extract the new ZIP into a new folder, open START-HERE.cmd and choose 1. This uses the existing tracked installer, preserves other preferences, disables co-op notifications/sounds, and verifies the installed payload. Choose 2 in that menu to remove co-op. Developer tools are not required on either player's PC.

## Next short test

1. Both confirm foundation.3 / protocol 19 and connect in fresh Freeform Training.
2. Guest knocks out a fresh guard and takes its disguise. Confirm the corpse remains stripped and that both players now see the discarded old outfit bag at the same location.
3. Host picks up that floor bag. Check that the outfit changes and the consumed bag disappears on both PCs. Distinguish the newly discarded outfit bag from the consumed one. Do not test simultaneous pickups yet.
4. Guest drags the body straight about five metres, turns, then releases it. Compare the moving body's alignment and final position. Host can then drag it back to check takeover.
5. Store the body once, expecting the known incomplete animation. This captures dump diagnostics. Save both HitmanCoop.log files and the dragging player's HitmanCoop.log.interaction-types.txt before relaunching. If a step fails, record who acted and preserve the logs instead of repeating pickups on the same body.

## Validation scope

105 automated tests pass, including strict bag-message round trips, bad native-argument rejection, duplicate/consumption ordering, drag sample discontinuities, bounded movement calculation and the existing real UDP/session tests. These tests establish protocol/state/control logic; they cannot verify game-native outfit bags or physics. Build, SDK ABI checks, installer lifecycle, package hashes and host installation results are recorded separately in checks/foundation-3-validation.txt.

Original Jordan Rodrigues / ZHMModSDK credits and licenses are retained. Corresponding modified source is included. This is an experimental community mod, not an official IO Interactive product.
