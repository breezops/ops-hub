# Reproduce Loop-1

Use a fresh Release build against the shipped SDK 4.2.1 developer package.

```text
-DCOOP_PARTY_NATIVE_RESTART=ON
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.loop.1
```

Build `HitmanCoop` and run the core suite. Package with
`tools/build-ops-release.py`, this profile's config and instructions, the
verified OPS 0.1.3 extracted package and exact OPS 0.1.3 rollback ZIP. Reuse
the verified `RESTORE-0.1.3.cmd` and `Restore-Tested.ps1` from Pool-1.
