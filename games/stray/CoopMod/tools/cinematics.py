"""Bounded, repeated skip intents scoped to chapter/cinematic occurrence.
These identifiers are logical keys and are never loaded as game assets."""
import re,time
ASSET=re.compile(r'^/Game/StrayCoop/Cinematics/Chapter_M?[0-9]{1,4}$')
def validate(value):
    if not isinstance(value,list) or len(value)!=4: raise ValueError('cinematic shape')
    asset,occurrence,skip,active=value
    if not isinstance(asset,str) or len(asset)>80 or not (asset=='-' or ASSET.fullmatch(asset)): raise ValueError('cinematic scope')
    if type(occurrence) is not int or not 0<=occurrence<=10000 or type(skip) is not int or skip not in (0,1): raise ValueError('cinematic intent')
    if type(active) is not int or active not in (0,1): raise ValueError('cinematic active')
    if (asset=='-' and (occurrence or skip or active)) or (asset!='-' and occurrence==0): raise ValueError('cinematic scope')
    return value
def read(path):
    try:
        if path.stat().st_size>300 or time.time()-path.stat().st_mtime>2: return ['-',0,0,0]
        text=path.read_text(encoding='ascii');v=text.rstrip('\n').split('\t')
        if not text.endswith('\n') or len(v)!=5 or v[0]!='CINEMATIC': return ['-',0,0,0]
        return validate([v[1],int(v[2]),int(v[3]),int(v[4])])
    except (OSError,ValueError,UnicodeError): return ['-',0,0,0]
def validate_events(events):
    if not isinstance(events,list) or len(events)>4: raise ValueError('skip event count')
    for e in events:
        if not isinstance(e,list) or len(e)!=3 or type(e[2]) is not int or e[2] not in (1,2): raise ValueError('skip event flag')
        validate([e[0],e[1],1,0])
    return events
class Skips:
    def __init__(self): self.events={};self.seen=set();self.latest={}
    def merge(self,intent,now,player=1):
        self.events={key:event for key,event in self.events.items() if event['expires']>now}
        asset,count,requested,active=validate(intent)
        self.latest[player]=intent
        key=(asset,count)
        # Do not extend a completed scene forever from a repeated local file.
        if requested and key not in self.seen and len(self.events)<4 and len(self.seen)<128:
            self.events[key]={'expires':now+90,'phase':1};self.seen.add(key)
        for k,event in self.events.items():
            if all(p in self.latest and tuple(self.latest[p][:2])==k and self.latest[p][3]==0 for p in (1,2)):
                event['phase']=2
    def snapshot(self,now):
        return [[*key,event['phase']] for key,event in self.events.items() if event['expires']>now]
