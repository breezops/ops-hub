from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch
import checkpoint_harness as harness


class HarnessTests(unittest.TestCase):
    def test_parses_real_telemetry_shape(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'state.tsv'
            path.write_text('HARNESS\t1\t123\nCHAPTER\t4\nZONE\t/Game/Map/Slums\n'
                            'PLAYER\tBP_CatPawn_C /Game/Test.Cat\t1\t2\t3\ttrue\tfalse\n'
                            'SEQUENCE\tSequence /Game/Test.Event\tCheckpoint_A\nGEAR\ttrue\ttrue\n')
            state=harness.read_state(path)
            self.assertEqual(state['chapter'],4)
            self.assertEqual(state['player']['position'],[1.,2.,3.])
            self.assertEqual(state['sequences'][0]['checkpoint'],'Checkpoint_A')
            self.assertTrue(state['gear_objects']['drone'])

    def test_partial_or_invalid_telemetry_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'state.tsv'
            for text in ('HARNESS\t1\t123','HARNESS\t2\t123\n','HARNESS\t1\t123\nCHAPTER\t4\n',
                         'HARNESS\t1\t123\nCHAPTER\t4\nZONE\t/Test\nPLAYER\tx\tnan\t2\t3\ttrue\tfalse\n'):
                path.write_text(text)
                with self.subTest(text=text),self.assertRaises(ValueError): harness.read_state(path)

    def test_requires_active_isolated_session(self):
        with tempfile.TemporaryDirectory() as tmp,patch.object(harness,'PROJECT',Path(tmp)):
            reports=Path(tmp)/'reports';reports.mkdir()
            path=reports/'local-session.json'
            for session in ({'running':False},{'running':True,'harness':False},
                            {'running':True,'harness':True,'profile':'../../normal-saves'}):
                path.write_text(json.dumps(session))
                with self.subTest(session=session),self.assertRaises(RuntimeError): harness.active_session()
            session={'running':True,'harness':True,'profile':'harness-safe-01'}
            path.write_text(json.dumps(session))
            self.assertEqual(harness.active_session(),session)

    def test_stale_telemetry_rejected(self):
        with patch.object(harness,'read_state',return_value={'timestamp':0}):
            with self.assertRaises(ValueError): harness.states()


if __name__=='__main__': unittest.main(verbosity=2)
