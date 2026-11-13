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

- Vídeo: RGB565, como pide Salvia por defecto. Renderer por software o, en
  GBA, el renderer Direct3D 9 (Windows y Xbox 360, ver abajo).
- Sin zlib/png/lzma/sqlite/depurador/scripting (la ROM llega descomprimida
  del frontend).
- Ficheros: `VFile` por stdio (`vfs-file.c`), o la VFS de libretro si el
  frontend la ofrece. La BIOS se busca en el directorio de sistema
  (`gba_bios.bin`, `gb_bios.bin`, `gbc_bios.bin`, `sgb_bios.bin`); si no está,
  se usa la BIOS HLE.

## Renderer Direct3D 9 (GBA, Windows y Xbox 360)

Es el renderer OpenGL de mGBA (`src/gba/renderers/gl.c`) portado a Direct3D 9:

- `src/gba/renderers/d3d9.c`: la misma estructura que `gl.c`, función a
  función, en C89 con la interfaz C de `d3d9.h`.
- `src/gba/renderers/d3d9-shaders.h`: los shaders GLSL reescritos en HLSL
  (`vs_3_0`/`ps_3_0`). Se compilan la primera vez con `D3DXCompileShader` y
  se guardan compilados en el directorio de sistema del frontend
  (`mgba_d3d9_XXXXXXXX.bin`, uno por shader y plataforma); las siguientes
  veces se cargan de ahí. Si cambia el código de un shader cambia su nombre,
  así que no hace falta borrar nada.
- `include/mgba/internal/gba/renderers/d3d9.h`: la API.

Como en el GL, cada capa (4 BGs, sprites, ventanas) se pinta en su propia
textura a la resolución interna elegida y un último shader las compone. Los
fondos afines y los sprites rotados/escalados se muestrean a esa resolución,
así que los juegos con modo 7, mapas que giran o jefes escalados se ven
suaves. Para un juego 2D normal la imagen es la misma que a 1x, pero más
grande.

Opciones del núcleo (categoría vídeo):

| Opción | Valores |
|---|---|
| `mgba_gba_renderer` "GBA Renderer" | `software` (por defecto) / `direct3d` |
| `mgba_gba_render_scale` "GBA Direct3D Internal Resolution" | `1`..`6` (240x160 .. 1440x960), por defecto 3 |

Las dos se pueden cambiar en mitad de la partida. Con Direct3D no se aplican
"Color Correction", "Interframe Blending" ni los efectos LCD.

Integración con Salvia (`libretro.c`, sólo para GBA):

1. Pide el device con el environment 30002. Si no lo hay, si la tarjeta no
   tiene shader model 3 con 2 render targets o si falla un shader, se queda
   en software y lo dice por el log.
2. Le pasa a Salvia la textura de salida con el 30003 (A8R8G8B8,
   `240*escala x 160*escala`) cada vez que cambia, y en cada frame
   llama a `video_refresh(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)`.
3. Al descargar el juego o volver a software manda el 30003 con `NULL` antes
   de liberar la textura.
4. `max_width/max_height` de `retro_get_system_av_info` pasan a 1440x960 en
   GBA, y al cambiar de escala se manda `SET_GEOMETRY`.

El renderer guarda el render target 0, el depth/stencil y todo el estado del
device al empezar cada frame y lo restaura al acabar, así que Salvia
encuentra el device como lo dejó.

### Windows

Las capas son texturas `D3DUSAGE_RENDERTARGET` en las que se pinta
directamente, como en el GL. El estado del frontend se guarda con un state
block `D3DSBT_ALL`. Usa sólo `D3DPOOL_DEFAULT` y texturas dinámicas que se
suben enteras con `D3DLOCK_DISCARD`. Con un device D3D9Ex no hay problema con
un `Reset`; con uno normal, un `Reset` del device fallaría mientras el
renderer esté vivo. El proyecto añade `$(DXSDK_DIR)Include` en Win32; Salvia
ya enlaza `d3d9.lib` y `d3dx9.lib`.

### Xbox 360

La GPU sólo pinta en la EDRAM, y de ahí sólo se sale con `Resolve()` a una
textura (tileada). `Resolve` es caro (copia y para la GPU), así que las capas
**se quedan en la EDRAM todo el frame** y se resuelven una sola vez:

- Hay 7 superficies de color (color y flags de sprites, ventanas, BG0..3) y
  una de profundidad, todas a la vez en la EDRAM.
