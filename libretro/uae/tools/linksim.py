"""Static approximation of the link step.

For every function PROTOTYPE visible to the build (project headers), decide
whether some compiled .c actually DEFINES it and whether any compiled .c CALLS
it.  Declared + called + never defined == LNK2001.

Definitions are found by balancing the parameter list and requiring a '{'
afterwards, so multi-line signatures and function-pointer parameters are
handled.  Only the text this configuration keeps is considered.
ANALYSIS ONLY - never modifies sources.
"""
import io
import os
import re

ROOT = r'C:\develop\proyectos\personal\libretro-uae'
_src = io.open(os.path.join(ROOT, 'tools', 'c89check.py'), encoding='utf-8').read()
C = {'__name__': 'c89'}
exec(compile(_src.replace('\nmain()', '\npass'), 'c89check.py', 'exec'), C)
C['load_config'](os.path.join(ROOT, 'retrodep', 'sysconfig.h'))
C['load_all_defines']([os.path.join(ROOT, 'sources', 'src'),
                       os.path.join(ROOT, 'retrodep')])

sp = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'files2.txt')
cfiles = [l for l in io.open(sp, encoding='utf-8').read().split('\n') if l.strip()]

hfiles = []
for base in (os.path.join(ROOT, 'sources', 'src'), os.path.join(ROOT, 'retrodep')):
    for dp, _d, fs in os.walk(base):
        if any(x in dp.lower() for x in ('\\jit', '\\ppc', '\\pcem', '\\qemuvga',
                                         'archivers\\chd')):
            continue
        hfiles.extend(os.path.join(dp, f) for f in fs if f.endswith('.h'))

CRT = set('''memcpy memset memmove memcmp strcpy strncpy strcat strlen strcmp strncmp
strchr strrchr strstr strtok strdup sprintf snprintf printf fprintf vfprintf sscanf
fopen fclose fread fwrite fseek ftell fflush fgets fputs putc getc feof remove rename
malloc calloc realloc free abort exit atexit qsort bsearch abs labs atoi atol strtol
strtoul strtod time clock localtime gmtime mktime difftime setjmp longjmp signal raise
sin cos tan asin acos atan atan2 sinh cosh tanh exp log log10 pow sqrt ceil floor fabs
fmod frexp ldexp modf isalpha isdigit isspace isupper islower toupper tolower
va_start va_end va_arg va_copy offsetof assert
CloseHandle WaitForSingleObject CreateThread GetLastError Sleep
func fn callback cb handler'''.split())


def live(path):
    raw = io.open(path, encoding='utf-8', errors='replace').read()
    C['_FILE_DEFINES'] = set(re.findall(r'^\s*#\s*define\s+([A-Za-z_]\w*)', raw, re.M))
    return C['blank_preproc'](C['strip_noise'](raw))


NAME_AT = re.compile(r'([A-Za-z_]\w*)\s*\(')


def definitions(txt):
    """Names defined as functions in txt -> (extern_defs, static_defs)."""
    ext, sta = set(), set()
    for m in NAME_AT.finditer(txt):
        i = m.end() - 1                      # at '('
        depth, j, n = 0, i, len(txt)
        while j < n:
            if txt[j] == '(':
                depth += 1
            elif txt[j] == ')':
                depth -= 1
                if depth == 0:
                    break
            elif txt[j] == ';':
                j = -1
                break
            j += 1
        if j < 0 or j >= n:
            continue
        k = j + 1
        while k < n and txt[k] in ' \t\r\n':
            k += 1
        if k >= n or txt[k] != '{':
            continue
        # what precedes the name on its logical line decides extern vs static
        ls = txt.rfind('\n', 0, m.start()) + 1
        head = txt[ls:m.start()].strip()
        prev = txt.rfind('\n', 0, ls - 1) + 1
        head2 = (txt[prev:ls] + head).strip()
        if not head and not head2:
            continue                          # 'foo() {' with no return type
        if re.match(r'^(if|while|for|switch|else|do|return)\b', head):
            continue
        (sta if re.search(r'\bstatic\b', head + ' ' + head2) else ext).add(m.group(1))
    return ext, sta


INLINE = re.compile(r'(?:STATIC_INLINE|static\s+__?inline\w*)\s+[\w \t*]+?([A-Za-z_]\w*)\s*\(')
PROTO = re.compile(r'\b([A-Za-z_]\w*)\s*\([^;()]*\)\s*;')

fnptr = set()
# 'extern void (*x_do_cycles)(int);' is a VARIABLE, not a function: calling it
# resolves through the pointer, so it must not be counted as a missing symbol.
FNPTR = re.compile(r'\(\s*(?:[A-Za-z_]\w*\s+)?\*+\s*([A-Za-z_]\w*)\s*\)\s*\(')
defined, static_only, inline_def, called = set(), set(), set(), set()
live_c = {}
for f in cfiles:
    t = live(f)
    live_c[f] = t
    e, s = definitions(t)
    defined |= e
    static_only |= s
    inline_def |= set(INLINE.findall(t))
    fnptr |= set(FNPTR.findall(t))
    called |= set(NAME_AT.findall(t))

hdr_protos, macros = set(), set()
for h in hfiles:
    t = live(h)
    hdr_protos |= set(PROTO.findall(t))
    fnptr |= set(FNPTR.findall(t))
    e, s = definitions(t)
    inline_def |= e | s | set(INLINE.findall(t))
for p in hfiles + cfiles:
    raw = io.open(p, encoding='utf-8', errors='replace').read()
    macros |= set(re.findall(r'^\s*#\s*define\s+([A-Za-z_]\w*)', raw, re.M))

KW = set('if while for switch return sizeof do else defined case goto'.split())
missing = sorted(n for n in (called & hdr_protos)
                 if n not in defined and n not in static_only
                 and n not in inline_def and n not in macros
                 and n not in KW and n not in CRT and n not in fnptr)

print('llamadas distintas: %d' % len(called))
print('con prototipo en cabeceras del proyecto: %d' % len(called & hdr_protos))
print('declaradas + llamadas + SIN definicion: %d' % len(missing))
for n in missing:
    where = [os.path.basename(f) for f in cfiles
             if re.search(r'\b%s\s*\(' % re.escape(n), live_c[f])]
    print('   %-32s usado en: %s' % (n, ', '.join(where[:4])))
