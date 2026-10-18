/*
 * xbox/trucos_n64.c  --  los trucos GameShark de N64 (retro_cheat_set).
 *
 * El port de Wii64 no tenia motor de trucos y retro_cheat_set estaba vacio:
 * el menu de trucos de Salvia cargaba los .cht pero no pasaba nada.  Esto es
 * el motor de mupen64plus (mupen64plus-core/src/main/cheat.c, el de la
 * referencia mupen64plus-libretro-nx), con su misma semantica:
 *
 *   80/81, A0/A1   escritura de 8/16 bits (cacheada / sin cachear)
 *   F0/F1          escritura de 8/16 bits, una sola vez, en el VI de arranque
 *   D0/D1          la linea siguiente solo si el byte/media palabra ES igual
 *   D2/D3          la linea siguiente solo si NO es igual
 *   88/89, A8/A9   solo con el boton GameShark pulsado (aqui no hay: nunca)
 *   D8-DB          condiciones que tambien piden el boton (nunca se cumplen)
 *   EE             "desactivar el expansion pak", como mupen
 *
 * y el mismo ritmo (main.c gs_apply_cheats): los F0/F1 en el primer VI de la
 * ROM; el resto, en cada VI a partir del 60.
 *
 * Diferencias con mupen, y por que:
 *   - La memoria se lee y se escribe por la tabla `rwmem` del nucleo (como los
 *     parches de gamehacks.c), con la direccion en KSEG0: el byte alto del
 *     codigo es el TIPO, la direccion son los 24 bits bajos, igual que mupen
 *     (`address & 0xFFFFFF` sobre la RDRAM).
 *   - Solo se escribe, y se invalida el codigo recompilado de esa direccion,
 *     si el valor CAMBIA.  mupen escribe e invalida siempre; el resultado en
 *     memoria es el mismo y asi un truco de datos no libera funciones del
 *     dynarec en cada VI.
 *   - Tablas fijas en vez de listas con malloc, y una seccion critica: Salvia
 *     llama a retro_cheat_set desde el menu (hilo del frontend) y esto se
 *     aplica en el hilo del emulador.
 *
 * C89.
 */

#include <stdio.h>
#include <string.h>

#include "trucos_n64.h"

#include "../source/main/winlnxdefs.h"
#include "../source/gc_memory/memory.h"
#include "../source/r4300/r4300.h"
#include "../source/xenon64_diag.h"       /* XBOX64_TRAZAS */

/* Wrappers.h arrastra todo el recompilador; aqui solo hace falta esto.  Con
 * el interprete puro (interpcore) no hay nada que invalidar. */
void invalidate_func(unsigned int addr);

#if XBOX64_TRAZAS
void xbox64_log_line(const char *linea);
#endif

#define TRUCOS_MAX     128
#define CODIGOS_MAX    2048
#define TOKENS_MAX     512   /* tokens hex de un solo truco */
#define VIEJO_NINGUNO  0xDEAD0000u   /* CHEAT_CODE_MAGIC_VALUE de mupen */
#define VI_ARRANQUE    60

typedef struct
{
    unsigned int dir;     /* tipo en el byte alto + direccion de 24 bits */
    unsigned int val;
    unsigned int viejo;   /* valor antes del truco, para restaurar al apagar */
} CodigoGS;

typedef struct
{
    unsigned int indice;
    int          usado;
    int          activo;
    int          estabaActivo;
    int          primero;     /* en s_cod */
    int          n;
} Truco;

static CodigoGS s_cod[CODIGOS_MAX];
static int      s_ncod = 0;
static Truco    s_tr[TRUCOS_MAX];
static int      s_ntr = 0;
static int      s_vi  = 0;

static CRITICAL_SECTION s_cs;
static volatile LONG    s_csIni = 0;

static void cs_asegura(void)
{
    /* Primera llamada: la hace retro_cheat_reset/set o el primer VI, y en
     * cualquier caso antes de que haya dos hilos tocando las tablas. */
    if (InterlockedCompareExchange(&s_csIni, 1, 0) == 0)
    {
        InitializeCriticalSection(&s_cs);
        s_csIni = 2;
    }
    while (s_csIni != 2)
        Sleep(0);
}

/* --- acceso a memoria, como gamehacks.c -------------------------------- */

