/*
 * xbox/filebrowser_xbox.c  --  backend de fileBrowser sobre el CRT del XDK.
 *
 * Sustituye a fileBrowser/fileBrowser-libfat.c.  La semantica se copia de
 * alli literalmente, porque el codigo que llama cuenta con ella:
 *
 *   readFile(f, buf, n)   abre, hace seek a f->offset, lee n, AVANZA
 *                         f->offset por lo leido, cierra.  Devuelve los
 *                         bytes leidos o FILE_BROWSER_ERROR.
 *   writeFile(f, buf, n)  igual pero con "wb" (TRUNCA).  Vale porque los
 *                         cuatro sitios que escriben (saveEeprom,
 *                         saveSram, saveMempak, saveFlashram) copian la
 *                         struct del directorio, cuyo offset es 0, y
 *                         escriben el bloque entero de una vez.
 *   seekFile(f, off, t)   solo mueve f->offset.
 *   deleteFile / init / deinit
 *
 * DOS DESVIACIONES DELIBERADAS RESPECTO A libfat, LAS DOS OBLIGADAS:
 *
 * ★ 1. LA BARRA.  `fopen` del XDK NO normaliza '/' a '\'.  Y todo el arbol
 *      construye rutas con '/': `sprintf(name, "%s/%s%s.eep", dir, ...)` en
 *      pif.c, dma.c y flashram.c.  Sin normalizar, ninguna partida se abre
 *      y el fallo parece "no hay fichero", no "ruta mal formada".
 *
 * ★ 2. EL FICHERO DE LA ROM SE QUEDA ABIERTO.  libfat tiene dos juegos de
 *      funciones: las normales, que abren y cierran en cada llamada, y
 *      `fileBrowser_libfatROM_readFile`, que mantiene el descriptor
 *      (fileBrowser-libfat.c, al final).  ROM-Cache lee la ROM en trozos de
 *      32 KB, asi que con la version que abre y cierra serian ~2000
 *      fopen/fclose por carga.  Se copia el esquema de la variante ROM.
 *
 * ★ 3. Y LA PRIMERA LECTURA DE LA ROM TIENE QUE FIJAR `f->size`.
 *      `rom_gc.c:rom_read` hace una lectura de 32 bytes que el propio
 *      comentario del original llama *"dummy read to set up the file/size"*
 *      y acto seguido usa `file->size` para `rom_length`.  Si `size` se
 *      queda a 0, `ROMCache_init` cree que la ROM mide cero y no se carga
 *      nada, sin ningun error.
 *
 * ★ 4. LA ROM PUEDE NO SER UN FICHERO.  Con `need_fullpath = false` el
 *      frontend lee la ROM entera a RAM y la pasa en `retro_game_info.data`;
 *      el core no abre nada.  En vez de cortocircuitar ROM-Cache -- que seria
 *      perder `init_byte_swap`/`byte_swap`, o sea los .v64 y los .n64 -- se
 *      anyade aqui una FUENTE EN MEMORIA: los mismos `romFile_*` sirviendo de
 *      un puntero en vez de un `FILE*`.  Aguas arriba nadie se entera.
 *
 *      ★ EL BUFER DEL FRONTEND NO SOBREVIVE A `retro_load_game`.  Salvia lo
 *      libera nada mas volver (salvia.h:802).  Por eso `romFile_deinit` --
 *      que `ROMCache_load` llama al terminar-- pone el puntero a NULL y
 *      `s_rom_es_memoria` se queda a 1: una lectura posterior devuelve
 *      FILE_BROWSER_ERROR en vez de leer memoria liberada.  Un puntero
 *      colgando que "casi siempre funciona" es peor que un error.
 *
 * C89.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../source/fileBrowser/fileBrowser.h"
#include "../source/gui/DEBUG.h"
#include "../source/xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "filebrowser_xbox.h"

/* Xbox 360: las rutas son de la forma "game:\..." o "hdd:\...".  El limite
 * del struct es FILE_BROWSER_MAX_PATH_LEN (192). */
