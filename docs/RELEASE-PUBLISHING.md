# OPS HUB release publishing

HITMAN releases publish automatically to the existing OPS HUB Site when a GitHub
release with an independent `hitman-vX.Y.Z` tag is published. Prereleases are
supported. Drafts and ordinary source pushes do not publish launcher updates.

Attach all four prepared files before publishing the release:

- `HITMAN-Coop-X.Y.Z.zip`
- `HITMAN-Coop-X.Y.Z.zip.sha256`
- `CHANGELOG-OPS.md`
- `hitman-release.json`

The workflow validates the manifest, package structure, embedded updater config,
byte count and checksum. It uploads immutable versioned files, downloads the ZIP
publicly and checks it again, then publishes and re-reads the manifest. A failure
before the manifest step leaves the current update channel unchanged. Retry via
Actions > Publish HITMAN release to OPS HUB > Run workflow with the same tag.
Never replace a published ZIP: bump the version. Downgrades are rejected.

Players keep using Check for updates. Website links resolve the latest published
HITMAN release. The updater's canonical host remains
https://ops-hub.breezops.chatgpt.site because opscord.net forwarding drops paths.

`HITMAN_PUBLISH_TOKEN` lives only in GitHub Actions secrets. The Site stores its
SHA-256 verifier as a secret, with a dedicated R2 binding for release files.
The endpoint cannot publish Stray or change the Site source, identity or audience.
Keep the token out of launcher packages and logs. Current package upload limit:
32 MiB; protocol 2, runtime 1 and HITMAN 3.290.1 are explicitly validated.

Stray publication and website source deployment are still separate owner-managed
flows. This workflow does not compile native mods or deploy arbitrary website
changes. Add each future game's channel deliberately with its own credential and
validation rules; never use the repository-wide latest-release endpoint.
