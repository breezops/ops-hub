"""Bounded, host-owned carryable state; no executable commands or asset loading."""
from __future__ import annotations
import copy
import math
from pathlib import Path
import re
import time

MAX_OBJECTS=32
ID=re.compile(r'^/Game/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+:PersistentLevel\.[A-Za-z0-9_]+\.COMP_Grabbable$')
PART=re.compile(r'^[A-Za-z0-9_]{1,80}$')
ACTOR=re.compile(r'^/Game/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+:PersistentLevel\.[A-Za-z0-9_]+$')

def validate_effect(effect):
    if not isinstance(effect,dict) or set(effect)!={'fan','bucket'}: raise ValueError('effect fields')
    if not isinstance(effect['fan'],str) or len(effect['fan'])>340 or not ACTOR.fullmatch(effect['fan']): raise ValueError('fan identity')
    if not isinstance(effect['bucket'],str) or len(effect['bucket'])>360 or not ID.fullmatch(effect['bucket']): raise ValueError('bucket identity')
    return effect

def read_effects(path):
    try:
        if path.stat().st_size>24000 or time.time()-path.stat().st_mtime>2: return []
        text=path.read_text(encoding='ascii')
        if not text.endswith('\n'): return []
        result=[]
        for line in text.splitlines():
            f=line.split('\t')
            if f[0]=='FAN' and len(f)==3: result.append(validate_effect(dict(fan=f[1],bucket=f[2])))
        return result[:16]
    except (OSError,UnicodeError,ValueError): return []

def validate_object(obj):
    if not isinstance(obj,dict) or set(obj)!={'id','seq','held','parts'}: raise ValueError('object fields')
    if not isinstance(obj['id'],str) or len(obj['id'])>360 or not ID.fullmatch(obj['id']): raise ValueError('object identity')
    if type(obj['seq']) is not int or not 0<=obj['seq']<2**53: raise ValueError('object sequence')
    if type(obj['held']) is not int or obj['held'] not in (0,1): raise ValueError('object held')
    if not isinstance(obj['parts'],list) or not 1<=len(obj['parts'])<=3: raise ValueError('object parts')
    names=set()
    for p in obj['parts']:
        if not isinstance(p,list) or len(p)!=14 or not isinstance(p[0],str) or not PART.fullmatch(p[0]) or p[0] in names:
            raise ValueError('object part')
        names.add(p[0])
        for i,v in enumerate(p[1:13],1):
            bound=1e7 if i<=3 else 36000 if i<=6 else 100 if i<=9 else 1e5
            if type(v) not in (int,float) or not math.isfinite(v) or abs(v)>bound: raise ValueError('object transform')
        if any(abs(v)<.0001 for v in p[7:10]): raise ValueError('object scale')
        if type(p[13]) is not int or p[13] not in (0,1): raise ValueError('object physics')
    return obj

def read_objects(path):
    try:
        if path.stat().st_size>24000 or time.time()-path.stat().st_mtime>2: return []
        text=path.read_text(encoding='ascii')
        if not text.endswith('\n'): return None
        lines=text.splitlines()
        if not lines or lines[0]!='OBJECTS\t1': return None
        objects={}
        for line in lines[1:]:
            f=line.split('\t')
            if f[0]=='FAN': continue
            if len(f)!=18 or f[0]!='OBJECT': raise ValueError('object row')
            ident,seq,held=f[1],int(f[2]),int(f[3])
            o=objects.setdefault(ident,dict(id=ident,seq=seq,held=held,parts=[]))
            if o['seq']!=seq or o['held']!=held: raise ValueError('mixed object snapshot')
            o['parts'].append([f[4],*[float(v) for v in f[5:17]],int(f[17])])
        if len(objects)>8: raise ValueError('too many local objects')
        return [validate_object(o) for o in objects.values()]
    except (OSError,UnicodeError,ValueError): return None

class Authority:
    def __init__(self):
        self.records={}
        self.seen={1:{},2:{}}
        self.revision=0
        self.effects={}

    def effect(self,player,effect):
        validate_effect(effect)
        if effect['fan'] in self.effects or len(self.effects)>=16: return False
        bucket=self.records.get(effect['bucket'])
        if not bucket or (player!=1 and bucket['driver']!=player): return False
        self.effects[effect['fan']]=copy.deepcopy(effect)
        return True

    def merge(self,player,obj):
        validate_object(obj)
        ident=obj['id'];old=self.records.get(ident)
        previous=self.seen[player].get(ident,-1)
        if obj['seq']<=previous: return False
        # Bound both accepted and rejected identities, including malicious input.
        if ident not in self.seen[player] and len(self.seen[player])>=MAX_OBJECTS: return False
        self.seen[player][ident]=obj['seq']
        if old:
            if old['owner'] not in (0,player): return False
            if not obj['held'] and old['driver']!=player: return False
        elif len(self.records)>=MAX_OBJECTS: return False
        self.revision+=1
        self.records[ident]=dict(object=copy.deepcopy(obj),owner=player if obj['held'] else 0,
                                 driver=player,revision=self.revision)
        return True

    def disconnect(self,player):
        for r in self.records.values():
            if r['owner']==player:
                self.revision+=1;r['owner']=0;r['revision']=self.revision
                r['object']['held']=0
                for p in r['object']['parts']: p[13]=1

def validate_record(record):
    if not isinstance(record,dict) or set(record)!={'object','owner','driver','revision'}: raise ValueError('object record')
    validate_object(record['object'])
    for key,allowed in (('owner',(0,1,2)),('driver',(1,2))):
        if type(record[key]) is not int or record[key] not in allowed: raise ValueError('object ownership')
    if bool(record['object']['held'])!=(record['owner']!=0) or (record['owner'] and record['owner']!=record['driver']):
        raise ValueError('object holder')
    if type(record['revision']) is not int or not 0<record['revision']<2**53: raise ValueError('object revision')
    return record

def render_tsv(records,own_id,sequence,connected):
    rows=[f'OBJECTS\t1\t{own_id}\t{sequence}\t{int(connected)}\n']
    for r in records.values():
        o=r['object']
        for p in o['parts']:
            fields=['OBJECT',o['id'],r['revision'],r['owner'],r['driver'],o['seq'],o['held'],*p]
            rows.append('\t'.join(str(v) for v in fields)+'\n')
    return ''.join(rows)
