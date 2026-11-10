# OPS HUB HITMAN launcher 0.1.0

This release redesigns the existing WPF launcher and introduces a HITMAN-only
update client. Its native mod is exactly Preview 2, build
`0.13.94-four-player.preview.2`, game 3.290.1, party protocol 2. The three-player
vault/takedown regression still needs live testing; no new native NPC authority
is enabled here. Four slots are available; shared NPCs, bodies, items, objectives
and extraction are not yet implemented in this backend.

The interface follows DESIGN-BRIEF.txt: near-black/charcoal panels, sharp edges,
crimson actions, restrained architectural geometry and a single session screen.
Existing host/join/leave/ready/start/stop/reconnect/launch actions remain. Install,
repair and tracked removal use the existing verified PowerShell installer.
Notification sounds and upstream self-updates remain disabled.

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

## Site connection: verified facts and pending work

The supplied ops-hub README reserves games/hitman and independent hitman-v tags.
The repository checkout has no root deployment workflow or authenticated Sites
hosting manifest. The website contains a prepared HITMAN project listing, but
the live listing currently says no download is available.

On 2026-11-09 `https://opscord.net` and `/hitman/release.json` both returned 301
to the root of `https://ops-hub.breezops.chatgpt.site`, dropping the path. The
canonical host's `/hitman/release.json` returned 404. For this reason the updater
uses that canonical OPS HUB HTTPS origin directly. Branding remains OPS HUB;
opscord.net is the intended visitor address. Fix the domain forwarding so it
preserves paths before switching the pinned updater origin to that domain.

`tools/build-ops-release.py` produces a ready-to-host `publish` tree:

- `downloads/hitman/HITMAN-Coop-0.1.0.zip`: immutable player download.
- `hitman/release.json`: HITMAN-only manifest with version, protocol, build,
  byte count and SHA-256. It is independent of Stray's release.json/API.

Publishing this tree into the site's public assets and deploying the site is
still required. Do not claim that a git push deploys this site until the owner's
external deployment connection is confirmed. No site identity, deploy token,
GitHub credentials, binaries, player logs, saves or game assets belong in git.
The launcher reports an unpublished/unreachable channel without claiming success.

## Build and checks

Run `launcher/build.ps1` on Windows with .NET Framework (already on supported
Windows PCs); no SDK/game/compiler is needed to build just the launcher. Run
`tools/test-launcher.ps1` for standalone updater regression tests. The native
Preview 2 source/build/tests are described in docs/research/FOUR-PLAYER-PREVIEW-2.md.
`tools/build-ops-release.py --base-package <verified-preview-2-folder>
--native-dll <built-Preview-2.dll> --output <new-output-folder>` assembles a release.
The base package is an explicit verified binary/dependency input, never committed.
It supplies the SDK runtime and redistribution licenses; rollback archives are not shipped.

The updater rejects other games, wrong runtime/protocol/game, unsafe paths,
cross-origin redirects, oversized downloads, duplicate/symlink ZIP entries and
bad hashes. Its production happy path must be checked against the live published
channel after the site owner connects deployment. Local staging and installer
fixtures do not prove an online update or game-session test.
