#!/usr/bin/env python3
"""Static detector of non-C89 constructs (MSVC 2010 /TC).  ANALYSIS ONLY - never modifies sources."""
import os

# Raiz del arbol deducida de la ubicacion de este script (tools/ vive
# dentro del proyecto). Antes estaba cableada a una ruta absoluta: al
# mover el arbol, collect_types() recorria un directorio inexistente, se
# quedaba sin tipos y CADA declaracion pasaba por sentencia -- 646 falsos
# positivos en ficheros que compilaban limpios.
_TREE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
import re, sys, os

BUILTIN = set("""void char short int long float double signed unsigned bool _Bool
struct union enum const volatile register static auto extern inline __inline _inline
size_t ssize_t ptrdiff_t time_t FILE va_list""".split())

KEYWORDS = set("""if else for while do switch case default break continue return goto
sizeof typedef struct union enum static const volatile register extern inline
__inline _inline __declspec __cdecl __stdcall __fastcall REGPARAM REGPARAM2 REGPARAM3
STATIC_INLINE defined""".split())

STMT_STARTERS = set("if else for while do switch case default break continue return goto".split())


def strip_noise(src):
    """Blank out comments and literals, keeping line structure/length."""
    out = list(src)
    i, n = 0, len(src)
    state = None  # 'line','block','str','chr'
    while i < n:
        c = src[i]
        if state is None:
            if c == '/' and i + 1 < n and src[i+1] == '/':
                state = 'line'; out[i] = out[i+1] = ' '; i += 2; continue
            if c == '/' and i + 1 < n and src[i+1] == '*':
                state = 'block'; out[i] = out[i+1] = ' '; i += 2; continue
            if c == '"':
                state = 'str'; i += 1; continue
            if c == "'":
                state = 'chr'; i += 1; continue
            i += 1; continue
        if state == 'line':
            if c == '\n':
                state = None
            else:
                out[i] = ' '
            i += 1; continue
        if state == 'block':
            if c == '*' and i + 1 < n and src[i+1] == '/':
                out[i] = out[i+1] = ' '; state = None; i += 2; continue
            if c != '\n':
                out[i] = ' '
            i += 1; continue
        # string / char literal
        if c == '\\':
            out[i] = ' '
            if i + 1 < n and src[i+1] != '\n':
                out[i+1] = ' '
            i += 2; continue
        if (state == 'str' and c == '"') or (state == 'chr' and c == "'"):
            state = None; i += 1; continue
        if c != '\n':
            out[i] = ' '
        i += 1
    return ''.join(out)


# Build-configuration knowledge, so that code inside dead #if regions is ignored.
# ON  : macros known to be defined for the VS2010 / libretro Win32 build
# OFF : macros known NOT to be defined (commented out or #undef'd in sysconfig.h)
CFG_ON = set()
CFG_OFF = set()
# every NAME that appears in a '#define NAME' anywhere in the tree
ALL_DEFINED = set()


def load_all_defines(roots):
    for root in roots:
        for dirpath, _dirs, files in os.walk(root):
            for f in files:
                if not f.endswith(('.h', '.c', '.cpp')):
                    continue
                try:
                    txt = open(os.path.join(dirpath, f), 'r', encoding='utf-8',
                               errors='replace').read()
                except OSError:
                    continue
                for m in re.finditer(r'^\s*#\s*define\s+([A-Za-z_]\w*)', txt, re.M):
                    ALL_DEFINED.add(m.group(1))


def load_vcxproj_defines(root):
    """Los <PreprocessorDefinitions> del vcxproj tambien definen simbolos, y sin
    ellos regiones enteras se toman por codigo muerto y no se revisan.
    USE_LIBRETRO_VFS es el caso que dejo pasar errores reales."""
    path = os.path.join(root, 'libretro-uae.vcxproj')
    try:
        txt = open(path, 'r', encoding='utf-8-sig', errors='replace').read()
    except OSError:
        return
    for blob in re.findall(r'<PreprocessorDefinitions>([^<]*)', txt):
        for tok in blob.split(';'):
            tok = tok.strip()
            if not tok or tok.startswith('%('):
                continue
            name = tok.split('=', 1)[0].strip()
            if re.match(r'^[A-Za-z_]\w*$', name):
                CFG_ON.add(name)
                CFG_OFF.discard(name)


