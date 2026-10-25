#!/usr/bin/env python3
"""Local Linux launcher for the experimental, locally compiled native game."""
from pathlib import Path
import collections
import hashlib
import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import uuid

ROOT = Path(__file__).resolve().parents[2]
DATA = Path(os.environ.get('BEN10_DATA_DIR', Path(os.environ.get('XDG_DATA_HOME', Path.home()/'.local/share'))/'Ben10OmniversePC')).expanduser().resolve()
ELF_HASH = '3ed23bca61cc9e392582e0c2cf0f114f24ce746f5724a2ab194e5aba944f1bea'
BG, PANEL, GREEN, TEXT, MUTED = '#0b110e', '#16221b', '#b6ef51', '#eff5ed', '#a8b6ac'


def readiness():
    elf = DATA/'game/PS3_GAME/USRDIR/EBOOT.elf'
    binary = ROOT/'build/native/Ben10OmniverseRecomp'
    flash = DATA/'firmware/dev_flash/sys/external/flashATRAC.pic'
    if not elf.is_file(): return False, 'Importação e preparação do disco ainda necessárias.'
    try: digest = hashlib.sha256(elf.read_bytes()).hexdigest()
    except OSError: return False, 'Não foi possível ler o executável importado.'
    if digest != ELF_HASH:
        return False, 'O executável não corresponde à edição BLUS31017 1.00 suportada.'
    if not flash.is_file(): return False, 'O firmware do próprio disco ainda precisa ser preparado.'
    if not binary.is_file() or not os.access(binary, os.X_OK):
        return False, 'Os dados estão preparados. Compile o jogo para continuar.'
    if not shutil.which('systemd-run'): return False, 'Este launcher requer uma sessão Linux com systemd.'
    return True, 'Seu disco está importado. O jogo pode ser iniciado.'


def game_environment(fullscreen, workers):
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(('PAD_', 'YDKJ_', 'RSX_ENGINE_', 'SPU_')) or key in (
            'PS3RECOMP_VK_DUMP', 'PS3RECOMP_VK_DUMP_EVERY', 'BEN10_LLE_MEDIA',
            'BEN10_HOST_VIDEO', 'BEN10_WATCHDOG', 'PS3_GUEST_PROF'):
            env.pop(key, None)
    user = DATA/'user'
    cache = DATA/'cache/spirv'
    for path in (user/'hdd0', user/'cache', cache): path.mkdir(parents=True, exist_ok=True)
    libs = ROOT/'artifacts/native-libs'
    env.update(LD_LIBRARY_PATH=str(libs)+((':'+env['LD_LIBRARY_PATH']) if env.get('LD_LIBRARY_PATH') else ''),
        __NV_PRIME_RENDER_OFFLOAD='1', __VK_LAYER_NV_optimus='NVIDIA_only',
        SDL_VIDEODRIVER='x11', SDL_AUDIODRIVER='pulseaudio',
        PS3_TITLE='Ben 10 Omniverse — Linux', PS3_VFS_ROOT=str(DATA/'game'),
        PS3_HDD0_ROOT=str(user/'hdd0'), PS3_HDD1_ROOT=str(user/'cache'),
        PS3_GAMEDATA_ROOT=str(user), PS3_DEV_FLASH=str(DATA/'firmware/dev_flash'),
        PS3RECOMP_VK_WINDOW='1', PS3RECOMP_VK_GUEST_PROGRAMS='1', RSX_LIVE_DRAW='0',
        PS3RECOMP_VK_FULLSCREEN='1' if fullscreen else '0',
        PS3RECOMP_SPIRV_CACHE=str(cache), PS3_NO_SAIL='1', PS3_VERBOSE='0',
        PS3_JOB_WORKERS=str(workers))
    return env


