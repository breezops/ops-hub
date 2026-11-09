"""Windows friend-test launcher; ships with an isolated Python runtime."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import ipaddress
import json
import logging
import os
from pathlib import Path
import re
import secrets
import shutil
import socket
import subprocess
import sys
import threading
import time
sys.dont_write_bytecode=True

sys.path.insert(0,str(Path(__file__).resolve().parent))
from bridge import Bridge
from preferences import prepare_settings,remember_settings

EXE=Path('Hk_project/Binaries/Win64/Stray-Win64-Shipping.exe')
EXPECTED='117c2f28dede757ba32eafff482cdf02d23e15cebb4c57789292905228b2a139'
PACKAGE=Path(__file__).resolve().parents[1]

def digest(path):
    with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

def game_root(value=None):
    if value: root=Path(value.strip().strip('"')).resolve()
    elif (PACKAGE.parent/EXE).is_file(): root=PACKAGE.parent.resolve()
    else: root=Path(input('Paste your Stray game folder (contains Hk_project): ').strip().strip('"')).resolve()
    if not (root/EXE).is_file(): raise ValueError('Stray executable not found in that folder.')
    if digest(root/EXE)!=EXPECTED:
        raise ValueError('Game build differs from the tested build. Nothing was installed. Send the preflight output to the mod developer.')
    return root

def running_game():
    if sys.platform!='win32': return False
    class PROCESSENTRY32W(ctypes.Structure):
        _fields_=[('dwSize',wintypes.DWORD),('cntUsage',wintypes.DWORD),('th32ProcessID',wintypes.DWORD),
                  ('th32DefaultHeapID',ctypes.c_size_t),('th32ModuleID',wintypes.DWORD),('cntThreads',wintypes.DWORD),
                  ('th32ParentProcessID',wintypes.DWORD),('pcPriClassBase',wintypes.LONG),('dwFlags',wintypes.DWORD),
                  ('szExeFile',wintypes.WCHAR*260)]
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.CreateToolhelp32Snapshot.restype=wintypes.HANDLE
    k.Process32FirstW.argtypes=[wintypes.HANDLE,ctypes.POINTER(PROCESSENTRY32W)]
    k.Process32NextW.argtypes=[wintypes.HANDLE,ctypes.POINTER(PROCESSENTRY32W)]
    k.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=k.CreateToolhelp32Snapshot(2,0)
    if handle==ctypes.c_void_p(-1).value: raise OSError('Cannot inspect running game processes')
    try:
        entry=PROCESSENTRY32W();entry.dwSize=ctypes.sizeof(entry)
        ok=k.Process32FirstW(handle,ctypes.byref(entry))
        while ok:
            if entry.szExeFile.lower()=='stray-win64-shipping.exe': return True
            ok=k.Process32NextW(handle,ctypes.byref(entry))
        return False
    finally: k.CloseHandle(handle)

def install(root):
    if running_game(): raise RuntimeError('Close Stray before installing or starting a test.')
    payload=PACKAGE/'payload'
    loader=payload
    if not (loader/'UE4SS.dll').is_file():
        loader=Path(os.environ.get('STRAYCOOP_LAUNCHER_HOME',''))/'loader'
        if not (loader/'UE4SS.dll').is_file(): raise RuntimeError('Launcher loader files are missing. Extract the full launcher download.')
    bin=root/EXE.parent
    for name in ('dwmapi.dll','UE4SS.dll'):
        target=bin/name
        if target.exists() and digest(target)!=digest(loader/name):
            raise RuntimeError(f'{name} belongs to a different loader. Nothing was replaced; resolve the loader conflict first.')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+secrets.token_hex(3)
    backup=root/'StrayCoopTests/Backups'/stamp
    files=[(source,bin/source.relative_to(payload)) for source in payload.rglob('*') if source.is_file()]
    if loader!=payload:
        files.extend((loader/name,bin/name) for name in ('dwmapi.dll','UE4SS.dll','UE4SS-settings.ini'))
    for source,target in files:
        if not target.resolve().is_relative_to(bin.resolve()): raise ValueError('Invalid payload path')
    for source,target in files:
        if target.is_file() and digest(target)==digest(source): continue
        if target.exists():
            saved=backup/target.relative_to(bin);saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,saved)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    mods=bin/'Mods/mods.txt'
    text=mods.read_text(encoding='utf-8-sig') if mods.exists() else ''
    updated=re.sub(r'(?mi)^\s*StrayCoop\s*:\s*\d+\s*$', 'StrayCoop : 1',text)
    if not re.search(r'(?mi)^StrayCoop\s*:',updated): updated=updated.rstrip()+'\nStrayCoop : 1\n'
    if updated!=text:
        if mods.exists():
            saved=backup/'Mods/mods.txt';saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(mods,saved)
        mods.parent.mkdir(parents=True,exist_ok=True);mods.write_text(updated,encoding='utf-8')
    return bin

def local_ip():
    try:
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
            s.connect(('192.0.2.1',9))  # Select route; no datagram is sent.
            return s.getsockname()[0]
    except OSError: return '(your PC LAN address)'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('role',choices=['host','join','preflight'])
    p.add_argument('--game-root')
    p.add_argument('--host')
    p.add_argument('--port',type=int,default=7777)
    p.add_argument('--key-file',type=Path)
    p.add_argument('--stop-file',type=Path)
    p.add_argument('--quiet-key',action='store_true')
    args=p.parse_args()
    if not 1024<=args.port<=65535: p.error('port must be 1024 through 65535')
    root=game_root(args.game_root)
    print('Tested game build: MATCH\nGame folder: '+str(root),flush=True)
    if args.role=='preflight':
        print('Python runtime and game fingerprint passed. No game files changed.');return
    bin=install(root)
    mod=bin/'Mods/StrayCoop'
    for name in ('local.tsv','remote.tsv','story-local.tsv','story-remote.tsv','pause-local.tsv','pause-remote.tsv','objects-local.tsv','objects-remote.tsv','cinematic-local.tsv','cinematic-remote.tsv'):
        (mod/name).write_text('',encoding='ascii')
    host='127.0.0.1'
    if args.role=='host':
        key=args.key_file.read_text().strip() if args.key_file else secrets.token_hex(16)
        if not args.quiet_key:
            print(f'\nForward UDP {args.port} to {local_ip()}:{args.port}.')
            print('Tell your friend your public IPv4 address and this session key:\n\n'+key+'\n')
    else:
        host=str(ipaddress.IPv4Address(args.host or input('Host public IPv4 (or LAN/VPN IPv4): ').strip()))
        key=args.key_file.read_text().strip() if args.key_file else input('Session key from host: ').strip()
    if not 16<=len(key)<=256: raise ValueError('Session key must be 16 through 256 characters.')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+secrets.token_hex(3)
    profile=root/'StrayCoopTests'/stamp
    profile.mkdir(parents=True)
    preferences=prepare_settings(root,profile/'UserData')
    print('Player settings: '+(str(preferences) if preferences else 'first launch; settings will be remembered'),flush=True)
    logging.basicConfig(level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s',
        handlers=[logging.FileHandler(profile/'transport.log',encoding='utf-8')])
    bridge=Bridge(args.role,mod,key,host=host,port=args.port,bind='0.0.0.0')
    thread=threading.Thread(target=bridge.run,daemon=True)
    thread.start()
    game=None
    try:
        print('Waiting for the other PC. Leave this window open. Press Ctrl+C to stop.',flush=True)
        deadline=time.monotonic()+300
        while not bridge.connected(time.monotonic()):
            if args.stop_file and args.stop_file.exists(): return
            if not thread.is_alive(): raise RuntimeError('Transport stopped. Check '+str(profile/'transport.log'))
            if time.monotonic()>deadline: raise RuntimeError('No connection in five minutes. Check the address, session key, and UDP forwarding.')
            time.sleep(.2)
        print('CONNECTED. Starting Stray with a fresh isolated test save.\n'
              'Both players: START GAME > SLOT 1 > NEW GAME. Coordinate over voice.\n'
              'Test movement, then split the opening NPC interactions and let either player sleep.\n'
              'Movement, shared pause, carryables and the opening/bucket-fan adapters are supported.\n',flush=True)
        game=subprocess.Popen([str(bin/EXE.name),'-nosplash',f'-UserDir={profile/"UserData"}'],cwd=bin)
        connected=True
        next_performance=time.monotonic()+10
        while game.poll() is None:
            if args.stop_file and args.stop_file.exists(): break
            current=bridge.connected(time.monotonic())
            if current!=connected:
                print('Partner reconnected.' if current else 'Partner disconnected; waiting for reconnection.',flush=True)
                connected=current
            if not thread.is_alive(): raise RuntimeError('Transport stopped unexpectedly.')
            if time.monotonic()>=next_performance:
                next_performance=time.monotonic()+10
                try:
                    fields=(mod/'performance.tsv').read_text().strip().split('\t')
                    if len(fields)==9 and fields[:2]==['PERF','2']:
                        print(f'Game frame rate: {float(fields[8]):.1f} FPS; median mod update: {float(fields[3]):.2f} ms',flush=True)
                except (OSError,ValueError): pass
            time.sleep(.3)
    finally:
        if game and game.poll() is None:
            (mod/'command.txt').write_text('quit\n',encoding='ascii')
            try: game.wait(timeout=5)
            except subprocess.TimeoutExpired: game.terminate()
        if game: remember_settings(root,profile/'UserData')
        for name in ('local.tsv','remote.tsv','story-local.tsv','story-remote.tsv','performance.tsv','bridge-status.json','objects-local.tsv','objects-remote.tsv','cinematic-local.tsv','cinematic-remote.tsv'):
            source=mod/name
            if source.exists(): shutil.copy2(source,profile/('last-live-'+name))
        bridge.running=False;thread.join(timeout=2)
        for name in ('UE4SS.log','Mods/StrayCoop/bridge-status.json'):
            source=bin/name
            if source.exists(): shutil.copy2(source,profile/source.name)
        print('Test ended. Diagnostics: '+str(profile),flush=True)

if __name__=='__main__':
    try: main()
    except KeyboardInterrupt: print('\nStopped.')
    except Exception as exc:
        print('\nERROR: '+str(exc),file=sys.stderr,flush=True)
        sys.exit(1)
