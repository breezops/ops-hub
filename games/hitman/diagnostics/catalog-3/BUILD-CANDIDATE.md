# Reproduce Catalog-3

Use the SDK, Release configuration and flags from the Catalog-1 recipe with
`-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.catalog.3`.
Build `HitmanCoop` and run the core suite. Use the Catalog-3 config and
instructions with `tools/build-ops-release.py`, the verified extracted OPS
0.1.3 package, and the exact OPS 0.1.3 rollback ZIP. Reuse Pool-1's
`RESTORE-0.1.3.cmd` and `Restore-Tested.ps1` for rollback.
