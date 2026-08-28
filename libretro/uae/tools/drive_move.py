"""Aplica move_decl.py fichero por fichero, con backup .premove y verificacion
con verify.py (que ahora SI devuelve codigo de error). Si verify falla, el
fichero se revierte."""
import io
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
SKIP_DIRS = ('deps/7zip', 'libretro-common')

cur = None
per_file = {}
for line in io.open(os.path.join(ROOT, sys.argv[1]),
                    encoding='utf-8', errors='replace'):
    m = re.match(r'^===\s+(.*?)\s*$', line)
    if m:
        cur = m.group(1).strip()
        continue
    m = re.match(r'^\s+(\d+)\s+decl-after-stmt\s+(.*)', line)
    if m and cur and not any(cur.startswith(d) for d in SKIP_DIRS):
        per_file.setdefault(cur, []).append((int(m.group(1)),
                                             m.group(2).rstrip()))

print('ficheros: %d   sitios: %d'
      % (len(per_file), sum(len(v) for v in per_file.values())))
print()

for rel in sorted(per_file):
    path = os.path.join(ROOT, rel.replace('/', os.sep))
    if not os.path.isfile(path):
        print('  %-42s NO EXISTE' % rel)
        continue
    sites = os.path.join(TOOLS, '_sites.tmp')
    io.open(sites, 'w', encoding='utf-8', newline='\n').write(
        ''.join('%d\t%s\n' % (n, t) for n, t in per_file[rel]))

    bak = path + '.premove'
    shutil.copy2(path, bak)
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'move_decl.py'),
                        path, sites], capture_output=True, text=True)
    v = subprocess.run([sys.executable, os.path.join(TOOLS, 'verify.py'),
                        bak, path], capture_output=True, text=True)
    ok = v.returncode == 0
    if not ok:
        shutil.copy2(bak, path)
    out = r.stdout.strip().split('\n')
    print('  %-40s %-18s %s' % (rel, out[0] if out else '?',
                                'OK' if ok else 'VERIFY FALLA -> REVERTIDO'))
    for extra in out[1:]:
        print('        ' + extra.strip())
    if not ok:
        print('        ' + v.stdout.strip().replace('\n', ' | ')[:160])
