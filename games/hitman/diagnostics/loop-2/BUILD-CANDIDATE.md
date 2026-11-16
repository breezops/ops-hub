# Reproduce the Loop-2 diagnostic

Configure a fresh x64 Release build against the SDK 4.2.1 developer package.

```text
-DCOOP_PARTY_NATIVE_RESTART=ON
-DCOOP_PARTY_INITIAL_LOAD=ON
-DCOOP_NATIVE_BUILD_OVERRIDE:STRING=0.13.94-four-player.preview.4.loop.2
```

Build `HitmanCoop`, build the launcher with `launcher/build.ps1`, and run the
core and launcher suites. Pass this directory's config, start guide and changelog
to `tools/build-ops-release.py`. Use the verified OPS 0.1.3 base and rollback ZIP
with the rollback scripts from Pool-1, as for Loop-1. The candidate can run from
an extracted local package.

For the initial trace, use HITMAN's own menu to launch Freeform Training on
game build 3.290.1. The launch records contain the exact native contract
ID as reported by the game and the caller return RVAs for the scene request.
Use the matching game executable when resolving the addresses. Confirm the
contract initialization path before exposing a guest launch request.

Read-only inspection of the Steam 3.290.1.0 executable identified a native
start candidate at RVA `0x959C20` and a restart candidate at `0x95A440`.
The first references the game's `StartContract` reason and assigns the
contract ID at manager offset `0x8C0`. It also consumes a callback object.
The scene trace should be compared with this code and its callers before
choosing where to request a guest launch.

## Solo trace result

The Steam 3.290.1.0 executable with SHA-256
`0a337c7979a48b1c2d2e4cf7e80a63bf634c527b6bd301dfe46db8bb7f977b52`
loaded Freeform Training from the native menu as type `tutorial`, with
start enabled, codename `Polarbear Module 002_B`, and no additional bricks.
The playable avatar appeared about 2.9 seconds after the scene request.
The native contract ID matched the room allowlist,
`b573932d-7a34-44f1-bcf4-ea8f79f75710`.

The scene request trace was `0x497E2, 0x43F14, 0x43CCD, 0x3C46A, 0x3C051,
0x86E81, 0x3E40C, 0x1B27596`. The `0x497DC` virtual call passes the scene
request downstream. The trace did not contain the earlier contract-start
candidate. Static inspection found a direct caller at `0xA28994` that sets up
an engine-owned callback and passes the contract ID into `0x959C20`. Observe
that caller during a native menu launch before invoking any entry point.
