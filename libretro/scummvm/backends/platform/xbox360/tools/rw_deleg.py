#!/usr/bin/env python3
"""Inline C++11 delegating constructors (MSVC 2010). usage: rw_deleg.py chk.txt"""
import sys, re
from rw_common import *

warn = {}
for l in open(sys.argv[1]):
    m = re.match(r'^([^:]+):(\d+):(\d+): warning: delegating constructors', l)
    if m:
        warn.setdefault(m.group(1), set()).add((int(m.group(2)), int(m.group(3))))

def ctor_parts(c):
    """returns dict(params=[names], closep, list_start (offset after ':' or None), body=(start,end))"""
    s = src(c.location.file.name)
    toks = list(c.get_tokens())
    depth = 0; started = False; closep = None
    for t in toks:
        if not started and t.spelling == '(' and t.extent.start.offset > c.location.offset:
            started = True
        if started:
            if t.spelling == '(':
                depth += 1
            elif t.spelling == ')':
                depth -= 1
                if depth == 0:
                    closep = t.extent.end.offset
                    break
    body = [g for g in c.get_children() if g.kind == K.COMPOUND_STMT]
    body = body[0]
    bs, be = body.extent.start.offset, body.extent.end.offset
    between = s[closep:bs]
    colon = None
    m = re.search(r'(?<!:):(?!:)', between)
    if m:
        colon = closep + m.end()
    params = [a.spelling for a in c.get_arguments()]
    return dict(params=params, closep=closep, colon=colon, bs=bs, be=be, s=s)

def subst(text, mapping):
    # do not touch comments and string/char literals
    parts = re.split(r'(//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')', text, flags=re.S)
    out = []
    for i, p in enumerate(parts):
        out.append(p if i % 2 == 1 else subst_code(p, mapping))
    return ''.join(out)

def subst_code(text, mapping):
    def r(m):
        name = m.group(2)
        pre = m.group(1)
        if name in mapping and not re.search(r'(\.|->|::)\s*$', pre):
            return pre + mapping[name]
        return m.group(0)
    return re.sub(r'(^|[^\w])([A-Za-z_]\w*)', lambda m: r(m), text)

edits = []
for f, poss in warn.items():
    path = ROOT + '/' + f if not f.startswith('/') else f
    tu = parse(path)
    found = []
    def walk(c):
        for ch in c.get_children():
            if ch.location.file is None or ch.location.file.name != path:
                if ch.location.file is not None:
                    continue
            if ch.kind == K.CONSTRUCTOR and ch.is_definition():
                found.append(ch)
            walk(ch)
    walk(tu.cursor)
    for (ln, col) in sorted(poss):
        cands = [c for c in found if c.extent.start.line <= ln <= c.extent.end.line]
        if not cands:
            print('NOCTOR', f, ln); continue
        c = cands[-1]
        cls = c.semantic_parent
        # find delegating call
        call = None
        for g in c.walk_preorder():
            if g.kind == K.CALL_EXPR and g.referenced is not None and g.referenced.kind == K.CONSTRUCTOR and \
               g.referenced.semantic_parent is not None and g.referenced.semantic_parent.canonical == cls.canonical and \
               g.extent.start.line == ln:
                call = g; break
        if call is None:
            print('NOCALL', f, ln); continue
        tgt = call.referenced.get_definition()
        if tgt is None:
            print('NOTARGETDEF', f, ln); continue
        tp = ctor_parts(tgt)
        dp = ctor_parts(c)
        s = dp['s']
        args = []
        cargs = list(call.get_arguments())
        canon_params = list(call.referenced.canonical.get_arguments())
        for i, a in enumerate(cargs):
            if a.extent.start.file is None:
                toks = [t.spelling for t in canon_params[i].get_tokens()]
                if '=' in toks:
                    args.append(' '.join(toks[toks.index('=') + 1:]))
                else:
                    args.append('/*?*/')
            else:
                args.append(text(a.extent))
        if len(args) != len(tp['params']):
            # default arguments used by the call: take defaults from declaration
            decl = call.referenced
            print('ARGCOUNT', f, ln, len(args), len(tp['params']))
            continue
        mapping = {}
        for p, a in zip(tp['params'], args):
            a = a.strip()
            mapping[p] = a if re.fullmatch(r'[\w\.\->:]+', a) else '(' + a + ')'
        ts = tp['s']
        tinit = ts[tp['colon']:tp['bs']].strip() if tp['colon'] else ''
        tbody = ts[tp['bs'] + 1:tp['be'] - 1]
        if 'return' in tbody:
            print('RETURN-IN-TARGET-BODY', f, ln)
        new_init = subst(tinit, mapping)
        new_body_pre = subst(tbody, mapping)
        dbody = s[dp['bs'] + 1:dp['be'] - 1]
        # replace from delegating ctor colon to end of body
        a0 = dp['colon'] - 1  # position of ':'
        bpre = new_body_pre.strip('\n')
        if bpre.strip() and '\n' not in bpre:
            bpre = '\t' + bpre.strip()
        repl = (': ' + new_init + ' ' if new_init else '') + '{' + \
            '\n\t// (inlined from the delegated constructor: MSVC 2010 has no C++11 delegating constructors)\n' + \
            (bpre.rstrip() + '\n' if bpre.strip() else '') + (dbody.strip('\n') + '\n' if dbody.strip() else '') + '}'
        # keep the original whitespace before ':'
        edits.append((path, a0, dp['be'], repl.replace('{\n\t//', '{\n\t//', 1), 0))
print('edits', apply_edits(edits, dry='--dry' in sys.argv))
