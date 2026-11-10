# OPS HUB

Shared source workspace for independent game mods and launchers.

- `games/stray/CoopMod`: Stray Lua mod, Python launcher/transport and native UI sources.
- `games/hitman`: HITMAN native mod, launcher and independent release channel.
- `games/planet-zoo-2`: Planet Zoo 2 Co-op, in development; no release yet.
- `website`: OPS HUB website and existing lobby service source.
- `projects.json`: project directory and independent release-tag mapping.

## Collaborating from separate Codex accounts

Clone this repository on each developer PC. Work on a branch, commit only the
relevant game directory, and open a pull request. Neither developer needs the
other developer's Codex account or tokens. Repository access is managed through
GitHub collaborators after the owner's repository has been created.

## Independent releases

Use tags `stray-vX.Y.Z` and `hitman-vX.Y.Z`. Each game must have its own archive,
version manifest, changelog and updater URL. Never use the repository-wide
"latest release" endpoint to update a game: it could refer to the other game.
HITMAN releases now publish automatically when a `hitman-vX.Y.Z` GitHub release
is published with its ZIP, checksum, changelog and manifest attached. See
[release publishing](docs/RELEASE-PUBLISHING.md). Existing Stray downloads and its
manual publishing flow are unchanged. Private GitHub assets are transferred by
Actions to public OPS HUB downloads; players do not need GitHub authentication.

## Stray build status

This is a source-only seed, not a portable turnkey build. Existing build scripts
expect a Stray installation root containing CoopMod and locally prepared runtime,
UE4SS and WebView2 dependencies. Those binaries, game files, saves, logs and test
profiles are intentionally excluded. Configure the supported SDK/runtime build
inputs before enabling unattended release builds. The production updater URL
redirect compatibility issue also remains open; do not publish a new Stray
release claiming that migration is fixed.

## Website deployment

The existing production deployment belongs to the site owner. Its hosting identity,
runtime secrets and Git credentials are not in this repository. GitHub collaboration
does not itself grant website-source deployment access. HITMAN release uploads
use a dedicated authenticated publisher; arbitrary website changes still deploy
through the owner's existing Sites project. No general source-push deployment
pipeline is configured.
