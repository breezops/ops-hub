# OPS 0.1.9 / action.5 - clean allocation diagnostic

- Replace the invalid Action-4 diagnostic: its incremental build mixed objects
  compiled with different report layouts while the shared header was changing.
  Disassembly confirmed the old report return copied 96 bytes where the final
  caller expected 128. Discard Action-4 allocation telemetry and use this build.
- Encode report sizes into creation and installation API linker symbols so
  incompatible stale objects fail to link rather than silently exchanging data.
- Log allocator evidence on every creation result, and near-range scan values
  only when error 9 actually requested that scan. Correct the misplaced condition.
- Build in a new empty directory with all shared source/header edits finished
  before starting compilation. Keep diagnostic sources frozen until completion.
- Preserve the allocation tracing, unchanged allocator policy, byte-identical
  vendor files, fixture-only fault injection and exact tested 0.1.3 rollback.

Startup-only live check remains required. No fix for the original intermittent
allocation refusal, NPC control or native job-retirement proof is claimed.
