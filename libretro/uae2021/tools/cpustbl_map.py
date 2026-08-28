"""Map cpustbl.c: every 'op_smalltbl_N' table, the CPUEMU guard it sits under
(if any), and which cpuemu_*.c would have to be compiled to satisfy it.
ANALYSIS ONLY."""
import io
import re

p = r'C:\develop\proyectos\personal\libretro-uae\sources\src\cpustbl.c'
lines = io.open(p, encoding='utf-8', errors='replace').read().split('\n')

stack = []
cur = []           # table currently being defined
out = []
for i, l in enumerate(lines, 1):
    t = l.strip()
    m = re.match(r'#\s*if(n?)def\s+(\w+)', t)
    if m:
        stack.append(m.group(2) if not m.group(1) else '!' + m.group(2))
        continue
    if re.match(r'#\s*if\b', t):
        stack.append('?')
        continue
    if re.match(r'#\s*endif', t):
        if stack:
            stack.pop()
        continue
    if re.match(r'#\s*else\b', t):
        if stack:
            stack[-1] = '!' + stack[-1]
        continue
    m = re.match(r'(?:const\s+)?struct cputbl\s+(op_smalltbl_\d+)\s*\[\]', t)
    if m:
        out.append((m.group(1), i, ' '.join(g for g in stack if g.startswith('CPUEMU'))))

# which suffix numbers each table's entries use
txt = '\n'.join(lines)
print('%-20s %-8s %s' % ('tabla', 'linea', 'guarda CPUEMU'))
for name, ln, guard in out:
    print('%-20s %-8d %s' % (name, ln, guard or '*** NINGUNA ***'))
