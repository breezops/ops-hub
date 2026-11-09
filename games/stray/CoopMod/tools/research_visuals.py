"""Owned isolated game for bounded visual/cinematic research; never normal saves."""
from pathlib import Path
import secrets,shutil,subprocess,time
from preferences import prepare_settings
ROOT=Path(__file__).resolve().parents[2];BIN=ROOT/'Hk_project/Binaries/Win64';MOD=BIN/'Mods/StrayCoop'
def main():
    tasks=subprocess.check_output(['tasklist','/FI','IMAGENAME eq Stray-Win64-Shipping.exe','/FO','CSV'],creationflags=subprocess.CREATE_NO_WINDOW).decode(errors='replace')
    if 'Stray-Win64-Shipping.exe' in tasks: raise RuntimeError('Existing game left untouched')
    folder=ROOT/'CoopMod/test-profiles'/('visuals-'+time.strftime('%Y%m%d-%H%M%S'));folder.mkdir(parents=True)
    backup=folder/'scripts-before-test';shutil.copytree(MOD/'Scripts',backup)
    prepare_settings(ROOT,folder/'UserData')
    for src in (ROOT/'CoopMod/src/StrayCoop/Scripts').glob('*.lua'): shutil.copy2(src,MOD/'Scripts'/src.name)
    nonce=secrets.token_hex(16);(MOD/'harness-session.txt').write_text(nonce+'\n')
    (MOD/'command.txt').write_text('');stop=folder/'stop.txt'
    game=None
    try:
        game=subprocess.Popen([str(BIN/'Stray-Win64-Shipping.exe'),'-nosplash','-windowed','-ResX=960','-ResY=540','-nosound',f'-UserDir={folder/"UserData"}',f'-StrayCoopHarness={nonce}'],cwd=BIN)
        print(str(folder),flush=True)
        deadline=time.monotonic()+900
        while game.poll() is None and not stop.exists() and time.monotonic()<deadline: time.sleep(.5)
    finally:
        if game and game.poll() is None:
            (MOD/'command.txt').write_text('quit\n')
            try: game.wait(timeout=6)
            except subprocess.TimeoutExpired: game.terminate();game.wait(timeout=5)
        shutil.copy2(BIN/'UE4SS.log',folder/'UE4SS.log')
        for name in ('visual-probe.tsv','cinematic-local.tsv','cinematic-remote.tsv','performance.tsv'):
            if (MOD/name).exists(): shutil.copy2(MOD/name,folder/name)
        for src in backup.glob('*.lua'): shutil.copy2(src,MOD/'Scripts'/src.name)
        (MOD/'harness-session.txt').unlink(missing_ok=True)
if __name__=='__main__': main()
