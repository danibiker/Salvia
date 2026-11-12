#!/usr/bin/env python3
"""
c89ify.py - move C99 "declarations after statements" to the start of their
block so the code builds with C89 compilers such as MSVC 2010 / the Xbox 360
XDK compiler.

Usage:
    c89ify.py --root <srcroot> [--dry-run] [--report] -- <clang args> -- file1.c file2.c ...

For each block `{ decls; stmts; T x = e; ... }` it rewrites:

    { decls; T x; stmts; x = e; ... }

When hoisting would change meaning (a name used earlier in the block refers
to an outer declaration, array/aggregate initialisers, const objects, local
type declarations...) it falls back to opening a new block:

    { decls; stmts; { T x = e; ... } }

Declarations coming from macro expansions are not touched; they are reported
so they can be fixed in the macro definition.
"""
import sys, os, argparse, ctypes, re
import clang.cindex as ci
from clang.cindex import CursorKind as K, conf

# --- spelling location helper (python bindings only expose expansion loc) ---
_spell = conf.lib.clang_getSpellingLocation
_spell.argtypes = [ci.SourceLocation, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint),
                   ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
_spell.restype = None


def spelling_offset(loc):
    f = ctypes.c_void_p(); l = ctypes.c_uint(); c = ctypes.c_uint(); o = ctypes.c_uint()
    _spell(loc, ctypes.byref(f), ctypes.byref(l), ctypes.byref(c), ctypes.byref(o))
    return o.value


MACRO_RANGES = {}  # path -> sorted list of (start, end) of macro expansions


def collect_macros(tu):
    for c in tu.cursor.get_children():
        if c.kind == K.MACRO_INSTANTIATION and c.location.file:
            p = os.path.realpath(c.location.file.name)
            MACRO_RANGES.setdefault(p, set()).add((c.extent.start.offset, c.extent.end.offset))


def from_macro(cursor):
    """True when the cursor starts inside a macro expansion (its source text
    in the file is the macro invocation, not the code itself)."""
    s = cursor.extent.start
    if not s.file:
        return True
    p = os.path.realpath(s.file.name)
    e = cursor.extent.end.offset
    for (a, b) in MACRO_RANGES.get(p, ()):
        if a <= s.offset < b and e <= b:
            return True
    return False


def fname(loc):
    return os.path.realpath(loc.file.name) if loc.file else None


class FileEdits:
    def __init__(self, path):
        self.path = path
        self.text = open(path, 'rb').read().decode('latin-1')
        self.edits = {}  # (start, end, order) -> text

    def add(self, start, end, text, order=0):
        self.edits[(start, end, order, text)] = text

    def apply(self):
        out = []
        pos = 0
        items = sorted(self.edits.keys(), key=lambda k: (k[0], k[1], k[2]))
        for (s, e, o, t) in items:
            if s < pos:
                raise RuntimeError('overlapping edit in %s at %d' % (self.path, s))
            out.append(self.text[pos:s])
            out.append(re.sub('\x00[0-9]+\x00', '', t))
            pos = e
        out.append(self.text[pos:])
        return ''.join(out)


def line_start(text, off):
    return text.rfind('\n', 0, off) + 1


def indent_of(text, off):
    ls = line_start(text, off)
    i = ls
    while i < len(text) and text[i] in ' \t':
        i += 1
    return text[ls:i]


def is_decl_stmt(c):
    return c.kind == K.DECL_STMT


def within(inner, outer):
    a, b = inner.extent, outer.extent
    if fname(a.start) != fname(b.start):
        return False
    return a.start.offset >= b.start.offset and a.end.offset <= b.end.offset


