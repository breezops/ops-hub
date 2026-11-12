# Reproduce Hold-3

Reserve OPS 0.1.12 / native 0.13.94-four-player.preview.4.hold.3. Use a new empty
SDK build directory. Finalize/freeze shared sources before compiling. Add:

```
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.hold.3
-DCOOP_NPC_ACTION_DIAGNOSTICS=ON
-DCOOP_NPC_IDENTITY_DIAGNOSTICS=ON
-DCOOP_NPC_HOLD_DIAGNOSTICS=ON
-DCOOP_MINHOOK_ABSOLUTE_ENTRIES=ON
```

The opt-in x64 backend requires Python 3.8+ at configure time to generate the
documented build-local MinHook derivative; see third_party/minhook/PROVENANCE.md.
Normal backend defaults remain unchanged. No Python is required on player PCs.

Assemble with tools/build-ops-release.py using preserved extracted OPS 0.1.3,
the candidate DLL, a new output directory, this folder's updater-config.json,
START-HERE.txt, CHANGELOG.md and extras, plus exact original 0.1.3 rollback ZIP.
Check both native backend suites, SDK ABI, installer/rollback, updater staging,
archive/source/licenses/hashes and absence of fixture fault injection before
distribution. Hold-3 remains private; no tag/release/site publication implied.
