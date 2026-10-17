/*
 * xenon/rice_prof.cpp  --  los cubos del perfilador de Rice y su informe.
 *
 * El porque (y por que no un perfilador automatico) esta en rice_prof.h.
 * Aqui solo viven las globales y el volcado: las declaraciones NO pueden
 * estar en el arbol copiado de Rice, igual que pasa con gln64_prof.
 */

#include <xtl.h>
#include <stdio.h>
#include <ppcintrinsics.h>      /* __mftb */

#include "rice_prof.h"

extern "C" void DebugOutputString(const char *s);

#if RICE_PROF

extern "C" {
volatile unsigned long long ricep_flush_tb  = 0;  volatile unsigned int ricep_flush_n  = 0;
volatile unsigned long long ricep_zbias_tb  = 0;  volatile unsigned int ricep_zbias_n  = 0;
volatile unsigned long long ricep_vport_tb  = 0;  volatile unsigned int ricep_vport_n  = 0;
volatile unsigned long long ricep_ldtex_tb  = 0;  volatile unsigned int ricep_ldtex_n  = 0;
volatile unsigned long long ricep_verts_tb  = 0;  volatile unsigned int ricep_verts_n  = 0;
volatile unsigned long long ricep_pvtx_tb   = 0;  volatile unsigned int ricep_pvtx_n   = 0;
volatile unsigned long long ricep_mtx_tb    = 0;  volatile unsigned int ricep_mtx_n    = 0;
volatile unsigned long long ricep_lod_tb    = 0;  volatile unsigned int ricep_lod_n    = 0;
volatile unsigned long long ricep_tcache_tb = 0;  volatile unsigned int ricep_tcache_n = 0;
volatile unsigned long long ricep_crc_tb    = 0;  volatile unsigned int ricep_crc_n    = 0;
volatile unsigned long long ricep_dlist_tb  = 0;  volatile unsigned int ricep_dlist_n  = 0;
volatile unsigned long long ricep_pretex_tb = 0;  volatile unsigned int ricep_pretex_n = 0;
volatile unsigned long long ricep_endupd_tb = 0;  volatile unsigned int ricep_endupd_n = 0;
volatile unsigned long long ricep_combl_tb  = 0;  volatile unsigned int ricep_combl_n  = 0;
volatile unsigned long long ricep_omodes_tb = 0;  volatile unsigned int ricep_omodes_n = 0;
volatile unsigned long long ricep_child_tb  = 0;
volatile int                ricep_prof_nivel = 0;
volatile unsigned int       ricep_raros      = 0;
}

/* El time base del Xenon va a 49,875 MHz: ticks/50 ~= us.  Misma conversion
 * que el resto del arbol. */
static unsigned rp_usf(unsigned long long tb, unsigned f)
{
    if (!f) f = 1;
    return (unsigned)((tb / 50ULL) / (unsigned long long)f);
}

