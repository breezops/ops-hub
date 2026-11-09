import hashlib
import json
from pathlib import Path
import socket
import tempfile
import time
import unittest
from bridge import Bridge, decode, encode, validate_state, read_local, idle_state, VERSION

KEY = 'local-test-key-123456789'

class TransportTests(unittest.TestCase):
    def test_two_sockets_and_reconnect(self):
        with tempfile.TemporaryDirectory() as tmp:
            host = Bridge('host', Path(tmp)/'host', KEY, bind='127.0.0.1', port=0)
            client = Bridge('join', Path(tmp)/'client', KEY, port=host.sock.getsockname()[1], bind='127.0.0.1')
            def pump(now):
                for bridge in (host, client):
                    bridge.tick(now)
                for bridge in (host, client):
                    while True:
                        try: data, addr = bridge.sock.recvfrom(4097)
                        except BlockingIOError: break
                        bridge.receive(data, addr, now)
            try:
                for b, x in ((host, 100), (client, 200)):
                    (b.directory/'local.tsv').write_text(f'STRAYCOOP\t6\t42\t/Game/Test\t0\t0\t{x}\t0\t0\t0\t0\t0\t0\t0\t0\t0\t{x}\t0\t0\t0\t0\t0\t1\t/Game/Animation/CAT/Walk/Test.Test\t1.25\t-1\t1\t12.0\n')
                now = time.monotonic()
                for i in range(10): pump(now+i*.05)
                self.assertTrue(host.connected(now+.5))
                self.assertTrue(client.connected(now+.5))
                self.assertEqual(host.peer_state['pose'][0], 200)
                self.assertEqual(client.peer_state['pose'][0], 100)
                self.assertEqual(client.peer_state['visual'][0], 100)
                self.assertEqual(client.peer_state['animation'],[1,'/Game/Animation/CAT/Walk/Test.Test',1.25,-1,1])
                host.publish(validate_state(host.peer_state), now+.5)
                self.assertIn('PLAYER\t2\t42', (host.directory/'remote.tsv').read_text())
                packet = dict(v=VERSION,kind='state',nonce=client.nonce,session=host.session,seq=client.sequence,state=client.peer_state)
                with self.assertRaises(ValueError): host.receive(encode(packet,host.key),client.sock.getsockname(),now+.5)
                host.tick(now+6)
                self.assertIsNone(host.peer)
                self.assertEqual(json.loads((host.directory/'bridge-status.json').read_text())['connected'],False)
                for i in range(50): pump(now+6.1+i*.05)
                self.assertTrue(host.connected(now+8.5))
                self.assertTrue(client.connected(now+8.5))
            finally:
                host.sock.close(); client.sock.close()

    def test_authentication_and_bounds(self):
        key = hashlib.sha256(KEY.encode()).digest()
        packet = encode(dict(v=VERSION,kind='hello',nonce='a'*32),key)
        self.assertEqual(decode(packet,key)['kind'],'hello')
        with self.assertRaises(ValueError): decode(packet,b'wrong-key')
        with self.assertRaises(ValueError): decode(b'x'*4097,key)
        for n in (float('nan'),float('inf'),1e30):
            with self.assertRaises(ValueError):
                validate_state(dict(idle_state(),seq=1,map='/Game/Test',chapter=0,stage=0,pose=[n,0,0,0,0,0]))

    def test_animation_validation(self):
        for animation in ([7,'-',1,-1,1],[1,'/Script/Engine.SomeClass',1,-1,1],
                          [4,'-',1,1.1,0],[1,'-',float('nan'),-1,1],[1,'-',1,-1,2]):
            with self.assertRaises(ValueError): validate_state(dict(idle_state(),animation=animation))
        with self.assertRaises(ValueError):
            decode(encode(dict(v=1,kind='hello',nonce='a'*32),b'key'),b'key')

    def test_opening_validation(self):
        for story in ([1,8,0],[2,7,1],[1,3,1],[1,True,0],[1,7],"execute"):
            with self.assertRaises(ValueError): validate_state(dict(idle_state(),story=story))

    def test_opening_shared_and_repeated_after_packet_loss(self):
        with tempfile.TemporaryDirectory() as tmp:
            host=Bridge('host',Path(tmp)/'host',KEY,bind='127.0.0.1',port=0)
            client=Bridge('join',Path(tmp)/'client',KEY,bind='127.0.0.1',port=host.sock.getsockname()[1])
            try:
                row='STRAYCOOP\t6\t42\t/Game/Test\t0\t0\t'+'\t'.join(['0']*16+['0','-','1','-1','1','12'])+'\n'
                for b,mask in ((host,1),(client,6)):
                    (b.directory/'local.tsv').write_text(row)
                    (b.directory/'story-local.tsv').write_text(f'OPENING\t1\t{mask}\t0\n')
                def pump(now,drop=False):
                    for b in (host,client): b.tick(now)
                    for b in (host,client):
                        while True:
                            try: data,addr=b.sock.recvfrom(4097)
                            except BlockingIOError: break
                            if not drop: b.receive(data,addr,now)
                now=time.monotonic()
                for i in range(10): pump(now+i*.05)
                self.assertEqual(host.shared_story,[1,7,0])
                self.assertEqual(client.shared_story,[1,7,0])
                (client.directory/'story-local.tsv').write_text('OPENING\t1\t7\t1\n')
                for i in range(10,15): pump(now+i*.05,drop=True)
                self.assertEqual(host.shared_story,[1,7,0])
                for i in range(15,25): pump(now+i*.05)
                self.assertEqual(host.shared_story,[1,7,1])
                self.assertEqual(client.shared_story,[1,7,1])
                self.assertTrue((client.directory/'story-remote.tsv').read_text().endswith('\t1\t7\t1\n'))
                # A late or cleared local snapshot must not undo shared progress.
                (client.directory/'story-local.tsv').write_text('OPENING\t1\t0\t0\n')
                for i in range(25,30): pump(now+i*.05)
                self.assertEqual(host.shared_story,[1,7,1])
            finally:
                host.sock.close();client.sock.close()

    def test_jump_snapshot_and_packet_budget(self):
        state=idle_state()
        state.update(map='/Game/Map/_MainGame/BaseMap',
                     visual=[12345.,54321.,7184.,0.,179.9,0.],
                     animation=[4,'/Game/Animation/CAT/Jump/CAT_Jump_Run_L100_H100.CAT_Jump_Run_L100_H100',1.5,.75,0])
        validate_state(state)
        packet=encode(dict(v=VERSION,kind='roster',session='a'*32,nonce='b'*32,seq=42,players={'1':state,'2':state}),b'key')
        self.assertLess(len(packet),4096)
        self.assertEqual(decode(packet,b'key')['players']['2']['animation'][3],.75)

    def test_partial_file_keeps_last_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'local.tsv'
            previous = idle_state()
            previous['map'] = '/Game/Test'
            path.write_text('STRAYCOOP\t1\t')
            self.assertEqual(read_local(path,previous)['map'], '/Game/Test')
            import os
            os.utime(path,(time.time()-3,time.time()-3))
            self.assertEqual(read_local(path,previous)['map'], '-')

if __name__ == '__main__': unittest.main(verbosity=2)
