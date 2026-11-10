"""Prepare a tested mod release for the hosted updater; publish the Site afterward."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile
from build_friend_package import MAIN
from build_launcher import ROOT,PROJECT,SERVICE
from bridge import VERSION as PROTOCOL
from launcher_frontend_build import package_frontend

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--version',required=True);args=parser.parse_args()
    version=args.version
    if not re.fullmatch(r'\d+\.\d+\.\d+',version):parser.error('Use a three-part version number')
    home=PROJECT/'dist/StrayCoop-Launcher'
    release=PROJECT/'dist/prepared-releases'/version
    if release.exists():raise RuntimeError('This prepared release already exists; inspect before replacing it.')
    tools=release/'tools';tools.mkdir(parents=True)
    for name in ('bridge.py','cooperative.py','cinematics.py','shared_objects.py','preferences.py','remote_session.py','launcher.py','launcher_ui.py','launcher_branding.py','launcher_backend.py','launcher_web.py','lobby_client.py','launcher_update.py'):
        shutil.copy2(PROJECT/'tools'/name,tools/name)
    package_frontend(tools)
    branding=tools/'assets';branding.mkdir()
    for name in ('launcher.ico','StrayCoop.exe'):shutil.copy2(PROJECT/'assets/launcher'/name,branding/name)
    scripts=release/'payload/Mods/StrayCoop/Scripts';scripts.mkdir(parents=True)
    for name in ('movement.lua','cooperative.lua','animation.lua','interpolation.lua','objects.lua','pause.lua','story.lua','context.lua','cinematics.lua'):
        shutil.copy2(PROJECT/'src/StrayCoop/Scripts'/name,scripts/name)
    (scripts/'main.lua').write_text(MAIN)
    (release/'launcher-config.json').write_text(json.dumps({'version':version,'protocol':PROTOCOL,'runtime':1,'service':SERVICE},indent=2)+'\n')
    (release/'README-FIRST.txt').write_text('STRAY CO-OP '+version+' - protocol '+str(PROTOCOL)+'\n\n'
        'The launcher uses a frameless HTML interface with local fonts, custom\n'
        'drag/minimize/close controls, session codes and a live diagnostic feed.\n'
        'It uses the installed Microsoft WebView2 runtime. PCs without WebView2\n'
        'retain the standard native interface; no installer is forced.\n\n'
        'Existing users: close Stray on both PCs, click Check for updates, then\n'
        'Update & restart on each launcher. No manual ZIP extraction is needed.\n\n'
        'Hold Space for 0.8 seconds during a cinematic to request a shared skip.\n'
        'Scripted scenes fast-forward through their native completion callbacks.\n'
        'After skipping, each game waits for the other to finish before resuming.\n'
        'Normal speed is restored on completion, disconnect, pause or timeout.\n\n'
        'Remote standing now uses a full-body idle rather than an additive clip.\n'
        'Movement uses sender timestamps, a 150ms buffer and short correction\n'
        'smoothing. Animation clips are approximate; full skeletal pose mirroring\n'
        'and complete campaign synchronization remain work in progress.\n\n'
        'Open StrayCoop.exe, host a game and share the eight-character code. The\n'
        'host currently needs UDP 7777 forwarded. Keep the launcher open. It starts\n'
        'Stray automatically after connection. End session saves diagnostics.\n'
        'Your settings carry between isolated test profiles; normal campaign saves\n'
        'are not modified. Both players must use the same version/protocol.\n'
        'This package contains no game assets.\n')
    files={p.relative_to(release).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in release.rglob('*') if p.is_file()}
    (release/'manifest.json').write_text(json.dumps({'version':version,'protocol':PROTOCOL,'files':files},indent=2)+'\n')
    update=PROJECT/'dist'/f'StrayCoop-Release-{version}.zip'
    with zipfile.ZipFile(update,'w',zipfile.ZIP_DEFLATED) as z:
        for p in release.rglob('*'):
            if p.is_file():z.write(p,p.relative_to(release))
    full=PROJECT/'dist/StrayCoop-Launcher.zip'
    with zipfile.ZipFile(full,'w',zipfile.ZIP_DEFLATED) as z:
        for p in home.rglob('*'):
            rel=p.relative_to(home)
            if not p.is_file() or rel.parts[0] in ('releases','branding-backups') or rel.as_posix() in ('current.json','README-FIRST.txt','StrayCoop.exe','launcher.ico') or '__pycache__' in rel.parts:continue
            z.write(p,Path(home.name)/rel)
        for name in ('launcher.ico','StrayCoop.exe'):z.write(branding/name,Path(home.name)/name)
        z.writestr(home.name+'/current.json',json.dumps({'version':version,'previous':None})+'\n')
        z.write(release/'README-FIRST.txt',home.name+'/README-FIRST.txt')
        for p in release.rglob('*'):
            if p.is_file():z.write(p,Path(home.name)/'releases'/version/p.relative_to(release))
    public=PROJECT/'service/public';downloads=public/'downloads'
    for p in (full,update):shutil.copy2(p,downloads/p.name)
    metadata={'version':version,'protocol':PROTOCOL,'runtime':1,'path':'/downloads/'+update.name,'sha256':hashlib.sha256(update.read_bytes()).hexdigest()}
    (public/'release.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps({'version':version,'release':str(release),'update_bytes':update.stat().st_size,'full_bytes':full.stat().st_size},indent=2))

if __name__=='__main__':main()
