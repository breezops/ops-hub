# OPS 0.1.8 / action.4 - private allocation diagnostic

- Trace this private MinHook instance's VirtualAlloc, VirtualQuery, HeapAlloc
  and HeapReAlloc during synchronous hook creation. Preserve actual API errors
  and use fixed counters without allocations or locks in the tracing path.
- Distinguish near-code allocation failures, hook-table heap failures and a
  search finding no nearby candidate. Heap failures have no invented Win32 code.
- On creation error 9 only, scan the nearby address range read-only and report
  free bytes, largest free extent and aligned 4K candidates without addresses.
- Keep all 14 pinned vendor files unchanged through per-source compile aliases.
  Do not change allocation policy, relax signatures or enable native NPC control.
- Add isolated near-allocation, heap-allocation and no-candidate failure/retry
  fixtures plus concurrent, independent capture/report-reset validation.
  Fault injection is compiled only into fixture builds.
- Preserve exact checksum-verified public OPS 0.1.3 rollback.

This diagnostic does not claim to fix the intermittent live allocation refusal.
The next live check is startup-only, before requesting further mission actions.
