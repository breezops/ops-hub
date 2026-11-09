"""Record live movement/animation snapshots without sending game input."""
import argparse
import json
import math
from pathlib import Path
import time
from bridge import read_local

def capture(seconds, output):
    root=Path(__file__).resolve().parents[2]
    bins=[root/'Hk_project/Binaries/Win64',root/'CoopMod/test-client/Hk_project/Binaries/Win64']
    summaries=[dict(label=label,samples=0,distance=0.,turn_degrees=0.,modes={},transitions=[],previous=None)
               for label in ('host','client')]
    start=time.monotonic()
    while time.monotonic()-start<seconds:
        for bin,summary in zip(bins,summaries):
            s=read_local(bin/'Mods/StrayCoop/local.tsv')
            if s['map']=='-': continue
            prior=summary['previous']
            if prior and prior['seq']==s['seq']: continue
            summary['samples']+=1
            mode=str(s['animation'][0])
            summary['modes'][mode]=summary['modes'].get(mode,0)+1
            if prior and s['map']==prior['map']:
                summary['distance']+=math.dist(prior['pose'][:3],s['pose'][:3])
                summary['turn_degrees']+=abs((s['visual'][4]-prior['visual'][4]+180)%360-180)
            if not prior or prior['animation'][0]!=s['animation'][0]:
                summary['transitions'].append(dict(elapsed=round(time.monotonic()-start,3),animation=s['animation'],pose=s['pose']))
            summary['previous']=s
        time.sleep(.05)
    output.write_text(json.dumps(summaries,indent=2)+'\n')
    for s in summaries:
        print(f"{s['label']}: samples={s['samples']}, travel={s['distance']:.2f}cm, turns={s['turn_degrees']:.2f}deg, modes={s['modes']}")

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--seconds',type=float,default=30)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    capture(a.seconds,a.output)
