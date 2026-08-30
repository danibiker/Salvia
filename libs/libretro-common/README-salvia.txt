Copia vendorizada, parcial, de libretro-common.

Origen: libretro/prboom/libretro/libretro-common (misma revision que compila el
core de prboom, que ya construye rmp3.c como C con VS2010/XDK y por tanto tiene
el toolchain probado).

Se COPIA en vez de referenciar el arbol de prboom a proposito: apuntar el
frontend al libretro-common vendorizado de un core lo dejaria expuesto a que un
`git subtree merge` de ese core lo mueva o lo actualice por debajo.

Contenido actual (solo lo que usa el reproductor de musica de menu):
  include/retro_inline.h
  include/formats/rmp3.h
  formats/mp3/rmp3.c

Para anadir ogg o wav basta con traer de la misma ruta de origen:
  include/formats/rvorbis.h + formats/vorbis/rvorbis.c   (necesita ademas
                                                          encodings/crc32.h)
  include/formats/rwav.h    + formats/wav/rwav.c         (necesita ademas
                                                          retro_endianness.h)
y anadir el .c al Salvia.vcxproj y una rama al despachador de
src/audio/musicplayer.h.
