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

## Existing tags and verification

Create release tags from a commit that includes `.github/workflows/publish-hitman.yml`.
Pull the merged main branch before preparing the next release. Old tags created
before the workflow existed cannot trigger that workflow retroactively: use the
manual retry entry point for those releases.

The initial hitman-v0.1.0 channel was verified by GitHub Actions manual dispatch:
https://github.com/breezops/ops-hub/actions/runs/42730501808 (success).
The existing immutable ZIP remained 13,614,933 bytes with SHA-256
62506fecf45dc852d4d4f9b2b9b8b6c15574a5f15ae204fcef68ea4b52177b96.
This is a real hosted upload/manifest verification, not a native gameplay test.

Site source is mirrored under website/. When deploying it through the owner Sites
flow, preserve the Site ID and D1 binding and declare `r2: "BUCKET"` in the retained
hosting manifest. Preserve the configured secret HITMAN_PUBLISH_TOKEN_SHA256.
Do not upload source-repository credentials into GitHub Actions.
