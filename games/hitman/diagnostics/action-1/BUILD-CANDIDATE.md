# Reproduce the private action observer

Reserve OPS 0.1.5 for the action.1 artifact. Keep identity.1 and OPS 0.1.3
immutable. Use the SDK build from BUILDING.md in a separate directory, adding:

```
'-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.action.1'
'-DCOOP_NPC_ACTION_DIAGNOSTICS=ON'
'-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON'
```

Both diagnostic options default OFF. MinHook's pinned x64 sources build
statically; no additional redistributable or game file is required. The
package builder includes its license and source.

From games/hitman, build the launcher with launcher/build.ps1, then assemble:

```
python tools/build-ops-release.py
  --base-package <verified extracted OPS 0.1.3>
  --native-dll <candidate SDK build's HitmanCoop.dll>
  --output <new immutable output directory>
  --config diagnostics/action-1/updater-config.json
  --start-here diagnostics/action-1/START-HERE.txt
  --changelog diagnostics/action-1/CHANGELOG.md
  --extras-dir diagnostics/action-1
  --rollback-package <original HITMAN-Coop-0.1.3.zip>
```

The arguments are shown on separate lines for readability; pass them as one
command or a structured argument array. Verify the DLL ABI, archive contents,
installer and exact rollback in fixtures before distribution. Do not publish
this local manifest or create a GitHub release until live testing warrants it.
