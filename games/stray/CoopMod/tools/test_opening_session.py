"""Exercise the native opening interactions in two already-loaded test games.

Requires a fresh opening in both games with protocol-3 loopback bridges running.
Uses only the mod's fixed development commands, never simulated completion bits.
Leaves both games open for real movement/visual inspection after the cutscene.
"""
import argparse
from pathlib import Path
import re
import time

ROOT=Path(__file__).resolve().parents[2]
BINS=[ROOT/'Hk_project/Binaries/Win64', ROOT/'CoopMod/test-client/Hk_project/Binaries/Win64']

def log(i):
    return (BINS[i]/'UE4SS.log').read_text(errors='replace')

def command(i,text):
    (BINS[i]/'Mods/StrayCoop/command.txt').write_text(text+'\n',encoding='ascii')

def wait_for(label,predicate,timeout=120):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if predicate():
            print(label,flush=True)
            return
        time.sleep(.25)
    raise RuntimeError('Timed out: '+label)

def latest(i,pattern):
    found=re.findall(pattern,log(i))
    return found[-1] if found else ''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--initiator',choices=['host','client'],required=True)
    a=p.parse_args()
    # Initial level intro must finish before native cat interactions are available.
    wait_for('Both opening scenes playable',lambda:all(latest(i,r'Cinematic/locomotion=([a-z:]+)')=='false:true' for i in (0,1)))
    for i in (0,1): command(i,'story-trace')
    wait_for('Both opening traces ready',lambda:all('Opening interaction trace enabled' in log(i) for i in (0,1)),10)
    command(1,'story-test-interact white')
    command(0,'story-test-interact red')
    wait_for('Both games count the split interactions',lambda:all(latest(i,r'Opening mask/sleep=([0-9.]+):')=='3.0' for i in (0,1)),60)
    command(1,'story-test-interact black')
    wait_for('Both games count all three interactions',lambda:all(latest(i,r'Opening mask/sleep=([0-9.]+):')=='7.0' for i in (0,1)),60)
    def settled(i):
        state=latest(i,r'\[StrayCoop/StoryTrace\] (Flags=[^\r\n]+)')
        return all(field+':false,true,false,false' in state for field in ('WhiteAndRedCatButton','RedCatButton','BlackCatButton'))
    wait_for('All three NPCs settled in both worlds',lambda:all(settled(i) for i in (0,1)),40)
    initiator=0 if a.initiator=='host' else 1
    command(initiator,'story-test-sleep')
    wait_for('Both games started native sleep',lambda:all('Observed native opening sleep start' in log(i) for i in (0,1)),20)
    wait_for('Both games reached the morning cinematic',lambda:all('Opening sequence not loaded' in log(i) for i in (0,1)),45)
    wait_for('Both native morning cinematics active',lambda:all(log(i).rfind('Cinematic/locomotion=true:')>log(i).find('Observed native opening sleep start') for i in (0,1)),15)
    wait_for('Both morning cinematics released control',lambda:all(latest(i,r'Cinematic/locomotion=([a-z:]+)')=='false:true' for i in (0,1)),150)
    wait_for('Both games rebuilt visual proxies',lambda:all(log(i).rfind('Rebuilding visual proxies after native cinematic')>log(i).rfind('Opening sequence not loaded') for i in (0,1)),10)
    for i in (0,1): command(i,'story-status')
    print('Native progression check passed. Inspect both viewports and test real movement.',flush=True)

if __name__=='__main__': main()
