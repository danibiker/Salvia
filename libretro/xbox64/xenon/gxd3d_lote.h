/*
 * gxd3d_lote.h  --  entrega de un LOTE ENTERO al backend D3D9.
 *
 * ★★ POR QUE EXISTE ESTE CAMINO AL LADO DEL MODO INMEDIATO DE GX.
 *
 * En GameCube/Wii el modo inmediato de GX NO es una API: es un puerto de
 * hardware.  `GX_Position3f32` son tres `stfs` a una direccion fija que el
 * write-gather pipe agrupa y rafaguea al GP; `GX_Begin` son tres bytes; un
 * cambio de estado son 1-4 tokens en el mismo flujo ordenado.  No existe la
 * "draw call" y no existe el "lote": doscientos lotes diminutos por frame
 * cuestan lo mismo que uno grande.
 *
 * Aqui cada atributo es una llamada cross-modulo al shim que toca estaticas
 * de fichero (`s_v`, `s_curAttr`) y las vuelve a leer acto seguido, que en el
 * PPE es un Load-Hit-Store detras de otro.  Y todo eso para copiar un vertice
 * que el plugin YA TIENE en un array contiguo antes de empezar:
 * `OGL_DrawTriangles` recorre `OGL.vertices[0..numVertices]`.
 *
 * MEDIDO (log 33, OoT en regimen estacionario): `drawtri` = 1.298 us/frame
 * con 5.981 lotes/s de 18,4 vertices = 13,0 us por lote, o 0,71 us por
 * vertice.  De esos 13,0, la nota de gx_d3d9.cpp:274 situa ~9,3 en la cola de
 * `GX_End` y deja ~4,5 en el bucle de atributos -- 92 llamadas por lote.
 * Y la referencia que funciona en esta misma consola dice que un
 * `DrawPrimitive` cuesta 0,74 us (d3d9_backend.cpp:1476), o sea que las draw
 * calls son el 5,7% del total: el problema NO son ellas.
 *
 * Asi que el plugin abre el lote, escribe los vertices de una pasada
 * directamente sobre el vertex buffer bloqueado, y lo cierra.
 *
 * ★ ES UN CAMBIO DE TRANSPORTE, NO DE DIBUJO.  `gxd3d_lote_abrir` reutiliza
 * `GX_Begin` tal cual (reserva, rotacion del anillo, contadores) y
 * `gxd3d_lote_cerrar` reutiliza `GX_End` tal cual (fusion, estado, draw).  No
 * se mueve ningun punto de vaciado, que es la clase de fallo que este shim ya
 * ha dado CUATRO veces ([[reference_gxd3d_estado_sin_vaciar_lote]]).
 *
 * ★ SOLO PARA `GX_TRIANGLES`.  Los quads (rects, VI, framebuffer) siguen por
 * el modo inmediato: alli el coste medido es ~0 (`rect=0 us/frame` en esta
 * escena) y es donde vive el reparto en dos triangulos.
 */

#ifndef _XENON_GXD3D_LOTE_H_
#define _XENON_GXD3D_LOTE_H_

/* ★★ INTERRUPTOR DEL A/B PARA RICE.  1 = camino de lote, 0 = el modo
 * inmediato de siempre (`OGLRender.cpp`, `RenderFlushTris`).
 *
 * Existe porque el cambio de transporte es justo la clase de cosa que puede
 * romper el dibujo sin romper el build, y entonces hace falta poder
 * descartarlo en un solo caracter en vez de razonar.  glN64 NO pasa por
 * aqui: el suyo es incondicional. */
#define RICE_LOTE 1

