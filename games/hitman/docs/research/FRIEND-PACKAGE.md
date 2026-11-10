# Friend installer package

Deliverable: `deliverables/HITMAN-Coop-Friend-0.13.94.zip` (~25 MB).

Extract All, open HITMAN-Coop-Friend, run INSTALL.cmd. UPDATE.cmd installs the build inside that package over a tracked prior installation; future builds should be extracted separately and updated from the new package. REMOVE.cmd invokes the tracked upstream removal with validated paths, including when the game version has changed. Compatibility aliases Install.bat and Uninstall.bat point to the same wrapper.

This is a private-test convenience wrapper around Jordan Rodrigues's 0.13.94 beta, not a new multiplayer implementation. It includes upstream payloads, installer, licenses, original co-op source archive and wrapper source. No game executable, player saves, public host address or credentials are in the deliverable.

The wrapper checks payload hashes and requires game 3.290.1 for installation/update. It uses the upstream install manifest/backups and adds a tracked settings file. Updates retain existing co-op preferences while pinning automatic updates off and game-version mismatches off. Removal preserves other mods via upstream's handling; previously backed-up settings may remain restored as inactive configuration. This does not implement a remote automatic updater. Future custom packages must retain the tracking format or supply a migration.

Validation performed in isolated workspace fixtures (no changes to the live host game):

- Script parses in PowerShell and runs under Windows PowerShell 5.1.
- Correct real game version and payload preflight succeeds without mutation.
- Wrong game folder rejected.
- Changed payload rejected before mutation.
- Clean fixture: installation, preference changes, update, removal all succeed; co-op DLL and added loader binaries removed; original fixture executable unchanged.
- Existing-mod fixture: prior loader files restored and unrelated mod DLL/section preserved after removal.
- Incompatible game version blocks an update.
- Removal still succeeds after a simulated game-version change.
- Final ZIP extracted and revalidated before delivery.

This validates packaging and installer lifecycle; it does not certify gameplay or the friend's permissions/environment. The live host is still waiting in Freeform Training, and two-player behavior remains untested.
