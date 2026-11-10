# Clean installation audit — November 8, 2026

Game: `R:\SteamLibrary\steamapps\common\HITMAN 3`

Executable: 3.290.1.0. Steam BuildID: 26301025.

## Result

Steam's Installed Files screen reported **All 228 files successfully validated** after a complete integrity check. The installation inventory also contained 228 files. No recognizable extra mod loader, mod configuration, mod archive or mod framework was found inside this game directory.

One old external mod configuration was quarantined:

- Original: `C:\Users\deeds\AppData\Roaming\PeacockProject\peacock_patcher2.conf`
- Backup: `quarantine/2026-11-08/PeacockProject/peacock_patcher2.conf`
- SHA-256 before and after move: `D768253A25C6DB25ECC06B9881986143B8E8D57B9F1C3BA930A891889EFD3309`

It pointed the Peacock patcher at a local server. The configuration alone does not patch the game, and no Peacock/LocalGhost patcher was observed running. The source PeacockProject directory is now empty. No game saves or progression were removed.

## Checked locations and mechanisms

- Full game root, Retail, Retail/D3D12 and Runtime file/directory inventory.
- SDK/ASI/proxy-loader signatures such as dinput8.dll, ZHMModSDK, mods.ini, mods directories, dxgi/ReShade, winhttp.dll and version.dll; none found.
- Runtime extra directories, nonstandard archive names and higher-numbered patch archives; none found. Existing patch1–patch4 files were retained and verified by Steam.
- Steam integrity verification covers original binaries, packagedefinition.txt, thumbs.dat and original archives, including any in-place edits.
- Steam's game-specific custom launch-options field: empty.
- Desktop shortcut: normal `steam://rungameid/1659040` launch.
- Standard Simple Mod Framework locations under Local/Roaming AppData and the game directory: absent.
- Standard Vortex HITMAN staging locations under Roaming AppData: absent.
- IO Interactive Local/Roaming data: shader caches, temporary storage and old Epic profile saves were observed; retained.
- Standard Epic HITMAN installation paths checked: absent.
- Downloads/Desktop matching mod/framework/Peacock filenames: no matching downloaded mod files; only the ordinary Steam game shortcut.
- Top-level game libraries and common user locations searched for HITMAN/Peacock/framework folder names.
- No HITMAN/Peacock/LocalGhost/ZHM-named running process observed via available process listing.
- Hosts file: no HITMAN/IOI/Peacock entry matched.
- User startup folder and matching user Run-key entries: no matching mod auto-start entry found.

This establishes a verified clean baseline for the current Steam installation and checked loading paths. It is not a claim that every drive was exhaustively searched for unrelated dormant mod downloads. General overlays and graphics drivers were not removed.

## Evidence

- `checks/steam-clean-validation.txt`: Steam's app-specific validation log excerpt.
- `checks/clean-install-file-inventory.json`: full installation file names and sizes.
- `checks/pre-clean-file-inventory.json`: non-RPKG inventory before cleanup.
- `checks/clean-verified-key-hashes.json`: SHA-256 for executable, packagedefinition.txt and thumbs.dat after verification.
- `quarantine/2026-11-08/peacock-original-hash.json`: original configuration path and hash.

No co-op package has been installed. Next step is a vanilla mission launch before staging a matching co-op test build.
