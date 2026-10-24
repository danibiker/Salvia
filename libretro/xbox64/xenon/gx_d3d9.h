/*
 * gx_d3d9.h  --  interfaz publica del backend GX -> D3D9.
 *
 * Lo que el resto del core (gln64_glue.cpp, gfx_thread.cpp, libretro_core.cpp)
 * necesita del backend.  La API GX en si la declara xenon/gxshim/gccore.h.
 */

#ifndef _XENON_GX_D3D9_H_
#define _XENON_GX_D3D9_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Ciclo de vida.  gxd3d_init() necesita que `g_pd3dDevice` ya apunte al
 * device del frontend (env 30002), o sea que va DESPUES de
 * xenon64_video_set_device(). */
int  gxd3d_init(void);
void gxd3d_shutdown(void);

/* Fronteras de frame.  gxd3d_frame_begin() invalida el espejo de estado y
 * bloquea el siguiente vertex buffer del anillo.
 *
 * ★ Invalidar el espejo UNA VEZ POR DISPLAY LIST no es opcional: el device
 * esta compartido y el frontend pone en el exactamente los estados que
 * cacheamos (ALPHABLENDENABLE, SRCBLEND, DESTBLEND, CULLMODE, ZENABLE,
 * SCISSORTESTENABLE) al dibujar su quad y su menu. */
void gxd3d_frame_begin(void);
void gxd3d_frame_end(void);

/* ★★ [etapa 3] LAS FRONTERAS DEL CONSUMIDOR, que no son las de arriba.
 *
 * `gxd3d_frame_begin/end` son del PRODUCTOR (el shim que graba).  Estas dos
 * son del lado que EJECUTA, o sea `retro_run`: bloquean y sueltan el vertex
 * buffer de verdad, que desde la etapa 3 es del consumidor (ver la nota de
 * `s_staging` en gx_d3d9.cpp).  Van alrededor de la pasada:
 *
 *   xenon64_video_frame_begin()   render target del core
 *   gxd3d_consumidor_empieza()    rota y bloquea el vertex buffer
 *   d3dq_bind()                   reaplica la sombra de estado
 *   ...se graba y se vacia...
 *   gxd3d_consumidor_acaba()      suelta el vertex buffer
 *   xenon64_video_frame_end()     back buffer del frontend */
void gxd3d_consumidor_empieza(void);
void gxd3d_consumidor_acaba(void);

/* Caduca el espejo del estado de render sin tocar el vertex buffer. */
void gxd3d_state_invalidate(void);

/* El write gather pipe redirigido (VI.cpp lo usa para escribir el XFB). */
void *gxd3d_wgp(void);

/* ★★ EL INFORME DEL SHIM, QUE LLEVABA TODO EL PORT SIN IMPRIMIRSE.
 *
 * `gxsh_lote_*`, `gxsh_draw_tb`, el corte en cuatro de la cola de `GX_End`
 * (`bind/mtx/tev/prim`), `gxsh_merged` y los `gxd3d_dbg_*` se vienen
 * acumulando desde el principio y NO LOS LEIA NADIE: la parte que los
 * volcaba vivia en la maquinaria de diagnostico de Xenon64 y no se porto
 * con los shims.  Exactamente el mismo caso que `gln64_prof_informe`.
 *
 * Lo llama el resumen por segundo del wrapper, justo detras de aquel, y
 * reinicia los contadores.  `frames` = cuantos frames cubre la ventana;
 * todo se imprime POR FRAME para poder compararlo con los 16.667 us. */
void gxd3d_informe(unsigned frames);

/* ★ El coste de DESTILAR texturas (formato GX -> lineal), expuesto aparte.
 *
 * `conv_tb` son ticks acumulados y `texconv` cuantas texturas se convirtieron.
 * Estan aqui, en la cabecera PUBLICA, porque el resumen por segundo del
 * wrapper los imprime aunque las sondas esten apagadas: en regimen valen 0, y
 * si se disparan en un bajon aislado, los picos de `emu max` son la
 * destilacion -- que es el unico candidato serio de "esto deberia hacerlo la
 * GPU".  Medirlo tiene que ser posible SIN encender las sondas caras, porque
 * esas cambian justo lo que se quiere medir.
 *
 * Los reinicia el wrapper en su bloque general de fin de segundo, no aqui:
 * con las sondas encendidas `gxd3d_informe` tambien los lee. */
extern volatile unsigned int gxsh_conv_tb;
/* Tambien esta en `gx_d3d9_int.h`, con el resto de contadores del shim.  Dos
 * declaraciones IDENTICAS del mismo objeto son legales; se repite aqui porque
 * el wrapper no incluye la cabecera interna. */
extern volatile unsigned int gxd3d_dbg_texconv;

/* [Banjo-Kazooie] 1 = esta partida usa la copia del color SINCRONA
 * (GXD3D_CopiaColorARDRAMSincrona): el emulador no debe ir frames por delante
 * de lo presentado (xbox/hilo_emu.cpp, el ritmo).  0 siempre con el
 * interruptor GXD3D_CCOPIA_SINC a 0. */
int gxd3d_ccopia_sinc_activa(void);

#ifdef __cplusplus
}
#endif

/* `gxd3d_tex_release` YA NO EXISTE, y no es un olvido: las texturas de D3D
 * viven en un registro propio con LRU dentro de gx_d3d9.cpp, no colgando del
 * GXTexObj.  El motivo esta explicado encima de GX_InitTexObj: esa struct
 * nace sin inicializar y el plugin destruye texturas sin avisar, asi que no
 * habia donde engancharse. */

#endif /* _XENON_GX_D3D9_H_ */
