# Reproduce the Loop-6 guest-follow test

Use the Loop-2 build and packaging instructions with the Loop-6 profile.
Add `-DCOOP_PARTY_GUEST_START=ON` and `-DCOOP_PARTY_CONTRACT_TRACE=ON`.
Set the native build override to `0.13.94-four-player.preview.4.loop.6`.

The host starts Freeform using the normal game menu. Once the host's exact
native Freeform contract and local avatar are present, the room starts its
loading phase. A guest in that phase requests the same native continuation
verified in the Loop-5 solo run, once per room and epoch. Before dispatch, the
guest must be in the loaded main menu with an empty contract context. The
ordinary loaded barrier requires the correct scene, contract ID, and local
avatar on every PC. Same-room restart continues to use the previous route.

Test on two PCs from the main menu first. Verify guests load and enter
play automatically, then verify one native restart. A live four-player
test is separate.
