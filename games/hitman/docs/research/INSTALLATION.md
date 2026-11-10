# Co-op test installation — November 8, 2026

The user confirmed they loaded into Freeform Training on the clean, unmodded installation. This is a successful vanilla launch/mission-load baseline; it does not establish offline-mode or co-op functionality.

HITMAN was confirmed closed before installing.

## Installed

- Game: `R:\SteamLibrary\steamapps\common\HITMAN 3`, version 3.290.1.0.
- Co-op package: HITMAN Co-op 0.13.94 beta, current-game 3.290.x payload.
- Download: https://github.com/JordanRodrigues021/hitman-coop/releases/download/v0.13.94-beta/HitmanCoop-0.13.94-beta.zip
- Package SHA-256: `3fbef00a787425307c7f695b5753fd1e5bbf4b852d18ab99e73d5d9961dbb3c6`, matched the published release hash.
- Installer matched the inspected beta source after normalizing line endings.
- Installer invoked with the explicit game path, `-Quiet -NoFirewall`.
- All six installed payload files matched the verified package by hash.
- Original HITMAN3.exe, packagedefinition.txt and thumbs.dat hashes still match the verified vanilla baseline.

Payload added under Retail: crashpad_handler.exe, dinput8.dll, discord_game_sdk.dll, ResourceLib_HM3.dll, ZHMModSDK.dll, mods/HitmanCoop.dll. The installer created mods.ini and its installation manifest/log.

Additional test settings were created in `Retail\mods\HitmanCoop.ini`:

```ini
[general]
auto_update=false
any_game_version=false
port=47047

[log]
level=debug
show_addresses=false
```

No firewall rule was changed, no port-forward was configured, and no game was launched by the assistant. Screen capture remains unused for this phase.

## Next check

User relaunches normally and presses the tilde/backtick key below Escape. Confirm the SDK loads and CO-OP appears. If a mod-selection window appears, HitmanCoop should be enabled. Then load Freeform Training and inspect `Retail\mods\HitmanCoop.log` for compatibility/patch errors without screenshots.

Both players need the same beta and compatible matching game build. The friend can use the linked package; disable automatic updates on their side too. Direct host connection uses UDP 47047. Host-side inbound firewall access and router forwarding still need to be configured/verified before the two-PC connection test.

## Modded launch confirmed

The user provided a screenshot showing the CO-OP tab, then confirmed entering Freeform Training. Local log inspection confirmed co-op 0.13.94, game 3.290.1, ZHMModSDK 4.2.1, protocol 16, all six game patches applied, and the engine check reporting everything required present. The Facility mission scene loaded and the pose hook installed; subsequent performance lines report being in a mission. No startup/compatibility error appeared in the inspected log. SDK configuration records crash reporting disabled.

No session/peer connection is established yet in the inspected log. Two-PC play, AI awareness, mission completion, and Steam Offline Mode remain pending.
