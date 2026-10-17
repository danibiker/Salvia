/*
 * xenon/perfil_cap.h  --  perfilador por funcion sobre /callcap del XDK.
 *
 * ★★★ POR QUE ESTO Y NO UN MUESTREADOR DE PC, QUE ERA EL PLAN.
 * -------------------------------------------------------------
 * La idea original era un hilo en un nucleo libre que suspendiera al hilo
 * grafico 1.000 veces por segundo, le leyera el PC y montara un histograma:
 * eso es lo que hace PIX en su modo CPU y no exige elegir donde mirar.
 *
 * NO SE PUEDE.  Censado en las cabeceras del XDK
 * (C:\develop\proyectos\personal\xbox):
 *
 *   SuspendThread / ResumeThread / DuplicateHandle -> winbase.h, existen
 *   CONTEXT.Iar (el registro de direccion de instruccion) -> winnt.h:1427
 *   GetThreadContext -> SOLO en xbdm.h y xbcrash.h
 *
 * O sea que la estructura esta y el registro esta, pero la funcion que lo
 * lee vive en el monitor de depuracion, que es devkit.  Sin ella no hay
 * muestreo no cooperativo, y punto.
 *
 * Lo mismo pasa con el resto de la caja de herramientas "profesional", y se
 * comprobo antes de escribir una linea:
 *
 *   pix.h            solo ANOTACIONES (PIXBeginNamedEvent, PIXSetMarker,
 *                    PIXNameThread) y `PIXGetCaptureState`.  No hay ninguna
 *                    funcion que ARRANQUE una captura: eso lo hace PIX desde
 *                    el PC por xbdm.
 *   tracerecording.h `XTraceStartRecording`, y su propio comentario dice
 *                    "native (on the devkit) trace recording".
 *   pmcpb.h          solo structs del pass buffer, ninguna API invocable.
 *
 * Y el despliegue de este proyecto es `RemoteRoot: usb0:\...`, o sea copia a
 * USB: no hay enlace xbdm ni lo va a haber.
 *
 * ★ LO QUE SI QUEDA, Y ES MEJOR QUE EL MUESTREO.  `/callcap` es una opcion
 * del COMPILADOR, no de las cabeceras: hace que emita una llamada a
 * `_CAP_Start_Profiling` al entrar en cada funcion y a `_CAP_End_Profiling`
 * al salir, pasando la direccion de la funcion.  Esos dos ganchos los
 * definimos nosotros -- por eso no aparecen en ninguna cabecera del XDK.
 *
 * Ventajas sobre lo que llevo haciendo a mano:
 *   - NO HAY QUE ELEGIR QUE MEDIR.  Instrumenta todas las funciones de los
 *     ficheros a los que se le ponga la opcion.  Es la diferencia entre
 *     "cronometro cinco sitios que me parecen sospechosos" y "que me lo diga
 *     el binario".
 *   - No se toca ni una linea de source/Rice_GX/, que es copia verbatim.
 *   - Da tiempo PROPIO y tiempo TOTAL por funcion, mas el numero de
 *     llamadas, que es lo que distingue "una funcion cara" de "una funcion
 *     barata llamada un millon de veces".
 *
 * ★★ LOS GANCHOS SE LLAMAN `__CAP_Enter_Function` Y `__CAP_Exit_Function`,
 * Y ESTA MEDIDO.  El primer intento uso `_CAP_Start_Profiling` y el
 * enlazador dio los nombres buenos en el LNK2001.  Son DOS subrayados de
 * verdad: el `.map` lista `perfil_cap_init` sin adorno, o sea que este
 * toolchain no antepone subrayado a los `extern "C"`.
 *
 * ★★★ Y HAY QUE QUITAR `xbdm.lib` DEL ENLACE.  Enlazarlo tambien resuelve
 * esos dos simbolos -- el `.map` los da como
 * `__CAP_Enter_Function 8271e304 xbdm:xbdm.xex` -- pero con los ganchos de
 * Microsoft, que alimentan la captura de PIX y no sirven sin devkit.  El
 * sintoma de esa mezcla ya se vio: `[CAP] fn=0 propio=0us` con `hilo`
 * distinto de cero, o sea el hilo registrado y ni una funcion dentro.
 * Con los ganchos definidos en perfil_cap.cpp, dejar `xbdm.lib` puesto da
 * LNK2005 por duplicado.
 *
 * ★ La FIRMA no se usa: la clave sale de `_ReturnAddress()`, que apunta
 * dentro de la funcion instrumentada y es constante para cada una.  Asi no
 * hay que adivinar cuantos argumentos pasa el compilador -- en PowerPC van
 * en registros y declarar menos de los que se pasan es inofensivo.  Al
 * resolver contra el `.map`, quedarse con el simbolo ANTERIOR mas cercano.
 *
 * ★★ SOLO SE PERFILA EL HILO GRAFICO.  Los ganchos saltan en el hilo que
 * ejecute el codigo, y la pila en sombra que hace falta para separar tiempo
 * propio de tiempo total no es reentrante.  Filtrar por id de hilo (mismo
 * criterio que `gfx_hilo_soy_yo`) la deja con un solo escritor y ademas
 * quita del medio la primera lista, que va sincrona por diseno.
 *
 * ★ COMO SE LEEN LAS DIRECCIONES.  El informe saca direcciones, no nombres:
 * en la consola no hay tabla de simbolos.  Se resuelven EN FRIO contra el
 * `.map` del enlazador -- hay que activar "Generate Map File" en el proyecto
 * Salvia.  El desplazamiento entre el .map y la direccion de ejecucion se
 * saca con la linea `[CAP] base` del informe, que publica la direccion de
 * una funcion conocida.
 *
 * ASCII, LF, valido en C.
 */

