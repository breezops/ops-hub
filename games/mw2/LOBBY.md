# Launcher lobby implementation

The desktop UI remains native Win32/C++ and uses Windows' built-in WinHTTP and
JSON runtime. Network requests run on background threads; the UI collects results
on its timer. A request never writes game memory or starts a gameplay session.
Windows Runtime initialization remains alive for the application lifetime so
cached JSON factories remain valid across successive requests.

The separate Worker handler `website/lib/mw2-lobby.mjs` owns `/mw2/api/*` and
uses the site's existing D1 binding. It creates only the namespaced
`mw2_launcher_rooms` and `mw2_launcher_limits` tables. Existing Stray and HITMAN
routes, records, release channels, bindings and access settings are unchanged.

The room has four slots, one host, one mission and one difficulty. A six-letter
invitation admits a player; authenticated operations use a random 256-bit member
token. The server stores SHA-256 hashes, not plaintext member credentials. Room
reads require membership; there is no public room directory. Tokens are held
only in native process memory and are never exported in diagnostics.

Only the host can patch match settings or close the room. Every room mutation
uses an optimistic revision check in SQLite/D1; simultaneous joins cannot
overwrite a previously admitted player or exceed four slots. Repeating a join
with the same member token is idempotent. Guest heartbeats do not prolong a room
whose host has gone away. Guests time out after 45 seconds, rooms after 90
seconds without the host. Explicit host leave deletes the room. Expired rooms
and rate buckets are cleaned up on new room creation.

Requests have bounded bodies and responses, validated names/settings/protocol,
per-IP hashed rate limits, member authorization, and browser origin checks.
The production native endpoint is fixed to OPS HUB HTTPS, with normal certificate
validation and redirects disabled. A developer override accepts only HTTP on
127.0.0.1. Native errors never include member tokens in logs.

The service-round-trip metric is labelled SERVICE; it is not a measured
player-to-player or game ping. Online indicators come from actual service
responses and heartbeat freshness. UI controls and artwork do not imply that
the four-player game engine integration exists.

## Tests

```powershell
node --test website/lib/mw2-lobby.test.mjs
./games/mw2/launcher/build.ps1
./games/mw2/tools/test-launcher.ps1 -GamePath '<your MW2 folder>'
./games/mw2/launcher/bin/MW2-SpecOps4-Lobby-Check.exe --base http://127.0.0.1:5184
```

The service tests execute real SQL through Node's built-in SQLite adapter and
cover simultaneous joins, permissions, shared settings, duplicate joins, expiry,
closure, validation, hashed identities, rate limits and catalog/protocol matching.
The native network test uses four separate random member identities against the
real local Worker/D1 emulator, then closes its temporary room. Omitting `--base`
targets production and should be used only by the maintainer for a bounded smoke
test. It does not upload personal names, game files or diagnostic reports.

For local UI QA, run the built Worker with local D1 and test-only configuration:

```powershell
npm run start -- --port 5184 --var LOBBY_RATE_SALT:local-test-only --var DEV_ALLOW_LOOPBACK:1
```

Then run the launcher with `--test-lobby-base http://127.0.0.1:5184`.
The regular player launch always uses production HTTPS. No test-only flag or
loopback setting is added to the production hosting manifest or environment.

In-game four-client play, peer gameplay connectivity, launching a mission from
this selection, and cross-PC gameplay synchronization have not been tested or
implemented. The launcher intentionally keeps LAUNCH GAME disabled.
