# Reproduce private Hold-1

Reserve OPS 0.1.10 / native 0.13.94-four-player.preview.4.hold.1. Use a new empty
SDK build directory and finalize/freeze all shared sources before compilation.
Add these flags to the SDK configuration in BUILDING.md:

```
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.hold.1
-DCOOP_NPC_ACTION_DIAGNOSTICS=ON
-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON
-DCOOP_NPC_HOLD_DIAGNOSTICS=ON
```

Hold diagnostics default OFF and require the verified action diagnostic build.
The public package and launcher channel remain unchanged. Assemble with
tools/build-ops-release.py using the verified extracted OPS 0.1.3 base, candidate
DLL, a new output directory, this folder's updater-config.json/START-HERE.txt/
CHANGELOG.md as the corresponding config/instruction/changelog inputs, this
folder as extras-dir and original HITMAN-Coop-0.1.3.zip as rollback-package.
Check the ABI, native fixtures, installer, exact rollback, updater staging,
archives, source/licenses and hashes before delivery. Never ship game files,
saves, secrets or fixture fault injection. No GitHub release/site publication
is authorized merely by building this private candidate.
