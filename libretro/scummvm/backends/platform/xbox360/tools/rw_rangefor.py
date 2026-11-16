#!/usr/bin/env python3
"""Rewrite C++11 range-based for loops into iterator loops (MSVC 2010 compatible)."""
import sys, json
from multiprocessing import Pool
from rw_common import *

def unwrap(c):
    while c.kind in (K.UNEXPOSED_EXPR, K.PAREN_EXPR) and len(list(c.get_children())) == 1:
        c = list(c.get_children())[0]
    return c

SIMPLE_RE = re.compile(r'^[\w\s\.\->\*\[\]:&()]+$')

def classify(expr):
    """returns 'array', 'lvalue', 'lref_call', 'value'"""
    t = expr.type.get_canonical()
    if t.kind in (T.CONSTANTARRAY,):
        return 'array'
    u = unwrap(expr)
    if u.kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR, K.ARRAY_SUBSCRIPT_EXPR):
        return 'lvalue'
    if u.kind == K.UNARY_OPERATOR:
        toks = [x.spelling for x in u.get_tokens()]
        if toks and toks[0] == '*':
            return 'lvalue'
    if u.kind == K.CALL_EXPR:
        ref = u.referenced
        if ref is not None and ref.result_type.kind == T.LVALUEREFERENCE:
            return 'lref_call'
        if ref is None:
            return 'dependent_call'
        return 'value'
    if u.kind == K.INIT_LIST_EXPR:
        return 'initlist'
    return 'other:' + str(u.kind)

def find_stmt_end(s, ext_end, body):
    """offset just after the end of a (non-compound) statement body"""
    if body.kind == K.COMPOUND_STMT:
        return ext_end
    # scan forward for ';' (skip whitespace); handle statements whose extent already includes ';' or end with '}'
    i = ext_end
    if s[i - 1] in ';}':
        # e.g. if-with-compound; but expression statements normally exclude ';'
        j = i
        while j < len(s) and s[j] in ' \t':
            j += 1
        if j < len(s) and s[j] == ';' and s[i - 1] != ';':
            return j + 1
        return i
    while i < len(s) and s[i] != ';':
        i += 1
    return i + 1

def process(path):
    edits = []
    report = []
    tu = parse(path)
    def walk(c, depth):
        for ch in c.get_children():
            f = ch.location.file
            if f is not None and not inroot(f):
                continue
            if ch.kind == K.CXX_FOR_RANGE_STMT:
                try:
                    do_for(ch, depth, edits, report)
                except Exception as e:
                    report.append((ch.location.file.name, ch.location.line, 'EXC ' + repr(e)))
            walk(ch, depth + 1)
    walk(tu.cursor, 0)
    return edits, report