- Si a la escala elegida no caben enteras, el frame se divide en **bandas**
  horizontales (múltiplos de 8 scanlines). Al pasar a la siguiente banda
  (`_edramEnterBand`) se resuelven las 7 capas de la anterior a sus texturas
  (`_edramLeaveBand`) y se borran; las tandas de scanlines que cruzan una
  frontera se parten en dos.
- Cada banda tiene sus propias texturas de capa, así que `Resolve` siempre
  copia un rectángulo de (0, 0) a (0, 0) (con 2 bandas, resolver la segunda
  banda a la fila 240 de una textura entera daba basura en la consola).
- Al final del frame, con todas las capas ya en sus texturas, se compone la
  salida banda a banda en una superficie del tamaño de la salida (encima de
  las capas, que ya no hacen falta) y se resuelve entera a la textura que ve
  Salvia.

A 720p (frontend con 720 tiles) cabe todo detrás del back buffer: escala 1 y
2 en una banda, 3 en dos, 4 en cuatro, 5 en cinco, 6 en siete. Son
`bandas * 7 + 1` resolves por frame (8 a 1x/2x, 15 a 3x), frente a los más
de mil que hacía la primera versión (restaurar y resolver cada capa en cada
tanda). Si no cabe detrás del back buffer se empieza en el tile 0, porque
nada nuestro tiene que sobrevivir en la EDRAM entre dos `retro_run`. El log
dice lo que ha elegido: `D3D9: 720x480, 2 band(s), EDRAM 720..1800
(frontend 720)`.

La paleta y los datos por línea (scroll, matrices afines, registros) se suben
sólo para las filas de cada tanda (y la paleta sólo si ha cambiado), en vez
de enteros: en F-Zero o con degradados hay una tanda por scanline.

Los fondos de texto (modo 0, y BG0/BG1 en modo 1) se ven exactamente igual a
1x que a la resolución interna, así que se pintan a 1x (en sus propias
superficies y texturas de 240 de ancho) y la composición los lee con las
mismas coordenadas: el muestreo por punto da el mismo texel. Cuestan
`escala²` veces menos. Sólo los fondos afines/bitmap (los que se ven mejor
a más resolución), los sprites, las ventanas y la composición se pintan a la
escala elegida. Qué fondos van a 1x se decide antes de cada frame según el
anterior: si un fondo se pinta afín en un frame en el que iba a 1x, ese
frame sale a 1x (correcto, sólo menos suave) y el siguiente ya no.

Otras cosas que se han medido en la consola (F-Zero, el peor caso: una
tanda por cada cambio de registro, modo 7):

- Los anillos de texturas se vigilan con `GetCurrentFence`, que a diferencia
  de `InsertFence` no envía el command buffer a la GPU (cada envío cuesta
  CPU).
- `_begin` da 112 de los 128 GPR a los pixel shaders
  (`SetShaderGPRAllocation(0, 16, 112)`, como los ejemplos de postproceso del
  XDK: más hilos en vuelo mientras esperan a las texturas) y `_end` deja la
  asignación de antes, que tiene que ser la de por defecto en el `Present`.
- Los shaders se compilan primero con `D3DXSHADER_OPTIMIZATION_LEVEL3` y, si
  el compilador no puede, con las opciones por defecto.
- Las texturas que escribe la CPU se quedan lineales (`LIN_`): tileadas y
  llenadas con `XGTileSurface` costaban 16 ms de CPU por frame, y la GPU está
  parada la mitad del frame a 3x.
- Mandar el command buffer a la GPU después de cada tanda (con una fence: el
  XDK no tiene `KickOff` para el device) no ayudaba.
- Lo que sí frenaba era Salvia: `XBOX_DrawOverlay` volvía a bloquear con
  `LockRect` la textura del overlay justo después de dibujarla, y `LockRect`
  espera a que la GPU termine con ella, o sea, con todo el frame. Ahora en el
  juego ese bloqueo se hace cuando el core entrega el frame
  (`SDL_XBOX_SetOverlayDeferredLock` / `SDL_XBOX_SyncOverlay`).

Con todo eso F-Zero va a 60 fps a 3x, con la GPU parada la mitad del frame.

Al cambiar de escala (o de renderer, o al descargar el juego) el núcleo le
manda antes a Salvia la textura `NULL`, quita sus texturas de los samplers
del device y espera a la GPU antes de liberarlas: en la Xbox el device no
guarda referencias a lo que tiene puesto, y la textura de salida que Salvia
deja en el sampler 0 quedaba colgando (de ahí los cuelgues al cambiar la
resolución en caliente).