#define RAIZ_GUARDADO_POR_DEFECTO "game:\\saves"

static char s_raiz_guardado[FILE_BROWSER_MAX_PATH_LEN] = RAIZ_GUARDADO_POR_DEFECTO;

/* El descriptor persistente de la ROM.  Ver la nota 2 de la cabecera. */
static FILE *s_rom_fd = NULL;

/* La ROM en RAM.  Ver la nota 4 de la cabecera.
 *
 * `s_rom_es_memoria` y `s_rom_mem` son DOS cosas distintas a proposito: la
 * primera dice "esta sesion carga de memoria" y dura hasta el siguiente
 * `xbox64_fb_set_rom*`; la segunda dice "y el bufer todavia es valido" y la
 * borra `deinit`.  Con una sola variable no se podria distinguir "no hay ROM
 * en memoria" de "la habia y ya caduco", y el segundo caso tiene que dar
 * error, no caerse al camino de fichero. */
static int                  s_rom_es_memoria = 0;
static const unsigned char *s_rom_mem        = NULL;
static unsigned int         s_rom_mem_tam    = 0;

/* ---------------------------------------------------------------------- */
/* Utilidades                                                             */
/* ---------------------------------------------------------------------- */

/* Copia `src` en `dst` cambiando '/' por '\'.  `dst` tiene que tener sitio
 * para FILE_BROWSER_MAX_PATH_LEN.  Ver la nota 1 de la cabecera. */
static void normaliza(char *dst, const char *src)
{
	int i;

	for (i = 0; i < FILE_BROWSER_MAX_PATH_LEN - 1 && src[i]; i++)
		dst[i] = (src[i] == '/') ? '\\' : src[i];
	dst[i] = 0;
}

/* Tamanyo de un fichero ya abierto, sin tocar la posicion. */
static unsigned int tam_de(FILE *f)
{
	long pos, fin;

	pos = ftell(f);
	if (fseek(f, 0, SEEK_END) != 0)
		return 0;
	fin = ftell(f);
	fseek(f, pos, SEEK_SET);
	return (fin < 0) ? 0 : (unsigned int)fin;
}

/* ---------------------------------------------------------------------- */
/* romFile_*                                                              */
/* ---------------------------------------------------------------------- */

static int xbox_romFile_init(fileBrowser_file *f)
{
	char ruta[FILE_BROWSER_MAX_PATH_LEN];

	if (s_rom_es_memoria)
	{
		if (!s_rom_mem)
			return 0;
		f->size = s_rom_mem_tam;
		return 1;
	}

	if (s_rom_fd)
		return 1;

	normaliza(ruta, f->name);
	s_rom_fd = fopen(ruta, "rb");
	if (!s_rom_fd)
		return 0;

	f->size = tam_de(s_rom_fd);
	return 1;
}

static int xbox_romFile_readFile(fileBrowser_file *f, void *buffer,
                                 unsigned int length)
{
	int leidos;

	if (s_rom_es_memoria)
	{
		unsigned int resto;

		/* El bufer del frontend ya caduco: error, no memoria liberada. */
		if (!s_rom_mem)
			return FILE_BROWSER_ERROR;

		/* Se copia la semantica del camino de fichero: `fread` mas alla del
		 * final devuelve 0, no un error, y `ROMCache_load` cuenta con ese 0
		 * para salir del bucle.  Devolver FILE_BROWSER_ERROR aqui haria que
		 * una ROM con el tamanyo justo se diera por fallida. */
		if (f->offset >= s_rom_mem_tam)
			return 0;

		resto = s_rom_mem_tam - f->offset;
		if (length > resto)
			length = resto;

		memcpy(buffer, s_rom_mem + f->offset, length);
		f->offset += length;
		return (int)length;
	}

	if (!s_rom_fd)
	{
		/* ★ La lectura tonta de rom_gc.c:rom_read llega aqui la primera
		 * vez.  Abrir Y fijar f->size es justo lo que espera. */
		if (!xbox_romFile_init(f))
			return FILE_BROWSER_ERROR;
	}

	if (fseek(s_rom_fd, (long)f->offset, SEEK_SET) != 0)
		return FILE_BROWSER_ERROR;

	leidos = (int)fread(buffer, 1, length, s_rom_fd);
	if (leidos > 0)
		f->offset += leidos;

	return leidos;
}

