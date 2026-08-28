"""Filtra la salida DETAIL del checker a los ficheros que estan de verdad en el
vcxproj, y solo a los dos tipos que hoist2.py sabe tratar."""
import io
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

proj = io.open(os.path.join(ROOT, 'libretro-uae.vcxproj'),
               encoding='utf-8-sig', errors='replace').read()
inproj = set()
for v in re.findall(r'<ClCompile\s+Include="([^"]+)"', proj):
    inproj.add(v.replace('\\', '/').lower())

out = []
keep = False
nf = ns = 0
skipped_files = set()

for line in io.open(os.path.join(ROOT, '_detail_all.txt'),
                    encoding='utf-8', errors='replace'):
    m = re.match(r'^===\s+(.*?)\s*$', line)
    if m:
        rel = m.group(1).strip()
        keep = rel.replace('\\', '/').lower() in inproj
        if keep:
            out.append(line)
            nf += 1
        else:
            skipped_files.add(rel)
        continue
    if keep and re.match(r'^\s+\d+\s+(decl-after-stmt|for-decl)\s', line):
        out.append(line)
        ns += 1

io.open(os.path.join(ROOT, '_detail_proj.txt'), 'w',
        encoding='utf-8', newline='\n').writelines(out)

print('ficheros del proyecto con hallazgos: %d' % nf)
print('sitios hoisteables en ellos        : %d' % ns)
print('ficheros con hallazgos FUERA del proyecto (se ignoran): %d'
      % len(skipped_files))
for s in sorted(skipped_files)[:12]:
    print('    ' + s)
