"""Mirror a synthetic idle peer into the owned, isolated native game."""
from pathlib import Path
import time,json
from bridge import Bridge,read_local
ROOT=Path(__file__).resolve().parents[2];MOD=ROOT/'Hk_project/Binaries/Win64/Mods/StrayCoop'
def main():
    assert (MOD/'harness-session.txt').exists(),'Owned research game required'
    folder=ROOT/'CoopMod/reports'/('native-avatar-'+time.strftime('%Y%m%d-%H%M%S'));folder.mkdir()
    h=Bridge('host',MOD,'native-avatar-test-key',bind='127.0.0.1',port=0)
    c=Bridge('join',folder/'peer','native-avatar-test-key',bind='127.0.0.1',port=h.sock.getsockname()[1])
    began=time.monotonic();seq=0
    try:
        while time.monotonic()-began<45:
            now=time.monotonic();seq+=1
            fields=(MOD/'local.tsv').read_text().strip().split('\t')
            if len(fields)==28 and fields[3]!='-':
                fields[2]=str(seq)
                for i in (7,17):fields[i]=str(float(fields[i])+120)
                fields[22:27]=['0','/Game/Animation/CAT/Idle/CAT_Stand_Idle.CAT_Stand_Idle','1','-1','1']
                (c.directory/'local.tsv').write_text('\t'.join(fields)+'\n')
            for b in (h,c):b.tick(now)
            for b in (h,c):
                while True:
                    try:data,address=b.sock.recvfrom(4097)
                    except BlockingIOError:break
                    b.receive(data,address,now)
            time.sleep(.05)
    finally:
        h.sock.close();c.sock.close()
        for name in ('performance.tsv','local.tsv','remote.tsv'):
            (folder/name).write_bytes((MOD/name).read_bytes())
        (folder/'UE4SS.log').write_bytes((ROOT/'Hk_project/Binaries/Win64/UE4SS.log').read_bytes())
        print('Evidence '+str(folder),flush=True)
if __name__=='__main__':main()