static unsigned int dir_real(unsigned int d)
{
    return 0x80000000u | (d & 0x00FFFFFFu);
}

static unsigned int lee8(unsigned int d)
{
    address = dir_real(d);
    read_byte_in_memory();
    return byte;
}

static unsigned int lee16(unsigned int d)
{
    address = dir_real(d) & ~1u;
    read_hword_in_memory();
    return hword;
}

static void invalida(unsigned int real)
{
    if (!interpcore)
    {
        invalidate_func(real & ~3u);
        invalidate_func((real & ~3u) - 0x80000000u + 0xA0000000u);
    }
}

static void escribe8(unsigned int d, unsigned int v)
{
    if (lee8(d) == (v & 0xFF))
        return;
    address = dir_real(d);
    byte = (unsigned char)v;
    write_byte_in_memory();
    invalida(dir_real(d));
}

static void escribe16(unsigned int d, unsigned int v)
{
    if (lee16(d) == (v & 0xFFFF))
        return;
    address = dir_real(d) & ~1u;
    hword = (unsigned short)v;
    write_hword_in_memory();
    invalida(dir_real(d));
}

/* execute_cheat de mupen: devuelve 0 si es una condicion que no se cumple. */
static int ejecuta(unsigned int dir, unsigned int val, unsigned int *viejo)
{
    switch (dir & 0xFF000000u)
    {
    case 0x80000000u:
    case 0x88000000u:
    case 0xA0000000u:
    case 0xA8000000u:
    case 0xF0000000u:
        if (viejo && *viejo == VIEJO_NINGUNO)
            *viejo = lee8(dir);
        escribe8(dir, val);
        return 1;
    case 0x81000000u:
    case 0x89000000u:
    case 0xA1000000u:
    case 0xA9000000u:
    case 0xF1000000u:
        if (viejo && *viejo == VIEJO_NINGUNO)
            *viejo = lee16(dir);
        escribe16(dir, val);
        return 1;
    case 0xD0000000u:
    case 0xD8000000u:
        return lee8(dir) == (val & 0xFF);
    case 0xD1000000u:
    case 0xD9000000u:
        return lee16(dir) == (val & 0xFFFF);
    case 0xD2000000u:
    case 0xDB000000u:
        return lee8(dir) != (val & 0xFF);
    case 0xD3000000u:
    case 0xDA000000u:
        return lee16(dir) != (val & 0xFFFF);
    case 0xEE000000u:
        ejecuta(0xF1000318u, 0x0040, NULL);
        ejecuta(0xF100031Au, 0x0000, NULL);
        return 1;
    default:
        return 1;
    }
}

/* cheat_apply_cheats de mupen, para un truco. */
static void aplica_truco(Truco *t, int arranque)
{
    int i;
    int fallo = 0;

    if (t->activo)
    {
        t->estabaActivo = 1;
        for (i = 0; i < t->n; i++)
        {
            CodigoGS *c = &s_cod[t->primero + i];
            const unsigned int tipo = c->dir & 0xFF000000u;

            if (arranque)
            {
                if ((c->dir & 0xF0000000u) == 0xF0000000u)
                    ejecuta(c->dir, c->val, &c->viejo);
                continue;
            }

            if ((c->dir & 0xF0000000u) == 0xD0000000u)
            {
                /* D8-DB piden el boton GameShark, que aqui no existe */
                if (tipo == 0xD8000000u || tipo == 0xD9000000u ||
                    tipo == 0xDA000000u || tipo == 0xDB000000u)
                    fallo = 1;
                if (!ejecuta(c->dir, c->val, NULL))
                    fallo = 1;
            }
            else
            {
                if (fallo)
                {
                    fallo = 0;
                    continue;
                }
                if (tipo == 0x88000000u || tipo == 0x89000000u ||
                    tipo == 0xA8000000u || tipo == 0xA9000000u)
                    continue;   /* boton GameShark */
                if ((c->dir & 0xF0000000u) != 0xF0000000u)
                    ejecuta(c->dir, c->val, &c->viejo);
            }
        }
    }
    else if (t->estabaActivo)
    {
        t->estabaActivo = 0;
        if (!arranque)
        {
            for (i = 0; i < t->n; i++)
            {
                CodigoGS *c = &s_cod[t->primero + i];
                if (c->viejo != VIEJO_NINGUNO)
                {
                    ejecuta(c->dir, c->viejo, NULL);
                    c->viejo = VIEJO_NINGUNO;
                }
            }
        }
    }
}

