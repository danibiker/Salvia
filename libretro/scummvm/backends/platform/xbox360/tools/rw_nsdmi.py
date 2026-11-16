#!/usr/bin/env python3
"""Move C++11 non-static data member initializers (NSDMI) into constructors (MSVC 2010)."""
import sys, json
from multiprocessing import Pool
from rw_common import *

RECORD_KINDS = (K.STRUCT_DECL, K.CLASS_DECL, K.UNION_DECL, K.CLASS_TEMPLATE, K.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION)

def loc_key(c):
    l = c.location
    return (l.file.name, l.line, l.column)

def zeroish(t):
    t = re.sub(r'\s+', '', t)
    t = re.sub(r'[{}]', '', t)
    t = re.sub(r',+', ',', t).strip(',')
    return t in ('', '0', 'nullptr', 'NULL', 'false', '0.0', '0.0f', '0u', '0x00', '0x0', '0,0', '0,0,0', '0,0,0,0') or re.fullmatch(r'(0|nullptr|false|0\.0f?)(,(0|nullptr|false|0\.0f?))*,?', t) is not None

def has_initlist_ctor(decl):
    d = decl.get_definition() or decl
    for c in d.get_children():
        if c.kind in (K.CONSTRUCTOR, K.FUNCTION_TEMPLATE):
            for p in c.get_arguments() if c.kind == K.CONSTRUCTOR else []:
                if 'initializer_list' in p.type.spelling:
                    return True
    return False

def has_user_ctor(decl):
    d = decl.get_definition() or decl
    for c in d.get_children():
        if c.kind == K.CONSTRUCTOR:
            return True
        if c.kind == K.FUNCTION_TEMPLATE and c.spelling == d.spelling:
            return True
    return False

def field_info(fd, s):
    """returns (delete_start, delete_end, conv_text or None, reason)"""
    name = fd.spelling
    no = fd.location.offset
    if s[no:no + len(name)] != name:
        return None, None, None, 'name-mismatch(macro?)'
    i = no + len(name)
    # skip array dims
    while True:
        j = i
        while j < len(s) and s[j] in ' \t\r\n':
            j += 1
        if j < len(s) and s[j] == '[':
            depth = 0
            while True:
                if s[j] == '[': depth += 1
                elif s[j] == ']':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            i = j + 1
            continue
        break
    j = i
    while s[j] in ' \t\r\n':
        j += 1
    end = fd.extent.end.offset
    if fd.extent.end.file is None or fd.extent.end.file.name != fd.location.file.name:
        return None, None, None, 'macro-extent'
    init = s[j:end]
    if init.startswith('='):
        body = init[1:].strip()
        brace = body.startswith('{') and body.endswith('}')
        if brace:
            inner = body[1:-1].strip()
        else:
            inner = body
    elif init.startswith('{'):
        brace = True
        assert init.endswith('}'), init
        inner = init[1:-1].strip()
    else:
        return None, None, None, 'NOINIT'
    t = fd.type.get_canonical()
    tk = t.kind
    if tk in (T.CONSTANTARRAY, T.INCOMPLETEARRAY, T.DEPENDENTSIZEDARRAY):
        if brace and zeroish(inner.rstrip(',')):
            return i, end, '()', None
        return i, end, None, 'array-with-values'
    if tk == T.RECORD:
        decl = t.get_declaration()
        if brace:
            if has_initlist_ctor(decl) and inner:
                return i, end, None, 'initializer_list-ctor'
            if not has_user_ctor(decl):
                if inner == '' :
                    return i, end, '()', None
                # aggregate init with values
                return i, end, None, 'aggregate-with-values'
            return i, end, '(%s)' % inner, None
        return i, end, '(%s)' % inner, None
    # scalar / pointer / enum / reference
    if brace:
        if inner == '':
            return i, end, '()', None
        return i, end, '(%s)' % inner, None
    return i, end, '(%s)' % inner, None

