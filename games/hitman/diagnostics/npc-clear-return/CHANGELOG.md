# Private scene-clear diagnostic (OPS 0.1.28)

- Observe the return from the verified 3.290.1 ClearScene virtual slot.
  The wrapper calls the SDK-patched entry and preserves detour ordering.
- Associate the return with the matching pre-clear token on the same thread.
  Report it beside observed NPC job and destruction receipts.
- Keep NPC simulation local and leave native retirement unconfirmed.

The diagnostic build includes the party roster and restart audit, but
the first playtest needs only a solo Freeform restart. A dispatch return
does not establish that the SDK ran the native body or that all work drained.
Calls bypassing the virtual slot are outside this observation.
