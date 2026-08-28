"""Preprocess cpustbl.c for the A500 configuration and check that every op_*
symbol still referenced is defined by a cpuemu_*.c that the project compiles.
Also checks #if/#endif balance.  ANALYSIS ONLY."""
import io
import os
import re

ROOT = r'C:\develop\proyectos\personal\libretro-uae'
SRC = os.path.join(ROOT, 'sources', 'src')
p = os.path.join(SRC, 'cpustbl.c')
raw = io.open(p, encoding='utf-8', errors='replace').read()
lines = raw.split('\n')

# --- 1. directive balance --------------------------------------------------
depth, minimum = 0, 0
for i, l in enumerate(lines, 1):
    t = l.strip()
    if re.match(r'#\s*if', t):
        depth += 1
    elif re.match(r'#\s*endif', t):
        depth -= 1
        minimum = min(minimum, depth)
print('balance #if/#endif: final=%d  minimo=%d  %s'
      % (depth, minimum, 'OK' if depth == 0 and minimum == 0 else '*** MAL ***'))

# --- 2. which regions survive ---------------------------------------------
ON = set(['CPUEMU_0', 'CPUEMU_11', 'CPUEMU_13'])
OFF_PREFIX = 'CPUEMU_'
stack = []
live_lines = []
for l in lines:
    t = l.strip()
    m = re.match(r'#\s*if(n?)def\s+(\w+)', t)
    if m:
        name = m.group(2)
        if name.startswith(OFF_PREFIX) and name != 'CPUEMU_68000_ONLY':
            v = (name in ON) != bool(m.group(1))
        elif name == 'CPUEMU_68000_ONLY':
            v = bool(m.group(1))          # not defined -> #ifndef is true
        else:
            v = True                      # unknown: assume compiled
        stack.append(v)
        continue
    if re.match(r'#\s*else\b', t):
        if stack:
            stack[-1] = not stack[-1]
        continue
    if re.match(r'#\s*endif', t):
        if stack:
            stack.pop()
        continue
    if all(stack):
        live_lines.append(l)
live = '\n'.join(live_lines)

used = set(re.findall(r'\b(op_[0-9a-fA-F]+_\d+(?:_[a-z]+)?)\b', live))
suffix = {}
for u in used:
    m = re.match(r'op_[0-9a-fA-F]+_(\d+)', u)
    suffix.setdefault(int(m.group(1)), 0)
    suffix[int(m.group(1))] += 1
print('sufijos de tabla todavia referenciados:',
      ' '.join('%d(%d)' % kv for kv in sorted(suffix.items())))

have = set()
for f in ('cpuemu_0.c', 'cpuemu_11.c', 'cpuemu_13.c'):
    t = io.open(os.path.join(SRC, f), encoding='utf-8', errors='replace').read()
    have |= set(re.findall(r'\b(op_[0-9a-fA-F]+_\d+(?:_[a-z]+)?)\s*\(', t))

missing = sorted(used - have)
print('referenciados: %d   definidos por los cpuemu compilados: %d   FALTAN: %d'
      % (len(used), len(have), len(missing)))
for m in missing[:15]:
    print('   FALTA', m)
