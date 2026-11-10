"""Load isolated native chapter entries and collect checkpoint/event catalogs.

Loading a chapter is a fixture/setup check, not a campaign co-op pass.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
PROJECT=ROOT/'CoopMod'
BINS=[ROOT/'Hk_project/Binaries/Win64',PROJECT/'test-client/Hk_project/Binaries/Win64']
CHAPTERS={1:'Inside the Wall',2:'Dead City',3:'The Flat',4:'The Slums',5:'Rooftops',6:'The Slums Part 2',
          7:'Dead End',8:'The Sewers',9:'Antvillage',10:'Midtown',11:'Jail',12:'Control Room'}
SUITES={'smoke':[3,7,12],'all':list(CHAPTERS)}
FOCUS={3:['B-12 activation','backpack/inventory'],4:['dialogue','doors','checkpoint progression'],
       7:['chase/combat','death and respawn'],10:['inventory exchanges','dialogue'],
       11:['stealth','companion progression'],12:['control-room puzzles','ending cinematic']}

def read_state(path):
    text=path.read_text(encoding='utf-8')
    if len(text)>2_000_000 or not text.endswith('\n'):
        raise ValueError('incomplete telemetry')
    rows=[line.split('\t') for line in text.splitlines()]
    if not rows or rows[0][:2]!=['HARNESS','1'] or len(rows[0])!=3:
        raise ValueError('invalid telemetry header')
    result={'timestamp':int(rows[0][2]),'sequences':[]}
    for row in rows[1:]:
        if row[0]=='SEQUENCE' and len(row)==3:
            result['sequences'].append({'actor':row[1],'checkpoint':row[2]})
        elif row[0]=='PLAYER' and len(row)==7:
            position=[float(v) for v in row[2:5]]
            if not all(math.isfinite(v) and abs(v)<1e8 for v in position):
                raise ValueError('invalid player position')
            result['player']={'actor':row[1],'position':position,'locomotion':row[5]=='true','dead':row[6]=='true'}
        elif len(row)==2:
            result[row[0].lower()]=row[1]
        elif row[0]=='GEAR' and len(row)==3:
            result['gear_objects']={'drone':row[1]=='true','backpack':row[2]=='true'}
    if 'chapter' not in result or 'zone' not in result:
        raise ValueError('incomplete state')
    result['chapter']=int(result['chapter'])
    return result

def states():
    values=[read_state(b/'Mods/StrayCoop/harness-state.tsv') for b in BINS]
    if any(abs(time.time()-v['timestamp'])>3 for v in values):
        raise ValueError('stale telemetry')
    return values

def send(text):
    for b in BINS:
        (b/'Mods/StrayCoop/command.txt').write_text(text+'\n',encoding='ascii')

def wait_for(fn,timeout,label):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            value=fn()
            if value: return value
        except (OSError,ValueError,KeyError,RuntimeError): pass
        time.sleep(.25)
    raise RuntimeError('Timed out: '+label)

def active_session():
    session=json.loads((PROJECT/'reports/local-session.json').read_text())
    if not session.get('running') or not session.get('harness'):
        raise RuntimeError('No isolated harness session is running')
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,60}',session.get('profile','')):
        raise RuntimeError('Invalid isolated profile')
    return session

def run_case(chapter,timeout,output):
    previous=states()
    for b in BINS:
        if (b/'Mods/StrayCoop/command.txt').read_text().strip():
            raise RuntimeError('Another development command is pending')
    requested=int(time.time())
    start=time.monotonic()
    send('harness open '+str(chapter))
    stable_since=None
    last_resume=0.
    resume_attempts=0
    def ready():
        nonlocal stable_since,last_resume,resume_attempts
        values=states()
        if (all(v['chapter']==chapter and v.get('waiting')=='false' and 'player' in v for v in values)
            and any(v.get('paused')=='true' for v in values) and time.monotonic()-last_resume>=2
            and resume_attempts<1 and time.monotonic()-start>=10 and all(not (b/'Mods/StrayCoop/command.txt').read_text().strip() for b in BINS)):
            send('harness resume')
            last_resume=time.monotonic()
            resume_attempts+=1
            return None
        ok=all(v['timestamp']>=requested and v['chapter']==chapter and 'player' in v
               and not v['player']['dead'] and v.get('waiting')=='false' and v.get('paused')=='false'
               for v in values) and values[0]['zone']==values[1]['zone']
        # Repeating a chapter must wait for the native reload, not accept old data.
        if all(p['chapter']==chapter for p in previous) and time.monotonic()-start<5: ok=False
        if not ok: stable_since=None;return None
        if stable_since is None: stable_since=time.monotonic()
        return values if time.monotonic()-stable_since>=2 else None
    values=wait_for(ready,timeout,CHAPTERS[chapter]+' native entry')
    send('harness catalog')
    def catalogs_ready():
        for b in BINS:
            text=(b/'Mods/StrayCoop/harness-catalog.tsv').read_text()
            rows=text.splitlines()
            if not text.endswith('\n') or not rows or not rows[0].startswith('CATALOG\t1\t'): return False
            if rows[-1]!='END\t'+str(len(rows)-2): return False
        return True
    wait_for(catalogs_ready,10,'interaction catalog')
    result={'chapter':chapter,'name':CHAPTERS[chapter],'setup_status':'chapter_load_pass',
            'elapsed_seconds':round(time.monotonic()-start,2),'event_replication':'not_tested',
            'planned_event_tests':FOCUS.get(chapter,[]),'players':values}
    (output/f'chapter-{chapter:02}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f"Loaded {CHAPTERS[chapter]} in both games ({result['elapsed_seconds']}s); cataloged "
          f"{len(values[0]['sequences'])}/{len(values[1]['sequences'])} sequences. Co-op events: untested.",flush=True)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['list','run','status'])
    p.add_argument('--suite',choices=SUITES,default='smoke')
    p.add_argument('--chapters',type=int,nargs='+',choices=CHAPTERS)
    p.add_argument('--attach',action='store_true',help='Use an existing guarded test session')
    p.add_argument('--keep-open',action='store_true',help='Leave the owned test session open for event inspection')
    p.add_argument('--timeout',type=float,default=120)
    args=p.parse_args()
    if args.action=='list':
        for chapter,name in CHAPTERS.items(): print(f'{chapter:2}  {name}')
        return
    if args.action=='status':
        active_session();send('harness inspect');print(json.dumps(wait_for(states,10,'fresh status'),indent=2));return
    chapters=args.chapters or SUITES[args.suite]
    if (args.attach or args.keep_open) and len(chapters)!=1:
        p.error('--attach and --keep-open require exactly one --chapters ID')
    if args.timeout<=0 or args.timeout>600: p.error('--timeout must be between 0 and 600 seconds')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
    output=PROJECT/'reports'/('harness-'+stamp)
    output.mkdir(parents=True,exist_ok=False)
    results=[]
    try:
        for chapter in chapters:
            runner=None
            profile=None
            case_dir=output/f'case-{chapter:02}'
            case_dir.mkdir()
            try:
                if args.attach:
                    session=active_session()
                else:
                    profile=f'harness-{stamp}-ch{chapter:02}'
                    with (case_dir/'launcher.log').open('w',encoding='utf-8') as launcher_log:
                        runner=subprocess.Popen([sys.executable,str(PROJECT/'tools/local_test.py'),'--harness','--profile',profile,
                            '--duration',str(max(600,args.timeout+90))],cwd=ROOT,stdout=launcher_log,stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
                    def owned():
                        if runner.poll() is not None:
                            log_path=case_dir/'launcher.log'
                            detail=log_path.read_text(errors='replace')[-4000:] if log_path.exists() else ''
                            raise ChildProcessError('Isolated launcher exited before startup. '+detail)
                        value=active_session()
                        return value if value['profile']==profile else None
                    session=wait_for(owned,20,'isolated launcher')
                wait_for(lambda:all('Opening story synchronization enabled' in (b/'UE4SS.log').read_text(errors='replace') for b in BINS),30,'mod startup')
                send('harness restart')
                wait_for(lambda:all(not (b/'Mods/StrayCoop/command.txt').read_text().strip() for b in BINS),10,'harness command acknowledgement')
                wait_for(states,20,'guarded telemetry')
                result=run_case(chapter,args.timeout,case_dir)
                if args.keep_open: send('harness stop')
                result['profile']=session['profile']
                results.append(result)
            finally:
                for b,label in zip(BINS,('host','client')):
                    for name in ('UE4SS.log','Mods/StrayCoop/harness-state.tsv','Mods/StrayCoop/harness-catalog.tsv','Mods/StrayCoop/bridge-status.json'):
                        source=b/name
                        if source.exists(): shutil.copy2(source,case_dir/(label+'-'+source.name))
                if runner and not args.keep_open:
                    # Let the launcher clean up its own children even if a game
                    # is unresponsive. Never terminate a user-owned game by name.
                    (PROJECT/'reports'/f'local-stop-{profile}.txt').write_text('stop\n')
                    try: runner.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        raise RuntimeError('Owned launcher did not finish cleanup; inspect the session manifest')
                if profile and not args.keep_open and results and results[-1].get('chapter')==chapter:
                    # Archive native save data as an immutable fixture candidate.
                    # Restoring arbitrary mid-chapter saves is a separate test.
                    inventory=[]
                    for label in ('host','client'):
                        saved=PROJECT/'test-profiles'/profile/label/'Saved/SaveGames'
                        if saved.exists():
                            target=case_dir/'save-fixture'/label
                            shutil.copytree(saved,target)
                            for file in target.rglob('*.sav'):
                                inventory.append({'path':str(file.relative_to(case_dir)),
                                    'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
                    (case_dir/'save-fixture.json').write_text(json.dumps({'restoration':'not_verified','files':inventory},indent=2)+'\n')
    except Exception as exc:
        results.append({'setup_status':'failed','error':str(exc)})
        raise
    finally:
        (output/'summary.json').write_text(json.dumps({'cases':results,'event_replication':'not_tested'},indent=2)+'\n')
        print('Report: '+str(output),flush=True)

if __name__=='__main__': main()
