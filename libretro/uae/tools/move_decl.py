"""Mueve declaraciones ENTERAS al principio de su bloque.

Es la transformacion que hoist2.py no sabe hacer: hoist2 parte "T x = expr;" en
"T x;" arriba + "x = expr;" en su sitio, y eso no vale para un array (no se
puede asignar un array). Aqui se mueve la sentencia completa con su
inicializador.

Cuando es seguro:
  - sin inicializador: siempre. No genera codigo.
  - con inicializador CONSTANTE y el bloque contenedor NO es cuerpo de bucle:
    el inicializador se evalua antes, pero al ser constante da lo mismo.
Cualquier otro caso se rechaza y se reporta.

LOCALIZA POR TEXTO, no por numero de linea: al mover una sentencia hacia
ARRIBA se desplazan las lineas intermedias, asi que los numeros de los sitios
que quedan por tratar dejan de valer. Buscar el texto es inmune a eso.

Uso: python tools/move_decl.py <fichero.c> <sitios.txt>
     sitios.txt: una linea por sitio, "<linea><TAB><texto de la declaracion>"
"""
import io
import re
import sys

# el ';' final entra: sin el, "= {0};" se tomaba por no constante
CONSTISH = re.compile(r'^[\s{}\d,;\'"\.\-+*/()xXa-fA-F_]*$')


def strip_str(line):
    """Quita cadenas, caracteres y comentarios // para no contar sus llaves."""
    out = []
    i = 0
    while i < len(line):
        c = line[i]
        if c in '"\'':
            q = c
            i += 1
            while i < len(line):
                if line[i] == '\\':
                    i += 2
                    continue
                if line[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < len(line) and line[i + 1] == '/':
            break
        out.append(c)
        i += 1
    return ''.join(out)


def stmt_end(lines, start):
    depth = 0
    for i in range(start, min(start + 400, len(lines))):
        code = strip_str(lines[i])
        for c in code:
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
        if depth <= 0 and code.rstrip().endswith(';'):
            return i
    return -1


def block_open(lines, idx):
    depth = 0
    for i in range(idx - 1, -1, -1):
        for c in reversed(strip_str(lines[i])):
            if c == '}':
                depth += 1
            elif c == '{':
                if depth == 0:
                    return i
                depth -= 1
    return -1


def is_loop_body(lines, op):
    head = (lines[op - 1] if op > 0 else '') + ' ' + lines[op]
    return bool(re.search(r'\b(for|while|do)\b\s*[({]', strip_str(head)))


def find_line(lines, hint, text):
    """Busca la linea cuyo contenido empieza por 'text', primero cerca de
    'hint' y luego en todo el fichero. Devuelve -1 si no es unica."""
    key = text.strip()[:48]
    cands = [i for i, l in enumerate(lines) if l.strip().startswith(key)]
    if not cands:
        return -1
    if len(cands) == 1:
        return cands[0]
    return min(cands, key=lambda i: abs(i - (hint - 1)))


def main():
    path, sites = sys.argv[1], sys.argv[2]
    src = io.open(path, encoding='utf-8', errors='replace').read()
    nl = '\r\n' if '\r\n' in src else '\n'
    lines = [l.rstrip('\r') for l in src.split('\n')]

    todo = []
    for raw in io.open(sites, encoding='utf-8', errors='replace'):
        raw = raw.rstrip('\n')
        if not raw.strip():
            continue
        n, _, t = raw.partition('\t')
        todo.append((int(n), t))

    moved, refused = 0, []
    for hint, text in todo:
        idx = find_line(lines, hint, text)
        if idx < 0:
            refused.append((hint, 'no encuentro el texto'))
            continue
        end = stmt_end(lines, idx)
        if end < 0:
            refused.append((hint, 'no encuentro el final de la sentencia'))
            continue
        stmt = lines[idx:end + 1]
        joined = strip_str(' '.join(s.strip() for s in stmt))

        if '=' in joined and not CONSTISH.match(joined.split('=', 1)[1]):
            refused.append((hint, 'inicializador no constante'))
            continue
        op = block_open(lines, idx)
        if op < 0 or op >= idx:
            refused.append((hint, 'no encuentro la llave del bloque'))
            continue
        if '=' in joined and is_loop_body(lines, op):
            refused.append((hint, 'con inicializador y dentro de un bucle'))
            continue

        del lines[idx:end + 1]
        lines[op + 1:op + 1] = stmt
        moved += 1

    io.open(path, 'w', encoding='utf-8', newline='').write(nl.join(lines))
    print('%d movidas de %d' % (moved, len(todo)))
    for hint, why in refused:
        print('    RECHAZADA ~%d: %s' % (hint, why))


main()
