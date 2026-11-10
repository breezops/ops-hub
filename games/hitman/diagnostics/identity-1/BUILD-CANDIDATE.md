# Reproduce the private identity candidate

This configuration reserves OPS 0.1.4 for the immutable identity-check artifact.
It does not change the canonical launcher configuration or publish a release.

Configure the SDK build as described in BUILDING.md, adding:

```
'-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.identity.1'
'-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON'
```

Keep normal builds in a separate directory with both options at their defaults.
Build the launcher with launcher/build.ps1; its party build reads the packaged
updater-config.json, so the same compiled launcher accepts the candidate identity.

From games/hitman, assemble using tools/build-ops-release.py with:

```
--base-package <verified extracted OPS 0.1.3 folder>
--native-dll <candidate SDK build's HitmanCoop.dll>
--output <new immutable candidate output directory>
--config diagnostics/identity-1/updater-config.json
--start-here diagnostics/identity-1/START-HERE.txt
--changelog diagnostics/identity-1/CHANGELOG.md
--extras-dir diagnostics/identity-1
--rollback-package <original HITMAN-Coop-0.1.3.zip>
```

The builder verifies base payload hashes, native identity and rollback SHA-256,
then produces the ZIP/checksum, corresponding source and a manifest in the local
output directory. Do not publish its manifest while this is a private candidate.
Validate the package and installer in a synthetic game fixture before offering it
for the solo test. Do not install into a running game or modify existing archives.
