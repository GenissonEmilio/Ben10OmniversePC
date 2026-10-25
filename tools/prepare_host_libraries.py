#!/usr/bin/env python3
"""Copy missing Linux libraries from our local build image, without changing the OS."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMAGE = 'ben10-omniverse-tools:local'


def docker_base():
    return ['docker', 'run', '--rm', '--cpus=1', '--memory=512m',
            '--memory-swap=512m', '--network=none', '--cap-drop=ALL',
            '--security-opt=no-new-privileges', '--user', f'{os.getuid()}:{os.getgid()}']


def main():
    binary = ROOT/'build/native/Ben10OmniverseRecomp'
    if not binary.is_file():
        raise SystemExit('Compile o executável local antes de preparar as bibliotecas.')
    listing = subprocess.check_output(['ldconfig', '-p'], text=True)
    host = set(re.findall(r'^\s*(\S+)\s+\(', listing, re.M))
    inspect = '''import json,pathlib,re,subprocess
paths=['/workspace/build/native/Ben10OmniverseRecomp',
       '/usr/lib/x86_64-linux-gnu/libavcodec.so.60',
       '/usr/lib/x86_64-linux-gnu/libavutil.so.58']
libraries={}
for path in paths:
 s=subprocess.check_output(['ldd',path],text=True)
 if 'not found' in s: raise SystemExit(s)
 libraries.update(re.findall(r"^\\s*(\\S+) => (/\\S+)",s,re.M))
 if path.startswith('/usr/lib'):libraries[pathlib.Path(path).name]=path
print(json.dumps(libraries))
'''
    command = docker_base()+['-v', f'{ROOT}:/workspace:ro,z', IMAGE, 'python3', '-c', inspect]
    available = json.loads(subprocess.check_output(command, text=True))
    # The game is built against SDL2. Fedora may supply an SDL3 compatibility
    # shim under this SONAME; keep the actual build dependency local to the app.
    required = {name: path for name, path in available.items()
                if name not in host or name == 'libSDL2-2.0.so.0'}
    forbidden = ('libc.so', 'libm.so', 'libpthread.so', 'libdl.so', 'librt.so', 'ld-linux')
    if any(name.startswith(forbidden) for name in required):
        raise SystemExit('Este host não é compatível com a libc da imagem de build.')
    destination = ROOT/'artifacts/native-libs'
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='host-libs-', dir=ROOT/'artifacts') as directory:
        copy = '''import json,pathlib,shutil,sys
for name,source in json.loads(sys.argv[1]).items():
 if pathlib.Path(name).name != name or not name.startswith('lib'): raise ValueError(name)
 source=pathlib.Path(source).resolve()
 if not source.is_relative_to('/usr/lib'): raise ValueError(source)
 shutil.copyfile(source,pathlib.Path('/output')/name)
'''
        subprocess.run(docker_base()+['-v', f'{directory}:/output:z', IMAGE,
                       'python3', '-c', copy, json.dumps(required)], check=True)
        for source in Path(directory).iterdir():
            source.replace(destination/source.name)
    manifest = {name: {'source': source,
                      'sha256': hashlib.sha256((destination/name).read_bytes()).hexdigest()}
                for name, source in sorted(required.items())}
    image = subprocess.check_output(['docker', 'image', 'inspect', '--format={{.Id}}', IMAGE], text=True).strip()
    (ROOT/'artifacts/host-libraries.json').write_text(
        json.dumps({'image': image, 'libraries': manifest}, indent=2)+'\n')
    print(f'{len(required)} bibliotecas locais preparadas em {destination}')


if __name__ == '__main__':
    main()
