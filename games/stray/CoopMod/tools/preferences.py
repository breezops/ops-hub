"""Carry local player settings between disposable campaign profiles.

Only WindowsNoEditor INI files are copied; save slots never enter this cache.
"""
from __future__ import annotations
import codecs
import os
from pathlib import Path
import shutil

SECTION='[/Script/Hk_project.HKGameUserSettings]'

def settings_dir(user_data):
    return Path(user_data)/'Saved/Config/WindowsNoEditor'

def cache_dir(game_root):
    return Path(game_root)/'StrayCoopTests/Preferences/WindowsNoEditor'

def source_settings(game_root,exclude=None):
    root=Path(game_root)
    cache=cache_dir(root)
    candidates=[cache]
    # Once established, the co-op preferences belong to the test sessions.
    # Playing the normal game must not silently replace this chosen profile.
    if not (cache/'GameUserSettings.ini').is_file():
        candidates.append(root/'Hk_project/Saved/Config/WindowsNoEditor')
        local=os.environ.get('LOCALAPPDATA')
        if local: candidates.append(Path(local)/'Hk_project/Saved/Config/WindowsNoEditor')
    candidates.extend(root.glob('StrayCoopTests/*/UserData/Saved/Config/WindowsNoEditor'))
    valid=[]
    for folder in candidates:
        if exclude and folder.resolve()==Path(exclude).resolve(): continue
        ini=folder/'GameUserSettings.ini'
        if ini.is_file(): valid.append((ini.stat().st_mtime_ns,folder))
    return max(valid,key=lambda item:item[0])[1] if valid else None

def copy_ini(source,target):
    target.mkdir(parents=True,exist_ok=True)
    for file in source.glob('*.ini'):
        if file.is_file(): shutil.copy2(file,target/file.name)

def coop_flags(path):
    """Change only the two required flags; preserve repeated Unreal INI arrays."""
    raw=path.read_bytes() if path.exists() else b''
    encoding='utf-16' if raw.startswith((codecs.BOM_UTF16_LE,codecs.BOM_UTF16_BE)) else ('utf-8-sig' if raw.startswith(codecs.BOM_UTF8) else 'utf-8')
    text=raw.decode(encoding)
    lines=text.splitlines(keepends=True)
    newline='\r\n' if '\r\n' in text else '\n'
    start=next((i for i,line in enumerate(lines) if line.strip().lower()==SECTION.lower()),None)
    if start is None:
        if lines and not lines[-1].endswith(('\n','\r')): lines[-1]+=newline
        lines.extend([newline,SECTION+newline]);start=len(lines)-1
    end=next((i for i in range(start+1,len(lines)) if lines[i].lstrip().startswith('[')),len(lines))
    needed={'autopauseenabled':'AutoPauseEnabled=False','firstlaunch':'FirstLaunch=False'}
    for i in range(start+1,end):
        key=lines[i].partition('=')[0].strip().lower()
        if key in needed: lines[i]=needed.pop(key)+newline
    if end>0 and not lines[end-1].endswith(('\n','\r')): lines[end-1]+=newline
    lines[end:end]=[value+newline for value in needed.values()]
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(''.join(lines).encode(encoding))

def prepare_settings(game_root,user_data):
    target=settings_dir(user_data)
    source=source_settings(game_root,exclude=target)
    if source:
        copy_ini(source,target)
        cache=cache_dir(game_root)
        if source.resolve()!=cache.resolve(): copy_ini(source,cache)
    else: target.mkdir(parents=True,exist_ok=True)
    coop_flags(target/'GameUserSettings.ini')
    return source

def remember_settings(game_root,user_data):
    source=settings_dir(user_data)
    if (source/'GameUserSettings.ini').is_file(): copy_ini(source,cache_dir(game_root))
