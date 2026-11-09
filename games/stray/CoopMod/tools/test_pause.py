from pathlib import Path
import os
import tempfile
import time
import unittest
from bridge import Bridge, idle_state, validate_state, read_pause

class PauseTransportTests(unittest.TestCase):
    def test_pause_validation_and_partial_local_writes(self):
        for value in (True,-1,2,'1',[],None):
            with self.assertRaises(ValueError): validate_state(dict(idle_state(),pause=value))
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'pause.tsv'
            p.write_text('PAUSE\t1\n');self.assertEqual(read_pause(p),1)
            p.write_text('PAUSE\t');self.assertEqual(read_pause(p,1),1)
            os.utime(p,(time.time()-3,time.time()-3));self.assertEqual(read_pause(p,1),0)

    def test_votes_pause_independently_survive_loss_and_clear_on_disconnect(self):
        with tempfile.TemporaryDirectory() as tmp:
            host=Bridge('host',Path(tmp)/'host','pause-test-key-123456',bind='127.0.0.1',port=0)
            client=Bridge('join',Path(tmp)/'client','pause-test-key-123456',bind='127.0.0.1',port=host.sock.getsockname()[1])
            now=time.monotonic()
            counter=0
            def pump(drop=False):
                nonlocal counter
                counter+=1
                t=now+counter*.05
                for b in (host,client): b.tick(t)
                for b in (host,client):
                    while True:
                        try: data,addr=b.sock.recvfrom(4097)
                        except BlockingIOError: break
                        if not drop: b.receive(data,addr,t)
                return t
            def request(b,value): (b.directory/'pause-local.tsv').write_text(f'PAUSE\t{value}\n')
            def settle():
                for _ in range(8): pump()
            try:
                # No gameplay snapshot is needed for the pause channel to run.
                request(host,0);request(client,0);settle()
                request(client,1);settle()
                self.assertEqual(host.pause_votes,[0,1]);self.assertEqual(client.pause_votes,[0,1])
                request(host,1);settle()
                self.assertEqual(client.pause_votes,[1,1])
                request(client,0);settle()
                self.assertEqual(host.pause_votes,[1,0]);self.assertEqual(client.pause_votes,[1,0])
                request(host,0)
                for _ in range(4): pump(drop=True)
                self.assertEqual(client.pause_votes,[1,0])
                settle()
                self.assertEqual(client.pause_votes,[0,0])
                request(client,1);settle()
                self.assertEqual(host.pause_votes,[0,1])
                host.tick(now+counter*.05+6)
                self.assertEqual(host.pause_votes,[0,0])
                fields=(host.directory/'pause-remote.tsv').read_text().strip().split('\t')
                self.assertEqual(fields[2:],['0','0','0'])
            finally:
                host.sock.close();client.sock.close()

if __name__=='__main__': unittest.main(verbosity=2)