/* --- interfaz ------------------------------------------------------------ */

void xbox64_trucos_borrar(void)
{
    cs_asegura();
    EnterCriticalSection(&s_cs);
    s_ntr  = 0;
    s_ncod = 0;
    LeaveCriticalSection(&s_cs);
}

static int es_hex(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

static unsigned int valor_hex(char ch)
{
    if (ch >= '0' && ch <= '9') return (unsigned int)(ch - '0');
    if (ch >= 'a' && ch <= 'f') return (unsigned int)(ch - 'a' + 10);
    return (unsigned int)(ch - 'A' + 10);
}

void xbox64_trucos_poner(unsigned indice, int activo, const char *texto)
{
    static unsigned int tok[TOKENS_MAX];
    int ntok = 0;
    int i;
    Truco *t = NULL;

    if (texto == NULL)
        return;

    /* Como mupen64plus-libretro: cualquier caracter que no sea hex separa, y
     * los tokens van por parejas direccion + valor ("8033B21E 0008+..."). */
    {
        const char *p = texto;
        while (*p && ntok < TOKENS_MAX)
        {
            if (es_hex(*p))
            {
                unsigned int v = 0;
                while (es_hex(*p))
                {
                    v = (v << 4) | valor_hex(*p);
                    p++;
                }
                tok[ntok++] = v;
            }
            else
                p++;
        }
    }

    cs_asegura();
    EnterCriticalSection(&s_cs);

    /* El mismo indice otra vez: sustituye (find_or_create_cheat de mupen).
     * Sus codigos viejos se quedan en la tabla hasta el siguiente borrado. */
    for (i = 0; i < s_ntr; i++)
        if (s_tr[i].usado && s_tr[i].indice == indice)
        {
            t = &s_tr[i];
            break;
        }
    if (t == NULL && s_ntr < TRUCOS_MAX)
        t = &s_tr[s_ntr++];

    if (t != NULL && s_ncod + ntok / 2 <= CODIGOS_MAX)
    {
        t->indice       = indice;
        t->usado        = 1;
        t->activo       = activo ? 1 : 0;
        t->estabaActivo = 0;
        t->primero      = s_ncod;
        t->n            = ntok / 2;
        for (i = 0; i + 1 < ntok; i += 2)
        {
            s_cod[s_ncod].dir   = tok[i];
            s_cod[s_ncod].val   = tok[i + 1];
            s_cod[s_ncod].viejo = VIEJO_NINGUNO;
            s_ncod++;
        }
    }

    LeaveCriticalSection(&s_cs);

#if XBOX64_TRAZAS
    {
        char l[160];
        _snprintf(l, sizeof(l), "[TRUCOS] truco %u: %d codigos, activo=%d%s",
                  indice, ntok / 2, activo ? 1 : 0,
                  (t == NULL) ? " (SIN SITIO, descartado)" : "");
        l[sizeof(l) - 1] = 0;
        xbox64_log_line(l);
    }
#endif
}

void xbox64_trucos_vi(void)
{
    unsigned long sAddress;
    unsigned char sByte;
    unsigned short sHword;
    int arranque;
    int i;

    /* gs_apply_cheats de mupen: el contador corre aunque no haya trucos. */
    if (s_vi < VI_ARRANQUE)
    {
        arranque = (s_vi == 0);
        s_vi++;
        if (!arranque)
            return;
    }
    else
        arranque = 0;

    if (s_ntr == 0)
        return;

    cs_asegura();
    EnterCriticalSection(&s_cs);

    /* Las globales de memoria del nucleo, intactas para quien siga. */
    sAddress = address;
    sByte    = byte;
    sHword   = hword;

    for (i = 0; i < s_ntr; i++)
        if (s_tr[i].usado)
            aplica_truco(&s_tr[i], arranque);

    address = sAddress;
    byte    = sByte;
    hword   = sHword;

    LeaveCriticalSection(&s_cs);
}

void xbox64_trucos_rom_nueva(void)
{
    s_vi = 0;
    xbox64_trucos_borrar();
}
