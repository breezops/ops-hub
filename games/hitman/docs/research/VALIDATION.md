# Validation performed November 8, 2026

No game files were changed, no mod installer was executed, and no real-player in-game test was performed.

| Check | Result | Scope |
| --- | --- | --- |
| Co-op main source, engine-independent C++ core | 81 tests passed, 0 failures | Protocol, simulated two-world sessions, ownership, pause/restart, events, items, loss/jitter handling and local UDP loopback. |
| Co-op beta source, engine-independent C++ core | 92 tests passed, 0 failures | Same general layer plus beta coverage. |
| Main Python relay logic | 13 tests passed | Fake transport; not a public server deployment or internet connectivity test. |
| Beta Python relay logic | 13 tests passed | Fake transport; not two real clients. |
| Game plugin compilation | Not attempted | Upstream requires VS2022/C++23; inspected machine has VS2019 Build Tools. Core builds with VS2019. |
| Steam Offline Mode game launch | Pending | Must verify with the actual installed copy on each PC. |
| Two-player guard perception / damage | Pending | Main architecture risk. |
| Two-PC objective completion / extraction / restart | Pending | Required before declaring Freeform Training compatible. |

Core builds used the existing VS2019 compiler through its bundled CMake. MSBuild did not progress in the restricted process environment, so the two builds were stopped and rerun successfully with reviewed external process access. The first stable test run in the sandbox failed only its actual UDP-loopback case; the external rerun passed all 81. Do not treat that initial environment-specific result as an upstream gameplay defect.

Raw core outputs:

- `checks/stable-core-tests.txt`: initial sandbox run, 80/81 passed.
- `checks/stable-core-tests-unsandboxed.txt`: completed external run, 81/81 passed.
- `checks/beta-core-tests.txt`: completed external run, 92/92 passed.

Build products under `checks/*-core/Release` include upstream's connection-test executable. These are diagnostic tools, not a complete in-game co-op installation. Both source snapshots' relay tests were invoked as `python -I server/test_relay.py` from their respective directories and reported 13 successful tests.

See PLAN.md for pinned inspected commit IDs and TEST-MATRIX.md for the upcoming game session.
