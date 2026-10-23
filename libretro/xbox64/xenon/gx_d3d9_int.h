/*
 * gx_d3d9_int.h  --  estado interno compartido del backend GX -> D3D9.
 *
 * Lo usan xenon/gx_d3d9.cpp (estado, geometria, texturas, copia) y
 * xenon/gx_d3d9_tev.cpp (el combinador TEV -> HLSL).  No lo incluye nadie
 * de source/glN64_GX/.
 */

#ifndef _XENON_GX_D3D9_INT_H_
#define _XENON_GX_D3D9_INT_H_

#include "gxshim/gccore.h"
#include "d3d9_grabador.h"     /* g_pd3dDevice ya tipado (D3D_GRABADOR) */

/* ------------------------------------------------------------------ */
/* Vertice                                                             */
/* ------------------------------------------------------------------ */
/* ★ `struct GXD3DVertex` y `GXD3D_PACK_RGBA` VIVEN EN `gxd3d_lote.h`, NO
 * AQUI, porque el plugin tambien los necesita: `OGL_DrawTriangles` escribe
 * los vertices de un lote entero de una pasada sobre el vertex buffer
 * bloqueado (ver la nota larga de ese fichero).  [etapa 3 del renderizado
 * con hilo: ya no es el vertex buffer sino `s_staging`, el bufer de
 * preparacion en CPU del productor; el puntero que recibe el plugin es el
 * mismo tipo y el contrato no cambia.  Ver la nota de `s_staging` en
 * gx_d3d9.cpp.]
 *
 * UNA sola definicion, dos usuarios.  Duplicar el layout -- un "espejo" en
 * cada lado-- seria una trampa de deriva silenciosa: el dia que uno de los
 * dos cambie, el otro escribe basura y nadie avisa. */
#include "gxd3d_lote.h"

/* ------------------------------------------------------------------ */
/* Estado del TEV que el shim acumula                                  */
/* ------------------------------------------------------------------ */
#define GXD3D_MAX_STAGES 16

struct GXD3DTevStage
{
    u8 texcoord, color;            /* GX_SetTevOrder                     */
    u32 texmap;
    u8 cin[4], ain[4];             /* GX_SetTevColorIn / AlphaIn (a,b,c,d) */
    u8 cop, cbias, cscale, cclamp, creg;   /* GX_SetTevColorOp            */
    u8 aop, abias, ascale, aclamp, areg;   /* GX_SetTevAlphaOp            */
    u8 kcsel, kasel;               /* GX_SetTevKColorSel / KAlphaSel      */
};

struct GXD3DTevState
{
    GXD3DTevStage stage[GXD3D_MAX_STAGES];
    u8  numStages;
    u8  numTexGens;
    u8  numChans;

    /* Registros de color del TEV (GX_SetTevColor): prev, reg0..reg2 */
    GXColor reg[4];
    /* Constantes konst (GX_SetTevKColor) */
    GXColor konst[4];

    /* alpha compare (GX_SetAlphaCompare) */
    u8  acComp0, acRef0, acOp, acComp1, acRef1;

    /* Z texture (GX_SetZTexture) */
    u8  ztOp, ztFmt;
    u32 ztBias;

    /* niebla (GX_SetFog) */
    u8  fogType;
    f32 fogStartZ, fogEndZ, fogNearZ, fogFarZ;
    GXColor fogColor;

    /* Lectura "monocroma" de un framebuffer de 8 bits (GLideN64
     * uFbMonochrome/uFbFixedAlpha; ver GXD3D_SetCopyMono en gccore.h).
     * `texMono[m]` = 1 si lo ligado en GX_TEXMAP0/1 es una copia marcada
     * mono; lo pone GX_LoadTexObj en cada ligado, asi que cualquier otra
     * textura lo apaga.  `monoAlfaFija` = IMAGE_READ apagado. */
    u8  texMono[2];
    u8  monoAlfaFija;

    /* El blender del N64 CLR_IN*A_FOG + CLR_FOG*(1-A_FOG)
     * (GXD3D_SetBlenderNiebla, gccore.h). */
    u8  blFog;
    GXColor blFogColor;