def process(path):
    tu = parse(path)
    recs = {}   # key -> dict
    ctors = {}  # key -> dict
    def handle_record(r):
        if not r.is_definition():
            return
        k = loc_key(r)
        if k in recs:
            return
        s = src(r.location.file.name)
        fields = []
        has_ctor = False
        idx = 0
        for c in r.get_children():
            if c.kind == K.CONSTRUCTOR:
                has_ctor = True
            if c.kind == K.FUNCTION_TEMPLATE and c.spelling == r.spelling:
                has_ctor = True
            if c.kind == K.FIELD_DECL:
                idx += 1
                if c.is_bitfield():
                    continue
                if any(g.kind.is_expression() for g in c.get_children()):
                    ds, de, conv, reason = field_info(c, s)
                    if reason == 'NOINIT':
                        continue
                    fields.append(dict(name=c.spelling, idx=idx, ds=ds, de=de, conv=conv, reason=reason, line=c.location.line))
        if not fields:
            return
        # body brace
        toks = list(r.get_tokens())
        lb = None
        for t in toks:
            if t.spelling == '{':
                lb = t.extent.start.offset
                break
        recs[k] = dict(file=r.location.file.name, name=r.spelling, kind=str(r.kind), fields=fields, has_ctor=has_ctor, lb=lb,
                       line=r.location.line, union=(r.kind == K.UNION_DECL), anon=r.is_anonymous())
    def handle_ctor(c):
        p = c.semantic_parent
        if p is None or p.kind not in RECORD_KINDS:
            return
        pdef = p.get_definition() or p
        if pdef.location.file is None or not inroot(pdef.location.file):
            return
        pk = loc_key(pdef)
        if not c.is_definition():
            return
        k = loc_key(c)
        s = src(c.location.file.name)
        inited = []
        delegating = False
        body = None
        alltoks = list(c.get_tokens())
        # find the parameter list close paren: first '(' after the ctor name token
        closep = None
        depth = 0; started = False
        for t in alltoks:
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
        for g in c.get_children():
            if g.kind == K.MEMBER_REF:
                inited.append(g.spelling)
            elif g.kind == K.TYPE_REF and g.referenced is not None and g.referenced.canonical == pdef.canonical and closep is not None and g.extent.start.offset > closep and g.location.file.name == c.location.file.name:
                delegating = True
            elif g.kind == K.COMPOUND_STMT:
                body = g
        info = dict(file=c.location.file.name, rec=pk, inited=inited, delegating=delegating, line=c.location.line,
                    defaulted=c.is_default_method(), copy=c.is_copy_constructor() or c.is_move_constructor())
        ts = [t.spelling for t in alltoks]
        if c.is_default_method() and 'default' in ts:
            di = ts.index('default')
            info['defpos'] = (alltoks[di - 1].extent.start.offset, alltoks[di].extent.end.offset)
        elif body is not None:
            bo = body.extent.start.offset
            if body.extent.start.file.name != c.location.file.name or s[bo] != '{' or closep is None:
                info['err'] = 'body-macro'
            else:
                toks = [t for t in alltoks if t.extent.start.offset < bo]
                between = [t.spelling for t in toks if t.extent.start.offset >= closep]
                info['haslist'] = ':' in between
                info['ins'] = toks[-1].extent.end.offset if toks else bo
        else:
            return
        ctors[k] = info
    def walk(c):
        for ch in c.get_children():
            f = ch.location.file
            if f is not None and not inroot(f):
                continue
            if ch.kind in RECORD_KINDS:
                handle_record(ch)
            elif ch.kind == K.CONSTRUCTOR:
                handle_ctor(ch)
            walk(ch)
    walk(tu.cursor)
    return recs, ctors

if __name__ == '__main__':
    files = [l.strip() for l in open(sys.argv[1]) if l.strip().endswith('.cpp')]
    recs = {}; ctors = {}
    with Pool(2) as p:
        for r, c in p.imap_unordered(process, [ROOT + '/' + f for f in files], chunksize=4):
            recs.update(r); ctors.update(c)
    edits = []
    report = []
    ctor_by_rec = {}
    for k, c in ctors.items():
        ctor_by_rec.setdefault(c['rec'], []).append((k, c))
    for rk, r in recs.items():
        fn = r['file']
        if r['union'] or r['anon']:
            report.append((fn, r['line'], 'MANUAL union/anon record with NSDMI ' + r['name']))
            continue
        bad = [f for f in r['fields'] if f['conv'] is None]
        for f in bad:
            report.append((fn, f['line'], 'MANUAL field %s: %s' % (f['name'], f['reason'])))
        good = [f for f in r['fields'] if f['conv'] is not None]
        if not good:
            continue
        for f in good:
            edits.append((fn, f['ds'], f['de'], '', 0))
        rc = ctor_by_rec.get(rk, [])
        if not r['has_ctor']:
            # add implicit default ctor
            if r['lb'] is None:
                report.append((fn, r['line'], 'MANUAL no-lbrace ' + r['name']))
                continue
            inits = ', '.join(f['name'] + f['conv'] for f in good)
            if 'CLASS' in r['kind'] and 'STRUCT' not in r['kind']:
                # class: default private. Need to know if declared with 'struct' keyword in a template
                s = src(fn)
                head = s[:r['lb']]
                kw = re.findall(r'\b(class|struct)\b', head[-400:])
                iskw_struct = kw and kw[-1] == 'struct'
            else:
                iskw_struct = 'STRUCT' in r['kind']
            if iskw_struct:
                txt = '\n\t%s() : %s {}\n' % (r['name'], inits)
            else:
                txt = '\npublic:\n\t%s() : %s {}\nprivate:\n' % (r['name'], inits)
            edits.append((fn, r['lb'] + 1, r['lb'] + 1, txt, 0))
            continue
        for ck, c in rc:
            if c.get('err'):
                report.append((c['file'], c['line'], 'MANUAL ctor ' + c['err']))
                continue
            if c['delegating']:
                continue
            if c['copy'] and c.get('defaulted'):
                continue
            missing = [f for f in good if f['name'] not in c['inited']]
            if not missing:
                continue
            inits = ', '.join(f['name'] + f['conv'] for f in missing)
            if 'defpos' in c:
                a, b = c['defpos']
                edits.append((c['file'], a, b, ': %s {}' % inits, 0))
                # remove trailing ';' handled? "= default;" -> ": x() {};" is valid
            elif 'ins' in c:
                if c['haslist']:
                    edits.append((c['file'], c['ins'], c['ins'], ', ' + inits, 0))
                else:
                    edits.append((c['file'], c['ins'], c['ins'], ' : ' + inits, 0))
        if not rc:
            report.append((fn, r['line'], 'WARN no ctor definitions seen for ' + r['name']))
    for r in sorted(set(report)):
        print(r[0].replace(ROOT + '/', ''), r[1], r[2])
    n = apply_edits(edits, dry=('--dry' in sys.argv))
    print('edits', n)
