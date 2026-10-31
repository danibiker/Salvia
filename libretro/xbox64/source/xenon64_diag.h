/* xenon64_diag.h -- interruptor MAESTRO de la instrumentacion
 *
 * Al estilo de PCSXR_DIAG_INSTRUMENTATION en pcsxr-360
 * (libpcsxcore/gpu.h y plugins/xbox_soft/gpu.c).
 *
 * A 0 (por defecto) NO queda ni un printf ni un contador de instrumentacion en
 * el binario: todos los volcados de este port cuelgan de aqui, y con el maestro
 * apagado el preprocesador los elimina antes de que el compilador los vea.
 * Coste en runtime: cero.
 *
 * A 1 se recuperan, cada uno con su sub-interruptor local (que sigue estando en
 * su fichero, junto al codigo que mide, porque ahi es donde esta explicado que
 * mide y como leerlo).
 *
 * Puede llegar por /D del compilador: /D XENON64_DIAG=1
 *
 * -- Que cubre ------------------------------------------------------------
 *
 * 1. Los volcados del plugin grafico y del backend D3D9:
 *      d3d9_backend.cpp   VP_DUMP HUD_DUMP HUD2_DUMP HUD3_DUMP DUMP_MUX
 *                         DUMP_FRAMES MAT_DUMP TORSO_DUMP LOTE_DUMP ZONA_DUMP
 *                         CENSO_DUMP RT_CLEAR_PROBE ST_DUMP_PIXELS
 *                         TEX_PAD_PROBE TEXMAP_DUMP TEXPAD_DUMP LOG_GEN_HLSL
 *      RDP_Texture.h      TRC_DUMP DLMAP_DUMP REGS_DUMP SAVE_DUMP
 *      Render.cpp         TR_DUMP MAPA_DUMP
 *      RenderBase.cpp     REJ_DUMP
 *      RSP_GBI2.h         FOGDBG
 *      RSP_Parser.cpp     OPS_DUMP
 *      TextureManager.cpp EXP_DUMP
 *
 * 2. Los dos vigias permanentes, que hasta ahora estaban a 1:
 *      dma.c              DMA_DUMP    (truncado de transferencias ROM->RAM)
 *      rsp_main.c         RSPC_DUMP   (tareas del RSP descartadas)
 *
 * 3. El hack de la subpantalla de Zelda:
 *      interupt.c         ZH_DUMP
 *
 * 4. Los dos niveles de log del plugin (dbgswitch.h):
 *      DLOG   verboso, ya estaba tras BUG_HUNT_LOGGING
 *      DWARN  el que SI salia siempre en Release
 *
 * 5. El perfilado por subsistemas: los volcados [PERF] [PERF/CNT] [AUDIO]
 *    [GFX] [RC] [JC] y, mas importante, los DOS mftb por acceso a memoria y
 *    por instruccion interpretada que anaden start_section/end_section.
 *    Eso ultimo es lo unico de esta lista que cuesta de verdad, y es la razon
 *    principal de que exista este fichero.
 *
 * -- Que NO cubre (a proposito) -------------------------------------------
 *
 * Los toggles de COMPORTAMIENTO, que son arreglos o experimentos y no logs.
 * Estos NO deben depender del maestro, porque apagar la instrumentacion no
 * puede cambiar lo que se ve en pantalla:
 *
 *      EXP_STRIDE         el parche negro del traje de Link
 *      FOG_ENABLE         la niebla
 *      FOG_RANGO_GLN64    la banda de niebla como glN64
 *      EXP_FULL           relleno de textura en las dos direcciones
 *      FOG_FORZAR_RANGO   A/B de la banda de niebla
 *      PROJ_NEAR_FAR      los planos de recorte de la capa 2D
 *      CULL_2D_OFF SCISSOR_TEST ALPHA_TEST_FIX TEXTURE_POW2
 *      RTT_SKIP_CRC       FrameBuffer.cpp
 *      ZELDA_SUBSCREEN_HACK / ZH_WRITE
 *      FORCE_INTERP       main.cpp
 *      MAT_SKIP           saltarse un material (biseccion)
 */