    /* La niebla del TEV con el peso AL REVES: el blender del N64
     * `CLR_IN * A_SHADE + CLR_FOG * (1 - A_SHADE)` (GXD3D_SetNieblaInversa). */
    u8  fogInv;

    /* Profundidad sujeta de GXD3D_SetDepthClamp: el pixel shader escribe la
     * profundidad real por pixel. */
    u8  zClamp;

    /* GXD3D_SetRenderToDepth (gccore.h): 0 normal, 1 profundidad = rojo del
     * color, 2 ademas descarta si la z propia no pasa contra la copia de Z,
     * 3 [Mario Golf] profundidad = un Z guardado, pixel a pixel, 4 [Zelda MM]
     * idem solo donde el Z actual es 0, 5 [Resident Evil 2] profundidad = la
     * palabra de Z del N64 del texel (R alto, G bajo) pasada al Z del EFB. */
    u8  rtz;

    /* GXD3D_SetDecalN64 (gccore.h): el decal del N64 por shader contra la
     * copia de Z. */
    u8  decN64;

    /* GXD3D_SetNieblaZ (gccore.h): la niebla por Z de Beetle Adventure
     * Racing, contra la copia de Z y la curva de GXD3D_NIEBLAZ_SAMPLER. */
    u8  nieblaZ;
};

/* Lo llama GX_LoadTexObj: lo ligado en `mapid` es (1) o no es (0) una copia
 * monocroma. */
void gxd3d_tev_tex_mono(u8 mapid, u8 mono);
/* Si la textura ligada en `mapid` es un framebuffer resuelto.  Solo lo usa el
 * marcado `texalfa` (GLN64_MARCA_TEXALFA), que pinta el alfa SOLO de esos. */
void gxd3d_tev_tex_esfb(u8 mapid, u8 esfb);

/* Lo llama GXD3D_SetDepthClamp (gx_d3d9.cpp): la mitad del pixel shader. */
void gxd3d_tev_zclamp(u8 on);

/* Lo llama GXD3D_SetRenderToDepth (gx_d3d9.cpp): la mitad del pixel shader.
 * El sampler y la constante de la copia de Z los liga gx_d3d9.cpp. */
void gxd3d_tev_rtz(u8 modo);
/* Lo llama GXD3D_SetDecalN64 (gx_d3d9.cpp): la mitad del pixel shader.  Usa
 * la misma copia de Z, sampler y constante que el modo 2 de arriba. */
void gxd3d_tev_decn64(u8 on);
#define GXD3D_ZCOPIA_SAMPLER 3    /* uZCopia : register(s3)                  */
#define GXD3D_ZCOPIA_CONST   14   /* uZCopiaInv : register(c14), 1/ancho, 1/alto */
/* Lo llama GXD3D_SetNieblaZ (gx_d3d9.cpp): la mitad del pixel shader.  Usa
 * la copia de Z de arriba y la curva de 256x1 (ARGB: color de niebla y alfa
 * por indice) en su propio sampler, fuera de los de GX (0-2) y de la Z (3). */
void gxd3d_tev_nieblaz(u8 on);
#define GXD3D_NIEBLAZ_SAMPLER 4   /* uNieblaZ : register(s4)                 */
/* [Zelda MM] Modo 4 de GXD3D_SetRenderToDepth: la copia del Z ACTUAL, junto
 * a la guardada (uZCopia), para descartar donde el actual es > 0. */
#define GXD3D_ZACTUAL_SAMPLER 5   /* uZActual : register(s5)                 */
/* uNieblaZPar : register(c20) = (nearz, 261632 / (farz - nearz), umbral del
 * lejano, palabra de borrado del Z): el paso del Z del EFB al del N64, el
 * mismo de DepthCopy.cpp (_dcBuildLut).  OJO: la sombra de constantes de
 * d3dq.cpp (SOMBRA_PSC) tiene que llegar hasta c20. */