def load_config(sysconfig_path):
    CFG_ON.update(['__LIBRETRO__', 'WIN32', '_WIN32', '_MSC_VER'])
    CFG_OFF.update(['__GNUC__', '__linux__', '__APPLE__', '_XBOX', 'WIIU',
                    '__SWITCH__', 'VITA', '__PS3__', 'MSB_FIRST',
                    'WORDS_BIGENDIAN', '__cplusplus', 'AMIGA', '_AIX'])
    try:
        txt = open(sysconfig_path, 'r', encoding='utf-8', errors='replace').read()
    except OSError:
        return
    for ln in txt.split('\n'):
        s = ln.strip()
        m = re.match(r'^#\s*define\s+([A-Za-z_]\w*)', s)
        if m:
            CFG_ON.add(m.group(1))
            continue
        m = re.match(r'^(?://|/\*)\s*#\s*define\s+([A-Za-z_]\w*)', s)
        if m:
            CFG_OFF.add(m.group(1))
            continue
        m = re.match(r'^/\*\s*#\s*undef\s+([A-Za-z_]\w*)', s)
        if m:
            CFG_OFF.add(m.group(1))
    # sysconfig.h explicitly #undefs these for WIN32
    for n in ('HAVE_UNISTD_H', 'HAVE_UTIME_H', 'TIME_WITH_SYS_TIME', 'HAVE_SYS_TIME_H'):
        CFG_OFF.add(n)
        CFG_ON.discard(n)
    CFG_OFF.difference_update(CFG_ON)


def eval_cond(expr, numdefs=None):
    """Return True / False / None(unknown) for a #if-style condition."""
    e = expr.strip()
    if re.fullmatch(r'0+', e):
        return False
    if re.fullmatch(r'0*1', e):
        return True
    # NAME, NAME > 0, NAME == 2 ... where NAME has a numeric #define in this file
    if numdefs:
        m = re.fullmatch(r'([A-Za-z_]\w*)\s*(?:(==|!=|>=|<=|>|<)\s*(\d+))?', e)
        if m and m.group(1) in numdefs:
            lhs = numdefs[m.group(1)]
            if not m.group(2):
                return lhs != 0
            rhs = int(m.group(3))
            return {'==': lhs == rhs, '!=': lhs != rhs, '>=': lhs >= rhs,
                    '<=': lhs <= rhs, '>': lhs > rhs, '<': lhs < rhs}[m.group(2)]
    m = re.fullmatch(r'defined\s*\(?\s*([A-Za-z_]\w*)\s*\)?', e)
    if m:
        e = m.group(1)
    elif not re.fullmatch(r'[A-Za-z_]\w*', e):
        m = re.fullmatch(r'!\s*defined\s*\(?\s*([A-Za-z_]\w*)\s*\)?', e)
        if m:
            v = eval_cond(m.group(1))
            return None if v is None else not v
        return None  # compound expression: don't guess
    if e in CFG_ON:
        return True
    if e in CFG_OFF:
        return False
    # A SHOUTY_NAME that has no '#define' anywhere in the tree cannot be set
    # (project feature macros all come from sysconfig.h / headers, never the
    # compiler). Compiler/system macros use leading underscores or __X__.
    if ALL_DEFINED and re.fullmatch(r'[A-Z][A-Z0-9_]*', e) and e not in ALL_DEFINED:
        return False
    return None


