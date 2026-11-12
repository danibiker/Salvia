# mGBA para Salvia (Windows / Xbox 360)

Núcleo libretro de mGBA (rama `libretro/mgba`, commit `7a12d6d4b`, 0.11-dev)
compilado como **librería estática** con Visual Studio 2010, para enlazarlo en
Salvia igual que el resto de núcleos. Incluye GBA y GB/GBC.

```
src/platform/xbox360/
├── mgba/mgba.sln, mgba.vcxproj(.filters)   proyecto VS2010 (Win32 + Xbox 360)
├── msvc/                                    lo que le falta a VS2010/XDK de C99
│   ├── stdbool.h, inttypes.h               cabeceras que no existen en VS2010
│   ├── msvc-compat.h                        inline, static_assert, snprintf, strtof, fminf...
│   └── msvc-compat.c                        snprintf/vsnprintf con semántica C99
├── version.c                                sustituye al version.c que genera CMake
├── tools/                                   herramientas usadas para la conversión
└── README_SALVIA.md
```

Salida: `src/platform/xbox360/mgba/<Release|Debug>/<Win32|Xbox 360>/mgba.lib`.

## Por qué hay tantos ficheros tocados

mGBA está escrito en C99/C11 y se compila con CMake. VS2010 y el compilador
del XDK sólo entienden **C89**, así que los fuentes se han convertido (y nada
más: la lógica del emulador no se ha tocado):

| C99 | C89 |
|---|---|
| declaraciones mezcladas con código | subidas al principio del bloque (`tools/c89ify.py`, ~1100 casos) o un bloque `{ }` cuando no se podía |
| inicializadores designados `{ .x = 1 }`, `[REG] = "..."` | posicionales, con el designador original en un comentario (`tools/desig.py`) |
| literales compuestos `(struct X) { ... }` | `memset` + asignaciones, o arrays con nombre |
| `for (int i = 0; ...)` | variable declarada antes |
| `bool` (`_Bool`) | `unsigned char`, igual que hace `libretro.h` con MSVC < 2013 |

**Ojo con `bool`:** con `unsigned char` una conversión implícita no se
normaliza a 0/1 (`bool b = flags & 0x100;` daría `0`). Todas las conversiones
implícitas entero/puntero → `bool` de los ficheros del proyecto se han
reescrito como `!!(expr)` (`tools/boolcasts.py`, ~100 casos). Si se añade
código nuevo hay que seguir haciéndolo. Esto arreglaba, por ejemplo, los
turbos X/Y/L2/R2 de `libretro.c`, que se habrían perdido en Windows.

Además:

- `include/mgba-util/common.h`: detecta MSVC < 2013 (`MGBA_MSVC_C89`) e
  incluye `msvc-compat.h`; en Xbox 360 incluye `xtl.h`, define `__BIG_ENDIAN__`
  y usa `__loadwordbytereverse`/`__storewordbytereverse` (y compañía) para los
  accesos little-endian.
- `config.c` / `vfs.c`: en builds libretro de Windows no se usan las APIs del
  Shell (`SHGetKnownFolderPath`, `PathIsRelativeW`...), así Salvia no tiene que
  enlazar shell32/shlwapi/ole32. En Xbox 360 se evitan las APIs anchas y el
  separador de rutas es `\`.
- `serialize.c`: `timespec_get` no existe en VS2010 → `GetSystemTimeAsFileTime`.
- `windows/memory.c`: `xtl.h` en Xbox 360 (VirtualAlloc existe en las dos).
- `crc32` se renombra a `mgba_crc32` desde el proyecto, porque Salvia enlaza
  zlib, que exporta el mismo símbolo.

## Configuración del núcleo

Defines (los mismos que usa el objetivo `mgba_libretro` de CMake):
`__LIBRETRO__ M_CORE_GBA M_CORE_GB MINIMAL_CORE=2 COLOR_16_BIT COLOR_5_6_5
DISABLE_THREADING MGBA_STANDALONE ENABLE_VFS ENABLE_VFS_FILE HAVE_STRDUP`.

- Vídeo: RGB565, como pide Salvia por defecto. Renderer por software.
- Sin zlib/png/lzma/sqlite/depurador/scripting (la ROM llega descomprimida
  del frontend).
- Ficheros: `VFile` por stdio (`vfs-file.c`), o la VFS de libretro si el
  frontend la ofrece. La BIOS se busca en el directorio de sistema
  (`gba_bios.bin`, `gb_bios.bin`, `gbc_bios.bin`, `sgb_bios.bin`); si no está,
  se usa la BIOS HLE.

## Cómo se ha comprobado

Sin VS2010 disponible, la conversión se ha validado así:

1. `clang -std=gnu89 -pedantic` sobre todos los ficheros del proyecto, con las
   cabeceras de Linux y con las de Windows (mingw-w64): sin declaraciones
   mezcladas, inicializadores designados ni literales compuestos.
2. Un frontend libretro mínimo (`tools/harness.c`) que ejecuta 1200 frames con
   entrada simulada, hace un savestate de ida y vuelta y saca un hash de cada
   frame de vídeo y de todo el audio. Se ha comparado el núcleo original (C99,
   CMake) con el convertido en tres variantes: gcc en Linux, gcc con `bool` =
   `unsigned char`, y **binario Win32 (mingw, msvcrt, `-std=gnu89`, con
   `msvc-compat`) ejecutado con Wine**. Los hashes coinciden al bit en las 16
   ROMs de prueba: `arm`, `thumb`, `memory`, `bios`, `ppu/*`, `nes`, `save/*`
   de jsmolka/gba-tests, una ROM propia con sonido DirectSound + PSG y todos los
   modos de vídeo, y `cpu_instrs`, `dmg_sound`, `cgb_sound`, `instr_timing` y
   `oam_bug` de Blargg para GB.

Lo que no se ha podido probar es el compilador de VS2010 en sí: si da algún
error, lo más probable es que sea de cabeceras (algo que mingw tiene y el SDK
de 2010 no), no de la conversión.

## Pendiente para Xbox 360

- **Orden de los bitfields.** `arm.h` (PSR) y `sm83.h` (flags) invierten el
  orden de los campos cuando hay `__BIG_ENDIAN__`, como hace GCC en PowerPC.
  Hay que confirmar que el compilador del XDK asigna los bitfields del bit
  más significativo al menos (es lo que indica `#pragma bitfield_order`). Una
  comprobación rápida: `union PSR p; p.packed = 0x80000000;` → `p.n` debe ser 1.
- Rendimiento: intérprete ARM y renderer por software en un PPC in-order.
- `localtime_s`, `_vscprintf`, `_rotl`, `GetSystemTimeAsFileTime` deberían
  estar en el CRT del XDK; si alguno falta, es un cambio de una línea.

## Actualizar mGBA

Las herramientas de `tools/` necesitan Python 3 y `pip install libclang`.
Tras un `git subtree pull`, lo más sencillo es partir de los ficheros de
upstream y repetir:

```
python c89ify.py --root <mgba> -- <args de clang> -- <ficheros .c>
python desig.py <fichero> <variable> [línea] -- <args de clang>
FIX=1 python boolcasts.py <ficheros .c> -- <args de clang>
```

Los cambios que no son mecánicos (macros de `isa-arm.c`, `isa-sm83.c`, los
decodificadores, `software-*.c`, `table.c`, `image.c`, `common.h`...) se ven
con `git diff c80a1a60bd -- libretro/mgba`, que es el commit en el que se
añadió el subtree sin tocar.
