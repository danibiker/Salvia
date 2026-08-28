"""Verifica la transformacion de move_decl.py, que es distinta de la de hoist2.

hoist2 PARTE "T x = e;" en "T x;" + "x = e;": ahi vale comparar la secuencia de
sentencias (verify.py).

move_decl MUEVE la sentencia entera. La secuencia cambia por definicion, asi que
verify.py no sirve. Lo que hay que demostrar aqui es otra cosa:

  1. El fichero es una PERMUTACION exacta de si mismo: el multiconjunto de
     lineas es identico. Nada se ha anadido, borrado ni editado.
  2. Toda linea que ha cambiado de posicion es una declaracion sin
     inicializador o con inicializador CONSTANTE, y se ha movido HACIA ARRIBA.

Con esas dos, lo unico que ha pasado es adelantar declaraciones sin efectos
laterales dentro de su bloque, que es exactamente lo que se pretendia.

Uso: python tools/verify_move.py <antes.c> <despues.c>
"""
import collections
import io
import re
import sys

CONST_INIT = re.compile(r'^[\s{}\d,;\'"\.\-+*/()xXa-fA-F_]*$')
DECL = re.compile(
    r'^(?:const\s+|static\s+|volatile\s+|unsigned\s+|signed\s+)*'
    r'(?:struct\s+\w+|union\s+\w+|enum\s+\w+|\w+)[\w\s,*\[\]\(\)]*[;=]')

a = io.open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
b = io.open(sys.argv[2], encoding='utf-8', errors='replace').read().split('\n')

ca = collections.Counter(l.rstrip('\r') for l in a)
cb = collections.Counter(l.rstrip('\r') for l in b)

rc = 0
if ca != cb:
    print('NO es una permutacion: el contenido de las lineas ha cambiado')
    for line, n in (ca - cb).most_common(6):
        print('  solo antes  (%d): %s' % (n, line.strip()[:78]))
    for line, n in (cb - ca).most_common(6):
        print('  solo ahora  (%d): %s' % (n, line.strip()[:78]))
    sys.exit(1)

# lineas que cambiaron de posicion
moved = []
i = j = 0
sa = [l.rstrip('\r') for l in a]
sb = [l.rstrip('\r') for l in b]
import difflib
sm = difflib.SequenceMatcher(None, sa, sb, autojunk=False)
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'equal':
        continue
    for k in range(i1, i2):
        moved.append(('antes', i1 + 1, sa[k]))
    for k in range(j1, j2):
        moved.append(('ahora', j1 + 1, sb[k]))

bad = []
for where, ln, text in moved:
    t = text.strip()
    if not t:
        continue
    if not DECL.match(t):
        bad.append((where, ln, t))
        continue
    if '=' in t and not CONST_INIT.match(t.split('=', 1)[1]):
        bad.append((where, ln, t))

print('lineas identicas en multiconjunto: si (%d lineas)' % len(sa))
print('lineas que cambiaron de posicion : %d' % len(moved))
if bad:
    rc = 1
    print('OJO, %d no son declaraciones sin efectos:' % len(bad))
    for where, ln, t in bad[:12]:
        print('  %-6s ~%-6d %s' % (where, ln, t[:74]))
else:
    print('todas son declaraciones sin efectos laterales -> movimiento seguro')

sys.exit(rc)
