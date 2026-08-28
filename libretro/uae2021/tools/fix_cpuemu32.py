"""cpuemu_32.c es generado por gencpu y los 32 hallazgos son UN solo patron
repetido 16 veces: dentro de un bloque, gencpu emite primero la sentencia
mmu030_state[...] |= ... y detras las dos declaraciones. Se intercambian, que
es lo minimo y no mueve ningun efecto secundario (la sentencia no depende de
las declaraciones, y las declaraciones no tienen inicializador).

Verificado antes de tocar: el patron aparece exactamente 16 veces, y los otros
12 'int movem_cnt;' del fichero van detras de declaraciones (legales)."""
import io
import re

P = r'C:\develop\proyectos\personal\libretro-uae-2.6.1\sources\src\cpuemu_32.c'
s = io.open(P, encoding='utf-8', errors='replace').read()

pat = re.compile(
    r'(\{\t)(mmu030_state\[1\] \|= MMU030_STATEFLAG1_MOVEM1;\n)'
    r'(\tint movem_cnt;\n\tuae_u32 val;\n)')

n = len(pat.findall(s))
assert n == 16, 'esperaba 16 coincidencias, hay %d' % n

# el bloque abre con "{\t": la primera declaracion hereda ese prefijo y la
# sentencia pasa a una linea normal con tabulador.
s2 = pat.sub(lambda m: '{\tint movem_cnt;\n\tuae_u32 val;\n\t' + m.group(2), s)

assert len(pat.findall(s2)) == 0
assert s2.count('int movem_cnt;') == s.count('int movem_cnt;')
assert s2.count('uae_u32 val;') == s.count('uae_u32 val;')
assert s2.count('MMU030_STATEFLAG1_MOVEM1;') == s.count('MMU030_STATEFLAG1_MOVEM1;')

io.open(P, 'w', encoding='utf-8', newline='').write(s2)
print('cpuemu_32.c: %d bloques reordenados (declaraciones antes de la sentencia)' % n)
