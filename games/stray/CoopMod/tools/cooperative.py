"""Bounded native conversation intents and cosmetic backpack snapshots.

Carried in the existing authenticated, ordered gameplay envelopes. No remote
function names, class loads, code or arbitrary asset paths are accepted.
"""
import math,re,time
PATH=re.compile(r'^/Game/Map/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+:PersistentLevel\.[A-Za-z0-9_.]+$')
MAX_EVENTS=4

def empty():return {'gear':0,'pose':[0.]*6,'events':[]}

def validate(value):
    if not isinstance(value,dict) or set(value)!={'gear','pose','events'}:raise ValueError('invalid coop snapshot')
    if type(value['gear']) is not int or value['gear'] not in (0,1):raise ValueError('invalid gear flag')
    p=value['pose']
    if not isinstance(p,list) or len(p)!=6 or any(type(x) not in (float,int) or not math.isfinite(x) or abs(x)>(1e7 if i<3 else 36000) for i,x in enumerate(p)):raise ValueError('invalid gear pose')
    events=value['events']
    if not isinstance(events,list) or len(events)>MAX_EVENTS:raise ValueError('too many coop events')
    previous=0
    for e in events:
        if not isinstance(e,list) or len(e)!=4:raise ValueError('invalid coop event')
        seq,kind,path,line=e
        if type(seq) is not int or not previous<seq<2**53:raise ValueError('invalid event order')
        if kind not in ('use','advance','unroll','end','interaction','scene'):raise ValueError('invalid event kind')
        if not isinstance(path,str) or len(path)>300 or not PATH.fullmatch(path):raise ValueError('invalid local interaction identifier')
        if type(line) is not int or not 0<=line<=10000:raise ValueError('invalid dialog line')
        previous=seq
    return value

def read(path):
    try:
        if path.stat().st_size>1800 or time.time()-path.stat().st_mtime>2:return empty()
        text=path.read_text(encoding='ascii')
        if not text.endswith('\n'):return empty()
        rows=[x.split('\t') for x in text.splitlines()]
        if not rows or len(rows[0])!=8 or rows[0][0]!='GEAR':return empty()
        return validate({'gear':int(rows[0][1]),'pose':[float(x) for x in rows[0][2:]],
            'events':[[int(r[1]),r[2],r[3],int(r[4])] for r in rows[1:] if len(r)==5 and r[0]=='EVENT']})
    except (OSError,UnicodeError,ValueError):return empty()

def render(peer,own_id,sequence,connected,epoch):
    value=validate(peer) if connected else empty()
    rows=[f'COOP\t{sequence}\t{int(connected)}\t{own_id}\t{epoch}\n',
          'GEAR\t'+str(value['gear'])+'\t'+'\t'.join(format(x,'.6f') for x in value['pose'])+'\n']
    rows+=['EVENT\t'+'\t'.join(map(str,e))+'\n' for e in value['events']]
    return ''.join(rows)
