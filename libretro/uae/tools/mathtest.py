"""Re-implement the five uae_msvc_* functions exactly as written in
retrodep/machdep/support.c and compare them against C99 semantics.
Python floats are IEEE doubles, so this is a faithful model."""
import math
import random
import struct


def c_round(x):
    a = math.fabs(x)
    t = float(math.floor(a))
    if a - t >= 0.5:
        t += 1.0
    return -t if x < 0.0 else t


def c_remainder(x, y):
    if y == 0.0 or math.isnan(x) or math.isnan(y) or math.isinf(x):
        return float('nan')
    if math.isinf(y):
        return x
    r = math.fmod(x, y)
    ay = math.fabs(y)
    if math.fabs(r) > 0.5 * ay:
        r -= ay if r > 0.0 else -ay
    elif math.fabs(r) == 0.5 * ay:
        q = (x - r) / y
        if math.fmod(q, 2.0) != 0.0:
            r -= ay if r > 0.0 else -ay
    return r


def c_log1p(x):
    u = 1.0 + x
    if u == 1.0:
        return x
    return math.log(u) * (x / (u - 1.0))


def c_expm1(x):
    u = math.exp(x)
    if u == 1.0:
        return x
    if u - 1.0 == -1.0:
        return -1.0
    return (u - 1.0) * x / math.log(u)


def c_atanh(x):
    a = math.fabs(x)
    r = 0.5 * c_log1p(2.0 * a / (1.0 - a))
    return -r if x < 0.0 else r


def ref_round(x):
    # C99 round(): ties away from zero. Uses Fraction so it is exact even
    # past 2**52, where floor(x+0.5) is itself wrong.
    from fractions import Fraction
    f = Fraction(x)
    n = int(f)
    rem = f - n
    if rem >= Fraction(1, 2):
        n += 1
    elif rem <= Fraction(-1, 2):
        n -= 1
    return float(n)


bad = 0


def chk(name, got, want, x, y=None, tol=0.0):
    global bad
    if math.isnan(want) and math.isnan(got):
        return
    ok = (got == want) if tol == 0.0 else \
        (math.fabs(got - want) <= tol * max(1.0, math.fabs(want)))
    if not ok:
        bad += 1
        if bad < 12:
            print('  %s(%r%s) -> %r  esperado %r' %
                  (name, x, '' if y is None else ', %r' % y, got, want))


# --- round ----------------------------------------------------------------
cases = [0.5, -0.5, 1.5, -1.5, 2.5, 0.49999999999999994, -0.49999999999999994,
         0.0, -0.0, 1e300, -1e300, 2.675, 0.1, -0.1, 4503599627370497.0]
for x in cases + [random.uniform(-1e6, 1e6) for _ in range(20000)]:
    chk('round', c_round(x), ref_round(x), x)

# --- remainder ------------------------------------------------------------
pairs = [(5.0, 3.0), (-5.0, 3.0), (5.0, -3.0), (-5.0, -3.0),
         (3.0, 2.0), (1.0, 2.0), (-1.0, 2.0), (7.5, 5.0), (2.5, 5.0),
         (1e10, 3.0), (0.0, 1.0), (-0.0, 1.0)]
for _ in range(20000):
    pairs.append((random.uniform(-1e6, 1e6), random.uniform(-1e3, 1e3)))
for x, y in pairs:
    chk('remainder', c_remainder(x, y), math.remainder(x, y), x, y)

# --- log1p / expm1 / atanh -------------------------------------------------
for x in [0.0, 1e-20, -1e-20, 1e-8, -1e-8, 0.5, -0.5, 1.0, 10.0, -0.9999]:
    chk('log1p', c_log1p(x), math.log1p(x), x, tol=1e-14)
    chk('expm1', c_expm1(x), math.expm1(x), x, tol=1e-14)
for _ in range(20000):
    x = random.uniform(-0.9999, 50.0)
    chk('log1p', c_log1p(x), math.log1p(x), x, tol=1e-14)
    chk('expm1', c_expm1(min(x, 700.0)), math.expm1(min(x, 700.0)), x, tol=1e-13)

for x in [0.0, 1e-20, -1e-20, 0.5, -0.5, 0.9, -0.9, 0.999999]:
    chk('atanh', c_atanh(x), math.atanh(x), x, tol=1e-14)
for _ in range(20000):
    x = random.uniform(-0.999999, 0.999999)
    chk('atanh', c_atanh(x), math.atanh(x), x, tol=1e-14)

print('discrepancias: %d' % bad)