def blank_preproc(txt):
    """Blank preprocessor lines, and blank out code inside regions that are
    definitely not compiled for this build configuration."""
    # numeric #defines local to this file (e.g. '#define MMUOP_DEBUG 0').
    # Collected *inline* below so a #define inside a dead #if region cannot
    # override the live one (fpp.c / fpp_native.c define theirs twice).
    numdefs = {}
    lines = txt.split('\n')
    stack = []           # list of (state, seen_true) per open conditional
    cont = False
    for idx, ln in enumerate(lines):
        s = ln.lstrip()
        directive = None
        if not cont and s.startswith('#'):
            directive = s[1:].lstrip()
        if cont or s.startswith('#'):
            cont = ln.rstrip().endswith('\\')
            lines[idx] = ' ' * len(ln)
        elif any(st is False for st, _ in stack):
            lines[idx] = ' ' * len(ln)      # inside a dead region
            continue
        else:
            continue

        if directive is None:
            continue
        md = re.match(r'define\s+([A-Za-z_]\w*)\s+(\d+)\s*$', directive)
        if md and all(st is True for st, _ in stack):
            # Only trust a numeric #define whose region is unambiguously live.
            # fpp.c has '#define SUPPORT_MMU 1' / '#define SUPPORT_MMU 0' under
            # an #ifndef we cannot evaluate; taking the last one made the whole
            # '#if SUPPORT_MMU' body look dead and hid two for-decl violations.
            numdefs[md.group(1)] = int(md.group(2))
        m = re.match(r'(ifdef|ifndef|if|elif|else|endif)\b(.*)', directive)
        if not m:
            continue
        kw, rest = m.group(1), m.group(2)
        # strip trailing comment
        rest = re.sub(r'/[/*].*$', '', rest)
        if kw == 'ifdef':
            v = eval_cond(rest.strip(), numdefs)
            stack.append((v, v is True))
        elif kw == 'ifndef':
            v = eval_cond(rest.strip(), numdefs)
            v = None if v is None else not v
            stack.append((v, v is True))
        elif kw == 'if':
            v = eval_cond(rest.strip(), numdefs)
            stack.append((v, v is True))
        elif kw == 'elif':
            if stack:
                st, seen = stack[-1]
                v = eval_cond(rest.strip(), numdefs)
                if seen:
                    stack[-1] = (False, True)
                else:
                    stack[-1] = (v, seen or v is True)
        elif kw == 'else':
            if stack:
                st, seen = stack[-1]
                if seen:
                    stack[-1] = (False, True)
                elif st is False:
                    stack[-1] = (True, True)
                else:
                    stack[-1] = (None, seen)
        elif kw == 'endif':
            if stack:
                stack.pop()
    return '\n'.join(lines)


def typedef_names(txt):
    """Brace-aware typedef harvesting: from each 'typedef' scan to its terminating ';'
    at nesting depth 0 and take every identifier declared there."""
    names = set()
    for m in re.finditer(r'\btypedef\b', txt):
        i = m.end()
        depth = 0
        n = len(txt)
        while i < n:
            c = txt[i]
            if c in '{([':
                depth += 1
            elif c in '})]':
                depth -= 1
            elif c == ';' and depth <= 0:
                break
            i += 1
        decl = txt[m.end():i]
        # drop bracketed bodies / array sizes / parameter lists
        prev = None
        while prev != decl:
            prev = decl
            decl = re.sub(r'\{[^{}]*\}', ' ', decl)
            decl = re.sub(r'\[[^\[\]]*\]', ' ', decl)
        # function-pointer typedef: typedef R (*NAME)(args)
        fp = re.search(r'\(\s*\**\s*([A-Za-z_]\w*)\s*\)\s*\(', decl)
        if fp:
            names.add(fp.group(1))
            continue
        decl = re.sub(r'\([^()]*\)', ' ', decl)
        for part in decl.split(','):
            ids = re.findall(r'[A-Za-z_]\w*', part)
            if ids:
                names.add(ids[-1])
    return names


