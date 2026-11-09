"""Windowed co-op launcher. The fixed runtime hosts versioned mod releases."""
import json
import os
from pathlib import Path
import queue
import secrets
import subprocess
import sys
import threading
import time
sys.dont_write_bytecode=True

RELEASE=Path(__file__).resolve().parents[1]
HOME=Path(os.environ.get('STRAYCOOP_LAUNCHER_HOME',RELEASE)).resolve()
os.environ['TCL_LIBRARY']=str(HOME/'runtime/tcl/tcl8.6')
os.environ['TK_LIBRARY']=str(HOME/'runtime/tcl/tk8.6')
sys.path.insert(0,str(Path(__file__).resolve().parent))
import tkinter as tk
from tkinter import filedialog,messagebox
from launcher_ui import BG,CARD,FG,MUTED,DIM,ACCENT,GREEN,Card,Button,InputField,Header,Notice
from lobby_client import LobbyClient,normalize_code
from launcher_update import download_update
from remote_session import game_root,running_game
from launcher_branding import set_process_identity,set_window_icon,install_branding

CONFIG=json.loads((RELEASE/'launcher-config.json').read_text())
PREFS=Path(os.environ.get('LOCALAPPDATA',str(HOME)))/'StrayCoopLauncher'
HOST_NOTICE='Host setup requires UDP port 7777 forwarded on your router.'