class Launcher(tk.Tk):
    def __init__(self):
        super().__init__(className='Ben10OmniversePC')
        self.title('Ben 10 Omniverse PC — Prévia Linux')
        self.geometry('1000x760'); self.minsize(940,740); self.configure(bg=BG)
        self.events=queue.Queue(); self.proc=None; self.unit=None; self.busy=False
        self.tail=collections.deque(maxlen=700);self.tail_lock=threading.Lock()
        DATA.mkdir(parents=True,exist_ok=True)
        try: prefs=json.loads((DATA/'launcher.json').read_text())
        except (OSError,ValueError): prefs={}
        self.fullscreen=tk.BooleanVar(value=bool(prefs.get('fullscreen',False)))
        profile=prefs.get('profile','Equilibrado')
        self.profile=tk.StringVar(value='Compatibilidade' if isinstance(profile,str) and profile.startswith('Compatibilidade') else 'Equilibrado')
        style=ttk.Style(self);style.theme_use('clam')
        style.configure('TCombobox',fieldbackground=PANEL,background=PANEL,foreground=TEXT,arrowcolor=GREEN,padding=9)
        style.map('TCombobox',fieldbackground=[('readonly',PANEL)],foreground=[('readonly',TEXT)])
        style.configure('TProgressbar',background=GREEN,troughcolor=PANEL,borderwidth=0)
        top=tk.Frame(self,bg=BG);top.pack(fill='x',padx=34,pady=(28,20))
        icon=tk.Canvas(top,width=94,height=94,bg=BG,highlightthickness=0);icon.pack(side='left',padx=(0,18))
        icon.create_oval(5,5,89,89,fill='#263a2d',outline=GREEN,width=3)
        icon.create_oval(14,14,80,80,fill='#080d09',outline='#60764c',width=2)
        icon.create_polygon(29,25,65,25,52,47,65,69,29,69,42,47,fill=GREEN,outline='')
        titles=tk.Frame(top,bg=BG);titles.pack(side='left')
        self.label(titles,'BEN 10',18,GREEN,True).pack(anchor='w')
        self.label(titles,'OMNIVERSE',32,TEXT,True).pack(anchor='w')
        self.label(top,'PRÉVIA LINUX',10,GREEN,True).pack(side='right')
        body=tk.Frame(self,bg=BG);body.pack(fill='both',expand=True,padx=34)
        body.columnconfigure(0,weight=3);body.columnconfigure(1,weight=2);body.rowconfigure(0,weight=1)
        left=tk.Frame(body,bg=PANEL,padx=25,pady=25);left.grid(row=0,column=0,sticky='nsew',padx=(0,16))
        self.label(left,'A aventura começa aqui.',23,TEXT,True,PANEL).pack(anchor='w',pady=(0,12))
        self.label(left,'Jogue com os arquivos do seu disco.\nSeus saves ficam guardados neste computador.',12,MUTED,bg=PANEL).pack(anchor='w',pady=(0,24))
        self.status=self.label(left,'Verificando os arquivos…',12,TEXT,bg=PANEL)
        self.status.configure(wraplength=430);self.status.pack(fill='x',pady=(0,15))
        self.progress=ttk.Progressbar(left,mode='indeterminate');self.progress.pack(fill='x',pady=(0,17))
        self.play=self.button(left,'JOGAR',self.start_game,primary=True);self.play.pack(fill='x',ipady=8,pady=(0,12))
        self.build=self.button(left,'Recompilar arquivos preparados',self.start_build);self.build.pack(fill='x',pady=(0,12))
        actions=tk.Frame(left,bg=PANEL);actions.pack(fill='x')
        self.button(actions,'Ver diagnóstico',self.show_log).pack(side='left',expand=True,fill='x',padx=(0,8))
        self.button(actions,'Preparação',self.show_readme).pack(side='left',expand=True,fill='x')
        self.stop=self.button(left,'Encerrar jogo',self.stop_game);self.stop.pack(fill='x',pady=(14,0));self.stop.configure(state='disabled')
        right=tk.Frame(body,bg=PANEL,padx=23,pady=25);right.grid(row=0,column=1,sticky='nsew')
        self.label(right,'Seu jeito de jogar',19,TEXT,True,PANEL).pack(anchor='w',pady=(0,15))
        tk.Checkbutton(right,text='Tela cheia',variable=self.fullscreen,bg=PANEL,fg=TEXT,selectcolor=BG,activebackground=PANEL,activeforeground=GREEN,font=('Sans',12),command=self.save_preferences).pack(anchor='w',pady=(0,15))
        self.label(right,'Desempenho',11,MUTED,bg=PANEL).pack(anchor='w')
        combo=ttk.Combobox(right,textvariable=self.profile,state='readonly',values=['Equilibrado','Compatibilidade'],width=28)
        combo.pack(fill='x',pady=(6,22));combo.bind('<<ComboboxSelected>>',lambda _:self.save_preferences())
        self.label(right,'Teclado e controle',14,GREEN,True,PANEL).pack(anchor='w',pady=(0,10))
        for text in ['WASD   Mover', 'Espaço / Z   Pular', 'C / V   Ataques leve / forte', 'Setas   Selecionar alienígena', 'Q   Voltar à forma humana', 'F + ataque   Ataque especial', 'Enter / Esc   Pausar']:
            self.label(right,text,11,TEXT,bg=PANEL).pack(anchor='w',pady=3)
        self.label(right,'Controle USB/Bluetooth compatível.\nA janela do jogo precisa estar em foco.',10,MUTED,bg=PANEL).pack(anchor='w',pady=(15,0))
        footer=tk.Frame(self,bg=BG);footer.pack(fill='x',padx=34,pady=20)
        self.label(footer,'Logos de abertura omitidos. Desempenho e compatibilidade da campanha ainda em validação.',10,MUTED).pack(anchor='w')
        self.after(150,self.poll);self.after(200,self.refresh);self.protocol('WM_DELETE_WINDOW',self.close)

    def label(self,parent,text,size=12,color=TEXT,bold=False,bg=BG):
        return tk.Label(parent,text=text,bg=bg,fg=color,font=('Sans',size,'bold' if bold else 'normal'),justify='left',anchor='w')

    def button(self,parent,text,command,primary=False):
        return tk.Button(parent,text=text,command=command,bg=GREEN if primary else '#253b2d',fg=BG if primary else TEXT,
            activebackground='#cef991',activeforeground=BG,relief='flat',borderwidth=0,font=('Sans',13 if primary else 10,'bold'),cursor='hand2',padx=12,pady=11,disabledforeground='#708075')

    def save_preferences(self):
        path=DATA/'launcher.json';tmp=path.with_suffix('.tmp')
        tmp.write_text(json.dumps({'fullscreen':self.fullscreen.get(),'profile':self.profile.get()},ensure_ascii=False,indent=2)+'\n');tmp.replace(path)

    def refresh(self):
        ready,text=readiness();self.status.configure(text=text)
        self.play.configure(state='normal' if ready and not self.busy else 'disabled')
        self.build.configure(state='normal' if not self.busy and (DATA/'native/ppu/ppu_recomp.h').exists() else 'disabled')

    def run(self,command,env,game):
        self.busy=True;self.play.configure(state='disabled');self.build.configure(state='disabled')
        self.stop.configure(state='normal' if game else 'disabled')
        self.status.configure(text='Em jogo. Feche a janela do jogo para voltar.' if game else 'Compilando para Linux…')
        self.progress.start(15)
        def worker():
            try:
                self.proc=subprocess.Popen(command,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace',bufsize=1)
                for line in self.proc.stdout:
                    with self.tail_lock:self.tail.append(line.rstrip())
                code=self.proc.wait();self.events.put(('done',code,game))
            except Exception as error:self.events.put(('error',str(error),game))
        threading.Thread(target=worker,daemon=True).start()

    def start_game(self):
        if self.busy:return
        ready,text=readiness()
        if not ready:messagebox.showerror('Preparação necessária',text,parent=self);return
        self.save_preferences();self.unit='ben10-game-'+uuid.uuid4().hex[:12]
        command=['systemd-run','--user','--scope','--quiet','--unit='+self.unit,'-p','CPUQuota=400%','-p','MemoryMax=4G','-p','MemorySwapMax=0',str(ROOT/'build/native/Ben10OmniverseRecomp'),str(DATA/'game/PS3_GAME/USRDIR/EBOOT.elf')]
        self.run(command,game_environment(self.fullscreen.get(),2 if self.profile.get().startswith('Equilibrado') else 1),True)

    def start_build(self):
        if self.busy:return
        self.unit=None;self.run(['bash',str(ROOT/'scripts/build-native.sh')],os.environ.copy(),False)

    def stop_game(self):
        if self.unit:
            unit=self.unit
            subprocess.run(['systemctl','--user','kill','--kill-whom=all','--signal=SIGTERM',unit],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            def force():
                if self.busy and self.unit==unit:subprocess.run(['systemctl','--user','kill','--kill-whom=all','--signal=SIGKILL',unit],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            self.after(3000,force)

    def poll(self):
        try:
            while True:
                kind,value,game=self.events.get_nowait();self.busy=False;self.proc=None;self.progress.stop();self.stop.configure(state='disabled');self.refresh()
                if kind=='error' or value not in (0,-15,143,137):
                    self.status.configure(text='O processo não terminou normalmente. Consulte o diagnóstico.')
                    if kind=='error':
                        with self.tail_lock:self.tail.append(value)
                elif game:self.status.configure(text='Jogo encerrado.')
        except queue.Empty:pass
        self.after(150,self.poll)

    def show_log(self):
        window=tk.Toplevel(self);window.title('Diagnóstico — Ben 10 Omniverse');window.geometry('900x500')
        with self.tail_lock:content='\n'.join(self.tail)
        text=tk.Text(window,bg=BG,fg=TEXT,font=('Monospace',10),wrap='word');text.pack(fill='both',expand=True);text.insert('end',content or 'Nenhum processo executado nesta sessão.');text.configure(state='disabled')

    def show_readme(self):subprocess.Popen(['xdg-open',str(ROOT/'README.md')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

    def close(self):
        if self.busy:
            if self.unit:
                self.stop_game();self.status.configure(text='Encerrando o jogo…')
                self.after(3300,self.destroy);return
            elif self.proc:
                messagebox.showinfo('Compilação em andamento','A compilação continua limitada a dois workers. Aguarde o término antes de fechar o launcher.',parent=self);return
        self.destroy()


if __name__=='__main__':
    app=Launcher()
    if '--smoke' in sys.argv:app.after(12000,app.close)
    app.mainloop()