def collect_types(root):
    """Harvest typedef names from headers + sources so declarations are recognisable."""
    types = set(BUILTIN)
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            if not f.endswith(('.h', '.c', '.cpp')):
                continue
            p = os.path.join(dirpath, f)
            try:
                txt = strip_noise(open(p, 'r', encoding='utf-8', errors='replace').read())
            except OSError:
                continue
            types |= typedef_names(txt)
            # '#define mode_t int' style type aliases (retrodep/sysconfig.h)
            for m in re.finditer(r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+'
                                 r'(?:unsigned\s+|signed\s+)?'
                                 r'(?:int|long|short|char|float|double|void)\s*\**\s*$',
                                 txt, re.M):
                types.add(m.group(1))
            # ENUMDECL { ... } ENUMNAME (foo);  -> 'typedef enum {...} foo;'
            for m in re.finditer(r'ENUMNAME\s*\(\s*([A-Za-z_]\w*)\s*\)', txt):
                types.add(m.group(1))
    types -= KEYWORDS
    types.discard('')
    return types


DECL_RE = None


def build_decl_re(types):
    quals = r'(?:static|const|volatile|register|extern|unsigned|signed|__declspec\([^)]*\))'
    tnames = '|'.join(sorted((re.escape(t) for t in types), key=len, reverse=True))
    return re.compile(
        r'^\s*(?:' + quals + r'\s+)*'
        r'(?:(?:struct|union|enum)\s+[A-Za-z_]\w*'
        r'|(?:' + tnames + r'|[A-Za-z_]\w*_t)'
        r'(?:\s+(?:int|long|short|char|double|unsigned|signed))*)\b'
        r'(?:\s*(?:const|volatile))?'
        r'(?:'
        r'(?:\s*\*+\s*|\s+)'
        r'(?:const\s+|volatile\s+)*'
        r'[A-Za-z_]\w*\s*(?:\[|=|,|;|\))'
        r'|'                                # function pointer: void (*fn)(...), T *(*fn)(...)
        r'\s*\**\s*\(\s*\**\s*[A-Za-z_]\w*\s*\)\s*\('
        r')'
    )


def split_statements(txt):
    """Yield (line_no, depth, text) for each top-of-statement position inside functions."""
    # walk char by char tracking depth; statements delimited by ; { }
    res = []
    depth = 0
    line = 1
    start = 0
    i, n = 0, len(txt)
    paren = 0
    while i < n:
        c = txt[i]
        if c == '\n':
            line += 1
        elif c == '(':
            paren += 1
        elif c == ')':
            paren = max(0, paren - 1)
        elif paren == 0 and c in ';{}':
            if c == '{':
                # An initialiser brace ('= {', ', {') is not a compound statement.
                k = i - 1
                while k >= 0 and txt[k] in ' \t\r\n':
                    k -= 1
                if k >= 0 and txt[k] in '=,':
                    # skip the whole initialiser, counting newlines
                    d2 = 0
                    while i < n:
                        if txt[i] == '\n':
                            line += 1
                        elif txt[i] == '{':
                            d2 += 1
                        elif txt[i] == '}':
                            d2 -= 1
                            if d2 == 0:
                                break
                        i += 1
                    i += 1
                    continue
            frag = txt[start:i]
            res.append((frag, c, depth, line))
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            start = i + 1
        i += 1
    return res


# set by main() to the full harvested typedef list
TYPEWORD = (r'(?:int|long|short|char|unsigned|signed|float|double|bool|_Bool|size_t|void|'
            r'uae_\w+|uaecptr|evt_t|TCHAR|FILE|time_t|xcolnr|[A-Za-z_]\w*_t)')


def set_typeword(types):
    global TYPEWORD
    names = '|'.join(sorted((re.escape(t) for t in types), key=len, reverse=True))
    TYPEWORD = (r'(?:' + names + r'|int|long|short|char|unsigned|signed|float|double|bool|'
                r'_Bool|size_t|void|[A-Za-z_]\w*_t)')