static int xbox_romFile_readHeader(fileBrowser_file *f, void *buffer,
                                   unsigned int length)
{
	/* libfat no lo implementa y nadie del build lo llama (censado: solo
	 * readFile, seekFile y deinit).  Se deja con la semantica obvia en vez
	 * de a NULL para que, si algun dia lo llama alguien, haga algo
	 * sensato en vez de saltar a cero. */
	int r;

	f->offset = 0;
	r = xbox_romFile_readFile(f, buffer, length);
	return r;
}

static int xbox_romFile_seekFile(fileBrowser_file *f, unsigned int offset,
                                 unsigned int type)
{
	switch (type)
	{
	case FILE_BROWSER_SEEK_SET: f->offset  = offset; break;
	case FILE_BROWSER_SEEK_CUR: f->offset += offset; break;
	case FILE_BROWSER_SEEK_END: f->offset  = f->size - offset; break;
	default: return FILE_BROWSER_ERROR;
	}
	return 0;
}

static int xbox_romFile_deinit(fileBrowser_file *f)
{
	(void)f;
	if (s_rom_fd)
	{
		fclose(s_rom_fd);
		s_rom_fd = NULL;
	}
	/* ★ Se suelta el bufer del frontend PERO NO `s_rom_es_memoria`.  Ver la
	 * nota 4 de la cabecera: a partir de aqui leer es un error, no volver al
	 * fichero. */
	s_rom_mem     = NULL;
	s_rom_mem_tam = 0;
	return 0;
}

static int xbox_romFile_readDir(fileBrowser_file *f, fileBrowser_file **dir,
                                int recursive, int n64only)
{
	/* No hay navegador de ficheros: la ROM la elige el frontend. */
	(void)f; (void)dir; (void)recursive; (void)n64only;
	return FILE_BROWSER_ERROR;
}

/* ---------------------------------------------------------------------- */
/* saveFile_*                                                             */
/* ---------------------------------------------------------------------- */

static int xbox_saveFile_init(fileBrowser_file *f)
{
	(void)f;
	/* La carpeta ya se creo en xbox64_fb_set_save_dir.  En libfat esto
	 * montaba la SD; aqui no hay nada que montar. */
	return 1;
}

static int xbox_saveFile_readFile(fileBrowser_file *f, void *buffer,
                                  unsigned int length)
{
	char  ruta[FILE_BROWSER_MAX_PATH_LEN];
	FILE *fp;
	int   leidos;

	normaliza(ruta, f->name);
	fp = fopen(ruta, "rb");
	if (!fp)
		return FILE_BROWSER_ERROR;

	if (fseek(fp, (long)f->offset, SEEK_SET) != 0)
	{
		fclose(fp);
		return FILE_BROWSER_ERROR;
	}

	leidos = (int)fread(buffer, 1, length, fp);
	if (leidos > 0)
		f->offset += leidos;

	fclose(fp);
	return leidos;
}

static int xbox_saveFile_writeFile(fileBrowser_file *f, void *buffer,
                                   unsigned int length)
{
	char  ruta[FILE_BROWSER_MAX_PATH_LEN];
	FILE *fp;
	int   escritos;

	normaliza(ruta, f->name);
	fp = fopen(ruta, "wb");
	if (!fp)
	{
#if XBOX64_TRAZAS
		DEBUG_print("fb: no se pudo abrir para escribir", DBG_USBGECKO);
#endif
		return FILE_BROWSER_ERROR;
	}

	if (f->offset && fseek(fp, (long)f->offset, SEEK_SET) != 0)
	{
		fclose(fp);
		return FILE_BROWSER_ERROR;
	}

	escritos = (int)fwrite(buffer, 1, length, fp);
	if (escritos > 0)
		f->offset += escritos;

	fclose(fp);
	return escritos;
}

