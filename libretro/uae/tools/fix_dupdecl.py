"""Quita las declaraciones duplicadas que introdujo hoist2.py.

hoist2 inserta la declaracion al principio del bloque una vez por cada sitio
que hoistea, sin comprobar si ya la habia puesto: dos `for (int i = ...)` en el
mismo bloque dejan dos `int i;` seguidas. Solo se borra la linea si es IDENTICA
(ignorando espacios) a otra declaracion del mismo grupo contiguo, que es la
unica forma en la que hoist2 puede producirlas. Cualquier otra cosa se reporta
y se deja intacta.

Uso: python tools/fix_dupdecl.py <after.txt>
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

cur = None
per_file = {}
for line in io.open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'^===\s+(.*?)\s*$', line)
    if m:
        cur = m.group(1).strip()
        continue
    m = re.match(r'^\s+(\d+)\s+DUPLICATE-DECL\s+(\S+)', line)
    if m and cur:
        per_file.setdefault(cur, []).append((int(m.group(1)), m.group(2)))

total_removed = 0
manual = []

for rel in sorted(per_file):
    path = os.path.join(ROOT, rel.replace('/', os.sep))
    if not os.path.isfile(path):
        manual.append((rel, 0, 'no existe'))
        continue
    src = io.open(path, encoding='utf-8', errors='replace').read()
    nl = '\r\n' if '\r\n' in src else '\n'
    lines = src.split('\n')
    lines = [l.rstrip('\r') for l in lines]

    drop = set()
    for ln, name in sorted(per_file[rel], reverse=True):
        idx = ln - 1
        if idx < 1 or idx >= len(lines):
            manual.append((rel, ln, 'fuera de rango'))
            continue
        cand = lines[idx].strip()
        # tiene que ser una declaracion simple de ese nombre
        if not re.match(r'^[A-Za-z_][\w \*]*\b%s\s*;$' % re.escape(name), cand):
            manual.append((rel, ln, 'no es declaracion simple: %s' % cand[:50]))
            continue
        # y una linea contigua hacia arriba tiene que ser identica
        j = idx - 1
        found = False
        while j >= 0 and lines[j].strip():
            if lines[j].strip() == cand:
                found = True
                break
            if not re.match(r'^[A-Za-z_][\w \*\[\],=]*;$', lines[j].strip()):
                break
            j -= 1
        if not found:
            manual.append((rel, ln, 'sin gemela contigua: %s' % cand[:50]))
            continue
        drop.add(idx)

    if drop:
        out = [l for i, l in enumerate(lines) if i not in drop]
        io.open(path, 'w', encoding='utf-8', newline='').write(nl.join(out))
        total_removed += len(drop)
        print('  %-38s %d duplicadas quitadas' % (rel, len(drop)))

print()
print('total duplicadas quitadas: %d' % total_removed)
if manual:
    print()
    print('=== A MANO (%d) ===' % len(manual))
    for rel, ln, why in manual:
        print('  %-34s %6s  %s' % (rel, ln, why))
