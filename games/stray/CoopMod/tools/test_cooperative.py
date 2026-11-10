import copy,tempfile,time,unittest
from pathlib import Path
from unittest.mock import patch
import cooperative
from bridge import Bridge,idle_state,encode

TARGET='/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Guardian.Talkable'

class CooperativeTests(unittest.TestCase):
    def snapshot(self):return {'gear':1,'pose':[10.,20.,30.,0.,90.,0.],'events':[[1,'use',TARGET,0],[2,'advance',TARGET,1]]}

    def test_rejects_remote_code_asset_paths_bad_numbers_and_reordered_events(self):
        bad=[]
        s=self.snapshot();s['events'][0][1]='eval';bad.append(s)
        s=self.snapshot();s['events'][0][2]='/Script/Engine.KismetSystemLibrary';bad.append(s)
        s=self.snapshot();s['pose'][0]=float('nan');bad.append(s)
        s=self.snapshot();s['events'].reverse();bad.append(s)
        s=self.snapshot();s['gear']=True;bad.append(s)
        for value in bad:
            with self.assertRaises(ValueError):cooperative.validate(value)

    def test_partial_or_stale_files_clear_intent(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'local.tsv';p.write_text('GEAR\t1\t1\t2\t3\t0\t0\t0')
            self.assertEqual(cooperative.read(p),cooperative.empty())
            p.write_text('GEAR\t1\t1\t2\t3\t0\t0\t0\nEVENT\t1\tuse\t'+TARGET+'\t0\n')
            self.assertEqual(cooperative.read(p)['events'][0][1],'use')
            import os
            os.utime(p,(time.time()-5,time.time()-5))
            self.assertEqual(cooperative.read(p),cooperative.empty())

    def test_authenticated_two_peer_roundtrip_and_disconnect_clear(self):
        with tempfile.TemporaryDirectory() as tmp:
            h=Bridge('host',Path(tmp)/'host','coop-test-key-123456',bind='127.0.0.1',port=0)
            c=Bridge('join',Path(tmp)/'client','coop-test-key-123456',bind='127.0.0.1',port=h.sock.getsockname()[1])
            state=dict(idle_state(),map='/Game/Map/Test',chapter=4)
            for b in (h,c):
                (b.directory/'coop-local.tsv').write_text('GEAR\t1\t10\t20\t30\t0\t90\t0\nEVENT\t1\tuse\t'+TARGET+'\t0\n')
            now=time.monotonic()
            try:
                with patch('bridge.read_local',return_value=state):
                    for i in range(12):
                        t=now+i*.05
                        for b in (h,c):b.tick(t)
                        for b in (h,c):
                            while True:
                                try:data,addr=b.sock.recvfrom(4097)
                                except BlockingIOError:break
                                b.receive(data,addr,t)
                    for b in (h,c):
                        self.assertEqual(b.peer_coop['gear'],1)
                        self.assertIn('EVENT\t1\tuse', (b.directory/'coop-remote.tsv').read_text())
                    h.tick(now+10)
                    self.assertNotIn('EVENT', (h.directory/'coop-remote.tsv').read_text())
            finally:h.sock.close();c.sock.close()

if __name__=='__main__':unittest.main()
