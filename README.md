# OPS HUB

Shared source workspace for independent game mods and launchers.

- `games/stray/CoopMod`: Stray Lua mod, Python launcher/transport and native UI sources.
- `games/hitman`: reserved for the Hitman maintainer; no Stray files belong here.
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
Existing Stray downloads remain hosted by the current site. Publishing automation
is not connected yet. Private repository release assets require authentication;
public downloads must remain on the site or a separately public release channel.

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
does not itself grant production deployment access. Connect an authenticated
publishing workflow before claiming either maintainer can deploy automatically.
