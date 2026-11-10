# OPS HUB HITMAN launcher 0.1.3

This release redesigns the existing WPF launcher and introduces a HITMAN-only
update client. Its native mod is Preview 4, build
`0.13.94-four-player.preview.4`, game 3.290.1, party protocol 2. The prior two-PC
Preview 2 test passed repeated close vaults and smooth movement; no new native NPC authority
is enabled here. All players must update to this matching native build. Four slots are available; shared NPCs, bodies, items, objectives
and extraction are not yet implemented in this backend.

The interface follows DESIGN-BRIEF.txt: near-black/charcoal panels, sharp edges,
crimson actions, restrained architectural geometry and a single session screen.
Existing host/join/leave/ready/start/stop/reconnect/launch actions remain. Install,
repair and tracked removal use the existing verified PowerShell installer.
Notification sounds and upstream self-updates remain disabled. The window now
fits the Windows work area, scrolls vertically when needed, and stacks the roster
on narrower windows. Layout checks cover every control at 1120x820, 1120x690,
800x600 and 640x480 logical pixels, including long errors and an open-log button. Version 0.1.1 fixes
setup rejecting its own launcher. It captures stdout/stderr without a flashing
console, keeps the actual error visible, saves a setup log, and exposes Open
setup log. A launcher running from the destination game folder remains blocked
to protect files in use; an extracted package or cached-release launcher is safe.

## Player update flow

Extract the initial complete ZIP once. Open HITMAN-Coop-Party-Preview.exe and use
Install / repair with HITMAN closed. Check for updates fetches
`/hitman/release.json`. A newer compatible release changes the button to
Update & restart. With HITMAN closed it downloads the game-specific ZIP,
verifies its byte count/SHA-256, validates/extracts it into a unique release
directory, runs the existing installer, opens the new launcher and then selects
it in `%LOCALAPPDATA%/OpsHub/HitmanCoop/current.json`. Opening the original
launcher thereafter forwards to the verified selected launcher. Failed download,
validation or installer exit does not advance that pointer. An installer failure
can still leave partial game changes: repair with the previous complete package.
No mod files are replaced while HITMAN runs. Repair/removal preserve game saves.

Release archives include corresponding GPL source, credits/licenses and the SDK
runtime. Foundation 4.1/Preview 1 backups remain preserved separately; they are
not bundled into every player download. Previous archives are not overwritten. The site must publish ZIPs before publishing their manifest.

## Site connection

Published `hitman-vX.Y.Z` GitHub releases now upload automatically to the existing
OPS HUB site, through `.github/workflows/publish-hitman.yml`. Attach the ZIP,
`.sha256`, `CHANGELOG-OPS.md` and `hitman-release.json` before publishing. The
workflow verifies the package, uploads it, checks the public checksum, and only
then changes the public manifest. Source pushes and draft releases do not publish.
See the repository's `docs/RELEASE-PUBLISHING.md` for the owner-managed setup.

The public 0.1.0 manifest and ZIP were verified on 2026-11-09. The updater uses
`https://ops-hub.breezops.chatgpt.site` directly because opscord.net forwarding
currently drops paths. The public HITMAN feed is `/hitman/release.json`; downloads
are under `/downloads/hitman/`. Stray's update channel remains separate.

The two-PC 0.1.0-to-0.1.1 installation succeeded. The subsequent 0.1.2 test
passed repeated outfit changes without invisibility, movement and Stop/Ready/Start;
the guest reported the launcher usable at 200% scaling. Preview 4 addresses the
reported crouched gliding by carrying animation orientation with bone poses.
Its actual visual result needs the two-PC test in [POSE-FRAMES.md](POSE-FRAMES.md).
Already-visible avatars use native pose while a new outfit rig is rematched;
initial spawn/Stop/Leave still gate visibility. Remote writes are skipped if the
body bone count changed. The engine can still briefly stream geometry out. No publishing credentials belong in source or player packages.

## Build and checks

Run `launcher/build.ps1` on Windows with .NET Framework (already on supported
Windows PCs); no SDK/game/compiler is needed to build just the launcher. Run
`tools/test-launcher.ps1` for standalone updater regression tests. The native
Preview 2 source/build/tests are described in docs/research/FOUR-PLAYER-PREVIEW-2.md.
`tools/build-ops-release.py --base-package <verified-preview-2-folder>
--native-dll <built-current.dll> --output <new-output-folder>` assembles a release.
The base package is an explicit verified binary/dependency input, never committed.
It supplies the SDK runtime and redistribution licenses; rollback archives are not shipped.

The updater rejects other games, wrong runtime/protocol/game, unsafe paths,
cross-origin redirects, oversized downloads, duplicate/symlink ZIP entries and
bad hashes. A new release must be checked against the live published channel. Local staging and installer
fixtures do not prove an online update or game-session test.