def function_bodies(txt):
    """Yield (start_index, end_index) of each top-level function body."""
    depth = 0
    start = None
    i, n = 0, len(txt)
    while i < n:
        c = txt[i]
        if c == '{':
            if depth == 0:
                start = i
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0 and start is not None:
                yield (start, i)
                start = None
            if depth < 0:
                depth = 0
        i += 1


_HDR_IDENTS = None
_FILE_DEFINES = set()

def header_idents():
    # Identifiers declared in the project headers (globals, macros,
    # struct members). Over-approximated on purpose: this set only ever
    # suppresses reports, so a too-large set costs recall, never precision.
    global _HDR_IDENTS
    if _HDR_IDENTS is None:
        acc = set()
        _r = _TREE_ROOT
        for root in ('include', '.', os.path.join(_r, 'retrodep'),
                     os.path.join(_r, 'deps'),
                     os.path.join(_r, 'libretro'),
                     os.path.join(_r, 'libretro-common', 'include')):
            if not os.path.isdir(root):
                continue
            for dirpath, _dirs, files in os.walk(root):
                for fn in files:
                    if not fn.endswith('.h'):
                        continue
                    try:
                        h = open(os.path.join(dirpath, fn), encoding='utf-8',
                                 errors='replace').read()
                    except OSError:
                        continue
                    # Drop every brace-enclosed body first. Otherwise the
                    # locals of the STATIC_INLINE functions that live in these
                    # headers ('i', 'j', 'v', 'r'...) end up looking like
                    # file-scope names, and then an undeclared local with one
                    # of those very common names can never be reported -- which
                    # is exactly the case hoisting produces.
                    h = re.sub(r'/\*.*?\*/', ' ', h, flags=re.S)
                    h = re.sub(r'//[^\n]*', ' ', h)
                    # 'extern "C" { ... }' envuelve la cabecera ENTERA, asi que
                    # el borrado de cuerpos de abajo se llevaba todos los
                    # identificadores del fichero y cualquier global declarada
                    # ahi salia luego como UNDECLARED-VAR. Se quita la llave
                    # del bloque de enlace, que no es un cuerpo.
                    h = re.sub(r'extern\s*"C"\s*\{', ' ', h)
                    # a function-pointer global has its name inside parens:
                    # 'extern void (*x_phys_put_long)(uaecptr, uae_u32);'
                    acc.update(re.findall(r'\(\s*(?:[A-Za-z_]\w*\s+)?\*+\s*([A-Za-z_]\w*)\s*\)', h))
                    prev = None
                    while prev != h:
                        prev = h
                        h = re.sub(r'\{[^{}]*\}', ' ', h)
                        h = re.sub(r'\([^()]*\)', ' ', h)
                    acc.update(re.findall(r'[A-Za-z_]\w*', h))
        _HDR_IDENTS = acc
    return _HDR_IDENTS


