# MW2 / SPEC OPS 4

OPS HUB's native Windows launcher and four-player Spec Ops development project
for Call of Duty: Modern Warfare 2 (2009), Steam x64 build 25374375.

**The launcher works. The four-player mod is not playable yet.** Original Spec
Ops continues to support at most two players. A real four-slot launcher lobby is available. Game networking, runtime mod injection
and four-player mission gameplay remain unfinished.

## Launcher 1.0.2-prealpha.1 / pre-alpha1.0.2-COOP

- Hosts a real six-letter-code lobby with four callsigns and shared mission/difficulty.
- Lets only the host change settings; guests update automatically about every three seconds.
- Uses the green military HUD, four-star badge, camo details and original mission concept art.
- Keeps existing detection and diagnostics under Settings, with project status under Info.
- Detects MW2 through Steam's registry and libraryfolders.vdf, with manual folder selection.
- Recognizes the exact researched executable using SHA-256.
- Runs seven offline compatibility/component checks against a private memory image.
- Exports a local report when requested; sends no telemetry or reports.
- Opens the original game through Steam and links to OPS HUB and project source.
- Keeps four-player launch disabled while the mod is incomplete.

Native x64 C++ with the runtime statically linked. No .NET, Python, administrator
rights, installer, or separate Visual C++ runtime is required by players.
The app is unsigned. Downloads contain no game executables, assets, or saves.

## Build and check

On Windows, install Visual Studio 2022 C++ x64 build tools and the Windows SDK.

```powershell
./launcher/build.ps1
./tools/test-launcher.ps1 -GamePath 'D:\SteamLibrary\steamapps\common\Call of Duty Modern Warfare 2'
./tools/package-launcher.ps1
```

Outputs go to `launcher/bin` and `_releases`, both ignored. Tests without an
installed game still cover missing/unsupported installations; real-executable
checks run only with `-GamePath`. CI never downloads or redistributes the game.

## Mission images and launcher icon

The supplied squad artwork is the app/Explorer, taskbar and desktop shortcut icon
and is shown in Info. The icon includes 16-256 pixel resolutions.

All 23 original Spec Ops images are loaded directly from your installed
`main/iw_02.iwd` into memory. The dropdown shows individual thumbnails and the
briefing changes with the selected mission. No Python installation or extracted
image cache is needed. Set the game folder in Settings if a preview is missing.
Overkill has no official image because it is a concept and retains custom artwork.
See [image sources and decoding](launcher/assets/BRANDING.md).

## Joining friends in the launcher

1. Extract the complete ZIP and open MW2-SpecOps4-Launcher.exe.
2. Enter a callsign. The host selects HOST LOBBY, then CREATE.
3. Share the six-letter code. Each friend selects JOIN LOBBY, types their own
   callsign and the code, then JOIN. Names appear in the four roster slots.
4. The host chooses a mission and one of Recruit, Regular, Hardened or Veteran.
   Guests receive the selections automatically and cannot override the host.
5. LEAVE removes your slot; the host leaving closes the room. If the host loses
   connection, the room expires after 90 seconds. Stale guest slots clear after
   45 seconds. There is no automatic host migration.

Lobby codes contain six unambiguous letters (no I/O); the input accepts 4-8
letters. Lobby sync uses HTTPS through OPS HUB, so the launcher itself does not
need port forwarding. The displayed milliseconds measure the service request,
not peer-to-peer game ping. Tokens stay in memory; no lobby credentials are
written to disk. Names are shared with the lobby service and the other members.

LAUNCH GAME stays disabled until four-player gameplay is implemented. Settings
can launch the original game through Steam; the selected launcher mission and
difficulty are not applied to that original-game launch. Overkill is explicitly
marked as a concept; the other entries are original Spec Ops mission choices.

The single version.json controls the compiled build label, package version and
release manifest. Bump it for every patch; changing the displayed version alone
never makes a build gameplay-ready.

## Native mod progress

`core/frame_codec.cpp` is the first component: it widens simulation-frame player
IDs from one bit to two and raises the decoder's valid range to 0..3. Exact
writer/reader instruction signatures validate before any private-memory change.
It has no runtime loader and is not enabled in the game. This component alone
is incompatible with vanilla frame encoding and cannot enable four players.

Before a playable build: expand/relocate player and connection storage; audit
all references and loop bounds; extend lobby membership, routing, protocol
negotiation, acknowledgement and restart logic; update mission scripts, spawns,
revives and objectives; then verify four independent clients together.

## Source and release integration

Project directory: `games/mw2`; independent tag prefix: `mw2-v`.
Public launcher channel: `https://ops-hub.breezops.chatgpt.site/mw2/release.json`.
It is separate from Stray and HITMAN. There is no automatic download or update
installation in this launcher. Future updates must not use GitHub's global
`releases/latest`, which might belong to another game.

The package script writes a ZIP, checksum and matching manifest. The live Sites
source holds the small public download; root GitHub source excludes release
binaries. The `mw2.yml` workflow builds a development artifact, not a playable
four-player mod or an automatically published release.

Networking implementation and test commands are in [LOBBY.md](LOBBY.md).
The artwork prompt and provenance are in [launcher/assets/PROVENANCE.md](launcher/assets/PROVENANCE.md).

The only persisted preference is the chosen folder under
`HKCU\Software\BreezOps\MW2SpecOps4`. Diagnostics remain in memory until Save
report is used. Reports include the selected local game path and executable hash;
review them before sharing. No firewall, game configuration or game file changes.

## Research provenance

The independently written component was checked against the installed executable
SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`.
Research references: [MW2 Enhanced](https://github.com/stxlvn/mw2_enhanced), commit
`4afa16e16f5651437211f44065bf6acf000e5cc2`, particularly its co-op frame research.
That project is two-player and is not bundled or represented as four-player.
The launcher and component here are original project code, without game assets.
Community project; not affiliated with Activision or Infinity Ward.
