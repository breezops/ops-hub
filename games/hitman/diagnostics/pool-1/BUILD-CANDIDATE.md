# Reproduce Pool-1

Fresh SDK4.2.1/static Release build after finalizing shared sources:
```
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.pool.1
-DCOOP_NPC_ACTION_DIAGNOSTICS=ON
-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON
-DCOOP_NPC_LIFETIME_DIAGNOSTICS=ON
-DCOOP_NPC_DESTRUCTION_DIAGNOSTICS=ON
-DCOOP_NPC_HOLD_DIAGNOSTICS=ON
-DCOOP_NPC_POOL_DIAGNOSTICS=ON
-DCOOP_MINHOOK_ABSOLUTE_ENTRIES=ON
```
Use tools/build-ops-release.py with preserved extracted OPS0.1.3, this config,
instructions/changelog/extras and exact public0.1.3 rollback ZIP.
Verify normal/absolute native suites, clean SDK ABI, fixture injection exclusion,
actual synthetic install/rollback, updater staging and archive/payload/vendor
integrity. Private test only: no tag/release/site publication. Lifetime-3 and
public multiplayer remain separately preserved.
