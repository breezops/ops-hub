# Four-player movement preview 1

Build: 0.13.94-four-player.preview.1. Party admission/lifecycle protocol: 2. Required game: HITMAN 3.290.1, SDK 4.2.1 ABI 1. This is a separate movement/disguise preview backend; it does not replace the interaction functionality of the verified two-player Foundation 4.1 backend.

## Runtime now connected

The native game frame loop owns PartyPreviewRuntime while preview mode is active. It ticks PartyRoom, validates actual loaded Freeform Training/local pawn availability, acknowledges the loaded barrier, captures local transforms and bone poses at up to 30 Hz, refreshes reliable outfit/skeleton metadata, processes scoped typed streams through PartyAvatarPlayback, and updates up to three native remote avatars. The legacy Session backend is excluded while a preview runtime exists. Launcher's local current-user named pipe exposes bounded allowlisted preview commands and a real four-member roster/status response.

Native scene clear/loss sends a reliable SceneLost lifecycle control rather than inventing mission completion. The host publishes Interrupted; Stop preview returns to the lobby and selects a new epoch, resetting readiness. Ready requires the local Freeform scene to be available. Reconnect uses the existing room resume mechanism, with a fresh incarnation. It does not resume an interrupted shared gameplay mission automatically.

The dedicated WPF preview launcher has Host, Join, Ready, Start, Stop, Reconnect, roster/status, and Launch HITMAN controls. It refuses a different mod build and requires no SDK menu for normal use. Its visual layout was rendered from its own WPF tree into a PNG and inspected; no desktop capture was used.

## Scope and limits

The preview sends only avatar transforms, poses, outfits and skeleton metadata. Four-player NPC authority, bodies/knockouts, held/dropped items, targets/objectives, team death and shared native extraction are not implemented in this backend. Each PC still runs its own native mission simulation. Native actors and pose/AI guards were compiled, but this build has not yet been playtested inside HITMAN on real PCs. First validate the new path with the host and one friend, then expand to four total.

Freeform Training is loaded manually on every PC. Start preview synchronizes an already-loaded scene barrier; it does not choose a contract in the game or invoke mission loading. Offline Steam requires Windows networking to remain connected; no Steam matchmaking/IOI service is used by this preview, but the actual offline combination remains a live test.

Old actors stay scene-owned and are hidden/reused, not deleted on disconnect. Scene changes invalidate presentation. After leaving a room, reload the mission before opening a different room. A host exit cannot transparently migrate the session. Other maps are intentionally rejected by the native presentation adapter.

## Validation

All 151 core/network/presentation/runtime tests pass, plus six direct PoseHook tests and four generated speech/AI-guard tests: 161 total. Runtime tests connect four simulated games using the real PartyRoom admission and playback classes, deliver three distinct sets of captured avatar/pose/outfit/skeleton data per game, update disguises, enforce actual scene-readiness inputs, interrupt on scene loss, restart a new epoch and handle host shutdown without false completion. Existing real localhost four-socket tests still pass. The tests use fake worlds/sinks for game behavior; they cannot prove native avatar spawning is stable.

The full native project builds and passes SDK ABI 1 plus all 55 import/dependency checks; no Visual C++ redistributable dependency is introduced. The actual preview launcher EXE exchanges all preview commands with the native Windows named-pipe harness; invalid commands are rejected and idle clients time out without blocking the server.

Installer tests use a disposable workspace game-folder fixture containing only a read-only copy of the game's EXE for version metadata (never executed). They verify Foundation 4.1 -> preview upgrade, preflight without changes, nickname preservation, quiet settings and disabled upstream auto-update, repeat updates, exact 4.1 rollback and launcher switching, removal preserving an unrelated file and the copied EXE, and fresh preview installation/removal. The real installed game and both preserved 4.1 archives were not changed.

## Friend setup and rollback

START-HERE.cmd gives Install/update, Restore Foundation 4.1, Remove, Open current launcher, and Exit. Payload hashes are checked before installation. Updating preserves preferences while pinning auto-update/quiet settings. The package embeds the exact frozen Foundation 4.1 ZIP; restore checks its known SHA256 and runs its original installer, then changes the launch helper to the verified restored launcher. Installation/update switches it back to the preview launcher. Saves/game archives are not modified. No developer tools are needed on friends' PCs.

Evidence is kept in the project checks folder: four-player-preview-core-tests.txt, four-player-pose-tests.txt, four-player-guard-tests.txt, four-player-preview-build.txt, four-player-preview-dll-checks.txt, four-player-preview-bridge-tests.txt, four-player-preview-installer-tests.txt, and four-player-preview-launcher.png.

Original co-op: Jordan Rodrigues (GPL-3.0), beta commit 1a9572cac9358d96e5bde00137722ac7e6c0f9e3. ZHMModSDK: OrfeasZ and contributors. Original license/credits and corresponding derivative source are included.
