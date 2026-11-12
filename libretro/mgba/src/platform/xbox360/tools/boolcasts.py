#!/usr/bin/env python3
# List implicit conversions to bool (_Bool) from non-boolean expressions.
# With MSVC 2010 (no C99 _Bool) bool is `unsigned char`, so such conversions
# truncate instead of normalising to 0/1.
import sys, json, subprocess, os

class Tracker:
    def __init__(self):
        self.file = None; self.line = None
    def bare(self, l):
        if 'file' in l: self.file = l['file']
        if 'line' in l: self.line = l['line']
        return (self.file, self.line, l.get('col'), l.get('offset'), l.get('tokLen'))
    def loc(self, l):
        """process a location object; return its expansion (file,line,col) and whether macro"""
        if not l:
            return None, False
        if 'spellingLoc' in l or 'expansionLoc' in l:
            r = None
            for k in l:  # keep JSON order
                if k == 'spellingLoc':
                    self.bare(l[k])
                elif k == 'expansionLoc':
                    r = self.bare(l[k])
            return r, True
        return self.bare(l), False

TRIV = ('==', '!=', '<', '>', '<=', '>=', '&&', '||')
def triv(sub):
    while sub.get('kind') in ('ParenExpr',):
        sub = sub.get('inner', [{}])[0]
    k = sub.get('kind'); op = sub.get('opcode')
    if k == 'BinaryOperator' and op in TRIV: return True
    if k == 'UnaryOperator' and op == '!': return True
    if k == 'IntegerLiteral' and sub.get('value') in ('0', '1'): return True
    if k == 'CharacterLiteral' and sub.get('value') in (0, 1): return True
    return False

def walk(o, t, out):
    if not isinstance(o, dict):
        return
    here = None; mac = False
    for key in o:
        if key == 'loc':
            r, m = t.loc(o['loc'])
        elif key == 'range':
            rb, mb = t.loc(o['range'].get('begin', {}))
            re_, me = t.loc(o['range'].get('end', {}))
            here, mac = rb, (mb or me)
            if here and re_:
                here = here + (re_[3] + (re_[4] or 0) if re_[3] is not None else None, re_[0])
    if o.get('kind') in ('ImplicitCastExpr', 'CStyleCastExpr') and o.get('castKind') in ('IntegralToBoolean', 'PointerToBoolean', 'FloatingToBoolean'):
        sub = o.get('inner', [{}])[0]
        if not triv(sub):
            st = sub.get('type', {}).get('qualType', '')
            out.add((here[0], here[1], here[2], o['castKind'], st, 'macro' if mac else '', here[3], here[5] if len(here) > 5 else None, here[6] if len(here) > 6 else None))
    for key in o:
        v = o[key]
        if key == 'inner' or key == 'array_filler':
            for c in v:
                walk(c, t, out)

def main():
    i = sys.argv.index('--')
    files = sys.argv[1:i]; cargs = sys.argv[i+1:]
    res = set()
    for f in files:
        out = subprocess.run(['clang', '-fsyntax-only', '-Xclang', '-ast-dump=json'] + cargs + [f], capture_output=True, text=True).stdout
        if not out:
            print('no ast for', f, file=sys.stderr); continue
        walk(json.loads(out), Tracker(), res)
    fix = '--fix' in cargs
    edits = {}
    for r in sorted(res, key=lambda x: (str(x[0]), x[1] or 0, x[2] or 0)):
        print('%s:%s:%s %s from %s %s' % r[:6])
        if r[5] or r[8] != r[0] or r[6] is None or r[7] is None:
            continue
        edits.setdefault(r[0], set()).add((r[6], r[7]))
    if os.environ.get('FIX'):
        for f, es in edits.items():
            text = open(f, 'rb').read().decode('latin-1')
            # apply from the end; nested ranges are fine because we only insert
            ins = []
            for (b, e) in es:
                ins.append((e, ')'))
                ins.append((b, '!!('))
            for pos, t in sorted(ins, key=lambda x: (-x[0], 0 if x[1] == ')' else 1)):
                text = text[:pos] + t + text[pos:]
            open(f, 'wb').write(text.encode('latin-1'))
            print('fixed', f, len(es), file=sys.stderr)
main()
