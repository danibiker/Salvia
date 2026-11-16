import os, sys, json, re
import clang.cindex as ci
ci.Config.set_library_file(os.environ.get('LIBCLANG', '/usr/lib/llvm-18/lib/libclang-18.so.1'))
from clang.cindex import CursorKind as K, TypeKind as T

ROOT = os.path.abspath(os.environ.get('SCUMMVM_ROOT', os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../..')))
LR = ROOT + '/backends/platform/libretro'
DEFS = ['-D__LIBRETRO__', '-DNONSTANDARD_PORT', '-DUSE_RGB_COLOR', '-DUSE_OSD', '-DUSE_TRANSLATION',
        '-DDETECTION_STATIC', '-DHAVE_CONFIG_H', '-DENABLE_VKEYBD', '-DUSE_LIBCO', '-DRELEASE_BUILD',
        '-DUSE_ZLIB', '-DUSE_HIGHRES', '-DUSE_BINK', '-DUSE_INDEO3', '-DUSE_INDEO45', '-DUSE_CDTOONS', '-DUSE_JYV1',
        '-DUSE_TRUEMOTION1', '-DUSE_XAN', '-DUSE_FMTOWNS_PC98_AUDIO', '-DUSE_SID_AUDIO', '-DUSE_VGMTRANS_AUDIO',
        '-DUSE_MIDI', '-DENABLE_SCUMM=STATIC_PLUGIN', '-DENABLE_SCUMM_7_8', '-DENABLE_HE', '-DSIZEOF_SIZE_T=8', '-DSCUMM_64BITS',
        '-DCORE_NAME="ScummVM"', '-DCORE_EXTENSIONS="scummvm"', '-DSCUMMVM_SYSTEM_SUBDIR="scummvm"',
        '-DSCUMMVM_EXTRA_SUBDIR="extra"', '-DSCUMMVM_THEME_SUBDIR="theme"', '-DGIT_HASH="x"']
DEFS += os.environ.get('EXTRA_DEFS', '').split()
INCS = ['-I' + ROOT, '-I' + LR + '/include', '-I' + LR + '/deps/libretro-common/include',
        '-I' + ROOT + '/engines', '-I' + ROOT + '/backends/vkeybd', '-I' + os.environ.get('CLANG_RESOURCE_INCLUDE', '/usr/lib/llvm-18/lib/clang/18/include')]
ARGS = ['-x', 'c++', '-std=c++11', '-Wno-everything'] + DEFS + INCS


def inroot(f):
    return f is not None and f.name.startswith(ROOT + '/') and '/deps/' not in f.name


_src = {}
def src(path):
    s = _src.get(path)
    if s is None:
        s = open(path, 'rb').read().decode('latin-1')
        _src[path] = s
    return s


def text(ext):
    s = src(ext.start.file.name)
    return s[ext.start.offset:ext.end.offset]


def parse(path):
    idx = ci.Index.create()
    return idx.parse(path, args=ARGS)


def apply_edits(edits, dry=False):
    """edits: list of (file, start, end, repl, prio). Applied from end; for equal positions lower prio first in text."""
    byfile = {}
    for e in set(edits):
        byfile.setdefault(e[0], []).append(e)
    nchg = 0
    for f, es in byfile.items():
        s = open(f, 'rb').read().decode('latin-1')
        # check overlaps
        es.sort(key=lambda e: (e[1], e[2], e[4]))
        for a, b in zip(es, es[1:]):
            if b[1] < a[2]:
                print('OVERLAP', f, a, b, file=sys.stderr)
        # apply from end; for equal start, apply higher prio first so lower prio text ends first
        for e in sorted(es, key=lambda e: (e[1], e[2], e[4]), reverse=True):
            s = s[:e[1]] + e[3] + s[e[2]:]
            nchg += 1
        if not dry:
            open(f, 'wb').write(s.encode('latin-1'))
    return nchg
