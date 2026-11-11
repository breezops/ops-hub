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

## Two-PC result

Both logs identify Loop-6 on Steam 3.290.1. The host entered through the
native planning controller and reported the verified Freeform contract.
The guest submitted one room-triggered launch for epoch 1 and loaded the
same native contract. The host reported all players loaded about 3.7 seconds
after entering the room's loading phase. Both players could see each other
move. The host's native restart advanced the room to epoch 2 and both logs
confirmed a reload in the same room. A live four-player check remains.

## First three-PC attempt

The host and both guests connected. Guest 1 submitted the native launch
and loaded the exact Freeform contract. Guest 2 had entered the initial
facility arrival mission and returned to the main menu earlier in the
same process. Its native contract context was still populated, so the
current launch guard refused and interrupted the room. The host roster
then showed the host loaded and both guest receipts incomplete.

This run establishes a reproducible previous-mission limitation in the
fresh-menu launch adapter. Focus loss was reported but remains unverified
as a cause. Retry with fresh processes before changing the native startup
path, then add an actionable preflight and support native cleanup of a
previously visited mission.

## Three-PC retry

All three PCs ran Loop-6. The host selected Freeform Training, and both
guests submitted one automatic launch for epoch 1. Both guests confirmed
the exact Freeform contract, and the host reported everyone loaded about
3.5 seconds after the host's own load. The group confirmed that each
player could see both others moving. The host restarted from the game
menu, and all three logs confirmed epoch 2 loaded in the same room.