def check_loopvars(txt, decl_re):
    out = []
    bodies = list(function_bodies(txt))
    # Everything outside a function body: file-scope declarations, macros,
    # struct members... Anything named there is assumed in scope.
    outside = []
    prev = 0
    for (bs, be) in bodies:
        outside.append(txt[prev:bs])
        prev = be
    outside.append(txt[prev:])
    out_txt = ''.join(outside)
    # A function-pointer global carries its name inside parens; keep those.
    globs = set(re.findall(r'\(\s*(?:[A-Za-z_]\w*\s+)?\*+\s*([A-Za-z_]\w*)\s*\)', out_txt))
    # Then drop every parenthesised group, so the *parameter names of other
    # functions* stop counting as file-scope names. Without this, an undeclared
    # local called 'haddr' is invisible merely because some other prototype in
    # the same file has a parameter of that name (real case in traps.c).
    prev = None
    while prev != out_txt:
        prev = out_txt
        out_txt = re.sub(r'\([^()]*\)', ' ', out_txt)
    globs |= set(re.findall(r'[A-Za-z_]\w*', out_txt)) | header_idents() | _FILE_DEFINES
    for (s, e) in bodies:
        # parameter list: scan backwards from '{' for the matching '(' ... ')'
        head_end = s
        head_start = max(0, s - 400)
        head = txt[head_start:head_end]
        body = txt[s:e]
        # Over-approximate the set of names in scope: every identifier that
        # appears in the parameter list or in any declaration-looking statement
        # of this function. Over-approximating means we may miss a real problem,
        # but we never cry wolf -- which is what we want from a sanity check.
        scope = set(re.findall(r'[A-Za-z_]\w*', head)) | globs
        for frag, delim, depth, _line in split_statements(body):
            s2 = frag.strip()
            if s2 and decl_re.match(s2 + ';'):
                for ident in re.findall(r'[A-Za-z_]\w*', s2):
                    scope.add(ident)
        # SHADOW=1: declaracion interna que oculta un parametro de la funcion.
        # Hoistarla sin renombrar machacaria el parametro.
        if os.environ.get('SHADOW'):
            pl = head.rfind('(')
            params = set()
            if pl >= 0:
                for pc in head[pl + 1:].split(','):
                    pm = re.search(r'([A-Za-z_]\w*)\s*$', pc.strip().rstrip(')').rstrip(']').split('[')[0].strip())
                    if pm:
                        params.add(pm.group(1))
            for frag, delim, depth, line in split_statements(body):
                s2 = frag.strip()
                if not (s2 and decl_re.match(s2 + ';')):
                    continue
                dm = re.search(r'([A-Za-z_]\w*)\s*(?:=|$)', s2)
                if dm and dm.group(1) in params:
                    out.append((txt.count(chr(10), 0, s) + line, 'SHADOWS-PARAM', dm.group(1)))
        for m in re.finditer(r'\bfor\s*\(\s*([A-Za-z_]\w*)\s*=', body):
            name = m.group(1)
            if name not in scope:
                line = txt.count('\n', 0, s + m.start()) + 1
                out.append((line, 'UNDECLARED-LOOPVAR', name))
        # Plain assignment to a name that is nowhere declared in this
        # function. Catches the classic hoisting slip: turning
        # "int x = f();" into "x = f();" and forgetting the "int x;".
        for frag, delim, depth, line in split_statements(body):
            s2 = frag.strip()
            m2 = re.match(r'^([A-Za-z_]\w*)\s*=[^=]', s2)
            if not m2:
                continue
            nm = m2.group(1)
            if nm in scope or nm in STMT_STARTERS:
                continue
            out.append((txt.count(chr(10), 0, s) + line, 'UNDECLARED-VAR', nm))
    return out



def decl_names(stmt):
    """Names introduced by a simple declaration: 'int a, *b, c[4]' -> a, b, c.
    Returns [] for anything whose shape is not a plain declarator list
    (function prototypes, initialisers with commas inside calls, ...)."""
    body = stmt.strip().rstrip(';')
    if '(' in body and re.search(r'\)\s*$', body):
        return []                       # prototype / function-like
    parts, depth, cur = [], 0, []
    for c in body:
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(''.join(cur)); cur = []
        else:
            cur.append(c)
    parts.append(''.join(cur))
    names = []
    for k, part in enumerate(parts):
        d = part.split('=')[0] if '=' in part else part
        d = re.sub(r'\[[^\]]*\]', ' ', d)
        toks = re.findall(r'[A-Za-z_]\w*', d)
        if not toks:
            return []
        if k == 0 and len(toks) < 2:
            return []                   # no type + name: not a declaration
        names.append(toks[-1])
    return names


