from pathlib import Path
import hashlib,json,struct,tempfile,unittest
from launcher_branding import install_branding

ASSETS=Path(__file__).resolve().parents[1]/'assets/launcher'
class BrandingTests(unittest.TestCase):
    def test_multisize_icon_and_verified_bootstrap_install_preserve_runtime(self):
        icon=(ASSETS/'launcher.ico').read_bytes()
        self.assertEqual(struct.unpack_from('<HHH',icon), (0,1,7))
        self.assertEqual([icon[6+i*16] or 256 for i in range(7)],[16,24,32,48,64,128,256])
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp);release=home/'releases/0.2.4';release.mkdir(parents=True)
            files={'tools/assets/launcher.ico':icon,'tools/assets/StrayCoop.exe':(ASSETS/'StrayCoop.exe').read_bytes(),
                'tools/launcher.py':b'# launcher','tools/remote_session.py':b'# session','payload/Mods/StrayCoop/Scripts/main.lua':b'-- mod',
                'launcher-config.json':json.dumps({'version':'0.2.4','runtime':1}).encode()}
            for name,data in files.items():
                path=release/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            (release/'manifest.json').write_text(json.dumps({'version':'0.2.4','files':{n:{'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()} for n,b in files.items()}}))
            (home/'StrayCoop.exe').write_bytes(b'previous-bootstrap')
            (home/'runtime').mkdir();(home/'runtime/python.exe').write_bytes(b'unchanged-runtime')
            self.assertTrue(install_branding(home,release))
            self.assertEqual((home/'StrayCoop.exe').read_bytes(),files['tools/assets/StrayCoop.exe'])
            self.assertEqual((home/'runtime/python.exe').read_bytes(),b'unchanged-runtime')
            self.assertEqual(len(list((home/'branding-backups').iterdir())),1)
            self.assertTrue(install_branding(home,release))
            self.assertEqual(len(list((home/'branding-backups').iterdir())),1)
            (release/'tools/assets/StrayCoop.exe').write_bytes(b'tampered')
            with self.assertRaises(ValueError):install_branding(home,release)
            self.assertEqual((home/'StrayCoop.exe').read_bytes(),files['tools/assets/StrayCoop.exe'])
            self.assertFalse(install_branding(home,home/'other'))
if __name__=='__main__':unittest.main()
