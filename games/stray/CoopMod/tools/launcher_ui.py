"""Native Tk rendering of the supplied charcoal/orange launcher design.
No browser engine, remote fonts, or additional runtime packages are required.
"""
import math
import tkinter as tk
from tkinter import font as tkfont

BG='#090b0e';CARD='#12161d';INPUT='#0e1217';BORDER='#272d37'
FG='#f0f2f5';MUTED='#9aa3af';DIM='#798496';ACCENT='#ff6b2b';GREEN='#10b981'

def rounded(canvas,x1,y1,x2,y2,r=8,**options):
    r=min(r,max(0,(x2-x1)/2),max(0,(y2-y1)/2))
    return canvas.create_polygon(x1+r,y1,x2-r,y1,x2,y1,x2,y1+r,
        x2,y2-r,x2,y2,x2-r,y2,x1+r,y2,x1,y2,x1,y2-r,x1,y1+r,x1,y1,
        smooth=True,splinesteps=16,**options)

def mix(a,b,t):
    av=[int(a[i:i+2],16) for i in (1,3,5)];bv=[int(b[i:i+2],16) for i in (1,3,5)]
    return '#'+''.join(f'{round(x+(y-x)*t):02x}' for x,y in zip(av,bv))

class Card(tk.Frame):
    def __init__(self,parent,padding=16,**kwargs):
        super().__init__(parent,bg=BG,**kwargs)
        self.surface=tk.Canvas(self,bg=BG,highlightthickness=0,bd=0)
        self.surface.place(x=0,y=0,relwidth=1,relheight=1)
        self.body=tk.Frame(self,bg=CARD)
        self.body.pack(fill='both',expand=True,padx=padding,pady=padding)
        self.tk.call('lower',self.surface._w)
        self.bind('<Configure>',self.paint)
    def paint(self,event=None):
        c=self.surface;c.delete('all')
        rounded(c,1,1,self.winfo_width()-1,self.winfo_height()-1,10,fill=CARD,outline=BORDER,width=1)

class Button(tk.Canvas):
    """Rounded, keyboard-focusable control with the usual configure/invoke API."""
    def __init__(self,parent,text,command,variant='secondary',arrow=False,**kwargs):
        self.text=text;self.command=command;self.variant=variant;self.arrow=arrow
        self.state='normal';self.hover=False;self.focused=False;self.pressed=False
        self.font=tkfont.Font(family='Segoe UI',size=10,weight='bold')
        width=self.font.measure(text)+32+(22 if arrow else 0)
        super().__init__(parent,bg=parent.cget('bg'),highlightthickness=0,bd=0,
                         width=width,height=42,takefocus=1,cursor='hand2',**kwargs)
        self.bind('<Configure>',self.paint)
        self.bind('<Enter>',lambda e:self.interact(hover=True))
        self.bind('<Leave>',lambda e:self.interact(hover=False))
        self.bind('<FocusIn>',lambda e:self.interact(focused=True))
        self.bind('<FocusOut>',lambda e:self.interact(focused=False))
        self.bind('<Button-1>',self.press)
        self.bind('<ButtonRelease-1>',self.release)
        self.bind('<Return>',lambda e:self.invoke())
        self.bind('<space>',lambda e:self.invoke())
    def interact(self,**values):
        for name,value in values.items():setattr(self,name,value)
        self.paint()
    def configure(self,cnf=None,**kwargs):
        if cnf:kwargs.update(cnf)
        changed=False
        for key in ('text','state','variant'):
            if key in kwargs:setattr(self,key,kwargs.pop(key));changed=True
        if kwargs:super().configure(**kwargs)
        if changed:
            super().configure(cursor='hand2' if self.state=='normal' else '',takefocus=self.state=='normal')
            self.paint()
    config=configure
    def cget(self,key):
        if key in ('text','state','variant'):return getattr(self,key)
        return super().cget(key)
    def invoke(self):
        if self.state=='normal':return self.command()
    def press(self,event):
        if self.state=='normal':self.pressed=True;self.focus_set()
    def release(self,event):
        clicked=self.pressed and 0<=event.x<self.winfo_width() and 0<=event.y<self.winfo_height()
        self.pressed=False
        if clicked:self.invoke()
    def paint(self,event=None):
        self.delete('all');w,h=self.winfo_width(),self.winfo_height()
        if w<2:w=int(super().cget('width'))
        disabled=self.state=='disabled'
        background={'primary':ACCENT,'outline':'#201610','secondary':'#1c222c','ghost':BG,'danger':'#211619'}[self.variant]
        color=ACCENT if self.variant=='outline' else '#ffb0a6' if self.variant=='danger' else FG
        border='#74402b' if self.variant=='outline' else '#62323b' if self.variant=='danger' else BORDER
        if disabled:background='#1b2028';color='#748092';border=BORDER
        elif self.hover:background=mix(background,FG,.09)
        if self.focused and not disabled:border='#ffd0bb' if self.variant=='primary' else ACCENT
        rounded(self,1,1,w-1,h-1,7,fill=background,outline=border,width=1)
        if self.variant=='primary' and not disabled:
            # A clipped vertical gradient supplies the reference's orange sheen.
            radius=7
            for y in range(2,max(2,h-2)):
                dy=min(y-1,h-2-y)
                inset=radius-math.sqrt(max(0,radius*radius-(radius-min(radius,dy))**2))
                shade=mix('#ff824d' if self.hover else '#ff7b42',ACCENT,y/max(1,h-1))
                self.create_line(2+inset,y,w-2-inset,y,fill=shade)
        if self.arrow:
            self.create_text(17,h/2,text=self.text,fill=color,font=self.font,anchor='w')
            x=w-22;self.create_line(x-9,h/2,x+5,h/2,fill=color,width=2)
            self.create_line(x,h/2-5,x+5,h/2,x,h/2+5,fill=color,width=2)
        else:self.create_text(w/2,h/2,text=self.text,fill=color,font=self.font)

