import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import remote_session
from lobby_client import normalize_code,LobbyClient
from launcher_update import stage_archive,safe_name

class LauncherTests(unittest.TestCase):
    def test_versioned_launcher_installs_from_the_fixed_loader_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);home=root/'launcher';release=home/'releases/0.2.0'
            payload=release/'payload/Mods/StrayCoop/Scripts';payload.mkdir(parents=True)
            (payload/'main.lua').write_text('-- mod')
            loader=home/'loader';loader.mkdir()
            for name in ('dwmapi.dll','UE4SS.dll','UE4SS-settings.ini'):(loader/name).write_text(name)
            game=root/'game'
            with patch.object(remote_session,'PACKAGE',release),patch.object(remote_session,'running_game',return_value=False),patch.dict(remote_session.os.environ,{'STRAYCOOP_LAUNCHER_HOME':str(home)}):
                bin=remote_session.install(game)
            self.assertEqual((bin/'UE4SS.dll').read_text(),'UE4SS.dll')
            self.assertEqual((bin/'Mods/StrayCoop/Scripts/main.lua').read_text(),'-- mod')
    def test_codes_and_service_origin(self):
        self.assertEqual(normalize_code('abcd-efgh'),'ABCDEFGH')
        for value in ('12345678','ABCDEFG','../../anything'):
            with self.assertRaises(ValueError):normalize_code(value)
        for value in ('http://example.com','https://user:password@example.com','https://example.com/path'):
            with self.assertRaises(ValueError):LobbyClient(value,'0.2.0',5)

    def test_rejects_windows_escape_paths(self):
        for path in ('../other','/payload/evil','C:/payload/evil','tools\\evil.py','tools/CON.txt','tools/name.','runtime/python.exe'):
            with self.assertRaises(ValueError):safe_name(path)

    def build(self,tmp,bad=False):
        files={'tools/launcher.py':b'# launcher','tools/remote_session.py':b'# session',
               'payload/Mods/StrayCoop/Scripts/main.lua':b'-- mod',
               'launcher-config.json':json.dumps({'version':'0.2.1','runtime':1}).encode()}
        manifest={'version':'0.2.1','files':{p:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()} for p,data in files.items()}}
        archive=Path(tmp)/'update.zip'
        with zipfile.ZipFile(archive,'w') as z:
            for p,data in files.items():z.writestr(p,data)
            z.writestr('manifest.json',json.dumps(manifest))
            if bad:z.writestr('../escape.txt','bad')
        return archive,hashlib.sha256(archive.read_bytes()).hexdigest()

    def test_verified_update_switches_pointer_without_replacing_runtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp)/'launcher';home.mkdir();(home/'current.json').write_text('{"version":"0.2.0"}')
            runtime=home/'runtime';runtime.mkdir();(runtime/'python.exe').write_bytes(b'fixed-runtime')
            archive,digest=self.build(tmp)
            release=stage_archive(home,archive,'0.2.1',digest)
            self.assertTrue((release/'tools/launcher.py').exists())
            self.assertEqual(json.loads((home/'current.json').read_text()),{'version':'0.2.1','previous':'0.2.0'})
            self.assertEqual((runtime/'python.exe').read_bytes(),b'fixed-runtime')

    def test_bad_download_or_traversal_never_changes_current_release(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp)/'launcher';home.mkdir();pointer=home/'current.json';pointer.write_text('{"version":"0.2.0"}')
            archive,digest=self.build(tmp,bad=True)
            with self.assertRaises(ValueError):stage_archive(home,archive,'0.2.1',digest)
            self.assertEqual(json.loads(pointer.read_text())['version'],'0.2.0')
            self.assertFalse((Path(tmp)/'escape.txt').exists())
            with self.assertRaises(ValueError):stage_archive(home,archive,'0.2.1','0'*64)

if __name__=='__main__':unittest.main(verbosity=2)
