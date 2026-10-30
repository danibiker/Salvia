/**
 * xbox64 - mupen64plus_ini.c
 *
 * [Xbox360] ★ [BattleTanx, 2026-10-30] LA COUNTPEROP DE LA REFERENCIA.
 *
 * BattleTanx (U) se quedaba en negro antes de configurar el VI (la CPU en
 * el manejador de excepciones, `Count` siempre casi a cero) y arranca con
 * la velocidad de CPU 3 (comprobado por el usuario), que es la que le pone
 * mupen64plus.ini.  Era el sexto juego que acababa igual (Fighting Force 64,
 * Resident Evil 2, Bottom of the 9th, Rush 2049, Hyper-Bike): en vez de
 * copiar CRC a mano en gamehacks.c, se lee el ini como mupen64plus-core
 * (src/main/rom.c, open_rom):
 *
 *   1. MD5 de la ROM entera en orden z64 (big-endian) -> seccion "[MD5]".
 *   2. Si no esta, la primera seccion con "CRC=<crc1> <crc2>"
 *      (ini_search_by_crc).
 *   3. Si la seccion no trae CountPerOp y tiene RefMD5, la de la
 *      referenciada (romdatabase_resolve_round).
 *
 * Sin fichero, sin entrada o sin valor: 0, y el core se queda con su 2 (el
 * DEFAULT_COUNT_PER_OP de mupen64plus-core tambien es 2).
 *
 * Se lee el fichero entero (~460 KB) una vez por carga de ROM y se suelta.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "winlnxdefs.h"
#include "rom.h"
#include "ROM-Cache.h"
#include "../../xbox/plataforma.h"
#include "mupen64plus_ini.h"
/* ★ XBOX64_TRAZAS sale de aqui y de ningun otro sitio: ni xbox64_pre.h (el
 * include forzado) ni plataforma.h lo traen.  Sin esta linea la macro no
 * existe, `#if XBOX64_TRAZAS` vale 0 y la traza `[xbox64] mupen64plus.ini`
 * desaparecia en silencio (log de BattleTanx del 2026-10-30: llegaba el 3,
 * no la linea). */
#include "../xenon64_diag.h"

/* ★ El md5.c de L. Peter Deutsch (el de este directorio, el mismo que usa
 * mupen64plus-core), metido AQUI y con otros nombres.  No va suelto en el
 * proyecto porque rcheevos (libs/rcheevos/src/rhash/md5.c) y curl traen el
 * mismo codigo con los mismos simbolos `md5_init`/`md5_append`/`md5_finish`
 * y acaban en el mismo ejecutable: seria LNK2005. */
#define md5_init   m64ini_md5_init
#define md5_append m64ini_md5_append
#define md5_finish m64ini_md5_finish
#include "md5.c"

#define M64INI_TROZO 65536u   /* bytes de ROM por lectura para el MD5 */

/* El MD5 de la ROM cargada, en hexadecimal en mayusculas (como en el ini). */
static int m64ini_md5_rom(char hex[33])
{
	static const char dig[] = "0123456789ABCDEF";
	md5_state_t st;
	md5_byte_t d[16];
	unsigned char *buf;
	unsigned int off, n;
	int i;

	if (rom_length <= 0)
		return 0;
	buf = (unsigned char *)malloc(M64INI_TROZO);
	if (!buf)
		return 0;
	md5_init(&st);
	for (off = 0; off < (unsigned int)rom_length; off += n)
	{
		n = (unsigned int)rom_length - off;
		if (n > M64INI_TROZO)
			n = M64INI_TROZO;
		ROMCache_read((u8 *)buf, (u32)off, (u32)n);
#ifndef _BIG_ENDIAN
		/* [Windows] La cache guarda la ROM en palabras de 32 bits del
		 * ANFITRION (init_byte_swap, rom_gc.c), sea cual sea el formato del
		 * fichero.  El MD5 del ini es el de la imagen z64: cada palabra, de
		 * vuelta a big-endian. */
		{
			unsigned int k;
			for (k = 0; k + 3 < n; k += 4)
			{
				const unsigned char t0 = buf[k], t1 = buf[k + 1];
				buf[k]     = buf[k + 3];
				buf[k + 1] = buf[k + 2];
				buf[k + 2] = t1;
				buf[k + 3] = t0;
			}
		}
#endif
		md5_append(&st, (const md5_byte_t *)buf, (int)n);
	}
	free(buf);
	md5_finish(&st, d);
	for (i = 0; i < 16; i++)
	{
		hex[i * 2]     = dig[d[i] >> 4];
		hex[i * 2 + 1] = dig[d[i] & 15];
	}
	hex[32] = 0;
	return 1;
}

/* El texto que sigue a la cabecera "[md5]" (al principio de una linea), o
 * NULL si no esta. */
static const char *m64ini_seccion(const char *ini, const char *md5)
{
	char cab[40];
	const char *p = ini;
	size_t n;

	_snprintf(cab, sizeof(cab), "[%s]", md5);
	cab[sizeof(cab) - 1] = 0;
	n = strlen(cab);
	while ((p = strstr(p, cab)) != NULL)
	{
		if (p == ini || p[-1] == '\n')
			return p + n;
		p += n;
	}
	return NULL;
}

/* El valor de "clave=" dentro de la seccion que empieza en `sec` (hasta la
 * siguiente cabecera "["), o NULL. */
