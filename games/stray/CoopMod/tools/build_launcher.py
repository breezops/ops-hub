"""Build the one-time launcher bundle and a small, versioned mod update."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from bridge import VERSION as PROTOCOL
from launcher_frontend_build import package_frontend
from build_friend_package import MAIN

ROOT=Path(__file__).resolve().parents[2]
PROJECT=ROOT/'CoopMod'
VERSION='0.2.0'
SERVICE='https://stray-coop-lobby.breezops.chatgpt.site'

def manifest(folder):
    files={p.relative_to(folder).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
           for p in folder.rglob('*') if p.is_file() and p.name!='manifest.json' and '__pycache__' not in p.parts}
    (folder/'manifest.json').write_text(json.dumps({'version':VERSION,'protocol':PROTOCOL,'files':files},indent=2)+'\n')

def main():
    home=PROJECT/'dist/StrayCoop-Launcher'
    if home.exists():raise RuntimeError('Launcher build already exists. Inspect it before rebuilding.')
    release=home/'releases'/VERSION;release.mkdir(parents=True)
    runtime=home/'runtime'
    shutil.copytree(PROJECT/'dist/StrayCoop-Friend-0.1.4/runtime',runtime)
    python=Path(sys.base_prefix)
    shutil.copy2(python/'pythonw.exe',runtime/'pythonw.exe')
    with zipfile.ZipFile(runtime/'stdlib.zip','a',zipfile.ZIP_DEFLATED) as z:
        for p in (python/'Lib/tkinter').rglob('*.py'):
            if '__pycache__' not in p.parts: z.write(p,p.relative_to(python/'Lib').as_posix())
    for name in ('tcl8.6','tk8.6','tcl8'):
        shutil.copytree(python/'tcl'/name,runtime/'tcl'/name)
    loader=home/'loader';loader.mkdir()
    for name in ('dwmapi.dll','UE4SS.dll','UE4SS-settings.ini'):
        shutil.copy2(PROJECT/'dist/StrayCoop-Friend-0.1.4/payload'/name,loader/name)
    tools=release/'tools';tools.mkdir()
    for name in ('bridge.py','cinematics.py','shared_objects.py','preferences.py','remote_session.py','launcher.py','launcher_ui.py','launcher_branding.py','launcher_backend.py','launcher_web.py','lobby_client.py','launcher_update.py'):
        shutil.copy2(PROJECT/'tools'/name,tools/name)
    package_frontend(tools)
    branding=tools/'assets';branding.mkdir()
    for name in ('launcher.ico','StrayCoop.exe'):shutil.copy2(PROJECT/'assets/launcher'/name,branding/name)
    scripts=release/'payload/Mods/StrayCoop/Scripts';scripts.mkdir(parents=True)
    for name in ('movement.lua','animation.lua','interpolation.lua','objects.lua','pause.lua','story.lua','context.lua','cinematics.lua'):
        shutil.copy2(PROJECT/'src/StrayCoop/Scripts'/name,scripts/name)
    (scripts/'main.lua').write_text(MAIN)
    (release/'launcher-config.json').write_text(json.dumps({'version':VERSION,'protocol':PROTOCOL,'runtime':1,'service':SERVICE},indent=2)+'\n')
    (release/'README-FIRST.txt').write_text('''STRAY CO-OP LAUNCHER 0.2.0\n\nExtract the whole launcher ZIP once. Open StrayCoop.exe. Choose your Stray game\nfolder once, then use Host a game or enter your friend's eight-character code.\nThe host currently needs UDP 7777 forwarded. Keep the launcher open while playing.\nThe launcher checks for tested mod updates; use Update & restart when offered.\nBoth players must use the same release. Gameplay currently has the 0.1.4 features\nand limitations: common carryables, bucket/fan puzzle, shared pause and opening.\n\nUse End session to close the owned test game and save diagnostics. Normal player\nsettings are preserved across isolated test saves. This is experimental co-op.\nThis bundle contains no game assets. UE4SS and Python licenses are included.\n''')
    manifest(release)
    for name in ('launcher.ico','StrayCoop.exe'):shutil.copy2(branding/name,home/name)
    (home/'current.json').write_text(json.dumps({'version':VERSION,'previous':None})+'\n')
    shutil.copy2(PROJECT/'vendor/UE4SS-LICENSE.txt',home/'UE4SS-LICENSE.txt')
    shutil.copy2(runtime/'PYTHON-LICENSE.txt',home/'PYTHON-LICENSE.txt')
    shutil.copy2(release/'README-FIRST.txt',home/'README-FIRST.txt')
    update=PROJECT/'dist'/f'StrayCoop-Release-{VERSION}.zip'
    with zipfile.ZipFile(update,'w',zipfile.ZIP_DEFLATED) as z:
        for p in release.rglob('*'):
            if p.is_file():z.write(p,p.relative_to(release).as_posix())
    full=PROJECT/'dist/StrayCoop-Launcher.zip'
    with zipfile.ZipFile(full,'w',zipfile.ZIP_DEFLATED) as z:
        for p in home.rglob('*'):
            if p.is_file():z.write(p,Path(home.name)/p.relative_to(home))
    public=PROJECT/'service/public';downloads=public/'downloads';downloads.mkdir(exist_ok=True)
    for p in (full,update):shutil.copy2(p,downloads/p.name)
    metadata={'version':VERSION,'protocol':PROTOCOL,'runtime':1,'path':'/downloads/'+update.name,'sha256':hashlib.sha256(update.read_bytes()).hexdigest()}
    (public/'release.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps({'launcher':str(home/'StrayCoop.exe'),'full_zip':str(full),'full_bytes':full.stat().st_size,'update_bytes':update.stat().st_size},indent=2))

if __name__=='__main__':main()
