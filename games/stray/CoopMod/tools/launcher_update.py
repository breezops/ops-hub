"""Stage a verified mod release beside the running release, then switch atomically.

The fixed launcher/runtime paths never change, preserving Windows firewall rules.
"""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tempfile
from urllib.parse import urljoin,urlparse
from urllib.request import Request,urlopen
import zipfile

MAX_DOWNLOAD=50*1024*1024
MAX_EXPANDED=100*1024*1024
VERSION=re.compile(r'^\d+\.\d+\.\d+$')

def safe_name(name):
    p=PurePosixPath(name)
    if not name or '\\' in name or ':' in name or p.is_absolute() or '..' in p.parts: raise ValueError('Unsafe update path.')
    for part in p.parts:
        if part.endswith(('.',' ')) or part.split('.')[0].upper() in {'CON','PRN','AUX','NUL',*[f'COM{i}' for i in range(10)],*[f'LPT{i}' for i in range(10)]}:
            raise ValueError('Unsupported update path.')
    if not p.parts or p.parts[0] not in ('tools','payload','manifest.json','launcher-config.json','README-FIRST.txt'):
        raise ValueError('Update contains an unexpected top-level path.')
    return p

def validate_release(folder,version):
    manifest=json.loads((folder/'manifest.json').read_text())
    if manifest.get('version')!=version or not isinstance(manifest.get('files'),dict): raise ValueError('Update manifest version mismatch.')
    actual={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    if actual!={'manifest.json',*manifest['files']}: raise ValueError('Update files do not match the manifest.')
    for name,expected in manifest['files'].items():
        safe_name(name);file=folder/name
        if file.stat().st_size!=expected['bytes'] or hashlib.sha256(file.read_bytes()).hexdigest()!=expected['sha256']:
            raise ValueError('Update integrity check failed.')
    config=json.loads((folder/'launcher-config.json').read_text())
    if config.get('version')!=version or config.get('runtime')!=1: raise ValueError('A newer launcher installer is required.')
    for name in ('tools/launcher.py','tools/remote_session.py','payload/Mods/StrayCoop/Scripts/main.lua'):
        if name not in actual: raise ValueError('Update is missing required launcher files.')
    return manifest

def stage_archive(home,archive,version,expected_hash):
    home=Path(home).resolve()
    if not VERSION.fullmatch(version): raise ValueError('Invalid release version.')
    if Path(archive).stat().st_size>MAX_DOWNLOAD: raise ValueError('Update download is too large.')
    if hashlib.sha256(Path(archive).read_bytes()).hexdigest()!=expected_hash: raise ValueError('Downloaded update failed its SHA-256 check.')
    releases=home/'releases';releases.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix='.staging-',dir=releases))
    try:
        with zipfile.ZipFile(archive) as z:
            entries=z.infolist()
            if len(entries)>3000 or sum(f.file_size for f in entries)>MAX_EXPANDED: raise ValueError('Update is too large.')
            seen=set()
            for item in entries:
                relative=safe_name(item.filename)
                key=relative.as_posix().casefold()
                if key in seen or stat.S_ISLNK(item.external_attr>>16): raise ValueError('Duplicate path or link in update.')
                seen.add(key)
                target=staging/Path(*relative.parts)
                if not target.resolve().is_relative_to(staging): raise ValueError('Unsafe extraction path.')
                if item.is_dir(): target.mkdir(parents=True,exist_ok=True);continue
                target.parent.mkdir(parents=True,exist_ok=True)
                with z.open(item) as source,target.open('wb') as output: shutil.copyfileobj(source,output)
        validate_release(staging,version)
        destination=releases/version
        if destination.exists():
            validate_release(destination,version)
            if (destination/'manifest.json').read_bytes()!=(staging/'manifest.json').read_bytes(): raise ValueError('A different build already uses this release version.')
        else: staging.rename(destination)
        previous=json.loads((home/'current.json').read_text()).get('version') if (home/'current.json').exists() else None
        pointer=home/'current.new.json'
        pointer.write_text(json.dumps({'version':version,'previous':previous})+'\n')
        pointer.replace(home/'current.json')
        return destination
    finally:
        if staging.exists():
            if not staging.resolve().is_relative_to(releases.resolve()): raise ValueError('Update staging path escaped the release directory.')
            shutil.rmtree(staging)

def download_update(home,base,release,progress=None):
    version=release.get('version','')
    if not VERSION.fullmatch(version) or release.get('runtime')!=1: raise ValueError('A new launcher installer is required for this release.')
    digest=release.get('sha256','');path=release.get('path','')
    if not re.fullmatch('[0-9a-f]{64}',digest) or not path.startswith('/downloads/') or path.startswith('//'):
        raise ValueError('Invalid update metadata.')
    url=urljoin(base,path)
    if urlparse(url).netloc!=urlparse(base).netloc: raise ValueError('Update address changed origin.')
    with tempfile.TemporaryDirectory(prefix='straycoop-update-') as tmp:
        target=Path(tmp)/'update.zip';total=0
        request=Request(url,headers={'User-Agent':'StrayCoopLauncher-Updater/1.0','Accept':'application/zip'})
        with urlopen(request,timeout=45) as response,target.open('wb') as output:
            if urlparse(response.url).netloc!=urlparse(base).netloc: raise ValueError('Update redirected to an unexpected origin.')
            while chunk:=response.read(128*1024):
                total+=len(chunk)
                if total>MAX_DOWNLOAD: raise ValueError('Update download is too large.')
                output.write(chunk)
                if progress: progress(total)
        return stage_archive(home,target,version,digest)
