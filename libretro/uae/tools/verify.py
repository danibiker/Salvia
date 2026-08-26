"""
Verify a hoist transformation did not change the program.

Compares the sequence of executable statements before/after, mapping both
"TYPE name = expr;" and "name = expr;" to "name=expr", and dropping pure
declarations. Any reordering of side effects shows up as a difference.

Usage: python verify.py <orig.c> <new.c>
"""
import io
import re
import sys

TYPE_RE = re.compile(
    r'^(?:const\s+|static\s+|volatile\s+|unsigned\s+|signed\s+)*'
    r'(?:struct\s+\w+|union\s+\w+|enum\s+\w+|\w+)\s*\**\s*[\w\s,*\[\]]+$')


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


def norm_assign(chunk):
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
            lhs, rhs = chunk[:i].strip(), chunk[i + 1:].strip()
            m = re.match(r'^[\w\s*]*?(\w+)\s*$', lhs)
            if not m:
                return None
            return m.group(1) + '=' + re.sub(r'\s+', '', rhs)
    return None


def norm(path):
    out = []
    for raw in io.open(path, encoding='utf-8', errors='replace'):
        t = raw.strip()
        if not t:
            continue
        # for (init; cond; step)  ->  compare the three clauses
        m = re.match(r'^\}?\s*(?:else\s+)?for\s*\((.*)$', t)
        if m:
            rest = m.group(1)
            depth, cut = 1, -1
            for i, c in enumerate(rest):
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                elif c == ';' and depth == 1:
                    cut = i
                    break
            if cut >= 0:
                init = rest[:cut].strip()
                tail = rest[cut:]
                parts = []
                for ch in top_level_split(init, ','):
                    a = norm_assign(ch)
                    parts.append(a if a else re.sub(r'\s+', '', ch))
                out.append('for(' + ','.join(parts) + re.sub(r'\s+', '', tail))
                continue
        if t.endswith(';') and t.count(';') == 1:
            b = t[:-1].strip()
            parts = top_level_split(b, ',')
            res, ok = [], True
            for k, ch in enumerate(parts):
                a = norm_assign(ch)
                if a is None:
                    if k == 0 and TYPE_RE.match(b):
                        ok = False   # pure declaration: ignore
                    break
                res.append(a)
            if res and ok:
                out.extend(res)
                continue
            if not ok:
                continue
        out.append(re.sub(r'\s+', '', t))
    return out


a = norm(sys.argv[1])
b = norm(sys.argv[2])
print('sentencias: antes %d  despues %d' % (len(a), len(b)))
if a == b:
    print('IDENTICAS -> orden de efectos laterales preservado')
else:
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            print('primera diferencia en %d' % i)
            print('  antes : %s' % x[:100])
            print('  ahora : %s' % y[:100])
            break
    else:
        print('difieren solo en longitud')
