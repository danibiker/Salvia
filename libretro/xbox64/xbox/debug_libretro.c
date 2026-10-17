/*
 * xbox/debug_libretro.c  --  sustituye a gui/DEBUG.c.
 *
 * POR QUE SE CONSERVA LA CABECERA Y SOLO SE CAMBIA EL CUERPO.  `gui/DEBUG.h`
 * es la traza de TODO el arbol de Wii64: 93 llamadas a DEBUG_print y 54 a
 * print_gecko en el codigo que se compila.  Cambiar esa interfaz seria tocar
 * ~150 sitios; reponer las seis funciones son 100 lineas.
 *
 * La lista ENTERA de lo que aportaba gui/DEBUG.c y hay que reponer:
 *
 *     char txtbuffer[1024]        el buffer que todo el arbol rellena con
 *                                 sprintf ANTES de llamar a DEBUG_print
 *     char printToSD              lo DEFINIA DEBUG.c:16, no lo declaraba
 *     DEBUG_print(char*, int)     93 llamadas
 *     print_gecko(const char*,...) 54
 *     DEBUG_stats(int, char*, unsigned, unsigned)  6
 *     DEBUG_registerHeap(heap_cntrl*, const char*) 6:
 *                                   r4300/Recomp-Cache-Heap.c:352,360,370
 *                                   gc_memory/TLB-Cache.c:76 (fuera del build)
 *                                   glN64_GX/Textures.cpp:693
 *                                   glN64_GX/VI.cpp:464
 *     DEBUG_update(void)          1
 *     DEBUG_get_text(void)        lo usaba el OSD del menu
 *
 * ★ EL SEGUNDO ARGUMENTO DE DEBUG_print ES UNA FILA DE PANTALLA, NO UN
 * NIVEL.  En Wii64 es el indice de linea del OSD (DBG_DLIST=1,
 * DBG_TXINFO=4...), con dos valores especiales por encima de 0xFC que
 * significan "manda esto por el USB Gecko" en vez de pintarlo.  Aqui no hay
 * OSD, asi que TODO va al log del frontend y el indice se emite como
 * etiqueta: perder ese numero seria perder de que subsistema viene la
 * linea, que es la mitad de la informacion.
 *
 * C89.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../source/gui/DEBUG.h"
#include "../source/xenon64_diag.h"
#include "plataforma.h"

/* ★ Con XBOX64_TRAZAS a 0 (version publica) las cuatro salidas quedan VACIAS:
 * ni el sprintf de dentro se ejecuta.  Es la red de seguridad; los sitios que
 * las llaman estan gateados ellos mismos para no pagar su propio sprintf. */

/* gui/DEBUG.c:16 */
char printToSD;

/* Lo declara DEBUG.h:52 y lo rellenan con sprintf por todo el arbol. */
char txtbuffer[1024];

/* DEBUG.h:57-58.  El OSD de Wii64 guardaba las ultimas 25 lineas; aqui no
 * hay OSD, pero DEBUG_get_text tiene que devolver algo valido. */
static char  s_lineas[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH];
static char *s_indice[DEBUG_TEXT_HEIGHT];
static int   s_indice_hecho = 0;

static void asegura_indice(void)
{
	int i;

	if (s_indice_hecho)
		return;
	for (i = 0; i < DEBUG_TEXT_HEIGHT; i++)
	{
		s_lineas[i][0] = 0;
		s_indice[i] = s_lineas[i];
	}
	s_indice_hecho = 1;
}

void DEBUG_print(char *string, int pos)
{
#if XBOX64_TRAZAS
	char linea[DEBUG_TEXT_WIDTH + 32];

	if (!string)
		return;

	asegura_indice();

	if (pos >= 0 && pos < DEBUG_TEXT_HEIGHT)
	{
		strncpy(s_lineas[pos], string, DEBUG_TEXT_WIDTH - 1);
		s_lineas[pos][DEBUG_TEXT_WIDTH - 1] = 0;
	}

	/* La etiqueta es el `pos` original: ver la nota de la cabecera. */
	sprintf(linea, "[wii64:%3d] %.*s", pos, DEBUG_TEXT_WIDTH - 1, string);
	xbox64_log_line(linea);
#else
	(void)string; (void)pos;
#endif
}

void print_gecko(const char *fmt, ...)
{
#if XBOX64_TRAZAS
	char    linea[512];
	va_list ap;

	if (!fmt)
		return;

	va_start(ap, fmt);
	vsprintf(linea, fmt, ap);
	va_end(ap);

	xbox64_log_line(linea);
#else
	(void)fmt;
#endif
}

void DEBUG_stats(int stats_id, char *info, unsigned int stats_type,
                 unsigned int adjustment_value)
{
#if XBOX64_TRAZAS
	/* El original acumulaba o promediaba en una tabla que pintaba el OSD.
	 * Los 6 sitios que la llaman estan comentados (pure_interp.c:127,142,
	 * 326,354,359,371) salvo los del perfilador.  Se deja la firma y se
	 * emite la linea: sin la tabla no hay nada que promediar, pero si el
	 * dato crudo, que es lo unico que aqui se puede mirar. */
	char linea[256];

	(void)stats_type;
	sprintf(linea, "[wii64:stat] %d %s %u", stats_id,
	        info ? info : "", adjustment_value);
	xbox64_log_line(linea);
#else
	(void)stats_id; (void)info; (void)stats_type; (void)adjustment_value;
#endif
}

void DEBUG_update(void)
{
	/* Envejecia las cadenas del OSD.  Sin OSD no hay nada que envejecer. */
}

char **DEBUG_get_text(void)
{
	asegura_indice();
	return s_indice;
}

void DEBUG_registerHeap(heap_cntrl *theHeap, const char *heapName)
{
#if XBOX64_TRAZAS
	char linea[128];

	sprintf(linea, "[wii64:heap] %s registrado (%p)",
	        heapName ? heapName : "?", (void *)theHeap);
	xbox64_log_line(linea);
#else
	(void)theHeap; (void)heapName;
#endif
}
