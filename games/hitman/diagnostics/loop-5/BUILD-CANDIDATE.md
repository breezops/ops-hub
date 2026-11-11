# Reproduce the Loop-5 diagnostic

Use the Loop-4 build and packaging instructions with the Loop-5 profile.
Add `-DCOOP_PARTY_FREEFORM_MENU_START=ON` and
`-DCOOP_PARTY_CONTRACT_TRACE=ON`. Set the native build override to
`0.13.94-four-player.preview.4.loop.5`.

The public Start input at Steam RVA `0xB6E930` enters the contract-launch
routine immediately only if the entity's byte at `+0x38` is set. Otherwise
it sets a pending byte at `+0x39` and starts the busy indicator. The
entity's event callback at `0xB6F390` compares the event name to
`ContractEnd` and `ContractFailed`. Either event sets the ready byte and
releases a pending launch through `0xB6E990`. That wait accounts for the
Loop-4 loading indicator without a native contract request.

The prologue menu's ID-based JSON action has a different entry. Its
implementation at `0xC1C670` resolves the contract ID, obtains its current
menu page, and builds callbacks before calling the planning controller or
the common wrapper. It does not use the contract entity's end-event gate.

Loop-5 tests the entity continuation directly while the main-menu
contract context is empty. It checks the executable bytes, verified
manager location, starter vtable and the starter's ready/pending bytes.
No event is synthesized and the game supplies its own wrapper callbacks.
The result still requires a matching native scene and contract ID with
a playable avatar.

The solo run submitted the launch at 17:42:21.139. The wrapper returned
through `0xB6EE32`, with the first callback slot empty and the second set.
HITMAN requested the Freeform tutorial scene 0.152 seconds after dispatch
and reached the matching contract and playable game state in 3.143 seconds.
