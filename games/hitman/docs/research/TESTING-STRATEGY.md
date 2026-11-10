# Testing with fewer people

Two real Steam PCs remain the normal feature-development setup. The same room, per-player records, scoped messages and authority code run for two, three and four total participants. A two-PC test proves an actual network hop and native behavior on both sides; it is not a separate two-player implementation.

| Setup | Best use | What it cannot prove |
|---|---|---|
| Automated fake worlds with 2/4 peers | Ownership, handoffs, stale messages, KO/death policy, loss/reordering, disconnects, replay epochs, bounded queues | Engine animations, AI workers, native perception, graphics or real hardware behavior |
| Two real HITMAN PCs | Native movement/disguises, guard/perception tests, actual damage/KO/death, bodies/items, scene transitions and recovery as those features are implemented | Three simultaneous remote contexts or four independent native game simulations |
| Two real HITMAN PCs + two replay clients | Four-seat admission/readiness, three remote avatar/rig/pose contexts on each PC, real rendering and host relay traffic | Four independently controlled players, four AI simulations, independent combat/interactions or separate PC performance |
| Four real HITMAN PCs | Final count-specific validation: simultaneous actions, guest-to-guest NPC handoffs, mixed rigs/disguises, worst host/client load and disconnect combinations | Broad map compatibility without additional mission tests |

Automated tests run most combinations before involving people. NPC core tests assert that the host accepts reports from one assigned source and that handoffs do not start two fake simulators. They run the same path at two and four peers, including deliberate loss and native-callback failure models. Passing those does not claim native engine synchronization.

The optional Replay Lab 1 supports the frozen movement preview only. Names are [LAB BOT 1] and [LAB BOT 2]. Synthetic readiness means the replay participant is ready after the real host's loaded/readiness report; it does not mean another HITMAN instance exists or owns a mission. Bots copy only the selected real host's bounded movement, pose, disguise and skeleton stream, with small positional offsets. They do not receive NPC authority or simulate attacks/items/objectives. This tool is an optional count test, not the new shared-NPC build.

A practical cadence is: frequent two-PC feature tests, automated four-peer tests after core changes, optional replay slots for rendering/count regressions, and short four-person sessions at integration milestones. The morning four-person movement test can use the preserved Preview 1 regardless of ongoing NPC work.
