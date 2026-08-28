"""Clasifica los decl-after-stmt que quedan, para saber que transformacion
necesita cada uno antes de escribir nada:

  A  sin inicializador          -> mover la declaracion entera arriba. Siempre
                                   seguro: no genera codigo.
  B  inicializador CONSTANTE    -> mover entera, salvo que el bloque que la
                                   contiene sea el cuerpo de un bucle (ahi
                                   dejaria de reinicializarse en cada vuelta).
  C  inicializador con variables -> NO se puede mover: se evaluaria antes de
                                   tiempo. Hay que partirla, o a mano.
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SKIP_DIRS = ('deps/7zip', 'libretro-common')

cur = None
items = []
for line in io.open(os.path.join(ROOT, sys.argv[1]),
                    encoding='utf-8', errors='replace'):
    m = re.match(r'^===\s+(.*?)\s*$', line)
    if m:
        cur = m.group(1).strip()
        continue
    m = re.match(r'^\s+(\d+)\s+decl-after-stmt\s+(.*)', line)
    if m and cur and not any(cur.startswith(d) for d in SKIP_DIRS):
        items.append((cur, int(m.group(1)), m.group(2).strip()))

# palabras que en un inicializador NO son variables
CONSTISH = re.compile(r'^[\s{}\d,\'"\.\-+*/()xXa-fA-F_]*$')


def enclosing_is_loop(lines, idx):
    """Sube buscando la llave que abre el bloque; mira si la linea que la
    precede (o la contiene) es un for/while/do."""
    depth = 0
    i = idx - 1
    while i >= 0:
        t = lines[i]
        depth += t.count('}') - t.count('{')
        if depth < 0:
            head = (lines[i - 1] if i > 0 else '') + ' ' + t
            return bool(re.search(r'\b(for|while|do)\b\s*[({]', head))
        i -= 1
    return False


counts = {'A': [], 'B': [], 'C': []}
for rel, ln, text in items:
    path = os.path.join(ROOT, rel.replace('/', os.sep))
    lines = io.open(path, encoding='utf-8', errors='replace').read().split('\n')
    idx = ln - 1
    if idx >= len(lines):
        counts['C'].append((rel, ln, text, 'fuera de rango'))
        continue
    if '=' not in text:
        counts['A'].append((rel, ln, text, ''))
        continue
    init = text.split('=', 1)[1]
    if CONSTISH.match(init):
        loop = enclosing_is_loop(lines, idx)
        counts['B' if not loop else 'C'].append(
            (rel, ln, text, 'EN BUCLE' if loop else ''))
    else:
        counts['C'].append((rel, ln, text, 'init con variables'))

for k in 'ABC':
    print('=== %s: %d ===' % (k, len(counts[k])))
    for rel, ln, text, why in counts[k]:
        print('  %-26s %6d  %-46s %s'
              % (rel.split('/')[-1], ln, text[:46], why))
    print()
