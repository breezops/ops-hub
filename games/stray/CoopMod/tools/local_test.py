"""Run two isolated Stray instances and authenticated loopback bridges.

The second installation must already exist under CoopMod/test-client.
This runner owns and cleans up only processes it starts. No external ports open.
"""
import argparse
import json
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
import re
from preferences import prepare_settings,remember_settings

def command(root, text):
    (root/'Hk_project/Binaries/Win64/Mods/StrayCoop/command.txt').write_text(text+'\n',encoding='ascii')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--duration',type=float,default=1200)
    p.add_argument('--profile',default='',help='Optional isolated test-save profile name')
    p.add_argument('--harness',action='store_true',help='Enable guarded native chapter/checkpoint development commands')
    p.add_argument('--lab-layout',action='store_true',help='Explicitly use small silent windows for automated lab runs')
    args=p.parse_args()
    if args.profile and not re.fullmatch(r'[A-Za-z0-9_-]{1,60}',args.profile):
        p.error('profile must use letters, numbers, hyphens or underscores')
    if args.harness and not args.profile:
        p.error('--harness requires an isolated --profile')
    root=Path(__file__).resolve().parents[2]
    project=root/'CoopMod'
    reports=project/'reports'
    stop_file=reports/('local-stop-'+(args.profile or 'default')+'.txt')
    roots=[root,project/'test-client']
    labels=['host','client']
    children=[]
    streams=[]
    games=[]
    user_data=[]
    # Refuse to mix test sessions with any existing Stray process.
    if sys.platform=='win32':
        processes=subprocess.check_output(['tasklist','/FI','IMAGENAME eq Stray-Win64-Shipping.exe','/FO','CSV'],creationflags=subprocess.CREATE_NO_WINDOW).decode(errors='replace')
        if 'Stray-Win64-Shipping.exe' in processes:
            p.error('Stray is already running; finish the current game session before starting this test.')
    for game in roots:
        if not (game/'Hk_project/Binaries/Win64/Stray-Win64-Shipping.exe').is_file():
            p.error(f'Test executable missing: {game}')
    key=reports/'local-test.key'
    stop_file.unlink(missing_ok=True)
    key.write_text(secrets.token_hex(24),encoding='ascii')
    try:
        for game,label in zip(roots,labels):
            bin=game/'Hk_project/Binaries/Win64'
            scripts=bin/'Mods/StrayCoop/Scripts'
            scripts.mkdir(parents=True,exist_ok=True)
            for source in (project/'src/StrayCoop/Scripts').glob('*.lua'):
                shutil.copy2(source,scripts/source.name)
            command(game,'movement-start')
            (bin/'Mods/StrayCoop/local.tsv').write_text('')
            (bin/'Mods/StrayCoop/remote.tsv').write_text('')
            (bin/'Mods/StrayCoop/story-local.tsv').write_text('')
            (bin/'Mods/StrayCoop/story-remote.tsv').write_text('')
            (bin/'Mods/StrayCoop/pause-local.tsv').write_text('')
            (bin/'Mods/StrayCoop/pause-remote.tsv').write_text('')
            (bin/'Mods/StrayCoop/objects-local.tsv').write_text('')
            (bin/'Mods/StrayCoop/objects-remote.tsv').write_text('')
            (bin/'Mods/StrayCoop/harness-state.tsv').write_text('')
            (bin/'Mods/StrayCoop/harness-catalog.tsv').write_text('')
            stream=(reports/f'animation-test-{label}-bridge.log').open('w',encoding='utf-8')
            streams.append(stream)
            bridge=subprocess.Popen([sys.executable,str(project/'tools/bridge.py'),'host' if label=='host' else 'join',
                '--game-root',str(game),'--bind','127.0.0.1','--key-file',str(key),'--duration',str(args.duration)],
                stdout=stream,stderr=stream,creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            children.append(bridge)
            data=project/('test-host' if label=='host' else 'test-client')/'UserData'
            if args.profile: data=project/'test-profiles'/args.profile/label
            prepare_settings(root,data)
            user_data.append(data)
            extra=[]
            if args.harness:
                token=secrets.token_hex(16)
                (bin/'Mods/StrayCoop/harness-session.txt').write_text(token+'\n',encoding='ascii')
                extra=[f'-StrayCoopHarness={token}']
            display=['-windowed','-ResX=960','-ResY=540','-nosound'] if args.lab_layout else []
            executable=bin/'Stray-Win64-Shipping.exe'
            print(f'Launching {label}: {executable}',flush=True)
            try:
                process=subprocess.Popen([str(executable),'-nosplash',f'-UserDir={data}',*display,*extra],cwd=bin)
            except OSError as exc:
                raise RuntimeError(f'Could not launch {label} game at {executable}: {exc}') from exc
            children.append(process)
            games.append(process)
        manifest={'running':True,'protocol':5,'games':[x.pid for x in games],
                  'bridges':[children[0].pid,children[2].pid],'profile':args.profile,'harness':args.harness}
        (reports/'local-session.json').write_text(json.dumps(manifest,indent=2))
        print('Two-instance test running on loopback. Load the same chapter in both windows.',flush=True)
        start=time.monotonic()
        while time.monotonic()-start<args.duration and all(x.poll() is None for x in children) and not stop_file.exists():
            time.sleep(.5)
    except KeyboardInterrupt:
        pass
    finally:
        for game in roots: command(game,'movement-stop')
        time.sleep(.7)
        for game in roots: command(game,'quit')
        for proc in children:
            try: proc.wait(timeout=4)
            except subprocess.TimeoutExpired: proc.terminate()
        for game,label in zip(roots,labels):
            bin=game/'Hk_project/Binaries/Win64'
            for source,name in [(bin/'UE4SS.log',f'animation-test-{label}.log'),
                                (bin/'Mods/StrayCoop/bridge-status.json',f'animation-test-{label}-status.json')]:
                if source.exists(): shutil.copy2(source,reports/name)
        for stream in streams: stream.close()
        if not args.lab_layout:
            for data in user_data: remember_settings(root,data)
        if args.harness:
            for game in roots:
                (game/'Hk_project/Binaries/Win64/Mods/StrayCoop/harness-session.txt').unlink(missing_ok=True)
        (reports/'local-session.json').write_text(json.dumps({'running':False}))
        stop_file.unlink(missing_ok=True)
        print('Test processes stopped; reports saved.',flush=True)

if __name__=='__main__': main()
