"""Guarded first-meeting sequence test; never touches a normal UserDir."""
import json,time
from native_cooperative_audit import command,status,wait
from checkpoint_harness import active_session,states,PROJECT,BINS

def main():
    session=active_session();out=PROJECT/'reports'/('guardian-sequence-'+time.strftime('%Y%m%d-%H%M%S'));out.mkdir()
    result={'profile':session['profile'],'checks':[],'normal_player_input':'not_tested'}
    expected='/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2'
    try:
        command('harness restart');time.sleep(1)
        wait(lambda:all('chapter' in v for v in states()),20)
        if not all(v['chapter']==4 for v in states()):command('harness open 4')
        wait(lambda:all(v['chapter']==4 and v.get('waiting')=='false' for v in states()),60)
        time.sleep(15);command('harness resume')
        wait(lambda:all(v.get('player',{}).get('locomotion') for v in states()),30)
        command('harness guardian-stage');time.sleep(3)
        command('harness guardian-scene',1)
        seen=[False,False];deadline=time.monotonic()+90;advances=0;next_input=0
        while time.monotonic()<deadline:
            values=status();worlds=states();result['last_status']=values;result['last_worlds']=worlds
            if expected in values[1]['sequences'] and expected in values[0]['sequences_applied'] and 'authored sequence entry reached both peers' not in result['checks']:
                result['checks'].append('authored sequence entry reached both peers');print('PASS shared authored Guardian sequence entry',flush=True)
            for i,v in enumerate(worlds):seen[i]=seen[i] or v.get('cinematic')=='true'
            result['cinematic_seen']=seen
            result['adventure_states']=[v.get('adventure_state') for v in worlds]
            if advances<15 and time.monotonic()>=next_input and all(v['RUNNING']=='true' and v['TEXT_READY']=='true' for v in values) and values[0]['DIALOG']==values[1]['DIALOG']:
                command('harness coop-next',1);advances+=1;next_input=time.monotonic()+2
            result['advance_requests']=advances
            if all(seen) and worlds[0].get('adventure_state')==worlds[1].get('adventure_state') and all(v.get('cinematic')=='false' and v.get('player',{}).get('locomotion') and v.get('paused')=='false' for v in worlds):
                result['checks'].append('both entered cinematic mode and regained native locomotion');print('PASS shared Guardian cinematic completion',flush=True);return
            time.sleep(.5)
        raise RuntimeError('Authored Guardian sequence did not meet both-entry and completion gate')
    except Exception as exc:result['failure']=str(exc);raise
    finally:
        for b,label in zip(BINS,('host','client')):
            for name in ['UE4SS.log','Mods/StrayCoop/coop-status.tsv','Mods/StrayCoop/harness-state.tsv']:
                p=b/name
                if p.exists():(out/(label+'-'+p.name)).write_bytes(p.read_bytes())
        (out/'result.json').write_text(json.dumps(result,indent=2));print('Evidence: '+str(out),flush=True)

if __name__=='__main__':main()
