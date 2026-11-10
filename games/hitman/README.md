# HITMAN / CO-OP

Windows Steam co-op research and OPS HUB launcher source for HITMAN World of
Assassination. This imports the Preview 2 native foundation and adds the first
OPS HUB launcher/update client. It is experimental, not a finished multiplayer
release. See [launcher and updates](docs/LAUNCHER-AND-UPDATES.md) for the actual
site integration status and player flow.

- `core`: transport, four-slot party lifecycle and presentation, independent of the SDK.
- `plugin`: native ZHMModSDK adapter and crash/pose/interaction guards.
- `launcher`: WPF interface, update client and pinned channel configuration.
- `distribution`: tracked installer/remover and archived restore script source.
- `tools`: build, updater tests and immutable game-specific release assembly.
- `docs/research`: development notes, known limits and test strategy.
- `docs/experimental-npc-foundation.patch`: preserved unfinished NPC authority work
  against Preview 1. This is research source, not enabled or directly applicable
  to Preview 2 without integrating its protection changes.

Current native build: `0.13.94-four-player.preview.4`, HITMAN 3.290.1,
ZHMModSDK 4.2.1 ABI 1, party protocol 2. Up to four players share movement,
poses and disguises in Freeform Training. NPCs, bodies, inventory, objectives
and extraction remain separate on each PC. Preview 1 had successful two/three-PC
presentation tests and a reported stand-in takedown bug. Preview 2's protection
fix passed the two-PC vault test. Preview 3 passed repeated outfit changes without
invisibility, movement and Stop/Ready/Start on two PCs; the guest reported the
launcher usable at 200% scaling. Preview 4 carries animation world orientation
with the pose to address crouched turning; this change needs a live two-PC test.
See [pose frame conversion](docs/POSE-FRAMES.md).

Development: the preserved four-player NPC coordinator and replay test clients
are integrated into the core. The runtime has an optional NPC adapter with a
host-only default and a native-preparation barrier. The shipped native preview
does not supply that adapter; shared NPCs are not enabled in player downloads.
See [NPC integration and reuse](docs/NPC-INTEGRATION.md).

## Developer entry points

Run `launcher/build.ps1` for the launcher and `tools/test-launcher.ps1` for updater
tests. Run `cmake -S core -B _build-core`, build and run coop_tests for the core.
For native builds, read BUILDING.md and supply the pinned SDK developer package
and DirectX-Headers paths to CMake. This repository contains source, not SDK
binaries or game assets. The root HITMAN workflow checks core and launcher code;
it does not pretend to publish a complete native release or deploy the site.

Use independent `hitman-vX.Y.Z` release tags. Player updates use the OPS HUB site's
HITMAN manifest, never the repository-wide latest GitHub release. GitHub is the
source/collaboration channel; public ZIPs and manifests belong on the website.
`projects.json` identifies this project's directory and tag prefix.

Foundation 4.1 and Preview 1 are preserved separately in the maintainer's frozen
archives. Their archive hashes and restoration details are in the research notes.
Do not check those binaries, personal logs, savegames, credentials, `_deps` or
build outputs into this repository. The release assembler includes corresponding source and third-party licenses;
the frozen rollback archives remain separate.

Original co-op: Jordan Rodrigues, GPL-3.0; see LICENSE/CREDITS.md.
ZHMModSDK: OrfeasZ and contributors. UPSTREAM-README.md retains the original
project documentation; docs/research describe this fork's narrower tested scope.
