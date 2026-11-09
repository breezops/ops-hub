"""Own one isolated Stray process and exercise native pause without UI input."""
from pathlib import Path
import json,secrets,shutil,subprocess,time
from checkpoint_harness import read_state
ROOT=Path(__file__).resolve().parents[2]
BIN=ROOT/'Hk_project/Binaries/Win64'
MOD=BIN/'Mods/StrayCoop'

def main():
    tasks=subprocess.check_output(['tasklist','/FI','IMAGENAME eq Stray-Win64-Shipping.exe','/FO','CSV'],creationflags=subprocess.CREATE_NO_WINDOW).decode(errors='replace')
    if 'Stray-Win64-Shipping.exe' in tasks: raise RuntimeError('Existing game left untouched; close before native test')
    profile=ROOT/'CoopMod/test-profiles'/('pause-'+time.strftime('%Y%m%d-%H%M%S'))
    profile.mkdir(parents=True)
    backup=profile/'scripts-before-test'
    shutil.copytree(MOD/'Scripts',backup)
    token=secrets.token_hex(16)
    for source in (ROOT/'CoopMod/src/StrayCoop/Scripts').glob('*.lua'):
        shutil.copy2(source,MOD/'Scripts'/source.name)
    (MOD/'harness-session.txt').write_text(token+'\n')
    for name in ('pause-local.tsv','pause-remote.tsv','harness-state.tsv'): (MOD/name).write_text('')
    def command(text): (MOD/'command.txt').write_text(text+'\n')
    command('movement-start')
    game=None
    results=[]
    remote=None
    seq=0
    def wait(predicate,label,timeout=45):
        nonlocal seq
        deadline=time.monotonic()+timeout
        next_inspect=0
        while time.monotonic()<deadline:
            if game.poll() is not None: raise RuntimeError('Game exited during '+label)
            if remote is not None:
                seq+=1
                path=MOD/'pause-remote.tsv'
                temp=path.with_suffix('.tmp')
                temp.write_text(f'PAUSE\t{seq}\t1\t0\t{remote}\n')
                try: temp.replace(path)
                except PermissionError: pass
            if time.monotonic()>next_inspect:
                command('harness inspect');next_inspect=time.monotonic()+.8
            try:
                state=read_state(MOD/'harness-state.tsv')
                if abs(time.time()-state['timestamp'])<2 and predicate(state):
                    results.append({'test':label,'state':state})
                    print('PASS '+label,flush=True)
                    return state
            except (OSError,ValueError,KeyError): pass
            time.sleep(.15)
        raise RuntimeError('Timed out: '+label)
    def action(text):
        command(text);time.sleep(.8)
    try:
        game=subprocess.Popen([str(BIN/'Stray-Win64-Shipping.exe'),'-nosplash','-windowed','-ResX=960','-ResY=540','-nosound',f'-UserDir={profile/"UserData"}',f'-StrayCoopHarness={token}'],cwd=BIN)
        time.sleep(4)
        wait(lambda s:s['chapter']==0,'fresh menu')
        action('movement-start')
        wait(lambda s:'pause_request' in s,'shared pause module ready')
        action('harness open 3')
        wait(lambda s:s['chapter']==3 and 'player' in s and s['waiting']=='false','native chapter loaded')
        action('harness resume')
        wait(lambda s:s['paused']=='false','initial simulation running')
        if (ROOT/'CoopMod/reports/pause-ui-hold.txt').exists():
            print('UI HOLD: isolated game ready',flush=True)
            while (ROOT/'CoopMod/reports/pause-ui-hold.txt').exists(): time.sleep(.5)
        action('harness pause-on')
        wait(lambda s:s.get('pause_request')=='true' and s['paused']=='true','local pause captured and applied')
        remote=1
        wait(lambda s:s.get('pause_remote')=='true' and s['paused']=='true','both players paused')
        action('harness pause-off')
        wait(lambda s:s.get('pause_request')=='false' and s.get('pause_remote')=='true' and s['paused']=='true','local resume cannot override peer')
        remote=0
        wait(lambda s:s.get('pause_remote')=='false' and s['paused']=='false','both released resumes simulation')
        remote=1
        wait(lambda s:s.get('pause_remote')=='true' and s['paused']=='true','peer-only pause')
        action('harness pause-on')
        wait(lambda s:s.get('pause_request')=='true' and s['paused']=='true','open local menu while peer paused')
        action('harness pause-off')
        wait(lambda s:s.get('pause_request')=='false' and s['paused']=='true','close local menu while peer paused')
        remote=None
        wait(lambda s:s.get('pause_remote')=='false' and s['paused']=='false','stale bridge recovery',timeout=10)
    finally:
        if game and game.poll() is None:
            command('quit')
            try: game.wait(timeout=6)
            except subprocess.TimeoutExpired: game.terminate();game.wait(timeout=5)
        if (BIN/'UE4SS.log').exists(): shutil.copy2(BIN/'UE4SS.log',profile/'UE4SS.log')
        (profile/'results.json').write_text(json.dumps(results,indent=2))
        (MOD/'harness-session.txt').unlink(missing_ok=True)
        for source in backup.glob('*.lua'): shutil.copy2(source,MOD/'Scripts'/source.name)
        print('Evidence: '+str(profile),flush=True)

if __name__=='__main__': main()
