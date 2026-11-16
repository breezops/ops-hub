# Reproduce Lifetime-1

Use a fresh SDK build directory after finalizing shared sources:
```
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.life.1
-DCOOP_NPC_ACTION_DIAGNOSTICS=ON
-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON
-DCOOP_NPC_LIFETIME_DIAGNOSTICS=ON
-DCOOP_NPC_HOLD_DIAGNOSTICS=OFF
-DCOOP_MINHOOK_ABSOLUTE_ENTRIES=ON
```
Build static Release against SDK4.2.1 and the verified directx headers.
Package using tools/build-ops-release.py, preserved extracted OPS0.1.3,
this configuration/instructions/changelog/extras and exact public0.1.3 rollback.
Verify native suites, ABI, actual synthetic installer/rollback, updater staging,
source/archive/payload/vendor integrity. Keep private; no site/tag/release.
Normal player options and tested Hold-3 remain unchanged.
