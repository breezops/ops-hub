"""A synthetic authenticated peer asks the owned native research game to skip.
Run only alongside research_visuals.py's isolated nonce-enabled game.
This is one native game + two real UDP bridges, not a two-PC campaign test.
"""
from pathlib import Path
import argparse,json,time
from bridge import Bridge
from cinematics import read
ROOT=Path(__file__).resolve().parents[2];MOD=ROOT/'Hk_project/Binaries/Win64/Mods/StrayCoop'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--chain-opening',action='store_true');args=parser.parse_args()
    assert (MOD/'harness-session.txt').exists(),'Isolated research fixture required'
    report=ROOT/'CoopMod/reports'/('native-skips-'+time.strftime('%Y%m%d-%H%M%S'));report.mkdir()
    h=Bridge('host',MOD,'native-cinematic-test-key',bind='127.0.0.1',port=0)
    c=Bridge('join',report/'synthetic-peer','native-cinematic-test-key',bind='127.0.0.1',port=h.sock.getsockname()[1])
    started=time.monotonic();scope=None;requested=None;results=[];peer_finished=False;finished_at=None;next_probe=0;passed=0;setup=[]
    try:
        while time.monotonic()-started<240:
            now=time.monotonic()
            if setup and now>=setup[0][0]:
                _,command=setup.pop(0);(MOD/'command.txt').write_text(command+'\n')
            intent=read(MOD/'cinematic-local.tsv')
            if intent[0]!='-' and intent[3] and (scope is None or scope[:2]!=intent[:2]):
                scope=intent;requested=now
                (c.directory/'cinematic-local.tsv').write_text(f'CINEMATIC\t{scope[0]}\t{scope[1]}\t1\t1\n')
                results.append({'requested':scope,'at':now-started});print('Peer requested '+repr(scope),flush=True)
            # Keep the peer file fresh while waiting for native completion.
            if scope:(c.directory/'cinematic-local.tsv').touch()
            for b in (h,c):b.tick(now)
            for b in (h,c):
                while True:
                    try:data,address=b.sock.recvfrom(4097)
                    except BlockingIOError:break
                    b.receive(data,address,now)
            if requested:
                if now>next_probe:
                    (MOD/'command.txt').write_text('cinematic-status\n');next_probe=now+.8
                try:status=(MOD/'cinematic-status.tsv').read_text().strip().split('\t')
                except OSError:status=[]
                if len(status)==6:
                    assert status[5]=='0','Native cinematic module error'
                    if intent[:2]==scope[:2] and intent[3]==0 and status[1]=='false' and not peer_finished:
                        assert status[2]=='true' and status[4]=='true','Early finisher did not wait for peer'
                        assert abs(float(status[3])-1)<.001,'World clock was not restored'
                        results.append({'native_completed_after':now-requested,'status':status})
                        (c.directory/'cinematic-local.tsv').write_text(f'CINEMATIC\t{scope[0]}\t{scope[1]}\t1\t0\n')
                        peer_finished=True;finished_at=now
                    elif peer_finished and now-finished_at>1 and status[1:4]==['false','false','1.000000']:
                        results.append({'released_after_peer_completed':True,'status':status})
                        print('PASS peer skip, native completion, restored clock, completion barrier and release',flush=True)
                        passed+=1
                        if not args.chain_opening or passed>=2:break
                        scope=None;requested=None;peer_finished=False;finished_at=None
                        # Bounded native setup, not a claim that NPC gameplay was tested.
                        setup=[(now+i*10,'story-test-complete '+color) for i,color in enumerate(('white','red','black'))]
                        setup.append((now+35,'story-test-sleep'))
                if requested and now-requested>65:raise AssertionError('Native cinematic completion/barrier was not observed')
            time.sleep(.05)
        else:raise AssertionError('No supported native cinematic was observed')
    finally:
        h.sock.close();c.sock.close()
        (report/'results.json').write_text(json.dumps(results,indent=2))
        (report/'UE4SS.log').write_bytes((ROOT/'Hk_project/Binaries/Win64/UE4SS.log').read_bytes())
        print('Evidence '+str(report),flush=True)
if __name__=='__main__':main()
