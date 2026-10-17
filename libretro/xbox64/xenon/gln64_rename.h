/*
 * gln64_rename.h  --  prefija los puntos de entrada de glN64_GX.
 *
 * ★ POR QUE TAMBIEN SE RENOMBRA glN64 Y NO SOLO Rice.
 *
 * El nucleo no llama al plugin por puntero: llama a `processDList()` como
 * funcion normal, y cada plugin se apropia de ese nombre con su propio
 * `GFXPlugin.h` (`#define ProcessDList processDList`).  Con dos plugins en el
 * mismo binario ese nombre tiene que quedar LIBRE para el despachador, asi
 * que se prefijan LOS DOS:
 *
 *     glN64  ->  GlN64_processDList      (esta cabecera)
 *     Rice   ->  Rice_processDList       (xenon/rice_rename.h)
 *     nucleo ->  processDList            (xbox/gfx_dispatch.c, elige)
 *
 * Renombrar solo Rice no habria servido: glN64 seguiria definiendo
 * `processDList` y el despachador chocaria contra el.
 *
 * ★ SOLO LOS QUE EL NUCLEO LLAMA.  Los demas simbolos comunes de los dos
 * plugins (`VI_GX_*`, `gettime`, `diff_sec`...) se resuelven prefijando UN
 * lado, y ese es Rice.  Aqui se toca lo minimo, porque cada nombre que se
 * renombra es una linea mas que alguien tendra que entender manyana.
 *
 * ★ COMO ENTRA.  Lo incluye `xbox/xbox64_pre.h` -- la cabecera forzada de
 * todas las unidades -- cuando ve `__GX__` SIN `XBOX64_PLUGIN_RICE`.  O sea
 * que lo disparan los 38 .cpp de glN64 sin tocar sus lineas del vcxproj: ya
 * llevan `__GX__;__LINUX__` desde la fase 2.  El porque de no usar un
 * `<ForcedIncludeFiles>` por fichero esta en su gemelo `rice_rename.h`.
 *
 * Tambien lo ve `xenon/gln64_config.cpp`, que lleva `__GX__` por incluir
 * cabeceras del plugin.  Es inofensivo: se comprobo que no menciona ninguno
 * de los diez nombres.
 *
 * El arbol copiado no se toca.
 */

#ifndef _XENON_GLN64_RENAME_H_
#define _XENON_GLN64_RENAME_H_

#define initiateGFX      GlN64_initiateGFX
#define processDList     GlN64_processDList
#define processRDPList   GlN64_processRDPList
#define updateScreen     GlN64_updateScreen
#define romOpen_gfx      GlN64_romOpen_gfx
#define romClosed_gfx    GlN64_romClosed_gfx
#define closeDLL_gfx     GlN64_closeDLL_gfx
#define showCFB          GlN64_showCFB
#define viStatusChanged  GlN64_viStatusChanged
#define viWidthChanged   GlN64_viWidthChanged

#endif /* _XENON_GLN64_RENAME_H_ */
