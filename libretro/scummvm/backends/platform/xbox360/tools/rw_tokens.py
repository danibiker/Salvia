#!/usr/bin/env python3
"""Token level rewrites driven by clang -Wc++98-compat warnings positions.
usage: rw_tokens.py <chk.txt>"""
import sys, re
from rw_common import ROOT, apply_edits

lines = open(sys.argv[1]).read().splitlines()
edits = []
report = []
_cache = {}
def getsrc(f):
    if f not in _cache:
        _cache[f] = open(ROOT + '/' + f, 'rb').read().decode('latin-1')
    return _cache[f]

def offset(f, line, col):
    s = getsrc(f)
    pos = 0
    for _ in range(line - 1):
        pos = s.index('\n', pos) + 1
    return pos + col - 1

seen = set()
for l in lines:
    m = re.match(r'^([^:]+):(\d+):(\d+): warning: (.*)$', l)
    if not m:
        continue
    f, ln, col, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
    if f.startswith('/') or (f, ln, col) in seen:
        continue
    seen.add((f, ln, col))
    s = getsrc(f)
    o = offset(f, ln, col)
    def tok_is(t):
        return s[o:o + len(t)] == t
    if "'final' keyword" in msg:
        if tok_is('final'):
            a = o
            while s[a - 1] in ' \t':
                a -= 1
            edits.append((ROOT + '/' + f, a, o + 5, '', 0))
        else:
            report.append((f, ln, 'final-not-at-pos(macro?)'))
    elif 'noexcept specifications' in msg:
        if not tok_is('noexcept'):
            j = s.find('noexcept', s.rfind('\n', 0, o))
            if j != -1 and j < s.find('\n', o):
                o = j
        if tok_is('noexcept'):
            a = o
            while s[a - 1] in ' \t':
                a -= 1
            edits.append((ROOT + '/' + f, a, o + 8, '', 0))
        else:
            report.append((f, ln, 'noexcept-not-at-pos'))
    elif 'scoped enumerations' in msg:
        mm = re.match(r'enum\s+(class|struct)\s+', s[o:])
        mm2 = re.match(r'(class|struct)\s+', s[o:])
        if mm2 and not mm:
            edits.append((ROOT + '/' + f, o, o + mm2.end(), '', 0))
        elif mm:
            edits.append((ROOT + '/' + f, o, o + mm.end(), 'enum ', 0))
        else:
            report.append((f, ln, 'enum-class-not-at-pos'))
    elif "'constexpr' specifier" in msg:
        if not tok_is('constexpr'):
            report.append((f, ln, 'constexpr-not-at-pos(macro?)'))
            continue
        rest = s[o + 9:]
        eol = rest.find('\n')
        line_rest = rest[:eol]
        pe = [i for i in (line_rest.find('('), line_rest.find('='), line_rest.find(';')) if i != -1]
        first = min(pe) if pe else -1
        isvar = first != -1 and line_rest[first] in '=;'
        # skip whitespace after
        b = o + 9
        while s[b] in ' \t':
            b += 1
        if isvar:
            if re.match(r'const\b', s[b:]):
                edits.append((ROOT + '/' + f, o, b, '', 0))
            else:
                edits.append((ROOT + '/' + f, o, b, 'const ', 0))
        else:
            # function / constructor: drop constexpr.  Namespace-scope functions need inline
            linestart = s.rfind('\n', 0, o) + 1
            before = s[linestart:o].strip()
            # heuristic: if the declaration starts at column 0 (not indented) and is not a member definition -> inline
            if s[linestart:o] == '' and '::' not in line_rest.split('(')[0] and 'inline' not in line_rest.split('(')[0]:
                edits.append((ROOT + '/' + f, o, b, 'inline ', 0))
            else:
                edits.append((ROOT + '/' + f, o, b, '', 0))
    elif 'alias declarations' in msg:
        if not s[o:].startswith('using'):
            j = s.rfind('using', 0, o)
            if j != -1 and s.rfind('\n', 0, o) < j:
                o = j
        mm = re.match(r'using\s+(\w+)\s*=\s*([^;]+);', s[o:])
        if not mm:
            report.append((f, ln, 'alias-parse'))
            continue
        # alias templates are reported at the 'using' token too; skip if preceded by template<...>
        before = s[:o].rstrip()
        if before.endswith('>') and re.search(r'template\s*<[^;{}]*>$', before):
            report.append((f, ln, 'ALIAS TEMPLATE manual'))
            continue
        rhs = mm.group(2).strip()
        edits.append((ROOT + '/' + f, o, o + mm.end(), 'typedef %s %s;' % (rhs, mm.group(1)), 0))
for r in sorted(set(report)):
    print(r)
print('edits', apply_edits(edits, dry='--dry' in sys.argv))
