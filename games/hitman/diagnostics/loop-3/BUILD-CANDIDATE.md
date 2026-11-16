# Reproduce the Loop-3 diagnostic

Use the Loop-2 build and packaging instructions with the Loop-3 profile.
Add `-DCOOP_PARTY_CONTRACT_TRACE=ON` and set the native build override to
`0.13.94-four-player.preview.4.loop.3`.

Static inspection found six direct callers of the wrapper at RVA `0xA27AE0`.
The wrapper prepares the Start Contract UI busy state, builds an engine-owned
completion handler, and calls the contract manager at RVA `0x959C20`. The
observer checks the wrapper entry and manager call bytes before installing.
It records the original menu caller and callback presence for the verified
Freeform ID. It neither retains callback pointers nor invokes the wrapper
on behalf of the player.

When comparing the trace with the binary, the return RVAs immediately after
the six calls are `0xADD948`, `0xB0ACC8`, `0xB52DA9`, `0xB54727`,
`0xB6EE32`, and `0xC1C96F`. Resolve the observed caller to a frontend action
and check its prerequisites before requesting a launch from a party.

## Solo trace result

A normal menu launch attached the observer and hit the wrapper once with
Freeform's verified ID. The return RVA was `0xB54727` and both callback
slots were populated. The game requested the tutorial scene about 0.2 seconds
later and reached playable Freeform about 3.1 seconds after the wrapper.

The call at `0xB54722` belongs to a function beginning at `0xB54360`.
Its wrapper context is the `+0xF8` interface on
`ZPlanningPageControllerEntity`, identified through the Steam executable's
RTTI. The function reads planning state (including `saveloadout` and
`data.Contract.Metadata.Location`) and clones both callbacks before calling
the wrapper. One upstream caller reads `contractid` from the planning page
and the other queries the native controller interface before forwarding its
callbacks. A guest sitting at the main menu needs a native way to establish
that planning context or an independently verified launch path.