class Launcher:
    def __init__(self):
        PREFS.mkdir(parents=True,exist_ok=True)
        try:self.prefs=json.loads((PREFS/'preferences.json').read_text())
        except (OSError,ValueError):self.prefs={}
        self.events=queue.Queue();self.active=False;self.proc=None;self.stop=None;self.update=None;self.closing=False;self.cancel=threading.Event();self.had_error=False
        self.client=LobbyClient(CONFIG['service'],CONFIG['version'],CONFIG['protocol'])
        set_process_identity()
        self.window=tk.Tk();set_window_icon(self.window,RELEASE);self.window.title('Stray Co-op');self.window.geometry('720x760');self.window.minsize(680,720);self.window.configure(bg=BG)
        self.window.protocol('WM_DELETE_WINDOW',self.close)
        box=tk.Frame(self.window,bg=BG);box.pack(fill='both',expand=True,padx=36,pady=26)
        box.columnconfigure(0,weight=1);box.rowconfigure(4,weight=1)
        Header(box).grid(row=0,column=0,sticky='ew')
        folder=Card(box);folder.grid(row=1,column=0,sticky='ew',pady=(0,14))
        self.root=tk.StringVar(value=self.prefs.get('game_root',''))
        if not self.root.get():
            for p in HOME.parents:
                if (p/'Hk_project/Binaries/Win64/Stray-Win64-Shipping.exe').is_file():self.root.set(str(p));break
        folder.body.columnconfigure(0,weight=1)
        tk.Label(folder.body,text='GAME DIRECTORY',bg=CARD,fg=DIM,font=('Segoe UI',9,'bold')).grid(row=0,column=0,columnspan=2,sticky='w',pady=(0,8))
        self.folder_field=InputField(folder.body,self.root,folder=True);self.folder_field.grid(row=1,column=0,sticky='ew',padx=(0,10))
        self.browse=Button(folder.body,text='Browse',command=self.choose);self.browse.grid(row=1,column=1)
        actions=tk.Frame(box,bg=BG);actions.grid(row=2,column=0,sticky='ew',pady=(0,14))
        actions.columnconfigure((0,1),weight=1,uniform='actions')
        host=Card(actions);host.grid(row=0,column=0,sticky='nsew',padx=(0,7))
        join=Card(actions);join.grid(row=0,column=1,sticky='nsew',padx=(7,0))
        host.body.columnconfigure(0,weight=1);join.body.columnconfigure(0,weight=1)
        self.host_caption=tk.StringVar(value='HOST A SESSION')
        tk.Label(host.body,textvariable=self.host_caption,bg=CARD,fg=DIM,font=('Segoe UI',9,'bold')).grid(row=0,column=0,sticky='w',pady=(0,8))
        tk.Label(join.body,text='DIRECT CONNECT',bg=CARD,fg=DIM,font=('Segoe UI',9,'bold')).grid(row=0,column=0,columnspan=2,sticky='w',pady=(0,8))
        self.host_button=Button(host.body,text='Host a Game',variant='primary',arrow=True,command=lambda:self.start('host'));self.host_button.grid(row=1,column=0,sticky='ew')
        self.code=tk.StringVar()
        self.join_field=InputField(join.body,self.code,mono=True,placeholder='Join code…');self.join_field.grid(row=1,column=0,sticky='ew',padx=(0,8))
        self.join_entry=self.join_field.entry;self.join_entry.bind('<Return>',lambda _:self.start('join'))
        self.join_button=Button(join.body,text='Join',variant='outline',command=lambda:self.start('join'));self.join_button.grid(row=1,column=1)
        self.code_display=tk.StringVar(value='')
        self.display_row=tk.Frame(host.body,bg=CARD,height=42)
        self.display=tk.Label(self.display_row,textvariable=self.code_display,bg=CARD,fg=ACCENT,font=('Consolas',18,'bold'),anchor='w');self.display.pack(side='left',fill='x',expand=True)
        self.copy_button=Button(self.display_row,text='Copy',command=self.copy_code,variant='outline');self.copy_button.pack(side='right')
        self.hint=tk.StringVar(value=HOST_NOTICE)
        Notice(box,self.hint).grid(row=3,column=0,sticky='ew',pady=(0,14))
        console=Card(box,padding=14);console.grid(row=4,column=0,sticky='nsew')
        console.body.columnconfigure(1,weight=1);console.body.rowconfigure(1,weight=1)
        self.status_dot=tk.Canvas(console.body,width=14,height=20,bg=CARD,highlightthickness=0)
        self.status_dot.grid(row=0,column=0,sticky='nw',padx=(0,6))
        self.status=tk.StringVar(value='Ready. In cinematics, hold Space to skip for both players.')
        self.status_label=tk.Label(console.body,textvariable=self.status,bg=CARD,fg=MUTED,font=('Segoe UI',9),anchor='w',justify='left',wraplength=550)
        self.status_label.grid(row=0,column=1,sticky='ew',pady=(0,10))
        console.body.bind('<Configure>',lambda e:self.status_label.configure(wraplength=max(100,e.width-26)))
        self.status.trace_add('write',self.paint_status)
        self.log=tk.Text(console.body,height=5,bg='#06080a',fg='#95a1b2',font=('Consolas',9),relief='flat',wrap='word',state='disabled',padx=12,pady=10,highlightthickness=1,highlightbackground='#1b2028')
        self.log.grid(row=1,column=0,columnspan=2,sticky='nsew')
        self.write('Stray starts automatically after your partner connects.')
        bottom=tk.Frame(box,bg=BG);bottom.grid(row=5,column=0,sticky='ew',pady=(18,0))
        tk.Frame(bottom,bg='#242a33',height=1).pack(fill='x',pady=(0,14))
        footer=tk.Frame(bottom,bg=BG);footer.pack(fill='x')
        metadata=tk.Frame(footer,bg=BG);metadata.pack(side='left',fill='x',expand=True)
        self.version=tk.StringVar(value='Version '+CONFIG['version'])
        tk.Label(metadata,textvariable=self.version,bg=BG,fg=DIM,font=('Segoe UI',9)).pack(anchor='w')
        self.update_state=tk.StringVar(value='Checking for updates…')
        self.update_label=tk.Label(metadata,textvariable=self.update_state,bg=BG,fg=MUTED,font=('Segoe UI',8));self.update_label.pack(anchor='w')
        self.update_button=Button(footer,text='Check for updates',variant='ghost',command=self.check_updates);self.update_button.pack(side='right')
        self.stop_button=Button(footer,text='End session',variant='danger',command=self.end)
        self.paint_status()
        self.window.after(100,self.drain);self.window.after(500,self.check_updates)
        self.window.after(250,self.apply_branding)

    def apply_branding(self,attempt=0):
        try:install_branding(HOME,RELEASE)
        except OSError:
            if attempt<3:self.window.after(500,lambda:self.apply_branding(attempt+1))
        except ValueError as error:self.write('Launcher icon update could not be verified: '+str(error))

    def paint_status(self,*_):
        color='#f18a78' if self.had_error else ACCENT if self.active else GREEN
        self.status_dot.delete('all');self.status_dot.create_oval(3,7,9,13,fill=color,outline='')

    def show_code(self,visible):
        if visible:
            self.host_button.grid_remove();self.display_row.grid(row=1,column=0,sticky='ew');self.host_caption.set('YOUR SESSION CODE')
        else:
            self.display_row.grid_remove();self.host_button.grid();self.host_caption.set('HOST A SESSION')

    def choose(self):
        folder=filedialog.askdirectory(title='Choose the Stray folder containing Hk_project',initialdir=self.root.get() or None)
        if folder:self.root.set(folder);self.save()
    def save(self):
        self.prefs['game_root']=self.root.get().strip();(PREFS/'preferences.json').write_text(json.dumps(self.prefs,indent=2))
    def post(self,kind,value=None):self.events.put((kind,value))
    def write(self,line):
        self.log.configure(state='normal');self.log.insert('end',line+'\n');self.log.see('end');self.log.configure(state='disabled')
    def set_active(self,value):
        self.active=value
        for button in (self.host_button,self.join_button,self.browse):button.configure(state='disabled' if value else 'normal')
        self.join_entry.configure(state='disabled' if value else 'normal')
        self.folder_field.entry.configure(state='disabled' if value else 'normal')
        self.paint_status()
        if value:self.stop_button.pack(side='right',padx=8)
        else:self.stop_button.pack_forget()
        self.stop_button.configure(state='normal')
    def start(self,role):
        if self.active:return
        try:
            if not self.root.get().strip():raise ValueError('Choose your Stray game folder once using Browse.')
            root=game_root(self.root.get())
            if running_game():raise ValueError('Close Stray first. The launcher will start it for you.')
            code=normalize_code(self.code.get()) if role=='join' else None
        except Exception as e:messagebox.showerror('Cannot start',str(e));return
        self.cancel.clear();self.had_error=False
        self.save();self.set_active(True);self.status.set('Creating a lobby…' if role=='host' else 'Looking up your friend’s lobby…')
        self.code_display.set('');self.show_code(False)
        threading.Thread(target=self.session,args=(role,root,code),daemon=True).start()
    def session(self,role,root,code):
        owned=None;keyfile=None;stopfile=None;heartbeat_stop=threading.Event()
        try:
            lobby=self.client.host() if role=='host' else self.client.join(code)
            if role=='host':owned=lobby;self.post('code',lobby['code'])
            else:self.post('status','Connecting to your friend…')
            if self.cancel.is_set():return
            session=PREFS/'sessions';session.mkdir(exist_ok=True)
            ident=secrets.token_hex(8);keyfile=session/(ident+'.key');stopfile=session/(ident+'.stop')
            keyfile.write_text(lobby['key'],encoding='ascii');self.stop=stopfile
            if self.cancel.is_set():return
            args=[str(HOME/'runtime/python.exe'),'-B','-u',str(RELEASE/'tools/remote_session.py'),role,'--game-root',str(root),'--key-file',str(keyfile),'--stop-file',str(stopfile),'--quiet-key','--port',str(lobby['port'])]
            if role=='join':args.extend(['--host',lobby['host']])
            if owned:
                def renew():
                    while not heartbeat_stop.wait(45):
                        try:self.client.renew(owned['code'],owned['owner_token'])
                        except Exception:self.post('log','Could not refresh the join code. Your existing game connection can continue.')
                threading.Thread(target=renew,daemon=True).start()
            self.proc=subprocess.Popen(args,cwd=RELEASE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace',creationflags=subprocess.CREATE_NO_WINDOW)
            for line in self.proc.stdout:
                line=line.strip()
                if not line:continue
                if line.startswith('CONNECTED.'):
                    self.post('status','Connected. Starting Stray…')
                    self.post('log','Both players: choose Start Game → Slot 1 → New Game.')
                elif line.startswith('Waiting for'):
                    self.post('status','Waiting for your friend to join…')
                    self.post('log','Stray starts automatically when the other player connects.')
                elif line.startswith('Game frame rate:'):self.post('status',line.split(';')[0])
                elif line.startswith('Partner '):self.post('status',line);self.post('log',line)
                elif line.startswith('ERROR:'):self.post('error',line[6:].strip())
                elif line.startswith('Test ended.'):self.post('log','Session closed. Diagnostics saved in StrayCoopTests.')
            result=self.proc.wait()
            if result and not (stopfile and stopfile.exists()):self.post('error','The session stopped. See the details above.')
        except Exception as e:self.post('error',str(e))
        finally:
            heartbeat_stop.set()
            if owned:
                try:self.client.close(owned['code'],owned['owner_token'])
                except Exception:pass
            if keyfile:keyfile.unlink(missing_ok=True)
            if stopfile:stopfile.unlink(missing_ok=True)
            self.proc=None;self.stop=None;self.post('ended')
    def end(self):
        self.cancel.set()
        self.status.set('Closing this co-op session…');self.stop_button.configure(state='disabled')
        if self.stop:self.stop.write_text('stop\n')
    def copy_code(self):
        self.window.clipboard_clear();self.window.clipboard_append(self.code_display.get());self.hint.set('Send this code to your friend. Leave the launcher open.')
    def check_updates(self):
        if self.update and self.active:messagebox.showinfo('End the session first','Finish this co-op session before installing an update.');return
        if self.update and not self.active:
            if running_game():messagebox.showinfo('Close Stray','Close Stray before installing an update.');return
            self.update_button.configure(state='disabled');self.status.set('Downloading the tested update…')
            threading.Thread(target=self.install_update,daemon=True).start();return
        self.update_button.configure(state='disabled')
        def check():
            try:self.post('release',self.client.latest())
            except Exception as e:self.post('update_error',str(e))
        threading.Thread(target=check,daemon=True).start()
    def install_update(self):
        try:
            download_update(HOME,self.client.base,self.update)
            self.post('restart')
        except Exception as e:self.post('error',str(e));self.post('update_ready')
    def drain(self):
        try:
            while True:
                kind,value=self.events.get_nowait()
                if kind=='log':self.write(value)
                elif kind=='status':self.status.set(value)
                elif kind=='code':
                    self.code_display.set(value[:4]+'-'+value[4:]);self.show_code(True);self.hint.set('Share this code with your friend. Leave the launcher open.')
                elif kind=='error':self.had_error=True;self.status.set(value if len(value)<=180 else value[:177]+'…');self.write(value)
                elif kind=='ended':
                    self.set_active(False);self.code_display.set('');self.show_code(False)
                    self.hint.set(HOST_NOTICE)
                    if self.closing:self.window.destroy();return
                    if not self.had_error:self.status.set('Session ended. Diagnostics saved in StrayCoopTests.')
                elif kind=='release':
                    if value.get('version')!=CONFIG['version']:
                        self.update=value;self.update_state.set('Version '+str(value.get('version'))+' available');self.update_label.configure(fg=ACCENT);self.update_button.configure(text='Update & restart')
                    else:self.update_state.set('Up to date');self.update_label.configure(fg=GREEN)
                    self.update_button.configure(state='normal')
                elif kind=='update_error':
                    self.update_state.set('Update check unavailable');self.update_label.configure(fg=MUTED);self.update_button.configure(state='normal');self.write(value)
                elif kind=='update_ready':self.update_button.configure(state='normal')
                elif kind=='restart':
                    subprocess.Popen([str(HOME/'StrayCoop.exe')],cwd=HOME);self.window.destroy();return
        except queue.Empty:pass
        self.window.after(100,self.drain)
    def close(self):
        if self.active:
            if not messagebox.askyesno('End co-op session?','This will close the Stray session started by this launcher. End the session?'):return
            self.closing=True;self.end()
        else:self.window.destroy()
    def run(self):self.window.mainloop()

if __name__=='__main__':
    try:
        from launcher_web import launch
        if not launch(RELEASE,HOME):Launcher().run()
    except Exception as e:
        PREFS.mkdir(parents=True,exist_ok=True);(PREFS/'startup-error.txt').write_text(str(e))
        try:messagebox.showerror('Stray Co-op could not start',str(e))
        except Exception:pass
