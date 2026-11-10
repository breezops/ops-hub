# OPS 0.1.7 / action.3 - private solo job diagnostic

- Reuse NpcActionProbe with a thread-aware MinHook installation and a pinned,
  process-lifetime bridge/trampoline. Retained mode deliberately cannot remove
  its hook, including after all observed calls return.
- Keep fixed-size job bookkeeping independent of SDK world state. No mutex or
  TLS ownership spans the original native call or a yielding continuation.
- Log native job entries, original-call returns, outstanding calls, pending
  native pointers without observed entries, and constructor births after
  attachment alongside the validated native action-boundary audits.
- Treat pre-attachment work as unknown. Unit addresses may be reused across
  actors/restarts; matching addresses and zero counts authorize no mutation.
- Add isolated late-attachment, alternate-stack/concurrent work, invalid-entry,
  mode-switch and patch-integrity fixtures. Preserve the legacy probe tests.
- Include the exact, checksum-verified public OPS 0.1.3 rollback.

No NPC control, cancellation, authority acknowledgements or shared world state
are enabled. Public downloads and Stray remain unchanged.