static int xbox_saveFile_deleteFile(fileBrowser_file *f)
{
	char ruta[FILE_BROWSER_MAX_PATH_LEN];

	normaliza(ruta, f->name);
	return (remove(ruta) == -1) ? 0 : 1;
}

static int xbox_saveFile_deinit(fileBrowser_file *f)
{
	(void)f;
	return 0;
}

/* ---------------------------------------------------------------------- */
/* API de esta capa                                                       */
/* ---------------------------------------------------------------------- */

void xbox64_fb_install(void)
{
	romFile_init       = xbox_romFile_init;
	romFile_readDir    = xbox_romFile_readDir;
	romFile_readFile   = xbox_romFile_readFile;
	romFile_readHeader = xbox_romFile_readHeader;
	romFile_seekFile   = xbox_romFile_seekFile;
	romFile_deinit     = xbox_romFile_deinit;

	saveFile_init       = xbox_saveFile_init;
	saveFile_readFile   = xbox_saveFile_readFile;
	saveFile_writeFile  = xbox_saveFile_writeFile;
	saveFile_deleteFile = xbox_saveFile_deleteFile;
	saveFile_deinit     = xbox_saveFile_deinit;
}

void xbox64_fb_set_save_root(const char *ruta)
{
	if (!ruta || !ruta[0])
		return;
	normaliza(s_raiz_guardado, ruta);
}

int xbox64_fb_set_rom(fileBrowser_file *f, const char *ruta)
{
	FILE *fp;
	char  normalizada[FILE_BROWSER_MAX_PATH_LEN];

	/* Si quedaba una ROM abierta de antes, cerrarla.  Sin esto, la segunda
	 * carga de juego leeria del fichero de la primera: el descriptor es
	 * estatico y xbox_romFile_readFile solo abre cuando esta a NULL. */
	if (s_rom_fd)
	{
		fclose(s_rom_fd);
		s_rom_fd = NULL;
	}

	/* Y si la anterior venia de memoria, dejar de creerlo. */
	s_rom_es_memoria = 0;
	s_rom_mem        = NULL;
	s_rom_mem_tam    = 0;

	memset(f, 0, sizeof(fileBrowser_file));
	normaliza(normalizada, ruta);

	fp = fopen(normalizada, "rb");
	if (!fp)
		return 0;

	strcpy(f->name, normalizada);
	f->offset = 0;
	f->size   = tam_de(fp);
	f->attr   = 0;

	/* Se deja abierto a proposito: es el descriptor persistente. */
	s_rom_fd = fp;
	return 1;
}

int xbox64_fb_set_rom_mem(fileBrowser_file *f, const void *datos,
                          unsigned int tam, const char *nombre)
{
	if (!datos || !tam)
		return 0;

	/* Si quedaba un descriptor de una carga anterior por ruta, cerrarlo.
	 * Mismo motivo que en xbox64_fb_set_rom. */
	if (s_rom_fd)
	{
		fclose(s_rom_fd);
		s_rom_fd = NULL;
	}

	memset(f, 0, sizeof(fileBrowser_file));

	/* ★ `name` es SOLO para el log.  Con un .zip cargado a memoria el
	 * frontend no da ruta (salvia.h:770-776 pasa `path = NULL`), asi que
	 * tiene que haber un valor por defecto; y con `s_rom_es_memoria` a 1
	 * nadie va a hacer `fopen` de esto. */
	if (nombre && nombre[0])
		normaliza(f->name, nombre);
	else
		strcpy(f->name, "mem:\\rom");

	f->offset = 0;
	f->size   = tam;
	f->attr   = 0;

	s_rom_es_memoria = 1;
	s_rom_mem        = (const unsigned char *)datos;
	s_rom_mem_tam    = tam;
	return 1;
}

void xbox64_fb_set_save_dir(fileBrowser_file *d)
{
	memset(d, 0, sizeof(fileBrowser_file));
	strcpy(d->name, s_raiz_guardado);
	d->offset = 0;
	d->size   = 0;
	d->attr   = FILE_BROWSER_ATTR_DIR;
}