def do_for(st, depth, edits, report):
    fn = st.extent.start.file.name
    s = src(fn)
    kids = list(st.get_children())
    # children: loop var decl, range expr, body (libclang may expose more for implicit stmts)
    var = [k for k in kids if k.kind == K.VAR_DECL]
    body = kids[-1]
    if not var:
        report.append((fn, st.location.line, 'novar'))
        return
    var = var[0]
    # range expr is child after var
    i = kids.index(var)
    expr = kids[i + 1]
    so = st.extent.start.offset
    if s[so:so + 3] != 'for':
        report.append((fn, st.location.line, 'macro/for-not-at-start'))
        return
    if expr.extent.start.file is None or expr.extent.start.file.name != fn or body.extent.end.file.name != fn:
        report.append((fn, st.location.line, 'macro'))
        return
    lp = s.index('(', so)
    eo = expr.extent.start.offset
    ee = expr.extent.end.offset
    header = s[lp + 1:eo]
    # header = DECL : (maybe with whitespace)
    m = re.match(r'^(.*?)\s*(?<!:):(?!:)\s*$', header, re.S)
    if not m:
        report.append((fn, st.location.line, 'header parse: ' + header))
        return
    decl = m.group(1).strip()
    # closing paren of for header
    rp = ee
    while s[rp] != ')':
        if not s[rp].isspace():
            report.append((fn, st.location.line, 'rparen: ' + s[ee:ee + 30]))
            return
        rp += 1
    exprtxt = s[eo:ee]
    kind = classify(expr)
    line = st.location.line
    col = st.location.column
    it = 'rfIt%d' % line
    en = 'rfEnd%d' % line
    rg = 'rfRange%d' % line
    if depth > 0 and False:
        pass
    pre = ''
    post = ''
    if kind == 'array':
        e = exprtxt if re.match(r'^[\w\.\->:\[\]]+$', exprtxt) else '(' + exprtxt + ')'
        newhdr = 'auto %s = &%s[0], %s = &%s[0] + sizeof(%s) / sizeof(%s[0]); %s != %s; ++%s' % (it, e, en, e, e, e, it, en, it)
    elif kind in ('lvalue',) or (kind == 'dependent_call' and False):
        e = exprtxt if re.match(r'^[\w\.\->:\[\]\*]+$', exprtxt) and not exprtxt.startswith('*') else '(' + exprtxt + ')'
        newhdr = 'auto %s = %s.begin(), %s = %s.end(); %s != %s; ++%s' % (it, e, en, e, it, en, it)
    elif kind in ('lref_call',):
        pre = '{ auto &%s = %s; ' % (rg, exprtxt)
        post = ' }'
        newhdr = 'auto %s = %s.begin(), %s = %s.end(); %s != %s; ++%s' % (it, rg, en, rg, it, en, it)
    elif kind in ('value', 'dependent_call') or kind.startswith('other'):
        # copy into a local (the temporary would be destroyed otherwise)
        if kind.startswith('other') and expr.type.kind == T.LVALUEREFERENCE:
            pass
        pre = '{ auto %s = %s; ' % (rg, exprtxt) if kind != 'dependent_call' else '{ auto &&%s = %s; ' % (rg, exprtxt)
        post = ' }'
        newhdr = 'auto %s = %s.begin(), %s = %s.end(); %s != %s; ++%s' % (it, rg, en, rg, it, en, it)
        report.append((fn, line, 'NOTE copy-hoist ' + kind + ' : ' + exprtxt))
    else:
        report.append((fn, line, 'UNHANDLED ' + kind + ' : ' + exprtxt))
        return
    vardef = '%s = *%s;' % (decl, it)
    edits.append((fn, lp + 1, rp, newhdr, 0))
    if body.kind == K.COMPOUND_STMT:
        bo = body.extent.start.offset
        assert s[bo] == '{', (fn, line)
        # keep indentation: insert after '{' on a new line
        # determine indentation of first statement
        nl = s.find('\n', bo)
        ind = ''
        if nl != -1:
            m2 = re.match(r'[ \t]*', s[nl + 1:])
            ind = m2.group(0)
        if nl != -1 and s[bo + 1:nl].strip() == '':
            edits.append((fn, nl + 1, nl + 1, ind + vardef + '\n', depth))
        else:
            edits.append((fn, bo + 1, bo + 1, ' ' + vardef, depth))
        bend = body.extent.end.offset
    else:
        bo = body.extent.start.offset
        bend = find_stmt_end(s, body.extent.end.offset, body)
        edits.append((fn, bo, bo, '{ ' + vardef + ' ', -depth))
        edits.append((fn, bend, bend, ' }', 100 - depth))
    if pre:
        edits.append((fn, so, so, pre, -100 - depth))
        edits.append((fn, bend, bend, post, 200 - depth))

if __name__ == '__main__':
    files = [l.strip() for l in open(sys.argv[1]) if l.strip().endswith('.cpp')]
    allE = []
    allR = set()
    with Pool(2) as p:
        for e, r in p.imap_unordered(process, [ROOT + '/' + f for f in files], chunksize=4):
            allE.extend(e)
            allR.update(r)
    for r in sorted(allR):
        print(r)
    n = apply_edits(allE, dry=('--dry' in sys.argv))
    print('edits', n, 'unique', len(set(allE)))
