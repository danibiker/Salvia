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

CONST_INIT = re.compile(r'^[\s{}\d,\'"\.\-+*/()xXa-fA-F_]*$')

TYPE_RE = re.compile(
    r'^(?:const\s+|static\s+|volatile\s+|unsigned\s+|signed\s+)*'
    r'(?:struct\s+\w+|union\s+\w+|enum\s+\w+|\w+)\s*\**\s*[\w\s,*\[\]]+$')

# Primer declarador de una declaracion, sin inicializador: 'uae_s32 upper',
# 'struct flag_struct oldflags', 'uae_u32 bdata[2]'. Sirve para reconocer
# 'TIPO a,b,c = expr;', donde lo unico ejecutable es el inicializador de 'c'.
DECL_HEAD_RE = re.compile(
    r'^(?:(?:const|static|volatile|unsigned|signed|register)\s+)*'
    r'(?:(?:struct|union|enum)\s+\w+|\w+)'
    r'(?:\s*\*+\s*|\s+)'
    r'\**\s*\w+\s*(?:\[[^\[\]]*\]\s*)*$')
NOT_A_TYPE = frozenset(('return', 'else', 'case', 'goto', 'do', 'break',
                        'continue', 'sizeof', 'typedef'))


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
        # gencpu pega las llaves de apertura al codigo ("{	int movem_cnt;").
        # Sin quitarlas, una declaracion pura con '{' delante no se reconoce
        # como tal y se cuenta como sentencia: daba una falsa diferencia.
        t = t.lstrip('{').strip()
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
            # 'uae_s32 upper,lower,reg = expr;': el primer declarador no lleva
            # inicializador, asi que ni norm_assign ni TYPE_RE (que se atraganta
            # con el '=' de mas adelante) la reconocian, y la linea entera
            # contaba como sentencia. Al hoistearla queda 'reg = expr;', que ya
            # no coincidia: falsa diferencia en los diez cpuemu_*.c. Es una
            # declaracion; lo unico ejecutable son sus inicializadores.
            head = parts[0].strip()
            is_decl = (DECL_HEAD_RE.match(head) is not None
                       and head.split()[0] not in NOT_A_TYPE)
            res, ok = [], True
            for k, ch in enumerate(parts):
                a = norm_assign(ch)
                if a is None:
                    if is_decl:
                        continue     # declarador sin inicializador: no ejecuta
                    if k == 0 and TYPE_RE.match(b):
                        ok = False   # pure declaration: ignore
                    break
                res.append(a)
            if is_decl:
                out.extend(res)      # solo los inicializadores, en orden
                continue
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
rc = 0
if a == b:
    print('IDENTICAS -> orden de efectos laterales preservado')
else:
    rc = 1
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            print('primera diferencia en %d' % i)
            print('  antes : %s' % x[:100])
            print('  ahora : %s' % y[:100])
            break
    else:
        print('difieren solo en longitud')

sys.exit(rc)
