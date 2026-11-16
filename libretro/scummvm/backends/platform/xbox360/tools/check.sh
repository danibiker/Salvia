#!/bin/bash
# usage: check.sh <root> <srclist> <out>
ROOT=$1; LIST=$2; OUT=$3
LR=$ROOT/backends/platform/libretro
DEFS="-D__LIBRETRO__ -DNONSTANDARD_PORT -DUSE_RGB_COLOR -DUSE_OSD -DUSE_TRANSLATION -DDETECTION_STATIC -DHAVE_CONFIG_H -DENABLE_VKEYBD -DUSE_LIBCO -DRELEASE_BUILD -DUSE_ZLIB -DUSE_HIGHRES -DUSE_BINK -DUSE_INDEO3 -DUSE_INDEO45 -DUSE_CDTOONS -DUSE_JYV1 -DUSE_TRUEMOTION1 -DUSE_XAN -DUSE_FMTOWNS_PC98_AUDIO -DUSE_SID_AUDIO -DUSE_VGMTRANS_AUDIO -DUSE_MIDI -DENABLE_SCUMM=STATIC_PLUGIN -DENABLE_SCUMM_7_8 -DENABLE_HE -DSIZEOF_SIZE_T=8 -DSCUMM_64BITS -DCORE_NAME=\"ScummVM\" -DCORE_EXTENSIONS=\"scummvm\" -DSCUMMVM_SYSTEM_SUBDIR=\"scummvm\" -DSCUMMVM_EXTRA_SUBDIR=\"extra\" -DSCUMMVM_THEME_SUBDIR=\"theme\" -DGIT_HASH=\"x\" $EXTRA_DEFS"
INCS="-I$ROOT -I$LR/include -I$LR/deps/libretro-common/include -I$ROOT/engines -I$ROOT/backends/vkeybd $EXTRA_INCS"
export ROOT DEFS INCS
grep '\.cpp$' $LIST | xargs -P 2 -I{} sh -c 'f=$(echo {} | tr / _); clang++ -fsyntax-only -std=c++11 -ferror-limit=50 -Wno-everything -Wc++98-compat $FORCE $DEFS $INCS "$ROOT/{}" > /tmp/chk_$f.log 2>&1'; cat /tmp/chk_*.log > $OUT.raw; rm -f /tmp/chk_*.log
grep -E "(warning|error):" $OUT.raw | sed "s|$ROOT/||" | sort -u > $OUT
