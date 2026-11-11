# Reproduce the Loop-4 diagnostic

Use the Loop-2 build and packaging instructions with the Loop-4 profile.
Add `-DCOOP_PARTY_FREEFORM_PROBE=ON` and set the native build override to
`0.13.94-four-player.preview.4.loop.4`.

Read-only inspection using the [RPKG CLI](https://github.com/glacier-modding/RPKG-Tool)
and the [Glacier resource index](https://github.com/glacier-modding/Hitman-Hashes)
found these resources in the installed game:

| Resource | Observation |
| --- | --- |
| `001843F7C2B99591.JSON` | The local Freeform seed contract matches the menu trace's ID, tutorial type, and scene. |
| `0014A4FA51735B9F.CPPT` | The native contract-start entity exposes `m_sContractId` and defaults to normal difficulty. |
| `0093B7139387B988.TEMP` | The shipped Start Prologue graph invokes `Start` on this entity. |
| `00D15A5F0F49C418.TEMP` | The tutorial completion graph also uses `Start` and wires an alternate scene-load branch. |
| `00AA39759F057AAF.JSON` | The prologue menu's native `start-contract` action accepts a contract ID without supplying a full contract object. |

The test uses the SDK to instantiate the contract-start entity under the
live main-menu scene, writes only the verified Freeform ID, and signals
`Start` after checking the reflected pin. The entity remains scene-owned
for any native completion. The probe does not alter the player's settings
or send a party request.

In the Steam executable, the native entity path calls the same contract
wrapper observed in Loop-3. At RVA `0xB6EE2D`, the game passes the entity's
contract ID and a callback that captures the entity. Its `+0x18` interface
vtable identifies it as `ZContractStartEntity`. The SDK call uses the entity's
reflected input so the game builds and owns that callback.

Success requires a playable Freeform scene and a matching native contract
ID. A normal return from the Start pin is only a request receipt. If the
scene never loads, trace the entity completion and the separate native
level-control branch before choosing the party adapter.

## Solo result

The Steam 3.290.1 run created the entity, set the Freeform ID and returned
from `Start` at 17:31:00. The user saw a loading indicator in the main
menu. Game frames continued, with no scene-clear or Freeform scene request.
A later read-only check found the native manager's scene, location, and
contract strings empty. This establishes dispatch and a visible UI
transition, but gives no evidence that the game accepted the contract.
Leave guest launch disabled while resolving the missing initialization.

The successful Loop-3 route starts from the populated planning controller.
The shipped prologue also has a menu `start-contract` action that accepts an
ID. Compare that action's initialization with the entity path, especially
contract data and loading context, before adding another launch attempt.
