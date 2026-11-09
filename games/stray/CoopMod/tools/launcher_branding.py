"""Windows launcher identity and artwork; never modifies the Python runtime."""
import ctypes
from ctypes import wintypes
import hashlib
from pathlib import Path
import shutil
import sys

APP_ID='StrayCoop.Launcher'

def set_process_identity():
    if sys.platform!='win32':return
    shell=ctypes.WinDLL('shell32',use_last_error=True)
    shell.SetCurrentProcessExplicitAppUserModelID.argtypes=[wintypes.LPCWSTR]
    shell.SetCurrentProcessExplicitAppUserModelID.restype=ctypes.c_long
    shell.SetCurrentProcessExplicitAppUserModelID(APP_ID)

def set_window_icon(window,release):
    icon=Path(release)/'tools/assets/launcher.ico'
    if icon.is_file():
        window.iconbitmap(default=str(icon))
        window.iconbitmap(str(icon))

def install_branding(home,release):
    """Install only manifest-verified branding from the selected release.
    The tiny bootstrap has the same launcher code, with the image embedded.
    """
    home=Path(home).resolve();release=Path(release).resolve()
    if release.parent!=home/'releases':return False
    assets=release/'tools/assets'
    if not all((assets/name).is_file() for name in ('launcher.ico','StrayCoop.exe')):return False
    from launcher_update import validate_release
    validate_release(release,release.name)
    backup=home/'branding-backups'
    for name in ('launcher.ico','StrayCoop.exe'):
        source=assets/name;destination=home/name
        if destination.exists() and source.read_bytes()==destination.read_bytes():continue
        if destination.exists():
            backup.mkdir(exist_ok=True)
            tag=hashlib.sha256(destination.read_bytes()).hexdigest()[:16]
            saved=backup/(destination.stem+'-'+tag+destination.suffix)
            if not saved.exists():shutil.copy2(destination,saved)
        pending=home/(name+'.new')
        shutil.copy2(source,pending);pending.replace(destination)
    if sys.platform=='win32':
        notify=ctypes.WinDLL('shell32').SHChangeNotify
        notify.argtypes=[wintypes.LONG,wintypes.UINT,wintypes.LPCWSTR,wintypes.LPCWSTR]
        # Refresh just this executable/icon, without clearing global icon caches.
        for name in ('StrayCoop.exe','launcher.ico'):notify(0x2000,0x0005,str(home/name),None)
    return True
