"""Build an immutable player ZIP and a HITMAN-only website upload directory.

The verified base package supplies the licensed SDK binaries and frozen 4.1
rollback. It is an explicit input, never downloaded from another game's latest.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--base-package', type=Path, required=True)
parser.add_argument('--native-dll', type=Path, required=True)
parser.add_argument('--output', type=Path, default=Path('outputs/release'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
config = json.loads((root/'launcher/updater-config.json').read_text())
version = config['release_version']
out = args.output.resolve()
stage = out/'HITMAN-Coop'
if stage.exists():
    raise SystemExit('Release output already exists; use a new output folder.')
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
base = args.base_package.resolve()
# Verify the entire base payload and exact known rollback before reusing it.
for entry in json.loads((base/'payload-hashes.json').read_text()):
    if digest(base/entry['file']) != entry['sha256']:
        raise SystemExit('Base package integrity check failed: '+entry['file'])
rollback = base/'rollback/HITMAN-Coop-Foundation-4.1.zip'
if digest(rollback) != '988e9c38819e01a98297fffff34c7df7026aaaa132e2946659e926accf40c24f':
    raise SystemExit('Frozen Foundation 4.1 rollback hash mismatch.')
if config['mod_build'].encode() not in args.native_dll.read_bytes():
    raise SystemExit('Native DLL does not identify the configured mod build.')
stage.mkdir(parents=True)
for folder in ('files', 'licenses', 'rollback'):
    shutil.copytree(base/folder, stage/folder)
for file in (root/'distribution').iterdir():
    if file.is_file():
        shutil.copyfile(file, stage/file.name)
shutil.copyfile(args.native_dll, stage/'files/mods/HitmanCoop.dll')
launcher = root/'launcher/bin/HITMAN-Coop-Party-Preview.exe'
for destination in (stage/'HITMAN-Coop-Party-Preview.exe', stage/'files/HITMAN-Coop-Launcher.exe'):
    shutil.copyfile(launcher, destination)
for destination in (stage/'updater-config.json', stage/'files/updater-config.json'):
    shutil.copyfile(root/'launcher/updater-config.json', destination)
(stage/'VERSION.txt').write_text(f"OPS HUB {version} / {config['mod_build']} / party protocol 2 / HITMAN 3.290.1\n")
(stage/'START-HERE.txt').write_text('''HITMAN / CO-OP — OPS HUB

Extract this entire folder once. Open HITMAN-Coop-Party-Preview.exe.
Close HITMAN and click INSTALL / REPAIR for the first installation.
Use CHECK FOR UPDATES, then UPDATE & RESTART when a published release is offered.
The updater verifies the ZIP, runs the tracked installer, then selects the new launcher.
Use the same release on every PC. No GitHub login or developer tools are required.

Load Freeform Training on all PCs. Host/join, ready, then the host starts.
This remains a movement/disguise preview. NPCs/items/objectives/exits are separate.
The host forwards UDP 47047. Guests do not forward ports.
Keep the launcher open. For a damaged avatar, restart the native mission as instructed.

REMOVE MOD removes tracked files while preserving saves. START-HERE.cmd also
offers the verified Foundation 4.1 restore. Previous working archives remain safe.
Updater downloads use the canonical OPS HUB host behind opscord.net. The HITMAN
manifest must first be published by the site owner; a missing channel is reported.
''', encoding='utf-8')
(stage/'BUILD-NOTES.md').write_text((root/'docs/LAUNCHER-AND-UPDATES.md').read_text(), encoding='utf-8')
payload = [{'file': str(p.relative_to(stage)), 'sha256': digest(p)}
           for p in sorted((stage/'files').rglob('*')) if p.is_file()]
(stage/'payload-hashes.json').write_text(json.dumps(payload, indent=2))
(stage/'launcher-mode.json').write_text(json.dumps({'mode':'preview','launcher':launcher.name,'sha256':digest(launcher)}, indent=2))
source = stage/'source';source.mkdir()
sourcezip = source/f'HitmanCoop-OPS-{version}-source.zip'
with zipfile.ZipFile(sourcezip,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for file in sorted(root.rglob('*')):
        relative=file.relative_to(root)
        if file.is_file() and not any(part.startswith('_build') or part in {'_deps','bin','outputs','.git'} for part in relative.parts):
            archive.write(file, Path('hitman')/relative)
downloads=out/'publish/downloads/hitman';downloads.mkdir(parents=True)
package=downloads/f'HITMAN-Coop-{version}.zip'
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for file in sorted(stage.rglob('*')):
        if file.is_file():archive.write(file,Path('HITMAN-Coop')/file.relative_to(stage))
manifest={'schema':1,'project':'hitman','version':version,'mod_build':config['mod_build'],
          'game_version':config['game_version'],'runtime':1,'protocol':2,
          'path':'/downloads/hitman/'+package.name,'sha256':digest(package),'bytes':package.stat().st_size}
channel=out/'publish/hitman';channel.mkdir(parents=True)
(channel/'release.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'release-audit.json').write_text(json.dumps({'source_sha256':digest(sourcezip),
    'dll_sha256':digest(args.native_dll),'launcher_sha256':digest(launcher),'release':manifest},indent=2))
print(json.dumps(manifest,indent=2))
