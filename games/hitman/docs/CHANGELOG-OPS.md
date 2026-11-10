# OPS HUB HITMAN co-op changelog

## 0.1.1 — 2026-11-09

- Fix Install / repair and in-launcher updates rejecting their own running launcher.
- Continue blocking changes while HITMAN or a destination game-folder launcher runs.
- Capture setup output without a flashing console; display the actual error in the launcher.
- Save setup logs and add an Open setup log button for successful and failed runs.
- Use Windows PowerShell's own module paths even when inherited from PowerShell 7.
- Native Preview 2 gameplay code and party protocol remain unchanged.

## 0.1.0 — 2026-11-09

- Import HITMAN native/core source, development notes and experimental NPC source patch into games/hitman.
- Redesign the WPF launcher using the supplied monochrome/crimson design brief.
- Preserve host/join/leave/ready/start/stop/reconnect and manual Freeform Training setup.
- Add install/repair, tracked removal, and a separate HITMAN check/download/verify/install/restart updater.
- Reuse native Preview 2 (0.13.94-four-player.preview.2, game 3.290.1, party protocol 2).
- Keep notification sounds and upstream automatic updates disabled.
- Add standalone core/launcher source checks and a versioned package/site-manifest assembler.

Experimental limits: movement, poses and disguises only, up to four slots. NPCs,
bodies, items, objectives and extraction are separate on each PC. Preview 2's
vault/takedown protection still needs a live regression test. The public update
channel needs Breezops to publish the initial manifest and package before online
updates can be verified. No game files, saves, player logs or credentials ship.