#define GXD3D_NIEBLAZ_CONST   20
/* [Resident Evil 2] Modo 5 de GXD3D_SetRenderToDepth: uRe2Z : register(c20) =
 * (nearz, (farz - nearz) / 261632, 0, 0), la palabra de Z del N64 al Z del
 * EFB.  El MISMO registro que uNieblaZPar: el shader no lleva nunca los dos
 * (gx_d3d9_tev.cpp) y cada lote pone el suyo (gxd3d_zcopia_lote). */
#define GXD3D_RE2Z_CONST      20
/* GXD3D_SetRuido (entradas GXD3D_CC_RUIDO / GXD3D_CA_RUIDO): su constante,
 * uRuido : register(c15) = (semilla, 1/escX, 1/escY, 0).  No es TEVC_TOTAL + n:
 * TEVC_MARCA ya vive justo detras (c13) y c14 es de la Z. */
#define GXD3D_RUIDO_CONST    15
/* GXD3D_SetLod (entradas GXD3D_CC_LODFRAC / GXD3D_CA_LODFRAC): DOS constantes,
 * uLod0 : register(c16) = (kS, kT, minLod, maxTile) y
 * uLod1 : register(c17) = (detalle, activo, 0, 0).  OJO: la sombra de
 * constantes de d3dq.cpp (SOMBRA_PSC) tiene que llegar hasta c17. */
#define GXD3D_LOD_CONST      16
/* GXD3D_SetLodAtlas (atlas de mipmap en el texmap 1): DOS constantes,
 * uAtlas0 : register(c18) = (1/atlasW, 1/atlasH, realW1, realH1) y
 * uAtlas1 : register(c19) = (bilineal, nivMax, clampS, clampT).  OJO: la
 * sombra de constantes de d3dq.cpp (SOMBRA_PSC) tiene que llegar hasta c19. */
#define GXD3D_ATLAS_CONST    18

extern GXD3DTevState g_gxTev;

/* Marca el combinador como sucio: la proxima emision de geometria
 * reconstruira/seleccionara el pixel shader. */
void gxd3d_tev_dirty(void);

/* Aplica al device el pixel shader que corresponde al estado actual del TEV
 * y sube sus constantes.  Lo llama gx_d3d9.cpp al ARRANCAR un lote, no al
 * vaciarlo: el vaciado tiene que poder ocurrir desde cualquier sitio sin
 * cambiar nada (ver la nota en gxd3d_flush_pending). */
void gxd3d_tev_apply(void);

/* ¿Aplicar ahora cambiaria el shader o sus constantes?  Es la condicion que
 * decide si un lote se puede seguir extendiendo.  Resuelve la clave y la
 * compara con la del shader ligado, asi que las reescrituras del combinador
 * con los mismos valores -- que glN64 hace antes de casi cada lote-- no
 * rompen la fusion.  El porque largo esta junto a la definicion. */
int  gxd3d_tev_difiere(void);

/* ★ "Lo que YO ligue ya no esta ligado": lo llama `gxd3d_frame_begin` UNA vez
 * por pasada, porque entre pasadas el frontend comparte el device y pone SU
 * pixel shader.  Sin esto, `gxd3d_tev_apply` tenia que reafirmarlo en CADA
 * lote (~6.000 veces/s).  Mismo criterio que `s_bindValid` para el vertex
 * shader, la declaracion y el stream source. */
void gxd3d_tev_invalidar_ligado(void);

/* Ciclo de vida del generador de shaders. */
void gxd3d_tev_init(void);
void gxd3d_tev_shutdown(void);

