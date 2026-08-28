"""Driver: aplica hoist2.py a todos los ficheros con hallazgos C89 y verifica
cada uno con verify.py.

Por que un script y no a mano: son ~1.040 sitios. A mano salen 2 llamadas de
herramienta por sitio. hoist2.py ya existe, esta validado de la sesion
anterior, hoistea al BLOQUE contenedor (no a la funcion) y **deja intacto lo
que no entiende**, reportandolo. Cada fichero pasa por verify.py, que compara
la secuencia de sentencias ejecutables antes/despues: si el hoisting hubiera
movido un efecto secundario, salta. Si verify falla, el fichero se REVIERTE y
se marca para hacerlo a mano.

Uso: python tools/drive_hoist.py <detalle.txt>
donde detalle.txt es la salida de DETAIL=1 c89check.py (lineas
"    <linea>  <tag>  <texto>").
"""
import io
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')

detail = sys.argv[1]

# --- parsear el detalle: se agrupa por fichero ---
cur = None
per_file = {}
for line in io.open(detail, encoding='utf-8', errors='replace'):
    m = re.match(r'^===\s+(.*?)\s*$', line)
    if m:
        cur = m.group(1).strip()
        continue
    m = re.match(r'^\s+(\d+)\s+(\S+)\s', line)
    if m and cur:
        # solo los dos tipos que hoist2 sabe tratar
        if m.group(2) in ('decl-after-stmt', 'for-decl'):
            per_file.setdefault(cur, []).append((int(m.group(1)), m.group(2)))

print('ficheros con hallazgos hoisteables: %d' % len(per_file))
print('sitios totales: %d' % sum(len(v) for v in per_file.values()))
print()

ok_files, failed, skipped = [], [], []

for rel in sorted(per_file, key=lambda k: -len(per_file[k])):
    path = os.path.join(ROOT, rel.replace('/', os.sep))
    if not os.path.isfile(path):
        skipped.append((rel, 'no existe'))
        continue

    findings = os.path.join(TOOLS, '_findings.tmp')
    io.open(findings, 'w', encoding='utf-8', newline='\n').write(
        '\n'.join('%d %s' % (n, k) for n, k in per_file[rel]) + '\n')

    bak = path + '.prehoist'
    shutil.copy2(path, bak)

    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'hoist2.py'),
                        path, findings],
                       capture_output=True, text=True)
    if r.returncode != 0:
        shutil.copy2(bak, path)
        failed.append((rel, 'hoist2 fallo: %s' % r.stderr.strip()[:120]))
        continue

    v = subprocess.run([sys.executable, os.path.join(TOOLS, 'verify.py'),
                        bak, path],
                       capture_output=True, text=True)
    if v.returncode != 0:
        shutil.copy2(bak, path)
        failed.append((rel, 'verify fallo: %s' % (v.stdout + v.stderr).strip()[:160]))
        continue

    left = ''
    m = re.search(r'(\d+)\s+(?:sin tratar|untouched|left)', r.stdout)
    if m:
        left = ' (%s sin tratar)' % m.group(1)
    ok_files.append((rel, len(per_file[rel]), r.stdout.strip()[-90:] + left))

print('=== OK y VERIFICADOS (%d) ===' % len(ok_files))
for rel, n, msg in ok_files:
    print('  %-42s %4d sitios   %s' % (rel, n, msg))

if failed:
    print()
    print('=== REVERTIDOS, a mano (%d) ===' % len(failed))
    for rel, why in failed:
        print('  %-42s %s' % (rel, why))

if skipped:
    print()
    print('=== SALTADOS (%d) ===' % len(skipped))
    for rel, why in skipped:
        print('  %-42s %s' % (rel, why))