#ifndef _XBOX64_PERFIL_CAP_H_
#define _XBOX64_PERFIL_CAP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ★★★ A 0, Y NO ES PROVISIONAL: ESTE CAMINO NO PUEDE FUNCIONAR EN C.
 *
 * MEDIDO el 2026-09-29, con una violacion de acceso:
 *
 *     icbi r3, r4  ->  Access violation reading location 0x00000080
 *     xenon_icbi_one(offset=0, base=0x00000080)      xenon_cache.h:55
 *     xenon_icache_invalidate(startaddr=0x00000000, len=2223908992)
 *     xenon_icache_invalidate(startaddr=0x848e2890, len=16)   <- los BUENOS
 *     ICInvalidateRange / dyna_get_stub / recompile_block
 *
 * El marco de fuera lleva los argumentos correctos y el de dentro basura.
 * Con `startaddr = 0` el bucle de `xenon_icache_invalidate` hace `p = 0` y
 * luego `p = 128`, o sea `0x80`: falla en la segunda vuelta, exactamente en
 * la direccion del fallo.  Los argumentos se destruyeron ENTRE la llamada y
 * el cuerpo.
 *
 * ★ LA CAUSA: el compilador inserta `bl __CAP_Enter_Function` en la ENTRADA
 * de la funcion, cuando sus argumentos todavia viven en r3-r12.  Un gancho
 * escrito en C los machaca sin enterarse -- llama a `GetCurrentThreadId`,
 * usa temporales, y r3-r12 son volatiles por ABI.  Los ganchos de Microsoft
 * que hay en xbdm estan en ensamblador justo por esto.
 *
 * (Lo destapo quitar la optimizacion: `xenon_icache_invalidate` es
 * `static __inline` y con /O2 se inlineaba, asi que no era instrumentable.
 * Con /Od pasa a ser una llamada de verdad.  El defecto estaba desde el
 * primer momento en todo lo que /callcap tocara.)
 *
 * ★★ Y NO BASTA CON ESCRIBIR EL GANCHO EN ENSAMBLADOR "Y YA".  Para ser
 * correcto tendria que salvar y restaurar r3-r12, LR, CR **y f1-f13**: son
 * volatiles igual, y el camino caliente de Rice pasa flotantes en cada
 * vertice.  Eso son ~25 salvados por entrada y otros tantos por salida, en
 * funciones que se llaman millones de veces por segundo.  El instrumento
 * costaria mas que lo que intenta medir, que es justo lo que descalifica la
 * medida.
 *
 * ★ Con esto a 0 los dos ganchos siguen DEFINIDOS pero vacios: asi el
 * enlace funciona aunque quede `/callcap` puesto en algun sitio, y una
 * funcion C vacia no toca ningun registro.  Aun asi, quitalo del vcxproj:
 * instrumentar para no medir nada solo cuesta tiempo. */
#define XBOX64_PERFIL_CAP 0

/* Desde `retro_init`.  Idempotente. */
void perfil_cap_init(void);

/* La llama el HILO GRAFICO nada mas arrancar, para registrar su id: es el
 * unico hilo cuyas llamadas se contabilizan.  Ver la nota de la cabecera. */
void perfil_cap_marcar_hilo(void);

/* Una vez por segundo, desde `resumen_por_segundo`.  Solo saca el informe
 * cada `PERFIL_CAP_SEGUNDOS`; el resto de las veces vuelve enseguida. */
void perfil_cap_informe(void);

#ifdef __cplusplus
}
#endif

#endif /* _XBOX64_PERFIL_CAP_H_ */
