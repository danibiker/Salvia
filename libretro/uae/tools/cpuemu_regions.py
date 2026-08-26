"""Every '#if...CPUEMU_nn' region in the compiled files: which half this
configuration drops, what that half introduces, and whether any of it is
referenced from outside.  Handles #else, which is the whole point: upstream's
pattern is '#ifndef CPUEMU_n / stub / #else / real / #endif', so a name defined
in BOTH halves is fine and must not be reported.  ANALYSIS ONLY."""
import io
import os
import re

ROOT = r'C:\develop\proyectos\personal\libretro-uae'
sp = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'files2.txt')
cfiles = [l for l in io.open(sp, encoding='utf-8').read().split('\n') if l.strip()]
cfiles += [os.path.join(ROOT, r'sources\src\include\cpu_prefetch.h')]

# which CPUEMU_* survive sysconfig.h's own #if guards
_src = io.open(os.path.join(ROOT, 'tools', 'c89check.py'), encoding='utf-8').read()
C = {'__name__': 'c89'}
exec(compile(_src.replace('\nmain()', '\npass'), 'c89check.py', 'exec'), C)
C['load_config'](os.path.join(ROOT, 'retrodep', 'sysconfig.h'))
# load_config does a flat scan and ignores sysconfig.h's own #if guards,
# so spell out the A500 configuration: only these three are compiled.
ON = set(['CPUEMU_0', 'CPUEMU_11', 'CPUEMU_13'])
print('CPUEMU activos:', ' '.join(sorted(ON)))
print()

INTRO = re.compile(
    r'^(?:static\s+|extern\s+)?[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*[\(\[]', re.M)

bad = 0
for f in cfiles:
    base = os.path.basename(f)
    if base.startswith('cpuemu_') or base == 'cpustbl.c':
        continue
    try:
        lines = io.open(f, encoding='utf-8', errors='replace').read().split('\n')
    except OSError:
        continue
    for i, l in enumerate(lines):
        m = re.match(r'#\s*if(n?)def\s+(CPUEMU_\w+)\s*(?:/.*)?$', l.strip())
        if not m:
            continue
        neg, macro = bool(m.group(1)), m.group(2)
        first_half_live = (macro in ON) != neg
        depth, end, els = 1, i, None
        for j in range(i + 1, len(lines)):
            s = lines[j].strip()
            if re.match(r'#\s*if', s):
                depth += 1
            elif re.match(r'#\s*endif', s):
                depth -= 1
                if depth == 0:
                    end = j
                    break
            elif depth == 1 and re.match(r'#\s*else\b', s):
                els = j
        if els is None:
            dropped, kept = ([], lines[i + 1:end]) if first_half_live \
                else (lines[i + 1:end], [])
        elif first_half_live:
            dropped, kept = lines[els + 1:end], lines[i + 1:els]
        else:
            dropped, kept = lines[i + 1:els], lines[els + 1:end]
        if not dropped:
            continue
        intro = set(INTRO.findall('\n'.join(dropped)))
        # a name the surviving half also provides is exactly upstream's stub
        # pattern working as intended
        intro -= set(INTRO.findall('\n'.join(kept)))
        outside = '\n'.join(lines[:i] + lines[end + 1:] + kept)
        used = sorted(n for n in intro if re.search(r'\b%s\b' % re.escape(n), outside))
        if used:
            bad += len(used)
            print('%-18s %-11s %5d-%-5d  se cae y se usa fuera: %s'
                  % (base, macro, i + 1, end + 1, ', '.join(used)))
print()
print('referencias vivas a codigo CPUEMU descartado: %d' % bad)
