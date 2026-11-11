# Solo Freeform restart result

Private OPS 0.1.29 ran on Steam HITMAN 3.290.1 with SDK 4.2.1.
The initial load and both restarts admitted the same 83 active NPC IDs.
Every ID had a newer construction stamp on each reload.

| Mission load | Binding scope | Active NPCs | Constructor mark |
| --- | ---: | ---: | ---: |
| Initial | 1 | 83 | 99 |
| First restart | 2 | 83 | 198 |
| Second restart | 3 | 83 | 297 |

The scene-clear return callback recorded 99 matching destructor body
returns and zero pending old observed jobs on each restart. The arbiter
released scope 1 before admitting scope 2 and released scope 2 before
admitting scope 3. The game ran local NPC simulation throughout the test.
