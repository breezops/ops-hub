"""Exercise native carryables and the bucket/fan adapter in isolated chapters."""
from pathlib import Path
import copy,json,secrets,shutil,subprocess,time
from checkpoint_harness import read_state
from shared_objects import Authority,read_objects,read_effects,render_tsv
from preferences import prepare_settings
ROOT=Path(__file__).resolve().parents[2];BIN=ROOT/'Hk_project/Binaries/Win64';MOD=BIN/'Mods/StrayCoop'

def fixture(mode):
    tasks=subprocess.check_output(['tasklist','/FI','IMAGENAME eq Stray-Win64-Shipping.exe','/FO','CSV'],creationflags=subprocess.CREATE_NO_WINDOW).decode(errors='replace')
    if 'Stray-Win64-Shipping.exe' in tasks: raise RuntimeError('Existing Stray left untouched')
    folder=ROOT/'CoopMod/test-profiles'/('objects-'+mode+'-'+time.strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True);backup=folder/'scripts-before-test';shutil.copytree(MOD/'Scripts',backup)
    prepare_settings(ROOT,folder/'UserData')
    for source in (ROOT/'CoopMod/src/StrayCoop/Scripts').glob('*.lua'): shutil.copy2(source,MOD/'Scripts'/source.name)
    token=secrets.token_hex(16);(MOD/'harness-session.txt').write_text(token+'\n')
    for name in ('objects-local.tsv','objects-remote.tsv','objects-probe.tsv','harness-state.tsv','command.txt'): (MOD/name).write_text('')
    game=None;results=[];remote=None;generation=0;fan_effect=None
    def command(s): (MOD/'command.txt').write_text(s+'\n')
    def action(s): command(s);time.sleep(.8)
    def update_remote():
        nonlocal generation
        if remote is not None:
            generation+=1
            text=render_tsv(remote,1,generation,True)
            if fan_effect: text+=f'FAN\t{fan_effect[0]}\t{fan_effect[1]}\n'
            f=MOD/'objects-remote.tsv';temp=f.with_suffix('.tmp');temp.write_text(text)
            try: temp.replace(f)
            except PermissionError: pass
    def wait(fn,label,probe=False,timeout=30):
        deadline=time.monotonic()+timeout;next_probe=0
        while time.monotonic()<deadline:
            if game.poll() is not None: raise RuntimeError('Game exited during '+label)
            update_remote()
            if probe and time.monotonic()>next_probe: command('objects-probe');next_probe=time.monotonic()+.6
            try:
                value=fn()
                if value:
                    results.append(label);print('PASS '+mode+': '+label,flush=True);return value
            except (OSError,ValueError,KeyError,IndexError): pass
            time.sleep(.1)
        raise RuntimeError('Timed out: '+label)
    def probe():
        return [line.split('\t') for line in (MOD/'objects-probe.tsv').read_text().splitlines()]
    def local(held):
        objects=read_objects(MOD/'objects-local.tsv') or []
        return next((o for o in objects if o['held']==held and len(o['parts'])==2),None)
    try:
        game=subprocess.Popen([str(BIN/'Stray-Win64-Shipping.exe'),'-nosplash','-windowed','-ResX=960','-ResY=540','-nosound',f'-UserDir={folder/"UserData"}',f'-StrayCoopHarness={token}'],cwd=BIN)
        time.sleep(4)
        def menu():
            command('harness inspect');time.sleep(.6)
            return read_state(MOD/'harness-state.tsv')['chapter']==0
        wait(menu,'fresh menu')
        action('harness open 2')
        def loaded():
            command('harness inspect');time.sleep(.6);s=read_state(MOD/'harness-state.tsv')
            return s['chapter']==2 and s.get('waiting')=='false' and 'player' in s
        wait(loaded,'chapter fixture loaded')
        action('harness resume');action('objects-start');time.sleep(.6)
        action('objects-test-near 1');time.sleep(3);action('harness resume')
        action('objects-test-pickup 1')
        action('harness resume')
        o=wait(lambda:local(1),'native pickup captured with handle and container')
        time.sleep(2)
        if mode=='source':
            action('objects-test-fan')
            wait(lambda:read_effects(MOD/'objects-local.tsv'),'native fan consumption captured as shared effect')
            wait(lambda:local(0),'consumed item retains a recoverable final snapshot')
        else:
            action('objects-test-drop')
            action('harness resume')
            wait(lambda:local(0),'native drop captured')
            target=copy.deepcopy(o)
            for part in target['parts']: part[1]+=300;part[13]=0
            a=Authority();a.merge(2,target);remote=a.records
            actor=o['id'].rsplit('.',1)[0]
            def both_parts():
                rows=probe();parts={r[1]:r for r in rows if r[0]=='PART'}
                for part in target['parts']:
                    r=parts.get(actor+'.'+part[0])
                    if not r or abs(float(r[2])-part[1])>2 or r[5]!='false': return False
                return True
            wait(both_parts,'remote holder moves both native parts and locks pickup',probe=True)
            target['seq']+=1;target['held']=0
            a.merge(2,target);remote=a.records
            wait(lambda:any(r[0]=='PART' and r[1].startswith(actor+'.') and r[5]=='true' for r in probe()),'remote drop unlocks native pickup',probe=True)
            action('objects-test-pickup 1')
            wait(lambda:local(1),'optimistic pickup of peer dropped object')
            mine=local(1);a.merge(1,mine);remote=a.records
            wait(lambda:any(r[0]=='PART' and r[1].startswith(actor+'.') and r[6]=='true' for r in probe()),'ownership transfer restores native actor tick',probe=True)
            action('objects-test-drop')
            rows=wait(lambda:probe(),'fan identity catalog',probe=True)
            fan=next(r[1] for r in rows if r[0]=='FAN')
            fan_effect=(fan,o['id'])
            wait(lambda:any(r[0]=='FAN' and r[1]==fan and r[2]=='true' for r in probe()),'received effect invokes native fan puzzle',probe=True)
            wait(lambda:not any(r[0]=='PART' and r[1].startswith(actor+'.') for r in probe()),'native fan consumes mirrored bucket',probe=True)
    finally:
        if game and game.poll() is None:
            command('quit')
            try: game.wait(timeout=6)
            except subprocess.TimeoutExpired: game.terminate();game.wait(timeout=5)
        if (BIN/'UE4SS.log').exists(): shutil.copy2(BIN/'UE4SS.log',folder/'UE4SS.log')
        for name in ('objects-local.tsv','objects-remote.tsv','objects-probe.tsv'):
            if (MOD/name).exists(): shutil.copy2(MOD/name,folder/name)
        (folder/'results.json').write_text(json.dumps(results,indent=2))
        (MOD/'harness-session.txt').unlink(missing_ok=True)
        for source in backup.glob('*.lua'): shutil.copy2(source,MOD/'Scripts'/source.name)
        print('Evidence '+str(folder),flush=True)
    log=(folder/'UE4SS.log').read_text(errors='replace')
    assert '[StrayCoop/Objects] ERROR:' not in log, 'Native object adapter errors: inspect '+str(folder)

if __name__=='__main__':
    fixture('mirror')
    fixture('source')
