#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
python3 - "$root" <<'PY'
import os,sys
from pathlib import Path
root=Path(sys.argv[1])
folder=Path(os.environ.get('XDG_DATA_HOME',Path.home()/'.local/share'))/'applications'
folder.mkdir(parents=True,exist_ok=True)
def quote(value):
    value=str(value)
    if '\n' in value or '\r' in value:raise ValueError('Caminho inválido para atalho')
    return '"'+value.replace('\\','\\\\').replace('"','\\"').replace('`','\\`').replace('$','\\$').replace('%','%%')+'"'
target=folder/'com.genisson.Ben10OmniversePC.desktop'
text='\n'.join(['[Desktop Entry]','Type=Application','Version=1.0',
    'Name=Ben 10 Omniverse PC (Experimental)',
    'Comment=Launcher Linux da recompilação nativa local',
    'Exec='+quote(sys.executable)+' '+quote(root/'tools/desktop/main.py'),
    'Icon='+str(root/'tools/desktop/assets/icon.svg'),
    'Terminal=false','Categories=Game;','StartupNotify=true',
    'StartupWMClass=Ben10OmniversePC','X-Ben10-Launcher=true',''])
if target.exists() and 'X-Ben10-Launcher=true' not in target.read_text():
    raise SystemExit('Atalho existente desconhecido preservado: '+str(target))
tmp=target.with_suffix('.tmp');tmp.write_text(text);tmp.chmod(0o755);tmp.replace(target)
print(target)
PY
if command -v update-desktop-database >/dev/null; then
    update-desktop-database "${XDG_DATA_HOME:-$HOME/.local/share}/applications"
fi
