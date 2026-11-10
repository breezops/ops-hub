"""Probe the guarded, already-running local pair; never launches a normal save.

This invokes a native Guardian dialogue fixture, not real player input or proof
of full Slums quest replication. Keep local_test.py alive in its own terminal.
"""
from pathlib import Path
import json,time
from checkpoint_harness import active_session,states,BINS,PROJECT

def wait(fn,timeout=15):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            value=fn()
            if value:return value
        except (OSError,ValueError,KeyError):pass
        time.sleep(.2)
    raise RuntimeError('Timed out waiting for native cooperative audit step')

def command(text,index=None):
    targets=BINS if index is None else [BINS[index]]
    wait(lambda:all(not (b/'Mods/StrayCoop/command.txt').read_text().strip() for b in targets))
    for b in targets:(b/'Mods/StrayCoop/command.txt').write_text(text+'\n')
    wait(lambda:all(not (b/'Mods/StrayCoop/command.txt').read_text().strip() for b in targets))

def status():
    paths=[b/'Mods/StrayCoop/coop-status.tsv' for b in BINS]
    old=[p.stat().st_mtime_ns if p.exists() else 0 for p in paths]
    command('harness coop-status')
    wait(lambda:all(p.exists() and p.stat().st_mtime_ns!=n for p,n in zip(paths,old)))
    result=[]
    for p in paths:
        rows=[r.split('\t') for r in p.read_text().splitlines()]
        value={r[0]:r[1] for r in rows if len(r)==2}
        value['lines']={r[1]:int(r[2]) for r in rows if r[0]=='LINE'}
        value['gear']=[r[2] for r in rows if r[0]=='REMOTE_GEAR']
        result.append(value)
    return result

def main():
    session=active_session()
    out=PROJECT/'reports'/('cooperative-native-'+time.strftime('%Y%m%d-%H%M%S'))
    out.mkdir();results={'fixture':session['profile'],'checks':[],'normal_player_input':'not_tested'}
    try:
        command('harness restart');time.sleep(.6)
        if not all(s['chapter']==4 for s in states()):command('harness open 4')
        # A one-time fixture resume only after streaming finishes; no polling
        # loop dismisses menus or toggles pause during the actual test.
        wait(lambda: all(s['chapter']==4 and s.get('waiting')=='false' for s in states()),60)
        command('harness resume')
        stable_since=None
        def ready():
            nonlocal stable_since
            values=states()
            ok=all(s['chapter']==4 and s.get('waiting')=='false' and s.get('player',{}).get('locomotion') for s in values)
            if not ok:stable_since=None;return False
            if stable_since is None:stable_since=time.monotonic()
            return time.monotonic()-stable_since>3
        wait(ready,60)
        command('harness resume');time.sleep(1)
        initial=status();results['initial']=initial
        assert all(s['ERRORS']=='0' for s in initial),initial
        assert all(s['AUTO_PAUSE']=='false' for s in initial),initial
        results['checks'].append('both native adapters started; focus auto-pause disabled')
        print('PASS native adapters and auto-pause setting',flush=True)
        command('harness guardian-stage');time.sleep(3)
        command('harness coop-talk',1)
        end=time.monotonic()+60
        while time.monotonic()<end:
            running=status();results['last_dialog']=running
            if all(s['RUNNING']=='true' and 'Dial_Guardian_C ' in s['DIALOG'] and s.get('DIALOG_CAT','-')!='-' and s['CURRENT_INTERACTION'].endswith('PersistentLevel.BP_Droid_Guardian.talkable') for s in running):break
            time.sleep(.5)
        else:raise RuntimeError('Guardian fixture did not begin on both peers')
        results['checks'].append('client-triggered native Guardian dialogue began on both peers')
        print('PASS client-triggered Guardian dialogue on both peers',flush=True)
        target=next(t for t in running[1]['lines'] if t.endswith('PersistentLevel.BP_Droid_Guardian.talkable'))
        expected=[s['DIALOG'] for s in running]
        before=[s['lines'].get(target,0) for s in running]
        # First reveal text, then wait for both native text-ready states.
        command('harness coop-next',1)
        end=time.monotonic()+12
        while time.monotonic()<end:
            ready=status();results['text_ready_state']=ready
            if any(s['DIALOG']!=d for s,d in zip(ready,expected)):
                raise RuntimeError('Guardian changed to another dialogue during text reveal; inspect text_ready_state')
            if all(s['TEXT_READY']=='true' for s in ready):break
            time.sleep(.5)
        else:raise RuntimeError('Both Guardian text states did not become ready; refusing blind advance')
        command('harness coop-next',1)
        end=time.monotonic()+10
        while time.monotonic()<end:
            advanced=status();results['after_advance']=advanced
            if any(s['DIALOG']!=d for s,d in zip(advanced,expected)):
                raise RuntimeError('Guardian changed dialogue during advance; completion/quest transition needs verification')
            if all(s['lines'].get(target,0)>n for s,n in zip(advanced,before)):break
            time.sleep(.5)
        else:raise RuntimeError('Shared native dialogue line advancement not confirmed')
        results['checks'].append('native dialogue advance reached next line on both peers')
        print('PASS shared native dialogue advancement',flush=True)
    except Exception as exc:
        results["failure"]=str(exc)
        raise
    finally:
        for b,label in zip(BINS,('host','client')):
            for name in ('UE4SS.log','Mods/StrayCoop/coop-local.tsv','Mods/StrayCoop/coop-remote.tsv','Mods/StrayCoop/coop-status.tsv','Mods/StrayCoop/performance.tsv'):
                p=b/name
                if p.exists():(out/(label+'-'+p.name)).write_bytes(p.read_bytes())
        (out/'result.json').write_text(json.dumps(results,indent=2)+'\n')
        print('Evidence: '+str(out),flush=True)

if __name__=='__main__':main()
