"""Local stdio UI adapter. Networking/session/update methods stay in Launcher.
No HTTP listener or browser-accessible game/session credentials are introduced.
"""
import heapq,json,queue,sys,threading,time,os
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parent))
import launcher as legacy

class UiProcesses:
    """Session/restart children must not inherit the UI's blocking input pipe.
    All existing launcher child commands already supply noninteractive args.
    """
    def __init__(self,module):self.module=module
    def __getattr__(self,name):return getattr(self.module,name)
    def Popen(self,*args,**kwargs):
        kwargs.setdefault('stdin',self.module.DEVNULL)
        return self.module.Popen(*args,**kwargs)

class Value:
    def __init__(self,app,value=''):self.app=app;self.value=value
    def get(self):return self.value
    def set(self,value):self.value=value;self.app.dirty=True

class Widget:
    def __init__(self,app,text=''):self.app=app;self.options={'state':'normal','text':text};self.visible=False
    def configure(self,**values):self.options.update(values);self.app.dirty=True
    def pack(self,**_):self.visible=True;self.app.dirty=True
    def pack_forget(self):self.visible=False;self.app.dirty=True

class Window:
    def __init__(self,app):self.app=app;self.jobs=[];self.counter=0
    def after(self,ms,callback):
        self.counter+=1;heapq.heappush(self.jobs,(time.monotonic()+ms/1000,self.counter,callback))
    def destroy(self):self.app.emit({'kind':'exit'});self.app.running=False
    def clipboard_clear(self):pass
    def clipboard_append(self,value):self.app.emit({'kind':'clipboard','value':value})

class Notices:
    def __init__(self,app):self.app=app
    def showerror(self,title,message):self.app.emit({'kind':'notice','title':title,'message':str(message)})
    showinfo=showerror

class Controller(legacy.Launcher):
    def __init__(self,emit):
        self.emit=emit;self.dirty=True;self.running=True;self.logs=[]
        if not isinstance(legacy.subprocess,UiProcesses):legacy.subprocess=UiProcesses(legacy.subprocess)
        legacy.PREFS.mkdir(parents=True,exist_ok=True)
        try:self.prefs=json.loads((legacy.PREFS/'preferences.json').read_text())
        except (OSError,ValueError):self.prefs={}
        self.events=queue.Queue();self.active=False;self.proc=None;self.stop=None;self.update=None;self.closing=False;self.cancel=threading.Event();self.had_error=False
        self.client=legacy.LobbyClient(legacy.CONFIG['service'],legacy.CONFIG['version'],legacy.CONFIG['protocol'])
        self.window=Window(self)
        root=self.prefs.get('game_root','')
        if not root:
            for parent in legacy.HOME.parents:
                if (parent/'Hk_project/Binaries/Win64/Stray-Win64-Shipping.exe').is_file():root=str(parent);break
        for name,value in {'root':root,'code':'','code_display':'','hint':legacy.HOST_NOTICE,
            'status':'Ready. In cinematics, hold Space to skip for both players.',
            'version':'Version '+legacy.CONFIG['version'],'update_state':'Checking for updates…'}.items():setattr(self,name,Value(self,value))
        for name in ('host_button','join_button','browse','join_entry','stop_button','copy_button','update_label'):setattr(self,name,Widget(self))
        self.update_button=Widget(self,'Check for updates');self.folder_field=SimpleNamespace(entry=Widget(self))
        legacy.messagebox=Notices(self)
        self.write('Stray starts automatically after your partner connects.')
        self.window.after(100,self.drain);self.window.after(500,self.check_updates)
        self.window.after(250,self.apply_branding)
    def paint_status(self,*_):self.dirty=True
    def show_code(self,visible):self.dirty=True
    def write(self,line):self.logs.append(str(line));self.logs=self.logs[-250:];self.dirty=True
    def close(self):
        if self.active:self.emit({'kind':'confirm_close'})
        else:self.window.destroy()
    def snapshot(self):
        return {'directory':self.root.get(),'status':self.status.get(),'version':self.version.get(),
            'updateState':self.update_state.get(),'updateText':self.update_button.options['text'],
            'updateDisabled':self.update_button.options['state']=='disabled',
            'stopDisabled':self.stop_button.options['state']=='disabled','active':self.active,
            'sessionCode':self.code_display.get(),'error':self.had_error,'logs':self.logs}
    def command(self,message):
        if not isinstance(message,dict) or not isinstance(message.get('action'),str):return
        action=message['action']
        if action in ('host','join'):
            if self.active:return
            root=message.get('root',self.root.get());code=message.get('code','')
            if not isinstance(root,str) or len(root)>4096 or not isinstance(code,str) or len(code)>64:return
            self.root.set(root);self.code.set(code);self.start(action)
        elif action=='folder':
            path=message.get('path')
            if not self.active and isinstance(path,str) and len(path)<=4096:self.root.set(path);self.save()
        elif action=='directory':
            path=message.get('root')
            if not self.active and isinstance(path,str) and len(path)<=4096:self.root.set(path)
        elif action=='end':
            if self.active:self.end()
        elif action=='updates':
            if self.update_button.options['state']!='disabled':self.check_updates()
        elif action=='copy':
            if self.code_display.get():self.copy_code()
        elif action=='close':self.close()
        elif action=='confirm_close':
            if self.active:self.closing=True;self.end()
            else:self.window.destroy()
        elif action=='ready':self.dirty=True
        elif action=='disconnect':
            if self.active:self.closing=True;self.end()
            else:self.running=False

def main():
    trace_path=os.environ.get('STRAYCOOP_BACKEND_TRACE')
    def trace(value):
        if trace_path:
            with open(trace_path,'a',encoding='utf-8') as f:f.write(str(time.monotonic())+' '+str(value)+'\n')
    if trace_path:
        original_popen=legacy.subprocess.Popen
        class OutputTrace:
            def __init__(self,stream):self.stream=stream
            def __iter__(self):
                for line in self.stream:trace('session stdout '+repr(line));yield line
        def traced_popen(*args,**kwargs):
            process=original_popen(*args,**kwargs)
            if process.stdout is not None:process.stdout=OutputTrace(process.stdout)
            return process
        legacy.subprocess.Popen=traced_popen
    output_lock=threading.Lock();broken=False
    def emit(value):
        nonlocal broken
        if broken:return
        try:
            with output_lock:sys.stdout.write(json.dumps(value,ensure_ascii=True)+'\n');sys.stdout.flush()
        except (BrokenPipeError,OSError):broken=True
    app=Controller(emit);commands=queue.Queue()
    def receive():
        try:
            while True:
                line=sys.stdin.readline(8193)
                if not line:break
                if len(line)>8192:break
                try:commands.put(json.loads(line))
                except ValueError:continue
        finally:commands.put({'action':'disconnect'})
    threading.Thread(target=receive,daemon=True).start()
    disconnected=None
    while app.running:
        while not commands.empty():app.command(commands.get())
        if broken and disconnected is None:disconnected=time.monotonic();app.command({'action':'disconnect'})
        now=time.monotonic()
        while app.window.jobs and app.window.jobs[0][0]<=now:
            _,_,callback=heapq.heappop(app.window.jobs);callback()
        if app.dirty and not broken:
            app.dirty=False;trace('state '+app.status.get());emit({'kind':'state','state':app.snapshot()})
        if disconnected and now-disconnected>60:
            if app.proc and app.proc.poll() is None:app.proc.terminate()
            break
        time.sleep(.025)

if __name__=='__main__':main()
