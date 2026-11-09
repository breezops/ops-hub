# Remote movement polish candidate

Source change, not a published launcher release.

Replaces straight-line snapshot position interpolation with monotone cubic
Hermite curves using neighboring sender timestamps. Weighted harmonic tangents
handle uneven packet spacing, preserve constant-speed travel and stop overshoot
at direction reversals and jump apexes. The existing 150ms buffer, bounded
walking prediction, jump phase timeline, rotation wrapping and teleport snaps
remain in place. No network protocol or save/settings changes.

52 Python/Lua tests pass, including three new cases for continuous velocity at
packet boundaries, uneven sample intervals and bounded jump-apex motion.

Native test attempted through checkpoint_harness.py, chapter 4, isolated profiles.
The test runner received Windows PermissionError [WinError 5] while launching a
game executable. Its cleanup completed and local-session.json reports not running.
Evidence: original workspace CoopMod/reports/harness-20261109-225328.
No visual improvement or two-PC result is claimed from the math tests.

Next: restore native test launch access, test walking/running/turning and jumps
in the isolated local pair, then repeat a short two-PC test. Publish only after
that result and resolving the existing launcher update URL migration issue.