def analyse(path, decl_re):
    raw = open(path, 'r', encoding='utf-8', errors='replace').read()
    global _FILE_DEFINES
    # Nombres definidos con #define en este mismo fichero: son macros, no
    # variables, pero aparecen como si lo fueran al asignarlas.
    _FILE_DEFINES = set(re.findall(r'^\s*#\s*define\s+([A-Za-z_]\w*)', raw, re.M))
    txt = blank_preproc(strip_noise(raw))
    rawlines = raw.split('\n')
    findings = []

    # --- for-loop declarations -------------------------------------------------
    for m in re.finditer(r'\bfor\s*\(', txt):
        j = m.end(); d = 1
        while j < len(txt) and d:
            if txt[j] == '(':
                d += 1
            elif txt[j] == ')':
                d -= 1
            elif txt[j] == ';' and d == 1:
                break
            j += 1
        init = txt[m.end():j]
        if init.strip() and decl_re.match(init + ';'):
            findings.append((txt.count('\n', 0, m.start()) + 1, 'for-decl', init.strip()[:70]))

    # --- declaration after statement -----------------------------------------
    stmts = split_statements(txt)
    # state per depth: seen a statement at this depth
    seen_stmt = {}
    # names already declared in the block at each depth, to catch a declaration
    # added on top of one that was already there (the classic hoisting slip:
    # inserting 'int i;' at the block top when the block already declared it).
    declared = {}
    in_typedef_block = 0     # >0 while inside a struct/union/enum definition
    for frag, delim, depth, line in stmts:
        s = frag.strip()
        if in_typedef_block:
            if delim == '{':
                in_typedef_block += 1
            elif delim == '}':
                in_typedef_block -= 1
            continue
        if depth == 0:
            # entering/leaving file scope
            if delim == '{':
                seen_stmt.clear()
                declared.clear()
                # 'struct X {' / 'typedef union {' etc: a type definition, not code
                if re.search(r'\b(?:struct|union|enum)\b[^()]*$', s):
                    in_typedef_block = 1
            continue
        if delim == '{':
            seen_stmt[depth + 1] = False
            declared[depth + 1] = set()
            # a compound statement counts as a statement in the enclosing block
            first = re.match(r'^\s*([A-Za-z_]\w*)', s)
            kw = first.group(1) if first else ''
            if kw not in ('else',) or True:
                seen_stmt[depth] = True
            continue
        if delim == '}':
            seen_stmt.pop(depth + 1, None)
            declared.pop(depth + 1, None)
            seen_stmt[depth] = True
            continue
        if not s:
            # A stray ';' after a complete statement is a null *statement*, so
            # anything declared after it is a declaration-after-statement.
            # cpummu30.c had 'x = f(a);;' followed by five declarations and
            # MSVC rejected the lot.
            if delim == ';':
                seen_stmt[depth] = True
            continue
        # strip labels
        s2 = re.sub(r'^\s*[A-Za-z_]\w*\s*:(?!:)', '', s) if not s.lstrip().startswith('case') else s
        first = re.match(r'^\s*([A-Za-z_]\w*)', s2)
        kw = first.group(1) if first else ''
        is_decl = bool(decl_re.match(s2 + ';')) and kw not in STMT_STARTERS and kw != 'typedef'
        if kw == 'typedef':
            is_decl = True
        if kw == 'extern':
            is_decl = True
        if is_decl:
            if seen_stmt.get(depth):
                findings.append((line, 'decl-after-stmt', s2.strip().replace('\n', ' ')[:70]))
            if kw not in ('typedef', 'extern', 'static'):
                here = declared.setdefault(depth, set())
                for nm in decl_names(s2):
                    if nm in here:
                        findings.append((line, 'DUPLICATE-DECL', nm))
                    else:
                        here.add(nm)
        else:
            seen_stmt[depth] = True

    # --- misc line-based checks ----------------------------------------------
    checks = [
        (r'^\s*\.[A-Za-z_]\w*\s*=[^=]', 'designated-init'),
        (r'^\s*\[\s*[A-Za-z_0-9]+\s*\]\s*=[^=]', 'designated-array-init'),
        (r'\(\s*(?:struct|union)\s+\w+\s*\)\s*\{', 'compound-literal'),
        (r'__attribute__|\btypeof\b|\b__typeof__\b', 'gcc-ext'),
        (r'\brestrict\b', 'restrict'),
        (r'\balloca\s*\(', 'alloca'),
        (r'%z[udx]', 'z-format'),
        (r'\bstdbool\.h\b|\binttypes\.h\b|\bunistd\.h\b|\bstrings\.h\b', 'header'),
        # 'x[] = {}' is not valid C at all; MSVC reports C2059 on the '}'
        (r'=\s*\{\s*\}', 'empty-initializer'),
        # snprintf / strcasecmp / strncasecmp / strdup are handled centrally via
        # <compat/msvc.h>, included from retrodep/sysconfig.h -- not flagged here.
    ]
    tl = txt.split('\n')
    for i, ln in enumerate(tl, 1):
        for pat, tag in checks:
            if re.search(pat, ln):
                findings.append((i, tag, ln.strip()[:70]))

    # --- sanity: 'for (x = ...)' where x has no declaration in that function
    # Catches the classic mistake of turning 'for (int i = 0; ...)' into
    # 'for (i = 0; ...)' without adding the declaration.
    findings += check_loopvars(txt, decl_re)

    # --- sanity: brace balance (catches botched hand edits) ---------------
    depth = 0
    minus = 0
    for ch in txt:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth < minus:
                minus = depth
    if depth != 0 or minus < 0:
        findings.append((0, 'BRACE-IMBALANCE', 'final depth=%d min=%d' % (depth, minus)))

    findings.sort()
    return findings, rawlines