class Converter:
    def __init__(self, root, report):
        self.root = os.path.realpath(root)
        self.files = {}
        self.report = report
        self.stats = {'hoisted': 0, 'wrapped': 0, 'macro': 0}
        self.macro_sites = set()
        self.done_blocks = set()

    def fe(self, path):
        if path not in self.files:
            self.files[path] = FileEdits(path)
        return self.files[path]

    def in_root(self, path):
        return path and path.startswith(self.root + os.sep) and '/third-party/' not in path

    def process_tu(self, tu):
        self.walk(tu.cursor)

    def walk(self, cursor):
        for c in cursor.get_children():
            p = fname(c.location)
            if c.kind == K.COMPOUND_STMT and self.in_root(p):
                self.compound(c)
            if p is None or self.in_root(p) or c.kind in (K.TRANSLATION_UNIT,):
                self.walk(c)

    # ------------------------------------------------------------------
    def compound(self, comp):
        path = fname(comp.extent.start)
        key = (path, comp.extent.start.offset, comp.extent.end.offset)
        if key in self.done_blocks:
            return
        if from_macro(comp):
            # Whole block inside a macro: report late decls only
            kids = list(comp.get_children())
            seen_stmt = False
            for k in kids:
                if is_decl_stmt(k) and seen_stmt:
                    self.macro_sites.add((path, k.extent.start.line))
                elif not is_decl_stmt(k):
                    seen_stmt = True
            return
        self.done_blocks.add(key)
        kids = list(comp.get_children())
        first_stmt = None
        for i, k in enumerate(kids):
            if not is_decl_stmt(k):
                first_stmt = i
                break
        if first_stmt is None:
            return
        late = [k for k in kids[first_stmt:] if is_decl_stmt(k)]
        if not late:
            return
        fe = self.fe(path)
        text = fe.text
        # insertion point: right after the opening brace of the block, so the
        # hoisted declarations never end up inside an #if region
        brace = comp.extent.start.offset
        eol = text.find('\n', brace)
        if text[brace + 1:eol].strip() == '':
            ins = eol + 1
            ind = indent_of(text, kids[0].extent.start.offset) if line_start(text, kids[0].extent.start.offset) > brace else indent_of(text, brace) + '\t'
            prefix = ''
        else:
            ins = brace + 1
            ind = ' '
            prefix = 'inline'
        hoisted = []
        wraps = []
        for d in late:
            if from_macro(d):
                self.macro_sites.add((path, d.extent.start.line))
                self.stats['macro'] += 1
                continue
            if self.pp_depth(text, comp.extent.start.offset, d.extent.start.offset) != 0:
                self.macro_sites.add((path, d.extent.start.line))
                self.stats['macro'] += 1
                continue
            res = self.try_hoist(comp, d, text)
            if res == 'macro':
                self.macro_sites.add((path, d.extent.start.line))
                self.stats['macro'] += 1
                continue
            if res is None:
                wraps.append(d)
            else:
                decl_text, repl = res
                hoisted.append(decl_text)
                s, e = d.extent.start.offset, self.stmt_end(text, d)
                if repl:
                    fe.add(s, e, repl)
                else:
                    # remove the whole line if it only contains this decl
                    ls = line_start(text, s)
                    le = text.find('\n', e)
                    if text[ls:s].strip() == '' and text[e:le].strip() == '':
                        fe.add(ls, le + 1, '')
                    else:
                        fe.add(s, e, '')
                self.stats['hoisted'] += 1
        if hoisted:
            if prefix == 'inline':
                block = ''.join(' %s;' % h for h in hoisted)
            else:
                block = ''.join('%s%s;\n' % (ind, h) for h in hoisted)
            fe.add(ins, ins, block, order=-1)
        for d in wraps:
            s = d.extent.start.offset
            fe.add(s, s, '{ /* C89 */ ', order=1)
            close = comp.extent.end.offset - 1  # position of '}'
            self.wrapid = getattr(self, 'wrapid', 0) + 1
            fe.add(close, close, '} ' + '\x00%d\x00' % self.wrapid, order=1)
            self.stats['wrapped'] += 1

    def local_types_before(self, comp, d):
        """True if a variable of `d` has a type declared locally inside `comp`
        (it could not be declared before that type)."""
        for v in d.get_children():
            if v.kind != K.VAR_DECL:
                return True
            t = v.type
            for _ in range(8):
                if t.kind == ci.TypeKind.POINTER:
                    t = t.get_pointee()
                elif t.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
                    t = t.element_type
                elif t.kind == ci.TypeKind.ELABORATED:
                    t = t.get_named_type()
                else:
                    break
            decl = t.get_declaration()
            if decl is not None and decl.kind != K.NO_DECL_FOUND and within(decl, comp):
                return True
        return False

    def pp_depth(self, text, a, b):
        depth = 0
        for line in text[a:b].split('\n'):
            l = line.strip()
            if l.startswith('#'):
                l = l[1:].strip()
                if l.startswith('if'):
                    depth += 1
                elif l.startswith('endif'):
                    depth -= 1
        return depth

    def stmt_end(self, text, d):
        e = d.extent.end.offset
        # DECL_STMT extent includes the ';'
        return e

    def try_hoist(self, comp, d, text):
        vars_ = list(d.get_children())
        if not vars_ or any(v.kind != K.VAR_DECL for v in vars_):
            return None
        names = [v.spelling for v in vars_]
        for v in vars_:
            if text[v.location.offset:v.location.offset + len(v.spelling)] != v.spelling:
                return 'macro'

        # whole declaration can move if every initializer is a constant expression
        allconst = all(self.initializer(v) is None or self.is_const_init(self.initializer(v)) for v in vars_)
        if allconst and any(self.initializer(v) is not None for v in vars_):
            if not self.name_conflict(comp, d, set(names)) and not self.local_types_before(comp, d):
                dtext = text[d.extent.start.offset:d.extent.end.offset].strip()
                if dtext.endswith(';'):
                    dtext = dtext[:-1].rstrip()
                return dtext, ''
        for v in vars_:
            t = v.type
            if v.storage_class in (ci.StorageClass.STATIC, ci.StorageClass.EXTERN):
                if len(vars_) != 1:
                    return None
            if t.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY, ci.TypeKind.VARIABLEARRAY):
                if self.initializer(v) is not None and v.storage_class != ci.StorageClass.STATIC:
                    return None
            init = self.initializer(v)
            if init is not None and init.kind == K.INIT_LIST_EXPR and v.storage_class != ci.StorageClass.STATIC:
                return None
            if t.is_const_qualified() and init is not None and v.storage_class != ci.StorageClass.STATIC:
                return None
        # local type declarations earlier in the block -> unsafe
        if self.local_types_before(comp, d):
            return None
        # shadowing: names used between block start and the decl that bind to an outer declaration
        if self.name_conflict(comp, d, set(names)):
            return None
        # build the hoisted declaration (decl text without initialisers) and the replacement
        s = d.extent.start.offset
        e = d.extent.end.offset
        dtext = text[s:e]
        if dtext.rstrip().endswith(';'):
            dtext = dtext.rstrip()[:-1]
        if any(v.storage_class == ci.StorageClass.STATIC for v in vars_) or any(v.storage_class == ci.StorageClass.EXTERN for v in vars_):
            return dtext.strip(), ''
        decl_parts = []
        assigns = []
        cut = []
        for v in vars_:
            init = self.initializer(v)
            if init is None:
                continue
            # find '=' between declarator name and init start
            istart = init.extent.start.offset
            if from_macro(init):
                # initializer starts with a macro expansion: use expansion offsets (fine for text)
                pass
            eq = text.rfind('=', s, istart)
            if eq < 0:
                return None
            # init end: for last var it's the decl end minus ';'
            iend = init.extent.end.offset
            if iend > e or iend <= istart:
                return None
            cut.append((eq, iend))
            assigns.append('%s = %s;' % (v.spelling, text[istart:iend].strip()))
        # remove initialisers from decl text
        out = []
        pos = s
        for (a, b) in cut:
            out.append(text[pos:a].rstrip())
            pos = b
        out.append(text[pos:e])
        h = ''.join(out).strip()
        if h.endswith(';'):
            h = h[:-1].rstrip()
        return h, ' '.join(assigns)

    def is_const_init(self, init):
        ok_ref = (K.ENUM_CONSTANT_DECL, K.FUNCTION_DECL)
        def ok(c):
            if c.kind == K.CALL_EXPR:
                return False
            if c.kind == K.DECL_REF_EXPR:
                r = c.referenced
                if r is None:
                    return False
                if r.kind in ok_ref:
                    pass
                elif r.kind == K.VAR_DECL and r.semantic_parent is not None and r.semantic_parent.kind == K.TRANSLATION_UNIT:
                    pass
                else:
                    return False
            for k in c.get_children():
                if not ok(k):
                    return False
            return True
        return ok(init)

    def initializer(self, v):
        """Return the initializer expression of a VAR_DECL (None if absent).
        The initializer is the child expression that follows a top-level '='
        after the declarator name (array sizes are also child expressions)."""
        text = self.fe(fname(v.extent.start)).text
        a = v.location.offset
        b = v.extent.end.offset
        depth = 0
        eq = None
        i = a
        while i < b:
            ch = text[i]
            if ch in '([':
                depth += 1
            elif ch in ')]':
                depth -= 1
            elif ch == '=' and depth == 0 and text[i + 1:i + 2] != '=':
                eq = i
                break
            i += 1
        if eq is None:
            return None
        for k in v.get_children():
            if (k.kind.is_expression() or k.kind == K.INIT_LIST_EXPR) and k.extent.start.offset > eq:
                return k
        return None

    def name_conflict(self, comp, d, names):
        start = comp.extent.start.offset
        dstart = d.extent.start.offset
        path = fname(comp.extent.start)

        def visit(c):
            for k in c.get_children():
                ks = k.extent.start
                if fname(ks) == path and ks.offset >= dstart:
                    return False
                if k.kind in (K.DECL_REF_EXPR, K.TYPE_REF) and k.spelling in names:
                    ref = k.referenced
                    if ref is not None and not within(ref, comp):
                        return True
                if k.kind == K.DECL_REF_EXPR and k.spelling in names and k.referenced is None:
                    return True
                if visit(k):
                    return True
            return False
        return visit(comp)


def main():
    argv = sys.argv[1:]
    if '--' not in argv:
        print(__doc__)
        return 1
    i = argv.index('--')
    opts = argv[:i]
    rest = argv[i + 1:]
    j = rest.index('--')
    cargs = rest[:j]
    files = rest[j + 1:]
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--report', action='store_true')
    o = ap.parse_args(opts)
    conv = Converter(o.root, o.report)
    idx = ci.Index.create()
    for f in files:
        tu = idx.parse(f, args=cargs, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        errs = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
        if errs:
            for d in errs[:5]:
                print('ERROR', d, file=sys.stderr)
            print('skipping', f, file=sys.stderr)
            continue
        collect_macros(tu)
        conv.process_tu(tu)
    for path, fe in sorted(conv.files.items()):
        if not fe.edits:
            continue
        new = fe.apply()
        if o.dry_run:
            print('would edit', path, len(fe.edits))
        else:
            open(path, 'wb').write(new.encode('latin-1'))
    print('stats', conv.stats, file=sys.stderr)
    for (p, l) in sorted(conv.macro_sites):
        print('MACRO %s:%d' % (os.path.relpath(p, o.root), l), file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
