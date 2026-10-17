/*
 * xbox/filebrowser_xbox.h  --  el backend de fileBrowser para el XDK.
 *
 * `fileBrowser/` de Wii64 NO se excluye: se conserva la interfaz
 * (fileBrowser.h, 11 punteros a funcion, y fileBrowser.c que los define a
 * NULL) y se sustituye el backend, que en el original era libfat sobre SD y
 * USB.  Es la unica forma de que sigan funcionando los cinco sitios que
 * dependen de el y que no se ven venir:
 *
 *     gc_memory/dma.c        sram
 *     gc_memory/flashram.c   flashram
 *     gc_memory/pif.c        eeprom y mempak
 *     main/rom_gc.c          lectura de la ROM
 *     main/ROM-Cache.c       idem, por bloques
 *
 * ★ Si esto se stubea, las partidas dejan de guardarse Y NO HAY ERROR DE
 * ENLACE: los punteros ya existen, solo que a NULL o a una funcion tonta.
 *
 * ★ TIENE QUE SER VALIDO EN C.
 */

#ifndef _XBOX64_FILEBROWSER_XBOX_H_
#define _XBOX64_FILEBROWSER_XBOX_H_

#include "../source/fileBrowser/fileBrowser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Asigna los 11 punteros de fileBrowser.h.  Idempotente.  Se llama una vez,
 * desde xbox64_init. */
void xbox64_fb_install(void);

/* La carpeta donde van las partidas.  La da el frontend
 * (RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY).  Si nadie la fija se usa
 * "game:\saves". */
void xbox64_fb_set_save_root(const char *ruta);

/* Rellena `f` para la ROM de `ruta`: nombre normalizado, offset 0, size 0
 * (lo pone la primera lectura) y attr 0.  Devuelve 1 si el fichero existe y
 * se puede abrir, 0 si no. */
int  xbox64_fb_set_rom(fileBrowser_file *f, const char *ruta);

/* Lo mismo pero con la ROM YA EN RAM, que es lo que da el frontend con
 * `need_fullpath = false`.  `nombre` puede ser NULL (con un .zip cargado a
 * memoria Salvia no da ruta) y solo sirve para el log.
 *
 * ★ EL BUFER NO SE COPIA y solo es valido mientras dure `retro_load_game`:
 * `ROMCache_load` lo vuelca a `ROMBase` dentro de esa llamada y despues
 * `romFile_deinit` lo suelta.  Ver la nota 4 de filebrowser_xbox.c.
 *
 * Devuelve 1 si `datos` y `tam` son utiles, 0 si no. */
int  xbox64_fb_set_rom_mem(fileBrowser_file *f, const void *datos,
                           unsigned int tam, const char *nombre);

/* Rellena `d` con la carpeta de partidas, creandola si hace falta. */
void xbox64_fb_set_save_dir(fileBrowser_file *d);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_FILEBROWSER_XBOX_H_ */
