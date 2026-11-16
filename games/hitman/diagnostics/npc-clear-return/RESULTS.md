# Solo Freeform restart result

Steam HITMAN 3.290.1, SDK 4.2.1, private OPS 0.1.28.
The user completed two in-game mission restarts and closed HITMAN.
The log identifies native build `preview.4.npc-clear-return`.

| Observation | First restart | Second restart |
| --- | --- | --- |
| Pre-clear token | 3 | 4 |
| Full unload flag | 1 | 1 |
| Virtual dispatch observed | 1 | 1 |
| Dispatch returned | 1 | 1 |
| Captured actor births | 99 | 99 |
| Captured destructor entries | 99 | 99 |
| Captured destructor body returns | 99 | 99 |
| Pending old observed jobs | 0 | 0 |
| Healthy job and actor books | Yes | Yes |
| Enabled actors admitted after reload | 83 | 83 |

The post-reload constructor marks were 198 and 297. All 83 active actors
passed the fresh identity checks after each reload. Destruction observations
recorded no unknown actors or rejected completion receipts.

The pre-clear timestamps were 19:10:08.550 and 19:10:33.473 local time on
2026-11-10. Corresponding evidence was sampled on the SDK frame at
19:10:09.035 and 19:10:34.066. Thus this run establishes completed
dispatch and drained observed cuts by that frame. It does not establish
the precise draining order relative to the dispatch return.

The next development change saves job and actor cut states inside the
return callback and logs those independently from subsequent frame samples.
This refinement builds locally as `preview.4.npc-clear-return.2`.