extern "C" void rice_prof_informe(unsigned frames)
{
    char l[208];
    unsigned long long suma;

    if (!frames) frames = 1;

    /* ★★ LA SUMA VA PRIMERO Y ES EL NUMERO QUE VALIDA TODO LO DEMAS.
     *
     * Los cubos son DISJUNTOS (cada uno descuenta a sus hijos), asi que su
     * suma tiene que parecerse a `[GFXTH] trabajo`.  Si sale muy por debajo,
     * hay trabajo de Rice fuera de los diez ambitos y la particion esta
     * incompleta: eso es un dato, no un fallo, y dice que hay que
     * instrumentar otra funcion.
     *
     * ★ `flush` es el cubo que recoge el resto del draw: su tiempo PROPIO es
     * el prologo que vive en `Render.cpp` (PrepareTextures, InitOtherModes,
     * SetCombinerAndBlender), que no esta instrumentado a proposito.
     *
     * ★★ Y `raros` ES LO PRIMERO QUE HAY QUE MIRAR.  Si no es 0, hay ambitos
     * desemparejados y ningun cubo es de fiar; si es 0, los numeros son los
     * que son aunque sorprendan.  Sin el, un cubo enorme no distingue "esto
     * cuesta muchisimo" de "el instrumento esta roto" -- que es exactamente
     * lo que paso con `pvtx` saturado a 0xFFFFFFFF en la primera version. */
    suma = ricep_flush_tb + ricep_zbias_tb + ricep_vport_tb + ricep_ldtex_tb
         + ricep_verts_tb + ricep_pvtx_tb  + ricep_mtx_tb   + ricep_lod_tb
         + ricep_tcache_tb + ricep_crc_tb
         + ricep_dlist_tb + ricep_pretex_tb + ricep_endupd_tb
         + ricep_combl_tb + ricep_omodes_tb;

    _snprintf(l, sizeof(l),
              "[RICEP] us/frame SUMA=%u raros=%u niv=%d | flush=%u (%u/s)"
              " zbias=%u vport=%u ldtex=%u verts=%u",
              rp_usf(suma, frames), ricep_raros, ricep_prof_nivel,
              rp_usf(ricep_flush_tb, frames), ricep_flush_n,
              rp_usf(ricep_zbias_tb, frames),
              rp_usf(ricep_vport_tb, frames),
              rp_usf(ricep_ldtex_tb, frames),
              rp_usf(ricep_verts_tb, frames));
    DebugOutputString(l);

    _snprintf(l, sizeof(l),
              "[RICEP] us/frame pvtx=%u (%u/s) mtx=%u (%u/s) lod=%u |"
              " tcache=%u (%u/s) crc=%u (%u/s)",
              rp_usf(ricep_pvtx_tb, frames),   ricep_pvtx_n,
              rp_usf(ricep_mtx_tb, frames),    ricep_mtx_n,
              rp_usf(ricep_lod_tb, frames),
              rp_usf(ricep_tcache_tb, frames), ricep_tcache_n,
              rp_usf(ricep_crc_tb, frames),    ricep_crc_n);
    DebugOutputString(l);

    /* ★ `dlist` va el PRIMERO de esta linea y con su cuenta: es el padre de
     * todos los demas, asi que su tiempo PROPIO es el despacho de comandos
     * -- lo que antes no aparecia en ningun sitio-- y su `n` tiene que
     * salir igual que las listas por segundo del `[GFXTH]`. */
    _snprintf(l, sizeof(l),
              "[RICEP] us/frame dlist=%u (%u/s) | pretex=%u (%u/s)"
              " endupd=%u (%u/s) combl=%u (%u/s) omodes=%u (%u/s)",
              rp_usf(ricep_dlist_tb, frames),  ricep_dlist_n,
              rp_usf(ricep_pretex_tb, frames), ricep_pretex_n,
              rp_usf(ricep_endupd_tb, frames), ricep_endupd_n,
              rp_usf(ricep_combl_tb, frames),  ricep_combl_n,
              rp_usf(ricep_omodes_tb, frames), ricep_omodes_n);
    DebugOutputString(l);

    ricep_flush_tb  = 0;  ricep_flush_n  = 0;
    ricep_zbias_tb  = 0;  ricep_zbias_n  = 0;
    ricep_vport_tb  = 0;  ricep_vport_n  = 0;
    ricep_ldtex_tb  = 0;  ricep_ldtex_n  = 0;
    ricep_verts_tb  = 0;  ricep_verts_n  = 0;
    ricep_pvtx_tb   = 0;  ricep_pvtx_n   = 0;
    ricep_mtx_tb    = 0;  ricep_mtx_n    = 0;
    ricep_lod_tb    = 0;  ricep_lod_n    = 0;
    ricep_tcache_tb = 0;  ricep_tcache_n = 0;
    ricep_crc_tb    = 0;  ricep_crc_n    = 0;
    ricep_dlist_tb  = 0;  ricep_dlist_n  = 0;
    ricep_pretex_tb = 0;  ricep_pretex_n = 0;
    ricep_endupd_tb = 0;  ricep_endupd_n = 0;
    ricep_combl_tb  = 0;  ricep_combl_n  = 0;
    ricep_omodes_tb = 0;  ricep_omodes_n = 0;
    ricep_raros     = 0;
    /* `child_tb` y `prof_nivel` NO se tocan: pertenecen a la pila de ambitos
     * en curso, y este informe puede caer entre dos listas.  Ponerlos a cero
     * aqui le robaria su total al padre que este abierto. */
}

#else  /* !RICE_PROF */

extern "C" void rice_prof_informe(unsigned frames) { (void)frames; }

#endif /* RICE_PROF */