Lo demás, como en pcsxr-360 y xbox64:

- Texturas de CPU (VRAM, paleta, datos por línea) en formatos `LIN_`, sin
  `DYNAMIC`, en anillos (8 de VRAM, 32 de paleta, 64 de datos por línea) con
  fences: en la Xbox no hay `D3DLOCK_DISCARD`, y `LockRect` de una textura que
  la GPU aún usa espera a que termine, así que se escribe en la siguiente del
  anillo cuya fence ya ha pasado.
- La VRAM de mGBA está en little-endian: se le da la vuelta a cada palabra de
  16 bits al subirla (`D3DFMT_LIN_A8L8` espera el orden nativo).
- Sin state blocks: se guardan y restauran a mano los render states, samplers,
  texturas, shaders, declaración de vértices, viewport, scissor y constantes
  que toca el renderer. `D3DRS_VIEWPORTENABLE` a `TRUE` mientras pinta (el
  frontend lo deja a `FALSE`) y Hi-Z apagado.
- Sin FVF (no existe en la Xbox): una declaración de vértices, en las dos
  plataformas.
- Medio píxel: la convención de D3D9 (centros en enteros) también en la Xbox,
  sin tocar `D3DRS_HALFPIXELOFFSET`, igual que pcsxr-360.
- Los shaders tardan en compilarse en la consola: la caché en disco del
  directorio de sistema evita hacerlo más de una vez.
- El compilador de microcódigo del XDK se cae (`X4800: Microcode Compiler
  internal error`) con el shader de ventanas. Hay cuatro formas de escribirlo
  (`WINDOW_*` en `d3d9-shaders.h`), cada una con varias opciones de
  compilación, y se usa la primera que compila (el log dice cuál:
  `D3D9: psWindow WINDOW_B, flags ...`). `WINDOW_C` y `WINDOW_D` no suavizan
  las ventanas entre scanlines ni detectan círculos: son las ventanas exactas
  de la GBA, sólo que menos suaves a escalas altas. Si no compila ninguna, el
  renderer sigue sin WIN0/WIN1 (todo cuenta como fuera de las ventanas) en vez
  de volver a software, y el log dice por qué. Los intentos fallidos también
  se recuerdan en la caché (un fichero vacío), así que no se repiten en cada
  arranque.

El proyecto define `BUILD_D3D9` también en las configuraciones de Xbox 360
(no hace falta nada más: `xtl.h`, `d3dx9` y `xgraphics` vienen del XDK, y
Salvia ya los enlaza).

Mientras se escribía, el renderer se comprobó ejecutando `d3d9.c` contra una
emulación de D3D9 sobre OpenGL (Mesa) que compilaba el HLSL real con glslang,
comparando cada frame con el renderer GL original de mGBA, también por el
camino de Xbox 360 (EDRAM llena de basura entre frames y después de cada
`Resolve`, rectángulos alineados a 8, estado del frontend intacto al acabar,
texturas liberadas sin seguir puestas en un sampler). Con las ROMs de prueba,
incluida una con efectos por scanline (modo 7, ventana circular, degradado,
cambios de VRAM/OAM/paleta a mitad de frame, sprites afines/OBJWIN/mosaico),
la salida coincide con la del GL salvo diferencias de ±1 en el último bit de
color y unos pocos píxeles sueltos en bordes de sprites rotados, por redondeo
de coma flotante; a 3x o más, en algún frame cambia algún píxel de la línea
que separa dos bandas. Después se probó en Windows y en la Xbox 360.

Ojo: el renderer GL de mGBA, y por tanto este, no es exacto al ciclo como el
de software. Agrupa scanlines y sube VRAM y paleta por bloques, así que los
efectos raros a mitad de frame pueden salir distintos. Por eso la opción
viene en software por defecto.

## Cómo se ha comprobado

Sin VS2010 disponible, la conversión se ha validado así:

1. `clang -std=gnu89 -pedantic` sobre todos los ficheros del proyecto, con las
   cabeceras de Linux y con las de Windows (mingw-w64): sin declaraciones
   mezcladas, inicializadores designados ni literales compuestos.
2. Un frontend libretro mínimo (no incluido) que ejecuta 1200 frames con
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
  Con Direct3D, F-Zero va a 60 fps a 3x.
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