/* ------------------------------------------------------------------ */
/* Contadores de diagnostico (se leen desde profile.c, que compila como C) */
/* ------------------------------------------------------------------ */
#ifdef __cplusplus
extern "C" {
#endif
extern volatile unsigned int gxd3d_dbg_draws;      /* DrawPrimitive emitidos   */
extern volatile unsigned int gxd3d_dbg_verts;      /* vertices emitidos        */
extern volatile unsigned int gxd3d_dbg_shgen;      /* pixel shaders generados  */
extern volatile unsigned int gxd3d_dbg_shhit;      /* aciertos de cache        */
extern volatile unsigned int gxd3d_dbg_shfail;     /* fallos de compilacion    */
extern volatile unsigned int gxd3d_dbg_texconv;    /* texturas convertidas     */
extern volatile unsigned int gxd3d_dbg_resolve;    /* GX_CopyTex servidos      */
extern volatile unsigned int gxd3d_dbg_shcompus;   /* us gastados compilando   */
extern volatile unsigned int gxd3d_dbg_shovf;      /* cache de shaders LLENA   */
extern volatile unsigned int gxd3d_dbg_degen;      /* viewport/scissor 0x0     */
extern volatile unsigned int gxd3d_dbg_vbdrop;     /* vertices tirados: VB lleno */

/* [Xbox360] Sonda TEMPORAL de la niebla; ver la nota en gx_d3d9_tev.cpp. */
extern volatile unsigned int gxd3d_dbg_fogon;
extern volatile unsigned int gxd3d_dbg_fogoff;
extern volatile unsigned int gxd3d_dbg_fogcol;
extern volatile float        gxd3d_dbg_fogini;
extern volatile float        gxd3d_dbg_fogfin;
extern volatile unsigned int gxd3d_dbg_ztex;       /* draws con Z-texture REPLACE */
extern volatile unsigned int gxd3d_dbg_scisclip;   /* draws con la pantalla recortada */
extern volatile unsigned int gxd3d_dbg_scismin;    /* area del recorte mas pequeno */
extern volatile unsigned int gxd3d_dbg_scisminw;   /* y sus dimensiones            */
extern volatile unsigned int gxd3d_dbg_scisminh;
/* Cuentan en gxd3d_clear_efb, que es por donde se borra de verdad desde que
 * Wii64 1b5041a puso GX_ALWAYS en OGL_GXclearEFB.  (Antes habia aqui
 * `zclear`/`efbclear`, que contaban GX_GEQUAL y ese cambio los dejo a cero
 * para siempre sin avisar: retirados.) */
extern volatile unsigned int gxd3d_dbg_clrcol;     /* borrados de COLOR           */
extern volatile unsigned int gxd3d_dbg_clrz;       /* borrados de PROFUNDIDAD     */
extern volatile unsigned int gxd3d_dbg_clrrgb;     /* ultimo color, 0x00RRGGBB    */
extern volatile unsigned int gxd3d_dbg_vpz;        /* viewports con Z fuera de [0,1] */
extern volatile unsigned int gxd3d_dbg_rtthit;     /* texturas ya resueltas       */
extern volatile unsigned int gxsh_rtt_tb;          /* tiempo en el Resolve de RTT */
extern volatile unsigned int gxsh_rtt_n;
extern volatile unsigned int gxd3d_dbg_texfree;    /* bajas por liberacion     */
extern volatile unsigned int gxd3d_dbg_initobj;    /* GX_InitTexObj llamadas   */
extern volatile unsigned int gxd3d_dbg_texinval;   /* de esas, las que anulan  */
extern volatile unsigned int gxd3d_dbg_rttdesal;   /* resuelta VIVA desalojada */
extern volatile unsigned int gxd3d_dbg_drawtex;    /* draws CON textura en s0  */
extern volatile unsigned int gxd3d_dbg_drawnotex;  /* draws SIN textura en s0  */
extern volatile unsigned int gxd3d_dbg_uvtri;      /* max |UV0| en triangulos  */
extern volatile unsigned int gxd3d_dbg_uvrect;     /* max |UV0| en quads       */
extern volatile unsigned int gxd3d_dbg_uv0_2;      /* histograma de |UV0|      */
extern volatile unsigned int gxd3d_dbg_uv2_8;
extern volatile unsigned int gxd3d_dbg_uv8_32;
extern volatile unsigned int gxd3d_dbg_uv32_1k;
extern volatile unsigned int gxd3d_dbg_uvbig;
#ifdef __cplusplus
}
#endif

#endif /* _XENON_GX_D3D9_INT_H_ */
