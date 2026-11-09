"""Native Tk layout/state integration checks with all external actions mocked.
These inspect widget geometry, not desktop pixels or another app's UI.
"""
from pathlib import Path
import importlib.util,json,os,shutil,tempfile,unittest
from unittest.mock import patch,Mock

TOOLS=Path(__file__).resolve().parent
HOME=TOOLS.parents[0]/'dist/StrayCoop-Launcher'

class LauncherUITests(unittest.TestCase):
    def test_controls_fit_and_session_update_states_remain_usable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'tools').mkdir()
            shutil.copy2(TOOLS/'launcher.py',root/'tools/launcher.py')
            (root/'launcher-config.json').write_text(json.dumps({'version':'0.2.3','protocol':6,'runtime':1,'service':'https://test.invalid'}))
            spec=importlib.util.spec_from_file_location('launcher_ui_fixture',root/'tools/launcher.py')
            module=importlib.util.module_from_spec(spec)
            with patch.dict(os.environ,{'STRAYCOOP_LAUNCHER_HOME':str(HOME),'LOCALAPPDATA':str(root/'preferences')}):
                spec.loader.exec_module(module)
            with patch.object(module,'LobbyClient'),patch.object(module.threading,'Thread'),patch.object(module,'game_root',return_value=root),patch.object(module,'running_game',return_value=False):
                app=module.Launcher()
                try:
                    app.root.set(str(root))
                    for width,height in ((720,760),(680,720),(1000,850)):
                        app.window.geometry(f'{width}x{height}');app.window.update()
                        for widget in (app.host_button,app.join_button,app.join_entry,app.browse,app.update_button):
                            self.assertTrue(widget.winfo_ismapped(),f'{widget}: geometry={widget.winfo_geometry()} parent={widget.master.winfo_geometry()} window={app.window.winfo_geometry()}')
                            x=widget.winfo_rootx()-app.window.winfo_rootx();y=widget.winfo_rooty()-app.window.winfo_rooty()
                            self.assertGreaterEqual(x,0);self.assertGreaterEqual(y,0)
                            self.assertLessEqual(x+widget.winfo_width(),app.window.winfo_width())
                            self.assertLessEqual(y+widget.winfo_height(),app.window.winfo_height())
                    app.window.geometry('680x720');app.host_button.invoke();app.window.update()
                    self.assertTrue(app.active);self.assertEqual(app.join_button.cget('state'),'disabled')
                    self.assertTrue(app.stop_button.winfo_ismapped())
                    app.post('code','ABCDEFGH');app.drain();app.window.update()
                    self.assertEqual(app.code_display.get(),'ABCD-EFGH')
                    self.assertTrue(app.display_row.winfo_ismapped());self.assertFalse(app.host_button.winfo_ismapped())
                    self.assertGreater(app.display.winfo_width(),0)
                    self.assertGreaterEqual(app.display.winfo_width(),app.display.winfo_reqwidth())
                    app.post('ended');app.drain();app.window.update()
                    self.assertFalse(app.active);self.assertTrue(app.host_button.winfo_ismapped());self.assertFalse(app.stop_button.winfo_ismapped())
                    with patch.object(module.filedialog,'askdirectory',return_value=str(root/'Stray')):
                        app.choose()
                    self.assertEqual(app.root.get(),str(root/'Stray'))
                    self.assertEqual(json.loads((module.PREFS/'preferences.json').read_text())['game_root'],str(root/'Stray'))
                    app.code.set('abcd-efgh');app.join_button.invoke()
                    self.assertEqual(module.threading.Thread.call_args.kwargs['args'][2],'ABCDEFGH')
                    app.stop=root/'session.stop';app.end()
                    self.assertTrue(app.cancel.is_set());self.assertEqual(app.stop.read_text(),'stop\n')
                    app.post('ended');app.drain()
                    app.post('release',{'version':'0.2.3'});app.drain()
                    self.assertEqual(app.update_state.get(),'Up to date')
                    app.post('release',{'version':'0.2.4'});app.drain()
                    self.assertEqual(app.update_button.cget('text'),'Update & restart')
                    self.assertEqual(app.update_button.cget('state'),'normal')
                    # A long error wraps inside the console without hiding controls.
                    app.post('error','The session could not connect. '*12);app.drain();app.window.update()
                    self.assertTrue(app.update_button.winfo_ismapped())
                finally:app.window.destroy()

if __name__=='__main__':unittest.main()
