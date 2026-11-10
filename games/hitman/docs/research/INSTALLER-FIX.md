# Installer discovery repair and simplified entry point

November 8, 2026. The first Foundation 1 updater failed before mutating the game because Steam's library list contained disconnected Z: and E: drives. Find-SteamGames used Join-Path on the missing drive with ErrorActionPreference=Stop. Earlier fixture tests passed an explicit game path, so they did not exercise that route.

The repaired source skips unavailable library roots. The convenience wrapper falls back to folder selection on discovery failures instead of terminating. The batch entry point explicitly uses Windows PowerShell and its standard module paths, avoiding inherited PowerShell 7 module-path failures.

START-HERE.cmd now provides Install or update, Remove, and Exit. Automatic mode selects Update when an installation record exists and Install otherwise. Existing INSTALL/UPDATE/REMOVE entry points remain compatible. Future nontechnical users need only Extract All -> START-HERE.cmd -> option 1; the same option handles future packages. Option 2 removes co-op.

Validated actual Steam-library autodetection, actual CMD entry-point preflight, existing-install auto-selection and clean-fixture auto-selection. The repaired updater then successfully updated the real host on R:; its installed DLL matches the Foundation 1 payload, and automatic updates/game-version mismatches remain disabled. No screen capture was used.

Corrected package: deliverables/HITMAN-Coop-Foundation-1-Installer-Fix.zip. Gameplay DLL and protocol are unchanged from Foundation 1; only setup/launcher usability changed. A friend already successfully running Foundation 1 does not need to reinstall just for this repair. A friend still on the original beta must update.
