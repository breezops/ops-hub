# Foundation 4 — launcher controls and four-player groundwork

Version 0.13.94-foundation.4, gameplay protocol 19, local launcher bridge schema 1. Target: HITMAN 3.290.1 / SDK 4.2.1 ABI 1.

## Current capability

This build adds a standalone Windows launcher with real local control of the plugin: start HITMAN through Steam, host a direct session, join by address, leave, ask/reply to a ready check, request the existing countdown, and replay/restart Freeform Training as host. It shows the game/mod version, player roster, role and connection/mission status. The SDK menu is not required for those actions.

The playable networking backend still supports TWO players. The required product target is FOUR players total (one host and three guests). The launcher advertises both values honestly, and its status schema carries a player array rather than a single partner-only UI contract. A tested four-seat lifecycle model is in core/include/coop/LobbyRoster.h; transport, avatar routing and NPC authority have not yet adopted it. Four-player gameplay is not implemented by that model alone.

Automatic contract launch, native shared extraction and return to a persistent lobby remain unfinished. For this test, load Freeform Training through HITMAN on both PCs. The launcher can then perform readiness checks and the existing native replay/restart path. A native exit or mission-load call is not guessed or exposed as a working button.

No body-drag/locker fix is claimed here. Foundation 3's working bag exchange, silent notifications and existing gameplay remain the baseline. The received native Attach/Detach metadata is retained for the attachment pass.

## Local bridge

The plugin creates a nonblocking Windows named pipe scoped to its process ID and Windows user. Its ACL permits the current user; remote pipe clients are rejected. Commands have a bounded allowlist and no shell, file-path or arbitrary-engine-call operation. All gameplay actions run on the game thread. Idle clients time out, requests receive bounded JSON responses, and a response acknowledgement avoids blocking the game on FlushFileBuffers.

The WPF launcher uses Windows .NET Framework; player PCs do not need Visual Studio or a developer toolchain. It checks the local plugin build before enabling controls. The bridge adds no router or Windows Firewall port requirement.

## Install and test

1. Close HITMAN. Extract this ZIP into a new folder. Open START-HERE.cmd, choose 1 to install/update, then open HITMAN-Coop-Launcher.exe (or choose 3 in START-HERE).
2. Use Launch HITMAN, wait for the launcher to detect the plugin, then host on one PC and join from the other using the existing host address. Use the existing two-PC test setup; three guests are not supported yet.
3. Load Freeform Training through HITMAN on both PCs. Confirm the launcher changes to both players being in the mission. Check the ready request/reply and countdown.
4. The host uses Replay / restart once. Confirm both games reload and the launcher reconnects to the same ongoing session state.
5. Save both game logs. Report any button that fails to acknowledge, wrong role/roster, paused/minimized-game control issue, or replay mismatch.

This test establishes the real launcher-to-plugin path before multiplayer transport is expanded. It does not certify the eventual four-player play/extract/lobby loop.

## Validation

110 core/network tests pass, including bounded launcher commands, four-seat admission, all-player ready/load gating, completion acknowledgements, stale replay epochs and disconnect interruption. Actual WPF-client to native-bridge harness tests pass for status/actions, invalid request rejection and idle-client recovery outside the sandbox. The WPF layout was rendered directly to a PNG and inspected; no desktop screen capture was used. SDK ABI, package and installer results are recorded in checks/foundation-4-validation.txt. Real HITMAN/second-PC launcher verification remains pending.

Original credits/licenses are retained, and corresponding launcher/plugin source is included. This is an experimental community mod, not an official IO Interactive product.
