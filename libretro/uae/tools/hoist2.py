"""
Hoist C99 declarations to the top of their ENCLOSING BLOCK (not the function).

Handles both shapes the c89 checker reports:

    decl-after-stmt   int x = f();            ->  int x;      (block top)
                                                  x  = f();
    for-decl          for (int i = 0; ...)    ->  int i;      (block top)
                                                  for (i = 0; ...)

Hoisting to the enclosing block (rather than the function) is what keeps this
safe: a declaration that shadowed an outer variable or a parameter keeps
shadowing it, because it stays in the same scope.

Anything whose shape is not fully understood is left alone and reported; the
c89 checker will still flag it afterwards.

Usage: python hoist2.py <file.c> <findings.txt>
where findings.txt has "<line> <kind>" per line.
"""
import io
import re
import sys

TYPE_RE = re.compile(
    r'^(?:const\s+|static\s+|volatile\s+|unsigned\s+|signed\s+)*'
    r'(?:struct\s+\w+|union\s+\w+|enum\s+\w+|\w+)\s*\**\s*[\w\s,*\[\]]+$')
NAME_RE = re.compile(r'^[\w\s*]*?(\w+)\s*$')

# Declaracion de un array sin inicializador ('uae_u32 bdata[2]', 'int tmpreg[16]').
# NAME_RE no la reconoce porque acaba en ']', asi que parse_decl la rechaza. Al no
# haber nada ejecutable, la linea entera se mueve al principio del bloque tal cual.
ARRAY_DECL_RE = re.compile(
    r'^(?:(?:const|static|volatile|unsigned|signed|register)\s+)*'
    r'(?:(?:struct|union|enum)\s+\w+|\w+)'
    r'(?:\s*\*+\s*|\s+)'
    r'\w+\s*(?:\[[^\[\];=]*\]\s*)+$')


def strip_noncode(line):
    out = []
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n:
                if line[i] == '\\':
                    i += 2
                    continue
                if line[i] == q:
                    i += 1
                    break
                i += 1
            out.append(' ')
            continue
        if c == '/' and i + 1 < n and line[i + 1] == '/':
            break
        out.append(c)
        i += 1
    return ''.join(out)


def top_level_split(body, sep):
    parts, depth, cur = [], 0, []
    for c in body:
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        if c == sep and depth == 0:
            parts.append(''.join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append(''.join(cur))
    return parts


def split_init(chunk):
    depth = 0
    for i, c in enumerate(chunk):
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        elif c == '=' and depth == 0:
            if i + 1 < len(chunk) and chunk[i + 1] == '=':
                return None
            if i > 0 and chunk[i - 1] in '!<>=+-*/%&|^':
                return None
            return (chunk[:i].strip(), chunk[i + 1:].strip())
    return (chunk.strip(), None)


def parse_decl(body):
    """'uae_u16 a = x, b = y' -> ('uae_u16 a, b', [('a','x'), ('b','y')])."""
    chunks = top_level_split(body, ',')
    parsed = [split_init(c) for c in chunks]
    if any(p is None for p in parsed):
        return None
    first_lhs, first_init = parsed[0]
    if not TYPE_RE.match(first_lhs):
        return None
    m = NAME_RE.match(first_lhs)
    if not m:
        return None
    first_name = m.group(1)
    type_txt = first_lhs[:m.start(1)].rstrip()
    if not type_txt or type_txt in ('return', 'else', 'case', 'goto'):
        return None
    names = [first_name]
    assigns = []
    if first_init is not None:
        assigns.append((first_name, first_init))
    for lhs, init in parsed[1:]:
        m = NAME_RE.match(lhs)
        if not m:
            return None
        nm = m.group(1)
        stars = lhs[:m.start(1)].strip()
        if stars and stars != '*' * len(stars):
            return None
        names.append((stars + nm) if stars else nm)
        if init is not None:
            assigns.append((nm, init))
    return (type_txt + ' ' + ', '.join(names), assigns)


def block_openers(lines):
    stack = []
    opener = {}
    for i, raw in enumerate(lines):
        code = strip_noncode(raw)
        opener[i + 1] = stack[-1] if stack else None
        for c in code:
            if c == '{':
                stack.append(i)
            elif c == '}':
                if stack:
                    stack.pop()
    return opener


def process(path, findings):
    lines = io.open(path, encoding='utf-8', errors='replace').read().split('\n')
    opener = block_openers(lines)
    inserts, edits, skipped = {}, {}, []

    for ln, kind in findings:
        raw = lines[ln - 1]
        indent = raw[:len(raw) - len(raw.lstrip())]
        body = raw.strip()
        op = opener.get(ln)
        if op is None:
            skipped.append((ln, kind, body))
            continue

        if kind == 'decl-after-stmt':
            if not body.endswith(';') or body.count(';') != 1:
                skipped.append((ln, kind, body))
                continue
            inner = body[:-1].strip()
            parsed = parse_decl(inner)
            if parsed is None:
                if ARRAY_DECL_RE.match(inner):
                    inserts.setdefault(op, []).append(indent + inner + ';')
                    edits[ln] = []          # no queda nada en su sitio
                    continue
                skipped.append((ln, kind, body))
                continue
            decl, assigns = parsed
            inserts.setdefault(op, []).append(indent + decl + ';')
            edits[ln] = [indent + nm + ' = ' + rhs + ';' for nm, rhs in assigns]

        elif kind == 'for-decl':
            m = re.match(r'^(\s*)(\}?\s*(?:else\s+)?)for\s*\((.*)$', raw)
            if not m:
                skipped.append((ln, kind, body))
                continue
            rest = m.group(3)
            depth, cut = 1, -1
            for i, c in enumerate(rest):
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                elif c == ';' and depth == 1:
                    cut = i
                    break
            if cut < 0:
                skipped.append((ln, kind, body))
                continue
            init = rest[:cut].strip()
            parsed = parse_decl(init)
            if parsed is None:
                skipped.append((ln, kind, body))
                continue
            decl, assigns = parsed
            inserts.setdefault(op, []).append(indent + decl + ';')
            newinit = ', '.join(nm + ' = ' + rhs for nm, rhs in assigns)
            edits[ln] = [m.group(1) + m.group(2) + 'for (' + newinit + rest[cut:]]
        else:
            skipped.append((ln, kind, body))

    out = []
    for i, raw in enumerate(lines):
        ln = i + 1
        if ln in edits:
            out.extend(edits[ln])
        else:
            out.append(raw)
        for d in inserts.get(i, []):
            out.append(d)

    io.open(path, 'w', encoding='utf-8', newline='').write('\n'.join(out))
    return len(edits), skipped


if __name__ == '__main__':
    path = sys.argv[1]
    findings = []
    for line in io.open(sys.argv[2], encoding='utf-8'):
        parts = line.split()
        if len(parts) >= 2 and parts[0].isdigit():
            findings.append((int(parts[0]), parts[1]))
    n, skipped = process(path, findings)
    print('%s: transformadas %d de %d' % (path, n, len(findings)))
    if skipped:
        print('  NO tocadas (%d):' % len(skipped))
        for s in skipped[:20]:
            print('   ', s)