/* ★★ EL COLOR SE EMPAQUETA COMO `0xAABBGGRR`, IGUAL QUE RICE, Y NO COMO
 * "lo que sale de razonar sobre big-endian".
 *
 * La primera version usaba `(r<<24)|(g<<16)|(b<<8)|a` argumentando que
 * `D3DDECLTYPE_UBYTE4N` entrega los bytes en orden de MEMORIA hacia `.xyzw` y
 * que la consola es big-endian.  El argumento es plausible y **es exactamente
 * el que ya se probo y se descarto en el port de Rice**: alli es
 * `#define VERTEX_COLOR_RGBA 1` (d3d9_backend.cpp:224-229), esta MEDIDO EN
 * HARDWARE y el sintoma anotado es *"lo pone TODO rojo"*.  El que funciona es
 * el otro, y aqui se copia:
 *
 *     Rice, VERTEX_COLOR_RGBA 0 (el bueno):
 *         MAKE_COLOR4(r,g,b,a)  ->  (a<<24)|(b<<16)|(g<<8)|r
 *
 * Volvi a caer en lo mismo con glN64: pantalla de inicio y cara de Mario "muy
 * rojas" mas G y B cruzados en todo lo demas.  De una nota que dice "esta
 * hipotesis se PROBO y era falsa" hay que quedarse con la CONCLUSION MEDIDA,
 * no con el razonamiento que la acompanaba
 * ([[feedback_referencia_que_funciona]]).
 *
 * ★ NI `w` NI `s2/t2`: 44 -> 32 BYTES.
 *
 *   - `w` valia SIEMPRE 1.0f (lo ponia `GX_Position3f32`) y el vertex shader
 *     hace `mul(In.pos, uMV)`.  Con la declaracion en `D3DDECLTYPE_FLOAT3`
 *     D3D expande a (x,y,z,1): el resultado es identico y sobran 4 bytes.
 *     ★ Y no es un supuesto sobre el XDK: la combinacion exacta -- POSITION
 *     declarado `D3DDECLTYPE_FLOAT3` con un vertex shader que lo recibe como
 *     `float4` -- esta EN MARCHA Y FUNCIONANDO en esta consola en
 *     `libretro/pcsxr-360/plugins/xbox_soft/xb_video.cpp:93` (y su VS, :19).
 *     [[feedback_referencia_que_funciona]]
 *   - `s2/t2` valia SIEMPRE (0,0).  Los TRES sitios del plugin que declaran
 *     un tercer juego de coordenadas lo emiten como literal cero:
 *     OGL_DrawTriangles (OpenGL.cpp:1479), OGL_DrawTexturedRect (:2058, :2071,
 *     :2076, :2089) y ZSortBOSS (:565).  El pixel shader lo sigue recibiendo
 *     -- el vertex shader escribe `o.uv2 = float2(0,0)` -- asi que el
 *     generador de HLSL no se toca.
 *
 * 32 bytes ademas entran exactos cuatro por linea de cache de 128. */
struct GXD3DVertex
{
    float x, y, z;
    float s0, t0;
    float s1, t1;
    unsigned int color;   /* DWORD 0xAABBGGRR, como el TVertex de Rice */
    /* ★★ COEFICIENTE DE NIEBLA (0..255), CALCULADO POR VERTICE EN LA CPU.
     *
     * Es la formula literal del glN64 original (OpenGL.cpp:1309):
     *     fog = max(0, z/w * fog.multiplier + fog.offset)
     * con la `z` y la `w` DEL PLUGIN.  glN64_GX se salta ese bloque porque
     * delega en `GX_SetFog` y el shim de D3D9 no tiene ese hardware.
     *
     * ★ Y NO se puede mover al vertex shader: en el camino `combW` el lote
     * escribe `z = -w` y la z real se pierde, asi que la GPU no puede
     * reconstruirla.  Se probo y dejo a OoT sin niebla mientras 007, que va
     * por el otro camino, salia bien. */
    float fog;
};

#define GXD3D_PACK_RGBA(r,g,b,a) \
    ( ((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | \
      ((unsigned int)(g) <<  8) |  (unsigned int)(r) )

#ifdef __cplusplus
extern "C" {
#endif

/* Abre un lote de `nverts` triangulos sueltos y devuelve DONDE escribirlos.
 * Devuelve NULL si el lote no cabe ni en un vertex buffer entero (se cuenta
 * en `gxd3d_dbg_vbdrop`); en ese caso NO hay que llamar a `cerrar`.
 *
 * Entre `abrir` y `cerrar` no se puede tocar NADA del estado de dibujo: hay un
 * lote a medias, y eso es justo lo que rompe este shim. */
struct GXD3DVertex *gxd3d_lote_abrir(int nverts);

/* Cierra el lote.  `nverts` tiene que ser el MISMO que se paso a `abrir`. */
void gxd3d_lote_cerrar(int nverts);

#ifdef __cplusplus
}
#endif

#endif /* _XENON_GXD3D_LOTE_H_ */