class InputField(tk.Canvas):
    def __init__(self,parent,variable,mono=False,folder=False,placeholder=''):
        super().__init__(parent,bg=parent.cget('bg'),height=42,width=120,highlightthickness=0,bd=0)
        self.folder=folder;self.focused=False;self.variable=variable
        self.entry=tk.Entry(self,textvariable=variable,bg=INPUT,fg=FG,insertbackground=FG,
            disabledbackground=INPUT,disabledforeground=DIM,relief='flat',bd=0,
            font=('Consolas',12) if mono else ('Segoe UI',10),selectbackground='#874225')
        self.item=self.create_window(36 if folder else 12,21,window=self.entry,anchor='w')
        self.placeholder=tk.Label(self,text=placeholder,bg=INPUT,fg=DIM,font=('Consolas',10) if mono else ('Segoe UI',10),cursor='xterm')
        self.placeholder.bind('<Button-1>',lambda e:self.entry.focus_set())
        variable.trace_add('write',lambda *_:self.paint())
        self.bind('<Configure>',self.paint)
        self.entry.bind('<FocusIn>',lambda e:self.focus(True))
        self.entry.bind('<FocusOut>',lambda e:self.focus(False))
    def focus(self,value):self.focused=value;self.paint()
    def paint(self,event=None):
        self.delete('surface');w=self.winfo_width()
        rounded(self,1,1,w-1,41,7,fill=INPUT,outline=ACCENT if self.focused else BORDER,width=1,tags='surface')
        if self.folder:
            self.create_line(13,17,13,28,27,28,27,18,21,18,19,15,13,15,13,17,
                             fill=DIM,width=1.5,tags='surface')
        self.tag_lower('surface')
        self.itemconfigure(self.item,width=max(25,w-(48 if self.folder else 24)))
        if self.placeholder.cget('text') and not self.variable.get() and not self.focused:
            self.placeholder.place(x=12,y=10)
        else:self.placeholder.place_forget()

class Header(tk.Canvas):
    def __init__(self,parent):
        super().__init__(parent,bg=BG,height=124,highlightthickness=0,bd=0)
        self.title_font=tkfont.Font(family='Segoe UI',size=27,weight='bold')
        self.bind('<Configure>',self.paint)
    def paint(self,event=None):
        self.delete('all');w=self.winfo_width()
        for i in range(20,0,-1):
            radius=i*12
            self.create_oval(w/2-radius,-radius*.6-18,w/2+radius,radius*.6-18,
                             fill=mix(BG,'#2b1b19',(21-i)/50),outline='')
        self.create_oval(0,10,6,16,fill=ACCENT,outline='')
        self.create_text(15,13,text='STRAY MULTIPLAYER PROTOCOL',fill=ACCENT,
                         font=('Segoe UI',9,'bold'),anchor='w')
        f=self.title_font
        self.create_text(0,51,text='Bring a friend',fill=FG,font=f,anchor='w')
        self.create_text(f.measure('Bring a friend'),51,text='.',fill=ACCENT,font=f,anchor='w')
        self.create_text(0,90,text='Host a session or enter your partner’s session code.',fill=MUTED,
                         font=('Segoe UI',10),width=max(100,w),anchor='w')

class Notice(tk.Canvas):
    def __init__(self,parent,variable):
        super().__init__(parent,bg=BG,height=38,highlightthickness=0,bd=0)
        self.variable=variable;variable.trace_add('write',lambda *_:self.paint())
        self.bind('<Configure>',self.paint)
    def paint(self,event=None):
        self.delete('all');w=self.winfo_width()
        rounded(self,0,0,w,38,6,fill='#0c0f14',outline='')
        self.create_oval(11,12,25,26,outline=DIM,width=1)
        self.create_text(18,19,text='i',fill=DIM,font=('Segoe UI',8,'bold'))
        self.create_text(35,19,text=self.variable.get(),anchor='w',fill=MUTED,
                         font=('Segoe UI',9),width=max(50,w-47))
