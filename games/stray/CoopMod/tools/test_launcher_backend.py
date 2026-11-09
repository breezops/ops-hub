from pathlib import Path
import ast,importlib.util,json,os,sys,tempfile,unittest,shutil
from unittest.mock import patch
TOOLS=Path(__file__).resolve().parent
HOME=TOOLS.parent/'dist/StrayCoop-Launcher'

class WebBackendTests(unittest.TestCase):
    def test_existing_session_and_update_methods_are_unchanged(self):
        old=ast.parse((HOME/'releases/0.2.4/tools/launcher.py').read_text(encoding='utf-8'))
        new=ast.parse((TOOLS/'launcher.py').read_text(encoding='utf-8'))
        def methods(tree):
            cls=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='Launcher')
            return {n.name:ast.dump(n,include_attributes=False) for n in cls.body if isinstance(n,ast.FunctionDef)}
        before,after=methods(old),methods(new)
        for name in ('start','session','save','end','check_updates','install_update','drain','close'):
            self.assertEqual(before[name],after[name],name)

    def test_headless_adapter_preserves_input_session_and_confirmed_close(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'tools').mkdir();shutil.copy2(TOOLS/'launcher.py',root/'tools/launcher.py')
            (root/'launcher-config.json').write_text(json.dumps({'version':'0.2.5','protocol':6,'service':'https://test.invalid'}))
            spec=importlib.util.spec_from_file_location('web_legacy_fixture',root/'tools/launcher.py');legacy=importlib.util.module_from_spec(spec)
            with patch.dict(os.environ,{'STRAYCOOP_LAUNCHER_HOME':str(HOME),'LOCALAPPDATA':str(root/'prefs')}):spec.loader.exec_module(legacy)
            spec=importlib.util.spec_from_file_location('backend_fixture',TOOLS/'launcher_backend.py');backend=importlib.util.module_from_spec(spec)
            with patch.dict(sys.modules,{'launcher':legacy}):spec.loader.exec_module(backend)
            events=[]
            with patch.object(legacy,'LobbyClient'),patch.object(legacy.threading,'Thread'),patch.object(legacy,'game_root',return_value=root),patch.object(legacy,'running_game',return_value=False):
                app=backend.Controller(events.append)
                app.command({'action':'directory','root':'new typed folder'})
                self.assertEqual(app.snapshot()['directory'],'new typed folder')
                app.command({'action':'join','root':str(root),'code':'abcd-efgh'})
                self.assertTrue(app.active)
                self.assertEqual(legacy.threading.Thread.call_args.kwargs['args'][2],'ABCDEFGH')
                app.command({'action':'directory','root':'ignored while playing'})
                self.assertEqual(app.root.get(),str(root))
                app.post('code','ABCDEFGH');app.drain()
                self.assertEqual(app.snapshot()['sessionCode'],'ABCD-EFGH')
                app.command({'action':'copy'})
                self.assertEqual(events[-1],{'kind':'clipboard','value':'ABCD-EFGH'})
                app.command({'action':'close'});self.assertEqual(events[-1]['kind'],'confirm_close')
                self.assertTrue(app.running)
                app.stop=root/'session.stop';app.command({'action':'confirm_close'})
                self.assertEqual(app.stop.read_text(),'stop\n')
                app.post('ended');app.drain();self.assertFalse(app.running)
                self.assertEqual(events[-1]['kind'],'exit')

    def test_frontend_keeps_required_ids_and_uses_only_local_fonts(self):
        page=(TOOLS.parent/'assets/launcher-web/index.html').read_text(encoding='utf-8')
        for name in ('game-folder-input','browse-btn','host-btn','join-code-input','join-btn','status-text','log-output','version-text','check-updates-btn','min-btn','close-btn'):
            self.assertEqual(page.count('id="'+name+'"'),1,name)
        self.assertNotIn('fonts.googleapis.com',page)
        self.assertNotIn('value="F:',page)

    def test_no_interactive_session_child_inherits_the_ui_control_pipe(self):
        tree=ast.parse((TOOLS/'launcher_backend.py').read_text(encoding='utf-8'))
        node=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='UiProcesses')
        namespace={};exec(compile(ast.Module(body=[node],type_ignores=[]),'adapter','exec'),namespace)
        from unittest.mock import Mock
        process_module=Mock();process_module.DEVNULL=-3
        facade=namespace['UiProcesses'](process_module)
        facade.Popen(['python','remote_session.py','host'],stdout=-1)
        self.assertEqual(process_module.Popen.call_args.kwargs['stdin'],-3)

if __name__=='__main__':unittest.main()