#ifndef XENON64_DIAG_H
#define XENON64_DIAG_H

/* ★★★ XBOX64_TRAZAS -- EL MAESTRO DE LA VERSION PUBLICA.
 *
 * A 0 (version publica): NINGUNA traza, log de depuracion, volcado ni
 * contador de diagnostico se compila.  Ni el `sprintf` ni la llamada al log:
 * el preprocesador lo quita antes de que el compilador lo vea, y la CPU entera
 * se queda para la emulacion.  Las salidas comunes (DEBUG_print, print_gecko,
 * DebugOutputString, logf) quedan ademas VACIAS, como red de seguridad.
 *
 * A 1: vuelven todas las trazas tal y como se ha trabajado hasta ahora
 * ([GXD3D], [LISTA], [xbox64] frames=..., [DYNA]...).
 *
 * Regla para el codigo nuevo: todo lo que no sea emulacion -- un log, un
 * sprintf para un log, un contador o un cronometro que solo lee un log -- va
 * dentro de `#if XBOX64_TRAZAS` (o de `#if XBOX64_SONDAS` si es una sonda de
 * rendimiento cara).  Lo que alimenta a la emulacion (por ejemplo el coste
 * medido de la lista, que decide el plazo del DP) NO se gatea.
 *
 * Puede llegar por /D del compilador: /D XBOX64_TRAZAS=1 */
#ifndef XBOX64_TRAZAS
#define XBOX64_TRAZAS 0
#endif

#ifndef XENON64_DIAG
#define XENON64_DIAG XBOX64_TRAZAS
#endif

/* ★★ XBOX64_SONDAS -- EL MAESTRO DE LAS SONDAS DE RENDIMIENTO.
 *
 * A 0 desaparecen los contadores y los cronometros que miden el propio
 * emulador, y con ellos casi todas las lineas del log.  Sobreviven solo las
 * dos que SON la medida de rendimiento:
 *
 *     [xbox64] frames=N img=N ...
 *     [xbox64] reparto: emu=... entrega=... run=... de 16667 us/frame
 *
 * ★ Y NO ES SOLO VOLUMEN DE LOG.  Lo caro no son las lineas, es lo que se
 * mide para escribirlas: `GLN64_PROF_CNT(vtxn)` esta en `gSPProcessVertex`
 * (gSP.cpp:196), o sea UN incremento `volatile` POR VERTICE -- medio millon
 * por segundo en una escena normal --, y hay 34 ambitos mas con un par de
 * `__mftb()` cada uno.  Una sonda que cuesta lo que quieres medir no sirve
 * para medirlo.
 *
 * A 1 vuelve todo: `[GLN64T]` (reparto por funcion del plugin), `[GXSH]` (el
 * shim), `[RSPT]`, `[SOLAPE]` y compania.  Ese es el modo normal de trabajo;
 * el 0 es para una pasada de medida limpia. */
#ifndef XBOX64_SONDAS
#define XBOX64_SONDAS 0
#endif

/* Las sondas solo se leen en el log: encenderlas enciende las trazas. */
#if XBOX64_SONDAS && !XBOX64_TRAZAS
#undef XBOX64_TRAZAS
#define XBOX64_TRAZAS 1
#endif

/* PROFILE llega por /D del vcxproj (configuracion Release|Xbox 360).  Con el
 * maestro apagado se anula aqui, para no tener que tocar el proyecto: asi
 * start_section/end_section/prof_inc pasan a ser macros vacias en r4300.h y
 * desaparecen los dos mftb por acceso a memoria.
 *
 * Este fichero se incluye ANTES del `#ifdef PROFILE` tanto en r4300.h como en
 * profile.c, que son los dos sitios que lo prueban. */
#if !XENON64_DIAG && defined(PROFILE)
#undef PROFILE
#endif

#endif /* XENON64_DIAG_H */