static const char *m64ini_valor(const char *sec, const char *clave)
{
	const size_t n = strlen(clave);
	const char *p = sec;

	while (p && *p)
	{
		while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')
			p++;
		if (*p == '[' || *p == 0)
			return NULL;
		if (strncmp(p, clave, n) == 0 && p[n] == '=')
			return p + n + 1;
		p = strchr(p, '\n');
	}
	return NULL;
}

/* La primera seccion con "CRC=<crc1> <crc2>" (ini_search_by_crc). */
static const char *m64ini_seccion_crc(const char *ini, unsigned int crc1, unsigned int crc2)
{
	char lin[32];
	const char *p = ini;

	_snprintf(lin, sizeof(lin), "CRC=%08X %08X", crc1, crc2);
	lin[sizeof(lin) - 1] = 0;
	while ((p = strstr(p, lin)) != NULL)
	{
		if (p != ini && p[-1] == '\n')
		{
			/* Hacia atras, hasta la cabecera de su seccion. */
			const char *s = p;
			while (s > ini)
			{
				s--;
				if (*s == '[' && (s == ini || s[-1] == '\n'))
				{
					const char *f = strchr(s, ']');
					return f ? f + 1 : NULL;
				}
			}
			return NULL;
		}
		p += strlen(lin);
	}
	return NULL;
}

/* La CountPerOp de la seccion, o la de su RefMD5 si ella no la trae. */
static int m64ini_cpo_seccion(const char *ini, const char *sec)
{
	int saltos;

	for (saltos = 0; sec && saltos < 4; saltos++)
	{
		const char *v = m64ini_valor(sec, "CountPerOp");
		const char *r;
		char ref[33];
		int i;

		if (v)
			return atoi(v);
		r = m64ini_valor(sec, "RefMD5");
		if (!r)
			return 0;
		for (i = 0; i < 32 && r[i] && r[i] != '\r' && r[i] != '\n'; i++)
			ref[i] = (char)toupper((unsigned char)r[i]);
		ref[i] = 0;
		sec = m64ini_seccion(ini, ref);
	}
	return 0;
}

/* Lee el ini entero en memoria (terminado en 0), o NULL. */
static char *m64ini_lee(char *ruta, size_t tamRuta)
{
	static const char *const nombres[2] = {
		"mupen64plus.ini",
		"Mupen64plus\\mupen64plus.ini"   /* mupen64plus-libretro-nx */
	};
	char dir[300];
	FILE *f = NULL;
	char *ini;
	long tam;
	size_t leidos;
	int i;

	xbox64_rice_config_dir(dir);   /* el directorio system, con '\' final */
	if (!dir[0])
		return NULL;
	for (i = 0; i < 2 && !f; i++)
	{
		_snprintf(ruta, tamRuta, "%s%s", dir, nombres[i]);
		ruta[tamRuta - 1] = 0;
		f = fopen(ruta, "rb");
	}
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	tam = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (tam <= 0)
	{
		fclose(f);
		return NULL;
	}
	ini = (char *)malloc((size_t)tam + 1);
	if (!ini)
	{
		fclose(f);
		return NULL;
	}
	leidos = fread(ini, 1, (size_t)tam, f);
	fclose(f);
	ini[leidos] = 0;
	return ini;
}

int mupen64plus_ini_count_per_op(unsigned int crc1, unsigned int crc2)
{
	char ruta[340];
	char md5[33];
	const char *sec = NULL;
	const char *via = "";
	char *ini;
	int cpo = 0;
#if XBOX64_TRAZAS
	char l[400];
#endif

	ruta[0] = 0;
	md5[0] = 0;
	ini = m64ini_lee(ruta, sizeof(ruta));
	if (!ini)
	{
#if XBOX64_TRAZAS
		_snprintf(l, sizeof(l), "[xbox64] mupen64plus.ini: NO ESTA en el directorio system "
		          "(ultimo intento: %s); velocidad de CPU por defecto", ruta[0] ? ruta : "sin directorio");
		l[sizeof(l) - 1] = 0;
		xbox64_log_line(l);
#endif
		return 0;
	}

	if (m64ini_md5_rom(md5))
	{
		sec = m64ini_seccion(ini, md5);
		via = "MD5";
	}
	if (!sec)
	{
		sec = m64ini_seccion_crc(ini, crc1, crc2);
		via = "CRC";
	}
	if (sec)
		cpo = m64ini_cpo_seccion(ini, sec);

#if XBOX64_TRAZAS
	if (sec)
	{
		const char *g = m64ini_valor(sec, "GoodName");
		int n = 0;
		while (g && g[n] && g[n] != '\r' && g[n] != '\n' && n < 120)
			n++;
		_snprintf(l, sizeof(l), "[xbox64] mupen64plus.ini: \"%.*s\" (por %s, md5 %s) CountPerOp=%d%s",
		          n, g ? g : "?", via, md5, cpo, cpo ? "" : " (no la trae: por defecto)");
	}
	else
		_snprintf(l, sizeof(l), "[xbox64] mupen64plus.ini: la ROM no esta (md5 %s, crc %08X %08X); "
		          "velocidad de CPU por defecto", md5, crc1, crc2);
	l[sizeof(l) - 1] = 0;
	xbox64_log_line(l);
#endif

	free(ini);
	return cpo;
}
