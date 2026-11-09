import copy
from pathlib import Path
import tempfile
import time
import unittest
from bridge import Bridge, VERSION, encode
from shared_objects import Authority, validate_object, validate_record, read_objects, render_tsv

IDENT='/Game/Map/Test.Test:PersistentLevel.Bucket_2.COMP_Grabbable'
def obj(seq=1,held=1,x=10):
    return dict(id=IDENT,seq=seq,held=held,parts=[['Anse',x,20,30,0,179,0,.45,.45,.35,0,0,0,0],
                                               ['bucket',x,20,20,0,179,0,.45,.45,.35,0,0,0,1]])

class ObjectTests(unittest.TestCase):
    def test_fan_result_requires_a_known_bucket_and_correct_driver(self):
        a=Authority();e={'fan':'/Game/Map/Test.Test:PersistentLevel.Fan_2','bucket':IDENT}
        self.assertFalse(a.effect(2,e))
        a.merge(1,obj());self.assertFalse(a.effect(2,e))
        self.assertTrue(a.effect(1,e));self.assertFalse(a.effect(1,e))
    def test_pickup_conflict_drop_and_transfer(self):
        a=Authority();self.assertTrue(a.merge(2,obj()))
        self.assertFalse(a.merge(1,obj()))
        self.assertEqual(a.records[IDENT]['owner'],2)
        self.assertTrue(a.merge(2,obj(2,0)))
        self.assertTrue(a.merge(1,obj(2)))
        self.assertEqual(a.records[IDENT]['owner'],1)
        self.assertFalse(a.merge(2,obj(3,0)))
        self.assertFalse(a.merge(1,obj(1)))

    def test_lost_pickup_can_be_recovered_from_drop(self):
        a=Authority();self.assertTrue(a.merge(2,obj(5,0)))
        self.assertEqual(a.records[IDENT]['owner'],0)
        a.disconnect(2);self.assertEqual(a.records[IDENT]['owner'],0)
        a.merge(2,obj(6));a.disconnect(2)
        r=a.records[IDENT];self.assertEqual(r['owner'],0)
        self.assertTrue(all(p[13]==1 for p in r['object']['parts']))

    def test_bounded_schema_rejects_paths_numbers_duplicates_and_malformed_owners(self):
        for patch in ({'id':'/Script/Engine.Default__GameplayStatics'},{'seq':True},{'held':2},
                      {'parts':[]},{'parts':[['Anse']*14]}):
            with self.assertRaises(ValueError): validate_object(dict(obj(),**patch))
        for value in (float('nan'),float('inf'),1e15):
            o=obj();o['parts'][0][1]=value
            with self.assertRaises(ValueError): validate_object(o)
        o=obj();o['parts'][1][0]='Anse'
        with self.assertRaises(ValueError): validate_object(o)
        a=Authority();a.merge(1,obj());r=copy.deepcopy(a.records[IDENT]);r['owner']=2
        with self.assertRaises(ValueError): validate_record(r)

    def test_partial_file_and_two_part_bucket(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'objects-local.tsv'
            o=obj();rows=['OBJECTS\t1\n']
            for part in o['parts']:
                rows.append('\t'.join(str(v) for v in ['OBJECT',o['id'],o['seq'],o['held'],*part])+'\n')
            p.write_text(''.join(rows));self.assertEqual(read_objects(p),[o])
            p.write_text(''.join(rows)[:-1]);self.assertIsNone(read_objects(p))
            a=Authority();a.merge(2,o)
            self.assertEqual(len(render_tsv(a.records,1,1,True).splitlines()),3)

    def test_two_sockets_separate_channel_retries_and_replay(self):
        with tempfile.TemporaryDirectory() as tmp:
            h=Bridge('host',Path(tmp)/'host','object-test-key-123456',bind='127.0.0.1',port=0)
            c=Bridge('join',Path(tmp)/'client','object-test-key-123456',bind='127.0.0.1',port=h.sock.getsockname()[1])
            now=time.monotonic()
            def pump(i,drop=False):
                for b in (h,c): b.tick(now+i*.05)
                for b in (h,c):
                    while True:
                        try: data,addr=b.sock.recvfrom(4097)
                        except BlockingIOError: break
                        if not drop: b.receive(data,addr,now+i*.05)
            try:
                # A valid player scene is required; main movement channel is
                # independently authenticated and unaffected by object packets.
                row=f'STRAYCOOP\t{VERSION}\t1\t/Game/Map/Test\t2\t0\t'+'\t'.join(['0']*16+['0','-','1','-1','1','12'])+'\n'
                for b in (h,c): (b.directory/'local.tsv').write_text(row)
                o=obj();text='OBJECTS\t1\n'+''.join('\t'.join(map(str,['OBJECT',o['id'],1,1,*p]))+'\n' for p in o['parts'])
                (c.directory/'objects-local.tsv').write_text(text)
                for i in range(10): pump(i,drop=i in (3,4))
                self.assertEqual(h.objects.records[IDENT]['owner'],2)
                self.assertEqual(c.objects.records[IDENT]['owner'],2)
                self.assertIn('\tAnse\t',(h.directory/'objects-remote.tsv').read_text())
                fan='/Game/Map/Test.Test:PersistentLevel.Fan_2'
                (c.directory/'objects-local.tsv').write_text(text+f'FAN\t{fan}\t{IDENT}\n')
                for i in range(10,18): pump(i,drop=i in (10,11))
                self.assertIn(fan,h.objects.effects)
                self.assertIn(fan,c.objects.effects)
                self.assertIn('FAN\t'+fan,(h.directory/'objects-remote.tsv').read_text())
                packet=dict(v=VERSION,kind='object_request',nonce=c.nonce,session=h.session,
                            seq=1,map='/Game/Map/Test',chapter=2,object=o)
                with self.assertRaises(ValueError): h.receive(encode(packet,h.key),c.sock.getsockname(),now+.5)
                h.tick(now+6)
                self.assertEqual(h.objects.records[IDENT]['owner'],0)
            finally: h.sock.close();c.sock.close()

if __name__=='__main__': unittest.main(verbosity=2)
