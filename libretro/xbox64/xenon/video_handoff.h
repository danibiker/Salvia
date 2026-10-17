/* -- Entrega de video del core al frontend Salvia ----------------------------
 *
 * EL PROBLEMA.  Xenon64 renderiza con Direct3D 9, no a un buffer de CPU como
 * todos los demas cores de Salvia.  Y en Xbox 360 no hay salida barata a CPU:
 *
 *   - StretchRect, UpdateSurface, UpdateTexture, GetRenderTargetData,
 *     ColorFill y CreateOffscreenPlainSurface NO EXISTEN.  La cabecera del XDK
 *     los lista explicitamente (d3d9.h:2063-2081).  De las copias de D3D9 de
 *     escritorio no queda ninguna.
 *   - La unica copia que ofrece la consola es Resolve(), que lee de la EDRAM.
 *   - Y Resolve() escribe SIEMPRE TILEADO, declare el destino D3DFMT_LIN_ o
 *     no.  Esta medido en este arbol (d3d9_backend.cpp:6096-6122).
 *   - Leer memoria de GPU por CPU va a ~20 MB/s: 61 ms por frame de 1,2 MB.
 *
 * LA SOLUCION.  No se copia nada: se le pasa al frontend LA TEXTURA.  Salvia
 * dibuja el juego como un triangulo a pantalla completa con la textura del
 * juego en el sampler s0, y todo lo que importa -relacion de aspecto, shaders
 * HLSL, escalado entero, rotacion, overlay del menu- vive AGUAS ABAJO de ese
 * enlace.  O sea que basta con que s0 apunte a nuestra textura.
 *
 * Y el tileado deja de ser un problema porque NADIE la lee por CPU: solo la
 * muestrea el shader del frontend, y el muestreo de una textura tileada es
 * exactamente igual de valido.
 *
 * EL DEVICE ES UNO SOLO.  Xbox 360 no admite dos IDirect3DDevice9 por proceso,
 * asi que el core NO crea el suyo: usa el del frontend (environment 30002).
 * Consecuencias que hay que gestionar aqui, y no son opcionales:
 *
 *   1. El device del frontend NO tiene depth-stencil: su
 *      EnableAutoDepthStencil esta comentado (SDL_xboxvideo.c:571-572).  El
 *      N64 necesita Z, asi que el core crea el suyo.
 *   2. Su back buffer es X8R8G8B8 a la resolucion del menu (1280x720 por
 *      defecto), no los 640x480 A8R8G8B8 que el core se creaba.  Asi que el
 *      core crea tambien su render target.
 *   3. El render target 0 del device tiene que volver a ser el back buffer del
 *      frontend ANTES de retornar de retro_run, porque el frontend hace
 *      Clear + quad + Present sin llamar nunca a SetRenderTarget: si le
 *      dejasemos el nuestro puesto, dibujaria dentro de nuestra textura.
 *
 * De ahi el par frame_begin/frame_end, que lo llama retro_run.
 *
 * EDRAM.  ★★ AQUI PONIA QUE COMPARTIR BASE CON EL BACK BUFFER DEL FRONTEND
 * ERA "INOCUO POR EL ORDEN".  NO LO ES: el frontend dibuja su quad, su menu y
 * su overlay entre dos retro_run, y lo hace encima de nuestro color y de
 * nuestra profundidad si compartimos tiles.  Con la profundidad corrompida la
 * geometria se rechaza a trozos que cambian entre frames -- tiras que
 * parpadean.  El sintoma que lo delato: el overlay de FPS del frontend
 * visible DENTRO de la imagen del juego.
 *
 * Las superficies del core van DETRAS del back buffer, con su tamano
 * consultado por GetDesc y no supuesto.  Ver xenon64_video_init en el .cpp.
 */

#ifndef XENON64_VIDEO_HANDOFF_H
#define XENON64_VIDEO_HANDOFF_H

#ifdef __cplusplus
extern "C" {
#endif

/* El device lo da el frontend antes de abrir la ROM.  Sin el, todo lo demas
 * es no-op y el core no dibuja (no revienta). */
void xenon64_video_set_device(void *pDevice);
void *xenon64_video_get_device(void);

/* Crea el render target, el depth-stencil y la textura de entrega, al tamano
 * de la imagen del N64.  Idempotente para el mismo tamano. */
int  xenon64_video_init(int width, int height);
void xenon64_video_shutdown(void);

/* Principio y fin de frame, desde retro_run.  begin() pone el render target y
 * el depth del core; end() devuelve el back buffer del frontend. */
void xenon64_video_frame_begin(void);
void xenon64_video_frame_end(void);

/* Re-liga el estado de D3D que Rice da por supuesto y que el frontend cambia
 * al dibujar su quad entre frames.  Vive en d3d9_backend.cpp, que es donde
 * estan sus globales; lo llama frame_begin(). */
void xenon64_core_state_apply(void);

/* EDRAM -> textura de entrega.  Lo llama UpdateFrame() donde antes estaba el
 * Present.  Puede correr en el hilo grafico. */
int  xenon64_video_resolve(void);

/* Para el frontend: la textura y su tamano. */
void        *xenon64_video_texture(void);
int          xenon64_video_width(void);
int          xenon64_video_height(void);

/* 1 si este frame hubo resolve (o sea, hay imagen nueva), Y LIMPIA el aviso.
 * Con VI_LIMIT_DL a 1 hay retrazos que no dibujan -Conker renderiza a 30 fps
 * con VI a 60 Hz-, y en esos retro_run tiene que decirle al frontend que
 * repita el frame en vez de entregar el anterior como si fuera nuevo. */
int          xenon64_video_take_frame_ready(void);

/* ★★ CONTABILIDAD DE ENTREGA, y es la sonda de los MICROCORTES.
 *
 *   resuelve  el ultimo numero de secuencia de resolve visto
 *   nuevas    tomas que se llevaron una imagen nueva
 *   repite    tomas que no encontraron nada (el frontend repite la anterior)
 *   perdidas  ★ imagenes PRODUCIDAS Y TIRADAS: dos resolves entre dos tomas
 *
 * `perdidas` es el numero que ningun otro contador ve.  El frame no se pasa
 * de presupuesto, `late` sale 0 y `vbl/frame` 1,0000, y aun asi falta una
 * imagen.  La nota larga esta junto a la definicion, en video_handoff.cpp. */
extern volatile unsigned int xenon64_vh_resuelve;
extern volatile unsigned int xenon64_vh_nuevas;
extern volatile unsigned int xenon64_vh_repite;
extern volatile unsigned int xenon64_vh_perdidas;

#ifdef __cplusplus
}
#endif

#endif /* XENON64_VIDEO_HANDOFF_H */