def main():
    root = _TREE_ROOT
    load_config(os.path.join(root, 'retrodep', 'sysconfig.h'))
    load_vcxproj_defines(root)
    load_all_defines([os.path.join(root, 'sources', 'src'),
                      os.path.join(root, 'retrodep'),
                      os.path.join(root, 'libretro'),
                      os.path.join(root, 'deps'),
                      os.path.join(root, 'libretro-common', 'include')])
    # deps/ must be here too: without 7zip's typedefs (UInt32, Byte, CSzData...)
    # every 7zip declaration looks like a statement and the whole file turns
    # into noise.
    types = collect_types(os.path.join(root, 'sources', 'src'))
    types |= collect_types(os.path.join(root, 'libretro-common', 'include'))
    types |= collect_types(os.path.join(root, 'retrodep'))
    types |= collect_types(os.path.join(root, 'deps'))
    # libretro/ faltaba: sus cabeceras declaran tipos propios (cdrom_file en
    # libretro-glue.h, entre otros). Sin ellos, 'cdrom_file *file = NULL;' parece
    # una multiplicacion, o sea una SENTENCIA, y todas las declaraciones que la
    # siguen salian como decl-after-stmt. Falsos positivos en codigo correcto.
    types |= collect_types(os.path.join(root, 'libretro'))
    decl_re = build_decl_re(types)
    set_typeword(types)
    summary = []
    for path in sys.argv[1:]:
        f, _ = analyse(path, decl_re)
        counts = {}
        for _l, tag, _t in f:
            counts[tag] = counts.get(tag, 0) + 1
        summary.append((len(f), os.path.basename(path), counts))
        if os.environ.get('DETAIL'):
            print('===', path)
            for l, tag, t in f:
                print('  %6d  %-20s %s' % (l, tag, t))
    summary.sort(reverse=True)
    print('--- summary ---')
    total = 0
    for n, name, counts in summary:
        total += n
        print('%-24s %5d  %s' % (name, n, ' '.join('%s=%d' % kv for kv in sorted(counts.items()))))
    print('TOTAL', total)


main()
