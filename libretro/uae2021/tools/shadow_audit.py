"""Detector de SHADOWS introducidos al hoistear declaraciones a C89. SOLO ANALIZA.

El caso real que aparecio en expansion.c:

    uae_u32 next;                       /* ambito de funcion */
    while ((next = get_long (ml))) {
        uae_u32 next;                   /* <- hoisteada aqui arriba por mi */
        ...
        ml = next;                      /* antes veia la de FUERA, ahora la de dentro */
        ...
        while (first) { next = first; } /* el uso original de la interior */
    }

En el original la interior se declaraba justo antes de su bucle, asi que los usos
de mas arriba veian la exterior. Al subirla al principio del bloque su ambito se
extiende hacia atras y esos usos cambian de variable EN SILENCIO.

verify.py no puede verlo: compara la secuencia de SENTENCIAS, y una declaracion
no es una sentencia. Y el compilador solo avisa (C4700) si la interior se lee
antes de escribirse; si se escribe primero, no hay warning y el bug queda.

Criterio: por cada fichero, se compara la version actual con la de git HEAD. Si un
identificador esta declarado en un bloque interior en la version ACTUAL y tambien
en un ambito que lo engloba, se mira si en HEAD esa declaracion interior estaba
mas abajo. Si lo estaba, y entre la posicion nueva y la vieja hay usos del
nombre, es sospechoso: esos usos han cambiado de variable.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DECL = re.compile(
    r'^(?P<ind>[ \t]*)'
    r'(?:const\s+|static\s+|volatile\s+|register\s+|struct\s+|unsigned\s+|signed\s+)*'
    r'(?P<type>[A-Za-z_]\w*)\s*\**\s*'
    r'(?P<name>[A-Za-z_]\w*)\s*;[ \t]*$')

KEYWORDS = set('return break continue goto else do case default'.split())


def strip_noise(src):
    """Deja comentarios y literales en blanco, conservando longitud y saltos."""
    out = list(src)
    i, n, st = 0, len(src), None
    while i < n:
        c = src[i]
        if st is None:
            if c == '/' and i + 1 < n and src[i + 1] == '/':
                st = 'line'; out[i] = out[i + 1] = ' '; i += 2; continue
            if c == '/' and i + 1 < n and src[i + 1] == '*':
                st = 'block'; out[i] = out[i + 1] = ' '; i += 2; continue
            if c == '"':
                st = 'str'; i += 1; continue
            if c == "'":
                st = 'chr'; i += 1; continue
        elif st == 'line':
            if c == '\n':
                st = None
            else:
                out[i] = ' '
        elif st == 'block':
            if c == '*' and i + 1 < n and src[i + 1] == '/':
                out[i] = out[i + 1] = ' '; st = None; i += 2; continue
            if c != '\n':
                out[i] = ' '
        else:  # str / chr
            if c == '\\':
                out[i] = out[i + 1] = ' '; i += 2; continue
            if (st == 'str' and c == '"') or (st == 'chr' and c == "'"):
                st = None
            else:
                out[i] = ' '
        i += 1
    return ''.join(out)


def scopes(lines):
    """Devuelve, por linea, la profundidad de llaves ANTES de esa linea."""
    depth = 0
    out = []
    for l in lines:
        out.append(depth)
        depth += l.count('{') - l.count('}')
    return out


def func_ids(lines, depths):
    """Numero de funcion (indice de la linea que la abre) por linea. Sin esto el
    detector empareja el 'int i;' de funciones DISTINTAS: 301 falsos positivos."""
    out = []
    cur = -1
    for i, l in enumerate(lines):
        if depths[i] == 0 and '{' in l:
            cur = i
        out.append(cur)
    return out


def decls_by_scope(lines, depths, fids):
    """{(funcion, nombre): [(linea, profundidad)]} de declaraciones simples."""
    found = {}
    for i, l in enumerate(lines):
        m = DECL.match(l)
        if not m:
            continue
        name, typ = m.group('name'), m.group('type')
        if name in KEYWORDS or typ in KEYWORDS:
            continue
        found.setdefault((fids[i], name), []).append((i, depths[i]))
    return found


def audit(path, head_src):
    cur = strip_noise(open(path, 'r', encoding='utf-8', errors='replace').read())
    head = strip_noise(head_src)
    cl, hl = cur.split('\n'), head.split('\n')
    cd, hd = scopes(cl), scopes(hl)
    cf, hf = func_ids(cl, cd), func_ids(hl, hd)
    cdecl, hdecl = decls_by_scope(cl, cd, cf), decls_by_scope(hl, hd, hf)

    # Las funciones se identifican por su linea de apertura, que cambia entre las
    # dos versiones. Para cruzarlas se usa el NOMBRE de la funcion.
    def fname(lines, fid):
        if fid < 0:
            return '<top>'
        m = re.search(r'([A-Za-z_]\w*)\s*\(', lines[fid])
        return m.group(1) if m else str(fid)

    hnames = {}
    for (fid, name), places in hdecl.items():
        hnames.setdefault((fname(hl, fid), name), []).extend(places)

    hits = []
    for (cfid, name), places in cdecl.items():
        if len(places) < 2:
            continue
        depths = sorted(set(d for _l, d in places))
        if len(depths) < 2:
            continue  # mismo nivel en funciones distintas: no es shadow
        # candidato a shadow: el mismo nombre declarado a dos profundidades
        # DENTRO DE LA MISMA FUNCION. Solo interesa si en HEAD la declaracion
        # profunda estaba MAS ABAJO (es decir: la subi yo).
        hplaces = hnames.get((fname(cl, cfid), name), [])
        for line, depth in places:
            if depth == min(depths):
                continue
            moved = [hline for hline, hdep in hplaces if hdep == depth and hline > line]
            if not moved:
                continue
            gap = '\n'.join(cl[line + 1:min(moved) + 1])
            uses = len(re.findall(r'\b%s\b' % re.escape(name), gap))
            if uses:
                hits.append((line + 1, name, depth, min(moved) + 1, uses))
    return hits


def main():
    out = subprocess.run(['git', '-C', ROOT, 'diff', '--name-only', 'HEAD'],
                         capture_output=True, text=True)
    files = [f for f in out.stdout.split('\n')
             if f.endswith('.c') and f.startswith(('sources/src/', 'libretro/', 'retrodep/'))]
    total = 0
    for rel in files:
        path = os.path.join(ROOT, rel.replace('/', os.sep))
        if not os.path.isfile(path):
            continue
        blob = subprocess.run(['git', '-C', ROOT, 'show', 'HEAD:' + rel],
                              capture_output=True, text=True)
        if blob.returncode:
            continue
        hits = audit(path, blob.stdout)
        for line, name, depth, oldline, uses in hits:
            print('%-34s %5d  %-16s prof=%d  antes en %d, %d uso(s) en medio'
                  % (rel, line, name, depth, oldline, uses))
            total += 1
    print('--- sospechosos:', total, 'en', len(files), 'ficheros')


main()
