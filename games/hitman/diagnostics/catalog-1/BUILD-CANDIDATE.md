# Reproduce Catalog-1

Use the shipped SDK 4.2.1 developer package and a fresh static Release build.

```text
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.catalog.1
-DCOOP_NPC_ACTION_DIAGNOSTICS=ON
-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON
-DCOOP_NPC_LIFETIME_DIAGNOSTICS=ON
-DCOOP_NPC_DESTRUCTION_DIAGNOSTICS=ON
-DCOOP_NPC_HOLD_DIAGNOSTICS=ON
-DCOOP_NPC_POOL_DIAGNOSTICS=ON
-DCOOP_MINHOOK_ABSOLUTE_ENTRIES=ON
-DCOOP_NPC_CATALOG=ON
```

Build `HitmanCoop`. Use `tools/build-ops-release.py` with the verified extracted
OPS 0.1.3 package, this directory's config, instructions and changelog, and the
matching OPS 0.1.3 rollback ZIP. Reuse the tested `RESTORE-0.1.3.cmd` and
`Restore-Tested.ps1` from Pool-1. Verify the DLL ABI and package integrity, then
exercise installation and rollback against a synthetic game directory.
