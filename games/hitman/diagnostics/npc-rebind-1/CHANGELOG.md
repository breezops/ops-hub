# Private NPC binding restart test (OPS 0.1.29)

- Release a retired hold-controller pool after its mutation scope is
  revoked, all watched destructor bodies have returned, and active
  controller calls have finished.
- Serialize metadata access while preserving native continuations
  across calls that yield. Native execution runs outside the metadata mutex.
- Retry scope revocation on subsequent frames when a call is still pinned.
- Watch the original mod's full active mission catalog in each Freeform
  load, using fresh template IDs, actor slots and constructor stamps.
- Grant local interaction ownership to every watched actor. Native
  hold/resume operations are absent from this diagnostic.
- Sample old job and actor cuts inside the scene-clear return callback.

This candidate validates local binding reuse. Multiplayer NPC movement,
interactions, and a complete scene-retirement acknowledgement remain pending.
