"""Re-verifica DE VERDAD todos los ficheros hoisteados contra su copia
.prehoist. Necesario porque verify.py devolvia 0 siempre y el driver creyo que
estaba verificando cuando no lo hacia."""
import io
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')

pairs = []
for dp, dirs, files in os.walk(ROOT):
    for fn in files:
        if fn.endswith('.prehoist'):
            bak = os.path.join(dp, fn)
            cur = bak[:-len('.prehoist')]
            if os.path.isfile(cur):
                pairs.append((os.path.relpath(cur, ROOT).replace('\\', '/'),
                              cur, bak))

print('ficheros a re-verificar: %d' % len(pairs))
print()

ok, bad = [], []
for rel, cur, bak in sorted(pairs):
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'verify.py'),
                        bak, cur], capture_output=True, text=True)
    if r.returncode == 0:
        ok.append(rel)
    else:
        bad.append((rel, r.stdout.strip().replace('\n', ' | ')[:180]))

print('=== IDENTICAS, orden de efectos preservado (%d) ===' % len(ok))
print()
if bad:
    print('=== DIFIEREN, hay que mirarlas (%d) ===' % len(bad))
    for rel, msg in bad:
        print('  %-40s %s' % (rel, msg))
else:
    print('ninguna difiere.')
