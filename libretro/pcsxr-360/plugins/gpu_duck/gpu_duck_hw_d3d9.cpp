/*
 * gpu_duck_hw_d3d9.cpp  --  backend hardware Direct3D 9 de SwanStation.
 *
 * [2026-10-30] Fase 1: Windows, x1, sin filtros.  Fase 2: Xbox 360 (las ramas
 * `#if defined(_XBOX)`: EDRAM, Resolve, buffers de vertices; resumen abajo).
 * Ver HW_D3D9_FASE0.txt: alli estan el porque de cada decision y la
 * correspondencia con gpu_hw.cpp / gpu_hw_d3d11.cpp / gpu_hw_shadergen.cpp
 * de SwanStation (backups\swanstation\src\core).
 *
 * Resumen del modelo:
 *   - La VRAM de la PSX es un render target A8R8G8B8 de 1024x512.  Cada
 *     canal guarda el valor de 5 bits como c/31; el alfa es el bit de
 *     mascara.  Un texel de 16 bits se reconstruye EXACTO en float.
 *   - Las lotes muestrean una COPIA (m_read_tex), refrescada desde el
 *     render target cuando la pagina de textura o la CLUT tocan algo
 *     dibujado desde la ultima copia (como upstream).
 *   - El bit de mascara vive tambien en la profundidad (D24S8): cada pixel
 *     escribe mascara * z_del_primitivo; comprobar mascara = GREATEREQUAL.
 *   - Sin dual-source: el factor de destino es constante por lote
 *     (D3DRS_BLENDFACTOR) y el origen va premultiplicado en la shader;
 *     texturizado + transparencia = dos pasadas.
 *   - psxVuw solo se sincroniza cuando alguien lo necesita: LoadImage
 *     (subida), StoreImage (bajada), savestate (todo).
 *
 * Xbox 360 (fase 2), lo que cambia:
 *   - La VRAM es una superficie de la EDRAM (CreateRenderTarget), no una
 *     textura: no se puede muestrear.  La copia de lectura (m_read_tex) se
 *     llena con Resolve y es LA VERDAD entre llamadas: al desligar se
 *     resuelve lo sucio, y al ligar se repinta la EDRAM desde ella (color y
 *     profundidad desde el bit de mascara).  Asi da igual lo que el frontend
 *     haga con la EDRAM entre dos retro_run.
 *   - Colocacion: detras del back buffer del frontend si cabe (como xbox64,
 *     libretro/xbox64/xenon/video_handoff.cpp); si no (1080p), en la base 0,
 *     que con el repintado al ligar tambien es correcto.
 *   - La salida se pinta en otra superficie de la EDRAM y se resuelve a la
 *     textura que ve Salvia.
 *   - psxVuw es little-endian: se da la vuelta a cada palabra al subir y al
 *     bajar.  Texturas escritas por CPU: LIN_ (las tileadas se escriben en
 *     crudo).  Leer la VRAM a CPU: Resolve -> D3DXLoadSurfaceFromSurface a
 *     una LIN_ (Resolve escribe siempre tileado), como la captura del
 *     frontend (SDL_XBOX_CaptureGameTexture).
 *   - Los lotes van por buffers de vertices en anillo, bloqueados mientras
 *     se llenan (xbox64, gxd3d_consumidor_rota_vb); DrawPrimitiveUP solo
 *     para los rectangulos de 4 vertices.
 *   - Estados que la consola no tiene (fog, luces, sRGB...) fuera; se pone
 *     VIEWPORTENABLE (el frontend lo deja a FALSE) y se apaga la Hi-Z
 *     (la profundidad la escribe la shader).
 *   - Sin BeginScene/EndScene ni state block: el frontend rehace su
 *     pipeline en cada frame (XBOX_SetupQuadState); se le devuelve lo que no
 *     rehace (mezcla, Z, samplers).
 */

#include "gpu_duck_hw_d3d9.h"
#include "gpu_duck_c_api.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

/* El driver nos da su backend y el estado de mascara vigente
 * (gpu_duck_driver.cpp). */
GPUBackend* duck_get_backend_for_hw();
GPUBackendCommandParameters duck_get_current_params_for_hw();

/* Opcion libretro pcsxr360_dithering (libretro_core.cpp).  El backend
 * software la combina con el bit del juego (gpu_duck_sw_backend.cpp,
 * DrawPolygon/DrawLine); aqui igual.  Sin esto el dithering salia aunque la
 * opcion estuviera apagada. */
extern "C" int g_pcsxr_dithering;

/* [fase 3] Opciones del renderizador HW (libretro_core.cpp, en caliente):
 *   pcsxr360_true_color         -> g_duck_hw_true_color (0/1)
 *   pcsxr360_hw_texture_filter  -> g_duck_hw_texture_filter (0..3)
 * Se aplican al empezar el frame siguiente (ApplyRenderOptions). */
extern "C" int g_duck_hw_true_color = 1;
extern "C" int g_duck_hw_texture_filter = 0;

/* [PGXP] libpcsxcore/pgxp.c: motor activo (limitar u/v al poligono). */
extern "C" int g_pgxp_enabled;

template <typename T> static inline T hw_min(T a, T b) { return (a < b) ? a : b; }
template <typename T> static inline T hw_max(T a, T b) { return (a > b) ? a : b; }

/* ======================================================================
 * Registro (lo pone libretro_core.cpp)
 * ==================================================================== */
static void (*s_hw_log)(int level, const char* msg) = 0;

/* [2026-10-30] El registro de cores de Salvia (retro_log_printf, salvia.h)
 * corta cada mensaje a 127 caracteres y en la Xbox lo pasa a
 * OutputDebugStringA, que vuelve a interpretar los '%' como formato.  Un error
 * del compilador de shaders (varias lineas, largo) se perdia entero: en el
 * log de la consola solo quedaban dos lineas vacias.  Asi que aqui: sin '%',
 * los saltos de linea interiores pasan a " | ", y en trozos de 100. */
static void hw_log(int level, const char* fmt, ...)
{
  char buf[1024];
  char part[128];
  size_t n, i, o;
  va_list ap;
  va_start(ap, fmt);
  _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = 0;
  if (!s_hw_log)
    return;

  /* limpiar: '%' fuera, \r fuera, \n interiores -> " | " */
  {
    char clean[1024];
    o = 0;
    n = std::strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
      n--;
    for (i = 0; i < n && o + 4 < sizeof(clean); i++)
    {
      const char c = buf[i];
      if (c == '%')
        clean[o++] = '#';
      else if (c == '\r')
        continue;
      else if (c == '\n')
      {
        clean[o++] = ' '; clean[o++] = '|'; clean[o++] = ' ';
      }
      else
        clean[o++] = c;
    }
    clean[o] = 0;
    std::memcpy(buf, clean, o + 1);
  }

  /* trozos de 100 caracteres; los siguientes con "[DUCK-HW] +" delante */
  n = std::strlen(buf);
  i = 0;
  do
  {
    const size_t chunk = (n - i > 100) ? 100 : (n - i);
    if (i == 0)
      _snprintf(part, sizeof(part) - 1, "%.*s\n", (int)chunk, buf);
    else
      _snprintf(part, sizeof(part) - 1, "[DUCK-HW] +%.*s\n", (int)chunk, buf + i);
    part[sizeof(part) - 1] = 0;
    s_hw_log(level, part);
    i += chunk;
  } while (i < n);
}

#if DUCK_HW_D3D9

#include <d3dx9.h>
#if defined(_XBOX)
#include <xgraphics.h>   /* XGSurfaceSize */
#include <stdlib.h>      /* _byteswap_ushort */
#endif

/* Texturas que se escriben por CPU.  PC: DYNAMIC + DISCARD (LockRect sobre
 * DEFAULT sin DYNAMIC falla).  Xbox: LIN_ y sin banderas (una tileada se
 * escribe en crudo y sale en bloques; ver xbox64, gx_d3d9.cpp). */
/* [prueba MGS] 1 = no repetir estados que no cambian entre lotes
 * (m_pipe_valid / m_cur_*).  A 0 se ponen TODOS en cada lote, como antes de
 * la cache: si con 0 desaparecen la sombra en cuadrado y el vaho oscuro de
 * MGS, la cache se desincroniza en algun sitio. */
#ifndef DUCK_HW_STATE_CACHE
#define DUCK_HW_STATE_CACHE 1
#endif

/* [MGS / FIFA 99] "Sin valor" de la cache del BLENDFACTOR.  Era 0xFFFFFFFF,
 * que es JUSTO el factor de B+F, B-F y B+F/4 (ARGB 255,255,255,255): tras
 * invalidar la cache, esos modos creian que ya estaba puesto y se quedaba el
 * 0x80808080 del ultimo lote B/2+F/2.  De ahi la sombra en cuadrado (B-F con
 * el destino a la mitad), el vaho oscuro y el parpadeo segun el orden de los
 * lotes.  Los factores validos son 0x80808080 y 0xFFFFFFFF: 0 no choca. */
#define HW_BF_NONE 0u

#if defined(_XBOX)
#define HW_USAGE_CPU     0
#define HW_LOCK_CPU      0
#define HW_FMT_CPU_ARGB  D3DFMT_LIN_A8R8G8B8
#define HW_FMT_CPU_1555  D3DFMT_LIN_A1R5G5B5
#define HW_USAGE_OUT     0                       /* destino de Resolve */
#else
#define HW_USAGE_CPU     D3DUSAGE_DYNAMIC
#define HW_LOCK_CPU      D3DLOCK_DISCARD
#define HW_FMT_CPU_ARGB  D3DFMT_A8R8G8B8
#define HW_FMT_CPU_1555  D3DFMT_A1R5G5B5
#define HW_USAGE_OUT     D3DUSAGE_RENDERTARGET   /* se pinta directamente */
#endif

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = 0; } } while (0)

/* psxVuw guarda las palabras como la PSX: little-endian.  En la Xbox hay que
 * darles la vuelta al pasar a/desde una textura de 16 bits. */
#if defined(_XBOX)
static inline u16 hw_le16(u16 v) { return _byteswap_ushort(v); }
/* La salida mas grande de la PSX es 640x480 (entrelazado); la superficie de
 * salida de la EDRAM se crea de este tamano y se usa la esquina. */
enum { HW_OUT_MAX_W = 640, HW_OUT_MAX_H = 512 };
#else
static inline u16 hw_le16(u16 v) { return v; }
#endif

/* El desplazamiento de rasterizacion.  Upstream (D3D11) suma 0,5 al vertice
 * para que caiga en el centro del pixel; D3D9 tiene los centros en enteros,
 * asi que hay que restar 0,5.  Neto: 0 para las lotes, -0,5 para los
 * rectangulos de utilidad (que van de borde a borde). */
static const float HALF_PIXEL = -0.5f;

/* [traza] 1 = diagnostico pesado: lecturas de la VRAM a la CPU en unos pocos
 * frames (paran la GPU: tirones), mapa de la VRAM y los primeros
 * poligonos/rellenos/subidas.  Solo para depurar. */
#ifndef DUCK_HW_DIAG
#define DUCK_HW_DIAG 0
#endif

/* [traza] 1 = medidor de tiempos por frame ([DUCK-HW/PERF], cada 120 frames,
 * con los contadores de LogStats).  APAGADO por defecto desde 2026-10-31: mete
 * dos QueryPerformanceCounter por primitiva ("prims") y el log crece ~8 lineas
 * cada 2 segundos.  Encenderlo para medir.  Para saber DONDE se va el frame:
 *   run      retro_run entero (emulacion + todo lo nuestro)
 *   gap      fuera de retro_run: el frontend (su quad, Present, vsync)
 *   flush    codificar los lotes (FlushRender, incluye vblock)
 *   vblock   esperar un vertex buffer libre (Lock del anillo)
 *   upload   subidas enteras (uplock = esperar la textura del anillo)
 *   resolve  llamadas a Resolve / StretchRect
 *   present  la pasada de salida entera
 *   restore  repintar la EDRAM al ligar (Xbox)
 *   readback leer la VRAM a CPU (incluye esperar a la GPU)
 *   idle     BlockUntilIdle (Xbox) */
#ifndef DUCK_HW_PERF
#define DUCK_HW_PERF 0
#endif

/* [traza] 1 = contadores cada 600 frames ([DUCK-HW] N fr / cortes / prims)
 * aunque DUCK_HW_PERF este apagado.  Con DUCK_HW_PERF salen siempre. */
#ifndef DUCK_HW_STATS
#define DUCK_HW_STATS 0
#endif


static inline u64 hw_now()
{
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return static_cast<u64>(t.QuadPart);
}

#if DUCK_HW_PERF
/* [traza] Tiempo de CPU dentro de DrawPolygon/Rectangle/Line ("prims" en el
 * log), CON los vaciados de lote que caigan dentro (tambien en "flush"). */
struct HwPfScope
{
  u64& acc;
  const u64 t0;
  explicit HwPfScope(u64& a) : acc(a), t0(hw_now()) {}
  ~HwPfScope() { acc += hw_now() - t0; }
private:
  HwPfScope& operator=(const HwPfScope&);
};
#define HW_PF_SCOPE(idx) HwPfScope hw_pf_scope_(m_pf[idx])
#define HW_PF_BEGIN(name) const u64 name##_t0 = hw_now()
#define HW_PF_END(name, idx) (m_pf[idx] += hw_now() - name##_t0)
#else
#define HW_PF_BEGIN(name) ((void)0)
#define HW_PF_SCOPE(idx) ((void)0)
#define HW_PF_END(name, idx) ((void)0)
#endif

/* ======================================================================
 * Shaders (HLSL, Shader Model 3).  Todo en float: ver FASE0, punto 1.
 * ==================================================================== */

static const char* s_vs_batch_src =
  "float4 c_vs : register(c0);\n"   /* x = 2/W, y = 2/H, z = desplazamiento */
  "struct VSI { float4 pos : POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; float4 tp : TEXCOORD1;\n"
  "             float4 lim : TEXCOORD2; float mode : TEXCOORD3; };\n"
  "struct VSO { float4 pos : POSITION; float4 col : TEXCOORD2; float3 uvz : TEXCOORD0; float4 tp : TEXCOORD1;\n"
  "             float4 lim : TEXCOORD3; float mode : TEXCOORD4; };\n"
  "VSO main(VSI i)\n"
  "{\n"
  "  VSO o;\n"
  "  float x = i.pos.x + c_vs.z;\n"
  "  float y = i.pos.y + c_vs.z;\n"
  /* [PGXP] w por vertice: la GPU interpola u/v con perspectiva.  El color se
   * manda multiplicado por w con la w al lado y la PS divide: asi sale lineal
   * en pantalla como en la PSX (upstream: color sin perspectiva por defecto).
   * Con w = 1 todo queda igual que antes. */
  "  float w = i.pos.w;\n"
  "  o.pos = float4((x * c_vs.x - 1.0) * w, (1.0 - y * c_vs.y) * w, i.pos.z * w, w);\n"
  "  o.col = float4(i.col.rgb * w, w);\n"
  "  o.uvz = float3(i.uv, i.pos.z);\n"
  "  o.tp  = i.tp;\n"
  "  o.lim = i.lim;\n"
  "  o.mode = i.mode;\n"
  "  return o;\n"
  "}\n";

static const char* s_vs_util_src =
  "float4 c_vs : register(c0);\n"
  "struct VSI { float4 pos : POSITION; };\n"
  "struct VSO { float4 pos : POSITION; };\n"
  "VSO main(VSI i)\n"
  "{\n"
  "  VSO o;\n"
  "  float x = i.pos.x + c_vs.z;\n"
  "  float y = i.pos.y + c_vs.z;\n"
  "  o.pos = float4(x * c_vs.x - 1.0, 1.0 - y * c_vs.y, i.pos.z, 1.0);\n"
  "  return o;\n"
  "}\n";

/* Funciones comunes de los pixel shaders.  idiv/imod: division entera de
 * valores enteros no negativos en float, con el medio paso que evita el
 * 15,99999 de una division inexacta. */
#define PS_COMMON \
  "static const float2 INV_VRAM = float2(1.0 / 1024.0, 1.0 / 512.0);\n" \
  "float idiv(float a, float d) { return floor(a / d + 0.5 / d); }\n" \
  "float imod(float a, float d) { return a - idiv(a, d) * d; }\n" \
  "float dec16(float4 t)\n" \
  "{\n" \
  "  float4 q = floor(t * float4(31.0, 31.0, 31.0, 1.0) + 0.5);\n" \
  "  return q.r + q.g * 32.0 + q.b * 1024.0 + q.a * 32768.0;\n" \
  "}\n" \
  "float2 pixpos(float2 vpos) { return floor(vpos + 0.25); }\n" \
  /* roundEven de upstream (FloatToIntegerCoords a x1): al par en el empate. \
   * floor(x + 0.5) subia SIEMPRE en .5, y en sprites escalados (u avanza \
   * medio texel por pixel) cogia el texel de al lado en columnas alternas. */ \
  "float2 roundeven2(float2 x)\n" \
  "{\n" \
  "  float2 r = floor(x + 0.5);\n" \
  "  float2 tie = step(abs(r - x - 0.5), 0.0001);\n" \
  "  float2 odd = r - 2.0 * floor(r * 0.5);\n" \
  "  return r - tie * odd;\n" \
  "}\n"

/* [Xbox] Sin `discard`: el compilador de microcodigo de Xenos lo rechaza
 * ("error X3500: Microcode Compiler not yet implemented: 'discard'
 * statement").  Se marca `descarta` y se hace un unico clip() al final, que es
 * texkill y vale igual en PC.
 *
 * [Fase 3] Variantes por macro, ademas de TEXMODE y RAW:
 *   TRUECOLOR  0 = 5 bits por canal como la PSX; 1 = 8 bits (sin recorte, sin
 *              dithering), gpu_hw_shadergen.cpp u_true_color.
 *   FILTER     0 = nearest, 1 = bilineal, 2 = JINC2, 3 = xBR
 *              (WriteBatchTextureFilter de upstream).
 * Diferencias con upstream, a proposito:
 *   - Sin dual-source en D3D9: los filtros son siempre de "alfa binario" (los
 *     texels con menos de la mitad de cobertura se descartan y el resto se
 *     dibuja entero), que es la variante BinAlpha de upstream.
 *   - Upstream copia los filtros de beetle-psx, escritos para centros de
 *     texel en n+0,5 y muestreo por floor; aqui (y en upstream a x1) el
 *     centro del texel n esta en n y se muestrea redondeando.  Las cuentas
 *     se hacen en la convencion de beetle (coords + 0,5) y se muestrea el
 *     texel entero: sin eso los sprites 1:1 salian mezclados al 50 % con el
 *     texel de al lado (bilineal) y JINC2/xBR leian texels repetidos. */
static const char* s_ps_batch_src =
  "sampler2D s_vram   : register(s0);\n"
  "sampler2D s_dither : register(s1);\n"
  "float4 c_p0   : register(c0);\n"   /* x = factor origen, y = modo (0..3), z = poner mascara, w = dithering */
  "float4 c_p1   : register(c1);\n"   /* x = entrelazado, y = campo activo */
  "float4 c_winx : register(c2);\n"   /* bits 0..3 de la mascara de ventana en X (bits 3..6 de u) */
  "float4 c_winy : register(c3);\n"
  "float4 c_win4 : register(c4);\n"   /* x = bit 4 X, y = bit 4 Y, z = OR X, w = OR Y */
  PS_COMMON
  "float4 vram(float2 p) { return tex2Dlod(s_vram, float4((p + 0.5) * INV_VRAM, 0.0, 0.0)); }\n"
  /* Ventana de textura: c = (c & and) | or, con la mascara en los bits 3..7.
   * Sin ventana todas las constantes son 0 y queda c tal cual. */
  "static const float4 SH = float4(0.125, 0.0625, 0.03125, 0.015625);\n"
  "static const float4 WT = float4(8.0, 16.0, 32.0, 64.0);\n"
  "float2 win2(float2 c)\n"
  "{\n"
  "  float4 bx = frac(floor(c.x * SH) * 0.5) * 2.0;\n"
  "  float4 by = frac(floor(c.y * SH) * 0.5) * 2.0;\n"
  "  float2 b7 = frac(floor(c * 0.0078125) * 0.5) * 2.0;\n"
  "  float2 a;\n"
  "  a.x = dot(bx, c_winx * WT) + b7.x * c_win4.x * 128.0;\n"
  "  a.y = dot(by, c_winy * WT) + b7.y * c_win4.y * 128.0;\n"
  "  return c - a + c_win4.zw;\n"
  "}\n"
  "#if TEXMODE == 4\n"
  "static float s_mode;\n"   /* modo del primitivo: 0 sin textura, 1..3 (main, desde el vertice) */
  "static float s_raw;\n"    /* textura "raw" (sin modular por el color) */
  "#define ISRAW (s_raw > 0.5)\n"
  "#else\n"
  "#define ISRAW (RAW != 0)\n"
  "#endif\n"
  "#if TEXMODE != 0\n"
  "float4 sampleTex(float2 uv, float4 tp)\n"
  "{\n"
  "  float2 ic = roundeven2(uv);\n"
  "  ic.x = imod(ic.x, 256.0);\n"
  "  ic.y = imod(ic.y, 256.0);\n"
  "  ic = win2(ic);\n"
  /* [rendimiento] Un solo shader para los tres modos: el modo viaja en el
   * vertice y cambiar de 4 a 8 bits o a directa ya no corta el lote. */
  "#if TEXMODE == 4\n"
  "  float4 r;\n"
  "  [branch] if (s_mode < 2.5)\n"
  "  {\n"
  "    const float four = (s_mode < 1.5) ? 1.0 : 0.0;\n"
  "    float per = 2.0 + 2.0 * four;\n"   /* texels por palabra de 16 bits */
  "    float ix  = idiv(ic.x, per);\n"
  "    float sub = ic.x - ix * per;\n"
  "    float v   = dec16(vram(float2(tp.x + ix, tp.y + ic.y)));\n"
  "    float d   = four > 0.5 ? (1.0 + 15.0 * step(0.5, sub) + 240.0 * step(1.5, sub) + 3840.0 * step(2.5, sub))\n"
  "                           : (1.0 + 255.0 * step(0.5, sub));\n"
  "    float idx = imod(idiv(v, d), four > 0.5 ? 16.0 : 256.0);\n"
  "    r = vram(float2(tp.z + idx, tp.w));\n"
  "  }\n"
  "  else\n"
  "    r = vram(float2(tp.x + ic.x, tp.y + ic.y));\n"
  "  return r;\n"
  "#elif TEXMODE == 1\n"
  "  float ix  = idiv(ic.x, 4.0);\n"
  "  float sub = ic.x - ix * 4.0;\n"
  "  float v   = dec16(vram(float2(tp.x + ix, tp.y + ic.y)));\n"
  "  float d   = 1.0 + 15.0 * step(0.5, sub) + 240.0 * step(1.5, sub) + 3840.0 * step(2.5, sub);\n"
  "  float idx = imod(idiv(v, d), 16.0);\n"
  "  return vram(float2(tp.z + idx, tp.w));\n"
  "#elif TEXMODE == 2\n"
  "  float ix  = idiv(ic.x, 2.0);\n"
  "  float sub = ic.x - ix * 2.0;\n"
  "  float v   = dec16(vram(float2(tp.x + ix, tp.y + ic.y)));\n"
  "  float d   = 1.0 + 255.0 * step(0.5, sub);\n"
  "  float idx = imod(idiv(v, d), 256.0);\n"
  "  return vram(float2(tp.z + idx, tp.w));\n"
  "#else\n"
  "  return vram(float2(tp.x + ic.x, tp.y + ic.y));\n"
  "#endif\n"
  "}\n"
  "#if FILTER != 0\n"
  "float opaq(float4 c) { return (dot(c, float4(1.0, 1.0, 1.0, 1.0)) < 0.001) ? 0.0 : 1.0; }\n"
  /* Texel ENTERO n (ya en la convencion de esta shader), limitado al rango
   * de u/v del poligono (ComputePolygonUVLimits). */
  "float4 T(float4 tp, float2 n, float4 lim) { return sampleTex(clamp(n, lim.xy, lim.zw), tp); }\n"
  "#endif\n"
  "#if FILTER == 1\n"
  "void filtered(float4 tp, float2 coords, float4 lim, out float4 texcol, out float ialpha)\n"
  "{\n"
  "  float2 pc  = coords + 0.5;\n"
  "  float2 tl  = frac(pc) - 0.5;\n"
  "  float2 off = sign(tl);\n"
  "  float2 b0  = floor(pc);\n"
  "  float4 fc  = max(b0.xyxy + float4(0.0, 0.0, off.x, off.y), 0.0);\n"
  "  float4 s00 = T(tp, fc.xy, lim);\n"
  "  float4 s10 = T(tp, fc.zy, lim);\n"
  "  float4 s01 = T(tp, fc.xw, lim);\n"
  "  float4 s11 = T(tp, fc.zw, lim);\n"
  "  float2 w = abs(tl);\n"
  "  texcol = lerp(lerp(s00, s10, w.x), lerp(s01, s11, w.x), w.y);\n"
  "  ialpha = lerp(lerp(opaq(s00), opaq(s10), w.x), lerp(opaq(s01), opaq(s11), w.x), w.y);\n"
  "}\n"
  "#elif FILTER == 2\n"
  "float jinc2w(float x)\n"
  "{\n"
  "  x = max(x, 0.0001);\n"
  "  return sin(x * 1.382300768) * sin(x * 2.576105976) / (x * x);\n"
  "}\n"
  "void filtered(float4 tp, float2 coords, float4 lim, out float4 texcol, out float ialpha)\n"
  "{\n"
  "  float2 pc = coords + 0.5;\n"
  "  float2 tc = floor(pc - 0.5) + 0.5;\n"
  "  float4 csum = float4(0.0, 0.0, 0.0, 0.0);\n"
  "  float asum = 0.0;\n"
  "  float wsum = 0.0;\n"
  "  float4 mn = float4(1.0, 1.0, 1.0, 1.0);\n"
  "  float4 mx = float4(0.0, 0.0, 0.0, 0.0);\n"
  "  float amn = 1.0;\n"
  "  float amx = 0.0;\n"
  "  [loop] for (int j = 0; j < 4; j++)\n"
  "  {\n"
  "    [loop] for (int k = 0; k < 4; k++)\n"
  "    {\n"
  "      float2 p = tc + float2((float)k - 1.0, (float)j - 1.0);\n"
  "      float w = jinc2w(distance(pc, p));\n"
  "      float4 c = T(tp, floor(p), lim);\n"
  "      float a = opaq(c);\n"
  "      csum += w * c;\n"
  "      asum += w * a;\n"
  "      wsum += w;\n"
  "      if (j >= 1 && j <= 2 && k >= 1 && k <= 2)\n"
  "      {\n"
  "        mn = min(mn, c); mx = max(mx, c);\n"
  "        amn = min(amn, a); amx = max(amx, a);\n"
  "      }\n"
  "    }\n"
  "  }\n"
  "  float4 col = csum / wsum;\n"
  "  float al = asum / wsum;\n"
  "  col = lerp(col, clamp(col, mn, mx), 0.8);\n"   /* anti-ringing */
  "  al = lerp(al, clamp(al, amn, amx), 0.8);\n"
  "  texcol = col;\n"
  "  ialpha = al;\n"
  "}\n"
  "#elif FILTER == 3\n"
  "static const float4 XW = float4(0.2627, 0.6780, 0.0593, 0.5);\n"
  "float dYC(float4 a, float4 b)\n"
  "{\n"
  "  float4 df = a - b;\n"
  "  float Y = dot(df, XW);\n"
  "  float Cb = (0.5 / (1.0 - XW.b)) * (df.b - Y);\n"
  "  float Cr = (0.5 / (1.0 - XW.r)) * (df.r - Y);\n"
  "  return sqrt(Y * Y + Cb * Cb + Cr * Cr);\n"
  "}\n"
  "bool peq(float4 a, float4 b) { return dYC(a, b) < 0.1176470588235294; }\n"
  "bool veq(float4 a, float4 b) { return all(a == b); }\n"
  "float leftRatio(float2 center, float2 origin, float2 direction)\n"
  "{\n"
  "  float2 P0 = center - origin;\n"
  "  float2 proj = direction * (dot(P0, direction) / dot(direction, direction));\n"
  "  float2 distv = P0 - proj;\n"
  "  float2 orth = float2(-direction.y, direction.x);\n"
  "  float side = sign(dot(P0, orth));\n"
  "  float v = side * length(distv * 8.0);\n"
  "  return smoothstep(-0.70710678, 0.70710678, v);\n"
  "}\n"
  "#define P(dx, dy) T(tp, coord + float2((dx), (dy)), lim)\n"
  "void filtered(float4 tp, float2 coords, float4 lim, out float4 texcol, out float ialpha)\n"
  "{\n"
  "  float2 pc = coords + 0.5;\n"
  "  float2 pos = frac(pc) - 0.5;\n"
  "  float2 coord = floor(pc);\n"
  "  float4 A = P(-1, -1); float Aw = A.w; A.w = opaq(A);\n"
  "  float4 B = P( 0, -1); float Bw = B.w; B.w = opaq(B);\n"
  "  float4 C = P( 1, -1); float Cw = C.w; C.w = opaq(C);\n"
  "  float4 D = P(-1,  0); float Dw = D.w; D.w = opaq(D);\n"
  "  float4 E = P( 0,  0); float Ew = E.w; E.w = opaq(E);\n"
  "  float4 F = P( 1,  0); float Fw = F.w; F.w = opaq(F);\n"
  "  float4 G = P(-1,  1); float Gw = G.w; G.w = opaq(G);\n"
  "  float4 H = P( 0,  1); float Hw = H.w; H.w = opaq(H);\n"
  "  float4 I = P( 1,  1); float Iw = I.w; I.w = opaq(I);\n"
  "  float4 br = float4(0.0, 0.0, 0.0, 0.0);\n"   /* 0 = nada, 1 = normal, 2 = dominante */
  "  if (!((veq(E, F) && veq(H, I)) || (veq(E, H) && veq(F, I))))\n"
  "  {\n"
  "    float dHF = dYC(G, E) + dYC(E, C) + dYC(P(0, 2), I) + dYC(I, P(2, 0)) + 4.0 * dYC(H, F);\n"
  "    float dEI = dYC(D, H) + dYC(H, P(1, 2)) + dYC(B, F) + dYC(F, P(2, 1)) + 4.0 * dYC(E, I);\n"
  "    bool dom = (3.6 * dHF) < dEI;\n"
  "    br.z = ((dHF < dEI) && !veq(E, F) && !veq(E, H)) ? (dom ? 2.0 : 1.0) : 0.0;\n"
  "  }\n"
  "  if (!((veq(D, E) && veq(G, H)) || (veq(D, G) && veq(E, H))))\n"
  "  {\n"
  "    float dGE = dYC(P(-2, 1), D) + dYC(D, B) + dYC(P(-1, 2), H) + dYC(H, F) + 4.0 * dYC(G, E);\n"
  "    float dDH = dYC(P(-2, 0), G) + dYC(G, P(0, 2)) + dYC(A, E) + dYC(E, I) + 4.0 * dYC(D, H);\n"
  "    bool dom = (3.6 * dDH) < dGE;\n"
  "    br.w = ((dGE > dDH) && !veq(E, D) && !veq(E, H)) ? (dom ? 2.0 : 1.0) : 0.0;\n"
  "  }\n"
  "  if (!((veq(B, C) && veq(E, F)) || (veq(B, E) && veq(C, F))))\n"
  "  {\n"
  "    float dEC = dYC(D, B) + dYC(B, P(1, -2)) + dYC(H, F) + dYC(F, P(2, -1)) + 4.0 * dYC(E, C);\n"
  "    float dBF = dYC(A, E) + dYC(E, I) + dYC(P(0, -2), C) + dYC(C, P(2, 0)) + 4.0 * dYC(B, F);\n"
  "    bool dom = (3.6 * dBF) < dEC;\n"
  "    br.y = ((dEC > dBF) && !veq(E, B) && !veq(E, F)) ? (dom ? 2.0 : 1.0) : 0.0;\n"
  "  }\n"
  "  if (!((veq(A, B) && veq(D, E)) || (veq(A, D) && veq(B, E))))\n"
  "  {\n"
  "    float dDB = dYC(P(-2, 0), A) + dYC(A, P(0, -2)) + dYC(G, E) + dYC(E, C) + 4.0 * dYC(D, B);\n"
  "    float dAE = dYC(P(-2, -1), D) + dYC(D, H) + dYC(P(-1, -2), B) + dYC(B, F) + 4.0 * dYC(A, E);\n"
  "    bool dom = (3.6 * dDB) < dAE;\n"
  "    br.x = ((dDB < dAE) && !veq(E, D) && !veq(E, B)) ? (dom ? 2.0 : 1.0) : 0.0;\n"
  "  }\n"
  "  float4 res = E;\n"
  "  float resW = Ew;\n"
  "  if (br.z > 0.5)\n"
  "  {\n"
  "    float dFG = dYC(F, G);\n"
  "    float dHC = dYC(H, C);\n"
  "    bool lb = (br.z > 1.5 || !((br.y > 0.5 && !peq(E, G)) || (br.w > 0.5 && !peq(E, C)) ||\n"
  "              (peq(G, H) && peq(H, I) && peq(I, F) && peq(F, C) && !peq(E, I))));\n"
  "    float2 org = float2(0.0, 0.70710678);\n"
  "    float2 dir = float2(1.0, -1.0);\n"
  "    if (lb)\n"
  "    {\n"
  "      bool sh = (2.2 * dFG <= dHC) && !veq(E, G) && !veq(D, G);\n"
  "      bool st = (2.2 * dHC <= dFG) && !veq(E, C) && !veq(B, C);\n"
  "      org = sh ? float2(0.0, 0.25) : float2(0.0, 0.5);\n"
  "      dir.x += sh ? 1.0 : 0.0;\n"
  "      dir.y -= st ? 1.0 : 0.0;\n"
  "    }\n"
  "    float sel = step(dYC(E, F), dYC(E, H));\n"
  "    float r = leftRatio(pos, org, dir);\n"
  "    res = lerp(res, lerp(H, F, sel), r);\n"
  "    resW = lerp(resW, lerp(Hw, Fw, sel), r);\n"
  "  }\n"
  "  if (br.w > 0.5)\n"
  "  {\n"
  "    float dHA = dYC(H, A);\n"
  "    float dDI = dYC(D, I);\n"
  "    bool lb = (br.w > 1.5 || !((br.z > 0.5 && !peq(E, A)) || (br.x > 0.5 && !peq(E, I)) ||\n"
  "              (peq(A, D) && peq(D, G) && peq(G, H) && peq(H, I) && !peq(E, G))));\n"
  "    float2 org = float2(-0.70710678, 0.0);\n"
  "    float2 dir = float2(1.0, 1.0);\n"
  "    if (lb)\n"
  "    {\n"
  "      bool sh = (2.2 * dHA <= dDI) && !veq(E, A) && !veq(B, A);\n"
  "      bool st = (2.2 * dDI <= dHA) && !veq(E, I) && !veq(F, I);\n"
  "      org = sh ? float2(-0.25, 0.0) : float2(-0.5, 0.0);\n"
  "      dir.y += sh ? 1.0 : 0.0;\n"
  "      dir.x += st ? 1.0 : 0.0;\n"
  "    }\n"
  "    float sel = step(dYC(E, D), dYC(E, H));\n"
  "    float r = leftRatio(pos, org, dir);\n"
  "    res = lerp(res, lerp(H, D, sel), r);\n"
  "    resW = lerp(resW, lerp(Hw, Dw, sel), r);\n"
  "  }\n"
  "  if (br.y > 0.5)\n"
  "  {\n"
  "    float dBI = dYC(B, I);\n"
  "    float dFA = dYC(F, A);\n"
  "    bool lb = (br.y > 1.5 || !((br.x > 0.5 && !peq(E, I)) || (br.z > 0.5 && !peq(E, A)) ||\n"
  "              (peq(I, F) && peq(F, C) && peq(C, B) && peq(B, A) && !peq(E, C))));\n"
  "    float2 org = float2(0.70710678, 0.0);\n"
  "    float2 dir = float2(-1.0, -1.0);\n"
  "    if (lb)\n"
  "    {\n"
  "      bool sh = (2.2 * dBI <= dFA) && !veq(E, I) && !veq(H, I);\n"
  "      bool st = (2.2 * dFA <= dBI) && !veq(E, A) && !veq(D, A);\n"
  "      org = sh ? float2(0.25, 0.0) : float2(0.5, 0.0);\n"
  "      dir.y -= sh ? 1.0 : 0.0;\n"
  "      dir.x -= st ? 1.0 : 0.0;\n"
  "    }\n"
  "    float sel = step(dYC(E, B), dYC(E, F));\n"
  "    float r = leftRatio(pos, org, dir);\n"
  "    res = lerp(res, lerp(F, B, sel), r);\n"
  "    resW = lerp(resW, lerp(Fw, Bw, sel), r);\n"
  "  }\n"
  "  if (br.x > 0.5)\n"
  "  {\n"
  "    float dDC = dYC(D, C);\n"
  "    float dBG = dYC(B, G);\n"
  "    bool lb = (br.x > 1.5 || !((br.w > 0.5 && !peq(E, C)) || (br.y > 0.5 && !peq(E, G)) ||\n"
  "              (peq(C, B) && peq(B, A) && peq(A, D) && peq(D, G) && !peq(E, A))));\n"
  "    float2 org = float2(0.0, -0.70710678);\n"
  "    float2 dir = float2(-1.0, 1.0);\n"
  "    if (lb)\n"
  "    {\n"
  "      bool sh = (2.2 * dDC <= dBG) && !veq(E, C) && !veq(F, C);\n"
  "      bool st = (2.2 * dBG <= dDC) && !veq(E, G) && !veq(H, G);\n"
  "      org = sh ? float2(0.0, -0.25) : float2(0.0, -0.5);\n"
  "      dir.x -= sh ? 1.0 : 0.0;\n"
  "      dir.y += st ? 1.0 : 0.0;\n"
  "    }\n"
  "    float sel = step(dYC(E, B), dYC(E, D));\n"
  "    float r = leftRatio(pos, org, dir);\n"
  "    res = lerp(res, lerp(D, B, sel), r);\n"
  "    resW = lerp(resW, lerp(Dw, Bw, sel), r);\n"
  "  }\n"
  "  ialpha = res.w;\n"
  "  texcol = float4(res.xyz, resW);\n"
  "}\n"
  "#undef P\n"
  "#endif\n"   /* FILTER */
  "#endif\n"   /* TEXMODE != 0 */
  "struct PSI { float4 col : TEXCOORD2; float3 uvz : TEXCOORD0; float4 tp : TEXCOORD1; float4 lim : TEXCOORD3;\n"
  "             float mode : TEXCOORD4; float2 vpos : VPOS; };\n"
  "struct PSO { float4 col : COLOR0; float depth : DEPTH; };\n"
  "PSO main(PSI i)\n"
  "{\n"
  "  PSO o;\n"
  "  float descarta = 0.0;\n"   /* <0 = descartar: clip() al final (ver arriba) */
  "  float2 pix = pixpos(i.vpos);\n"
  "  if (c_p1.x > 0.5 && abs(imod(pix.y, 2.0) - c_p1.y) < 0.5)\n"
  "    descarta = -1.0;\n"
  "  float3 vc = floor(i.col.rgb / i.col.a * 255.0 + 0.5);\n"
  "  float dith = 0.0;\n"
  "  if (c_p0.w > 0.5)\n"
  "    dith = floor(tex2Dlod(s_dither, float4((pix + 0.5) * 0.25, 0.0, 0.0)).r * 255.0 + 0.5) - 4.0;\n"
  "  float semi;\n"
  "  float3 ic;\n"
  "  float oalpha;\n"
  /* [rendimiento] TEXMODE 4: un solo shader para todo el lote; con y sin
   * textura, los tres modos y "raw" vienen en el vertice (modo + 4 * raw). */
  "#if TEXMODE == 4\n"
  "  float vm = floor(i.mode + 0.5);\n"
  "  s_raw = (vm > 3.5) ? 1.0 : 0.0;\n"
  "  s_mode = vm - 4.0 * s_raw;\n"
  "  semi = 1.0;\n"
  "#if TRUECOLOR\n"
  "  ic = vc;\n"
  "#else\n"
  "  if (c_p0.w > 0.5)\n"
  "    ic = clamp(floor((vc + dith) / 8.0), 0.0, 31.0);\n"
  "  else\n"
  "    ic = floor(vc / 8.0);\n"
  "#endif\n"
  "  oalpha = c_p0.z;\n"
  "  [branch] if (s_mode > 0.5)\n"
  "  {\n"
  "#endif\n"
  "#if TEXMODE != 0\n"
  "  float4 tpi = floor(i.tp + 0.5);\n"
  "  float4 tc;\n"
  "#if FILTER != 0\n"
  "  float ialpha;\n"
  "  filtered(tpi, i.uvz.xy, i.lim, tc, ialpha);\n"
  "  if (ialpha > 0.0)\n"
  "    tc.rgb /= ialpha;\n"       /* compensar los texels transparentes */
  "  if (ialpha < 0.5)\n"
  "    descarta = -1.0;\n"        /* alfa binario (no hay dual-source) */
  "#else\n"
  /* [PGXP] con decimales y perspectiva la u/v se puede salir un poco del
   * poligono en los bordes: limitarla como upstream (u_uv_limits) */
  "  float2 uvs = (c_p1.w > 0.5) ? clamp(i.uvz.xy, i.lim.xy, i.lim.zw) : i.uvz.xy;\n"
  "  tc = sampleTex(uvs, tpi);\n"
  "  if (dot(tc, float4(1.0, 1.0, 1.0, 1.0)) < 0.001)\n"
  "    descarta = -1.0;\n"
  "#endif\n"
  "  semi = (tc.a >= 0.5) ? 1.0 : 0.0;\n"
  "  float3 t8 = floor(saturate(tc.rgb) * 255.0 + 0.5);\n"
  "#if TRUECOLOR\n"
  "  ic = min(floor(t8 * vc / 128.0), 255.0);\n"
  "  if (ISRAW)\n"
  "    ic = t8;\n"
  "#else\n"
  "  float3 t5 = floor(t8 / 8.0);\n"   /* = floor(t * 31 + 0,5) en los valores de 5 bits */
  "  ic = floor(t5 * vc / 16.0);\n"
  "  if (c_p0.w > 0.5)\n"
  "    ic = clamp(floor((ic + dith) / 8.0), 0.0, 31.0);\n"
  "  else\n"
  "    ic = min(floor(ic / 8.0), 31.0);\n"
  "  if (ISRAW)\n"
  "    ic = t5;\n"
  "#endif\n"
  "  oalpha = max(c_p0.z, semi);\n"
  "  if (c_p0.y > 2.5)\n"
  "  {\n"
  "    if (semi < 0.5) descarta = -1.0;\n"     /* solo semitransparentes */
  "  }\n"
  "  else if (c_p0.y > 1.5)\n"
  "  {\n"
  "    if (semi > 0.5) descarta = -1.0;\n"     /* solo opacos */
  "  }\n"
  "#else\n"
  "  semi = 1.0;\n"
  "#if TRUECOLOR\n"
  "  ic = vc;\n"
  "#else\n"
  "  if (c_p0.w > 0.5)\n"
  "    ic = clamp(floor((vc + dith) / 8.0), 0.0, 31.0);\n"
  "  else\n"
  "    ic = floor(vc / 8.0);\n"
  "#endif\n"
  "  oalpha = c_p0.z;\n"
  "#endif\n"
  "#if TEXMODE == 4\n"
  "  }\n"
  "#endif\n"
  "  float pm = 1.0;\n"
  "  if (c_p0.y > 0.5 && semi > 0.5)\n"
  "    pm = c_p0.x;\n"
  "#if TRUECOLOR\n"
  "  o.col   = float4(ic * pm / 255.0, oalpha);\n"
  "#else\n"
  "  o.col   = float4(floor(ic * pm) / 31.0, oalpha);\n"
  "#endif\n"
  "  o.depth = oalpha * i.uvz.z;\n"
  "  clip(descarta);\n"
  "  return o;\n"
  "}\n";

/* Subida desde psxVuw.  La textura de subida es A1R5G5B5 con los bytes de
 * psxVuw tal cual: el rojo de la PSX (bits 0-4) cae en el AZUL de D3D. */
static const char* s_ps_upload_src =
  "sampler2D s_up : register(s0);\n"
  "float4 c_u0 : register(c0);\n"   /* x = poner mascara, y = profundidad */
  PS_COMMON
  "struct PSO { float4 col : COLOR0; float depth : DEPTH; };\n"
  "PSO main(float2 vpos : VPOS)\n"
  "{\n"
  "  PSO o;\n"
  "  float2 p = pixpos(vpos);\n"
  "  float4 t = tex2Dlod(s_up, float4((p + 0.5) * INV_VRAM, 0.0, 0.0));\n"
  "  float a = max(floor(t.a + 0.5), c_u0.x);\n"
  "  o.col = float4(t.b, t.g, t.r, a);\n"
  "  o.depth = (a > 0.5) ? c_u0.y : 0.0;\n"
  "  return o;\n"
  "}\n";

/* Copia VRAM -> VRAM.  Lee de la copia de lectura, asi que el solape entre
 * origen y destino no importa. */
static const char* s_ps_copy_src =
  "sampler2D s_vram : register(s0);\n"
  "float4 c_c0 : register(c0);\n"   /* xy = origen - destino, z = poner mascara, w = profundidad */
  PS_COMMON
  "struct PSO { float4 col : COLOR0; float depth : DEPTH; };\n"
  "PSO main(float2 vpos : VPOS)\n"
  "{\n"
  "  PSO o;\n"
  "  float2 p = pixpos(vpos) + c_c0.xy;\n"
  "  float4 t = tex2Dlod(s_vram, float4((p + 0.5) * INV_VRAM, 0.0, 0.0));\n"
  "  float a = max(floor(t.a + 0.5), c_c0.z);\n"
  "  o.col = float4(t.rgb, a);\n"
  "  o.depth = (c_c0.z > 0.5) ? 1.0 : ((a > 0.5) ? c_c0.w : 0.0);\n"
  "  return o;\n"
  "}\n";

/* Relleno de VRAM (GP0 02h) como en upstream (VRAMFill de gpu_hw_shadergen):
 * un quad con el color en c0 y la profundidad a 0 (bit de mascara a 0). */
static const char* s_ps_fill_src =
  "float4 c_f0 : register(c0);\n"
  "struct PSO { float4 col : COLOR0; float depth : DEPTH; };\n"
  "PSO main()\n"
  "{\n"
  "  PSO o;\n"
  "  o.col = c_f0;\n"
  "  o.depth = 0.0;\n"
  "  return o;\n"
  "}\n";

/* Profundidad desde el bit de mascara (escrituras de color apagadas). */
static const char* s_ps_depth_src =
  "sampler2D s_vram : register(s0);\n"
  PS_COMMON
  "struct PSO { float4 col : COLOR0; float depth : DEPTH; };\n"
  "PSO main(float2 vpos : VPOS)\n"
  "{\n"
  "  PSO o;\n"
  "  float2 p = pixpos(vpos);\n"
  "  float4 t = tex2Dlod(s_vram, float4((p + 0.5) * INV_VRAM, 0.0, 0.0));\n"
  "  o.col = t;\n"
  "  o.depth = floor(t.a + 0.5);\n"
  "  return o;\n"
  "}\n";

/* Salida de 15 bits: el texel ya esta en 8 bits por canal. */
static const char* s_ps_disp15_src =
  "sampler2D s_vram : register(s0);\n"
  "float4 c_d0 : register(c0);\n"   /* xy = origen en la VRAM, zw = esquina en la imagen */
  "float4 c_d1 : register(c1);\n"   /* xy = tamano del contenido */
  PS_COMMON
  "float4 main(float2 vpos : VPOS) : COLOR0\n"
  "{\n"
  "  float2 q = pixpos(vpos) - c_d0.zw;\n"
  "  if (q.x < 0.0 || q.y < 0.0 || q.x >= c_d1.x || q.y >= c_d1.y)\n"
  "    return float4(0.0, 0.0, 0.0, 1.0);\n"   /* margenes: negro */
  "  float2 p = c_d0.xy + q;\n"
  "  float4 t = tex2Dlod(s_vram, float4((p + 0.5) * INV_VRAM, 0.0, 0.0));\n"
  "  return float4(t.rgb, 1.0);\n"
  "}\n";

/* Salida de 24 bits (los videos MDEC): tres bytes por pixel repartidos entre
 * dos palabras de 16 bits, como SampleVRAM24 de upstream pero con bytes en
 * vez de desplazamientos de 32 bits (no caben exactos en float). */
static const char* s_ps_disp24_src =
  "sampler2D s_vram : register(s0);\n"
  "float4 c_d0 : register(c0);\n"
  "float4 c_d1 : register(c1);\n"   /* xy = tamano del contenido */
  PS_COMMON
  "float4 vram(float2 p) { return tex2Dlod(s_vram, float4((p + 0.5) * INV_VRAM, 0.0, 0.0)); }\n"
  "float4 main(float2 vpos : VPOS) : COLOR0\n"
  "{\n"
  "  float2 q = pixpos(vpos) - c_d0.zw;\n"
  "  if (q.x < 0.0 || q.y < 0.0 || q.x >= c_d1.x || q.y >= c_d1.y)\n"
  "    return float4(0.0, 0.0, 0.0, 1.0);\n"
  "  float bx = idiv(q.x * 3.0, 2.0);\n"
  "  float2 base = float2(c_d0.x + bx, c_d0.y + q.y);\n"
  "  float s0 = dec16(vram(base));\n"
  "  float s1 = dec16(vram(base + float2(1.0, 0.0)));\n"
  "  float lo0 = imod(s0, 256.0);\n"
  "  float hi0 = idiv(s0, 256.0);\n"
  "  float lo1 = imod(s1, 256.0);\n"
  "  float hi1 = idiv(s1, 256.0);\n"
  "  float odd = imod(q.x, 2.0);\n"
  "  float3 rgb = (odd > 0.5) ? float3(hi0, lo1, hi1) : float3(lo0, hi0, lo1);\n"
  "  return float4(rgb / 255.0, 1.0);\n"
  "}\n";

/* ======================================================================
 * Construccion y recursos
 * ==================================================================== */

static IDirect3DDevice9* s_hw_device = 0;

/* [2026-10-31] Hilo que tiene ahora el device, con el hilo de GPU activo
 * (duck_hw_thread_acquire / release, desde libpcsxcore/gpu.c).  0 = sin hilo
 * de GPU: el dueno es el que creo el backend (m_owner_thread). */
static volatile DWORD s_hw_owner_tid = 0;
/* Cambios de dueno del device (AcquireThreadOwnership), para [DUCK-HW/PERF]. */
static volatile u32 s_hw_acquires = 0;

/* Traza: avisar UNA vez si se toca el device desde un hilo que no lo tiene.
 * En la Xbox eso es un cuelgue o basura en pantalla, asi que mejor verlo en
 * el log que adivinarlo. */
static void hw_check_owner(const char* where, DWORD creator)
{
  static volatile int s_warned = 0;
  const DWORD owner = s_hw_owner_tid ? s_hw_owner_tid : creator;
  const DWORD me = GetCurrentThreadId();
  if (!s_warned && owner != 0 && me != owner)
  {
    s_warned = 1;
    hw_log(3, "[DUCK-HW] ERROR: %s desde el hilo %u, el device lo tiene el hilo %u\n", where, (unsigned)me,
           (unsigned)owner);
  }
}

GPU_HW_D3D9_Backend::GPU_HW_D3D9_Backend(u16* psx_vram)
  : m_dev(0),
    m_vram_tex(0), m_vram_surf(0), m_read_tex(0), m_read_surf(0), m_depth(0),
    m_upload_tex(0), m_dither_tex(0), m_sys_surf(0),
    m_out_tex(0), m_out_w(0), m_out_h(0), m_out_changed(false), m_frame_ready(false),
    m_decl_batch(0), m_decl_util(0), m_vs_batch(0), m_vs_util(0),
    m_ps_upload(0), m_ps_copy(0), m_ps_depth(0), m_ps_fill(0), m_ps_disp15(0), m_ps_disp24(0), m_tc(0), m_req_tc(-1), m_req_filter(-1), m_filter(0),
    m_bound(false), m_in_frame(false), m_in_scene(false), m_sb(0), m_saved_rt(0), m_saved_ds(0),
    m_verts(0), m_nverts(0), m_batch_valid(false), m_scissor_valid(false),
    m_depth_counter(1), m_last_tp_key(0), m_tp_key_valid(false),
    m_owner_thread(0), m_warned_thread(false), m_warned_draw(false),
    m_st_frames(0), m_st_prims(0), m_st_verts(0), m_st_draws(0), m_st_presents(0),
    m_st_uploads(0), m_st_readbacks(0), m_st_refresh(0), m_st_fills(0), m_st_copies(0),
    m_presents_logged(0), m_presents_total(0), m_warned_util(false),
    m_log_polys(0), m_log_uploads(0), m_log_fills(0),
    m_out_rt(0), m_sys_tex(0), m_upload_idx(0), m_vb_data(0), m_vb_idx(0), m_vb_used(0), m_vb_start(0),
    m_edram_valid(false), m_pf_frame_begin(0), m_pf_frame_end(0),
    m_pipe_valid(false), m_cur_ps(0), m_cur_vb(0), m_cur_psc_valid(false), m_cur_blend(0xFFFFFFFFu),
    m_cur_bf(HW_BF_NONE), m_cur_bop(0xFFFFFFFFu), m_cur_zfunc(0xFFFFFFFFu), m_flush_reason(BRK_OTHER),
    m_st_upload_waits(0), m_log_copies(0), m_log_readbacks(0),
    m_tp_mode(0), m_tp_x(0), m_tp_y(0), m_clut_x(0), m_clut_y(0),
    m_dump_armed(false), m_dump_active(false), m_dump_lines(0), m_dump_seq(0), m_dump_left(0)
{
  m_vram_ptr = psx_vram;
  std::memset(m_ps_batch, 0, sizeof(m_ps_batch));
  std::memset(m_uvlim, 0, sizeof(m_uvlim));
  m_vmode = 0.0f;
  m_batch_tex = false;
  std::memset(m_out_graveyard, 0, sizeof(m_out_graveyard));
  std::memset(m_upload_ring, 0, sizeof(m_upload_ring));
  std::memset(m_vb, 0, sizeof(m_vb));
  std::memset(m_pf, 0, sizeof(m_pf));
  std::memset(m_brk, 0, sizeof(m_brk));
  std::memset(m_cur_psc, 0, sizeof(m_cur_psc));
  std::memset(m_lp, 0, sizeof(m_lp));
  std::memset(m_upload_fence, 0, sizeof(m_upload_fence));
  std::memset(m_ct, 0, sizeof(m_ct));
  std::memset(m_upload_fence_ok, 0, sizeof(m_upload_fence_ok));
  std::memset(m_vb_fence, 0, sizeof(m_vb_fence));
  std::memset(m_vb_fence_ok, 0, sizeof(m_vb_fence_ok));
  std::memset(&m_batch, 0, sizeof(m_batch));
  std::memset(&m_scissor, 0, sizeof(m_scissor));
  m_dirty.valid = false;
  m_dirty.left = m_dirty.top = m_dirty.right = m_dirty.bottom = 0;
  m_last_params.bits = 0;
}

GPU_HW_D3D9_Backend::~GPU_HW_D3D9_Backend()
{
  Shutdown();
}

bool GPU_HW_D3D9_Backend::Initialize(bool force_thread)
{
  UNREFERENCED_VARIABLE(force_thread);

  m_dev = s_hw_device;
  if (!m_dev)
  {
    hw_log(3, "[DUCK-HW] sin device D3D9 del frontend\n");
    return false;
  }
  m_dev->AddRef();
  m_owner_thread = GetCurrentThreadId();

  m_verts = static_cast<HWVertex*>(std::malloc(sizeof(HWVertex) * MAX_BATCH_VERTICES));
  {
    /* [traza] Que paso falla: si no, el driver vuelve al software en
     * silencio (y sin hilo de GPU, que ya se descarto: va lento). */
    const char* fallo = 0;
    if (!m_verts)
      fallo = "memoria de vertices";
    else if (!CreateResources())
      fallo = "recursos";
    else if (!CompileShaders())
      fallo = "shaders";
    if (fallo)
    {
      hw_log(3, "[DUCK-HW] NO ARRANCA (%s): se usa SwanStation software, sin hilo de GPU\n", fallo);
      Shutdown();
      return false;
    }
  }

  Bind();
  Reset(true);
  EndOutsideFrame();

#if defined(_XBOX)
  hw_log(1, "[DUCK-HW] renderizador D3D9 listo (Xbox 360, EDRAM, x1)\n");
#else
  hw_log(1, "[DUCK-HW] renderizador D3D9 listo (VRAM 1024x512, x1)\n");
#endif
  return true;
}

void GPU_HW_D3D9_Backend::Shutdown()
{
  if (m_dev)
  {
    if (m_bound)
    {
      m_nverts = 0;
      Unbind();
    }
    DestroyShaders();
    DestroyResources();
    SAFE_RELEASE(m_sb);
    m_dev->Release();
    m_dev = 0;
  }
  if (m_verts)
  {
    std::free(m_verts);
    m_verts = 0;
  }
}

bool GPU_HW_D3D9_Backend::CreateResources()
{
  HRESULT hr;

#if defined(_XBOX)
  if (!CreateResourcesXbox())
    return false;
#else
  hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, &m_vram_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] VRAM: CreateTexture 0x%08X\n", (unsigned)hr); return false; }
  m_vram_tex->GetSurfaceLevel(0, &m_vram_surf);

  hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, &m_read_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] lectura: CreateTexture 0x%08X\n", (unsigned)hr); return false; }
  m_read_tex->GetSurfaceLevel(0, &m_read_surf);

  hr = m_dev->CreateDepthStencilSurface(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                        &m_depth, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] profundidad: 0x%08X\n", (unsigned)hr); return false; }

  hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, D3DUSAGE_DYNAMIC, D3DFMT_A1R5G5B5,
                            D3DPOOL_DEFAULT, &m_upload_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] subida: CreateTexture 0x%08X\n", (unsigned)hr); return false; }

  hr = m_dev->CreateOffscreenPlainSurface(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                          &m_sys_surf, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] lectura a CPU: 0x%08X\n", (unsigned)hr); return false; }
#endif

  /* La matriz de dithering de la PSX (DITHER_MATRIX), +4 para que quepa en
   * un byte sin signo. */
  hr = m_dev->CreateTexture(4, 4, 1, HW_USAGE_CPU, HW_FMT_CPU_ARGB, D3DPOOL_DEFAULT, &m_dither_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] dither: 0x%08X\n", (unsigned)hr); return false; }
  {
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(m_dither_tex->LockRect(0, &lr, NULL, HW_LOCK_CPU)))
    {
      for (int y = 0; y < 4; y++)
      {
        D3DCOLOR* row = reinterpret_cast<D3DCOLOR*>(static_cast<u8*>(lr.pBits) + y * lr.Pitch);
        for (int x = 0; x < 4; x++)
        {
          const u32 v = static_cast<u32>(DITHER_MATRIX[y][x] + 4);
          row[x] = D3DCOLOR_ARGB(255, v, v, v);
        }
      }
      m_dither_tex->UnlockRect(0);
    }
  }

  {
    static const D3DVERTEXELEMENT9 batch_elems[] = {
      { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
      { 0, 20, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      { 0, 28, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
      { 0, 44, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
      { 0, 60, D3DDECLTYPE_FLOAT1,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
      D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 util_elems[] = {
      { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      D3DDECL_END()
    };
    if (FAILED(m_dev->CreateVertexDeclaration(batch_elems, &m_decl_batch)) ||
        FAILED(m_dev->CreateVertexDeclaration(util_elems, &m_decl_util)))
    {
      hw_log(3, "[DUCK-HW] CreateVertexDeclaration fallo\n");
      return false;
    }
  }
  return true;
}

void GPU_HW_D3D9_Backend::DestroyResources()
{
  int i;
#if defined(_XBOX)
  /* Que la GPU no siga leyendo nada de lo que se suelta (en la Xbox Release
   * no espera a la GPU). */
  if (m_dev)
    m_dev->BlockUntilIdle();
  if (m_vb_data && m_vb[m_vb_idx])
    m_vb[m_vb_idx]->Unlock();
  m_vb_data = 0;
  for (i = 0; i < HW_VB_COUNT; i++)
    SAFE_RELEASE(m_vb[i]);
  for (i = 0; i < HW_UPLOAD_RING; i++)
    SAFE_RELEASE(m_upload_ring[i]);
  SAFE_RELEASE(m_sys_tex);
  SAFE_RELEASE(m_out_rt);
#endif
  ReleaseOutput();
  for (i = 0; i < 4; i++)
    SAFE_RELEASE(m_out_graveyard[i]);
  SAFE_RELEASE(m_decl_batch);
  SAFE_RELEASE(m_decl_util);
  SAFE_RELEASE(m_sys_surf);
  SAFE_RELEASE(m_dither_tex);
  SAFE_RELEASE(m_upload_tex);
  SAFE_RELEASE(m_depth);
  SAFE_RELEASE(m_read_surf);
  SAFE_RELEASE(m_read_tex);
  SAFE_RELEASE(m_vram_surf);
  SAFE_RELEASE(m_vram_tex);
}

#if defined(_XBOX)
/* Las superficies de la EDRAM (VRAM, su profundidad, la salida) y lo que en
 * PC hacia la textura render target. */
bool GPU_HW_D3D9_Backend::CreateResourcesXbox()
{
  HRESULT hr;
  D3DSURFACE_PARAMETERS sp;
  UINT fb_tiles = 0, color_tiles, z_tiles, out_tiles, base;
  int i;

  /* Detras del back buffer del frontend, con su tamano CONSULTADO (el
   * usuario puede cambiar la resolucion).  Ver la nota de xbox64 en
   * video_handoff.cpp: compartir base con el frontend le dejaba su overlay
   * dentro de la imagen del core. */
  {
    IDirect3DSurface9* bb = 0;
    if (SUCCEEDED(m_dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb)
    {
      D3DSURFACE_DESC d;
      if (SUCCEEDED(bb->GetDesc(&d)))
        fb_tiles = XGSurfaceSize(d.Width, d.Height, d.Format, d.MultiSampleType);
      bb->Release();
    }
  }
  color_tiles = XGSurfaceSize(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
  z_tiles = XGSurfaceSize(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_D24S8, D3DMULTISAMPLE_NONE);
  out_tiles = XGSurfaceSize(HW_OUT_MAX_W, HW_OUT_MAX_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);

  /* 2048 tiles de EDRAM.  Si no cabe detras (1080p), base 0: como la EDRAM
   * se repinta desde m_read_tex en cada Bind, solo hace falta que el
   * frontend no dibuje DURANTE retro_run. */
  base = (fb_tiles + color_tiles + z_tiles + out_tiles <= 2048u) ? fb_tiles : 0u;
  hw_log(1, "[DUCK-HW] EDRAM: frontend=%u tiles, VRAM base=%u (+%u color, +%u Z, +%u salida)\n", fb_tiles, base,
         color_tiles, z_tiles, out_tiles);

  std::memset(&sp, 0, sizeof(sp));
  sp.Base = base;
  hr = m_dev->CreateRenderTarget(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                 &m_vram_surf, &sp);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] VRAM (EDRAM): 0x%08X\n", (unsigned)hr); return false; }

  std::memset(&sp, 0, sizeof(sp));
  sp.Base = base + color_tiles;
  hr = m_dev->CreateDepthStencilSurface(VRAM_WIDTH, VRAM_HEIGHT, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                        &m_depth, &sp);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] profundidad (EDRAM): 0x%08X\n", (unsigned)hr); return false; }

  std::memset(&sp, 0, sizeof(sp));
  sp.Base = base + color_tiles + z_tiles;
  hr = m_dev->CreateRenderTarget(HW_OUT_MAX_W, HW_OUT_MAX_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                 &m_out_rt, &sp);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] salida (EDRAM): 0x%08X\n", (unsigned)hr); return false; }

  /* Destino de Resolve: TILEADA (Resolve escribe siempre tileado). */
  hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_read_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] lectura: CreateTexture 0x%08X\n", (unsigned)hr); return false; }
  m_read_tex->GetSurfaceLevel(0, &m_read_surf);

  /* Para leer la VRAM por CPU: LINEAL (D3DX la destila desde la tileada). */
  hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &m_sys_tex, NULL);
  if (FAILED(hr)) { hw_log(3, "[DUCK-HW] lectura a CPU: 0x%08X\n", (unsigned)hr); return false; }

  /* Subidas: un anillo, para que un LockRect no espere a la GPU por la
   * subida anterior (en la Xbox el LockRect de algo en uso espera). */
  for (i = 0; i < HW_UPLOAD_RING; i++)
  {
    hr = m_dev->CreateTexture(VRAM_WIDTH, VRAM_HEIGHT, 1, 0, D3DFMT_LIN_A1R5G5B5, D3DPOOL_DEFAULT,
                              &m_upload_ring[i], NULL);
    if (FAILED(hr)) { hw_log(3, "[DUCK-HW] subida %d: 0x%08X\n", i, (unsigned)hr); return false; }
  }
  m_upload_tex = m_upload_ring[0];
  m_upload_tex->AddRef();

  for (i = 0; i < HW_VB_COUNT; i++)
  {
    hr = m_dev->CreateVertexBuffer(MAX_BATCH_VERTICES * sizeof(HWVertex), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
                                   &m_vb[i], NULL);
    if (FAILED(hr)) { hw_log(3, "[DUCK-HW] vertex buffer %d: 0x%08X\n", i, (unsigned)hr); return false; }
  }
  m_vb_idx = HW_VB_COUNT - 1;
  m_vb_data = 0;
  m_vb_used = 0;
  m_edram_valid = false;
  return true;
}

/* Siguiente buffer de vertices del anillo.  El Lock sin banderas espera a
 * que la GPU haya terminado con el (como gxd3d_consumidor_rota_vb). */
void GPU_HW_D3D9_Backend::RotateVB()
{
  void* p = 0;
  if (m_vb_data)
  {
    m_vb[m_vb_idx]->Unlock();
    m_vb_data = 0;
    /* Todo lo que dibuja de este buffer ya esta encolado: la fence marca
     * cuando la GPU lo suelta. */
    m_vb_fence[m_vb_idx] = m_dev->InsertFence();
    m_vb_fence_ok[m_vb_idx] = true;
  }
  m_vb_idx = (m_vb_idx + 1) % HW_VB_COUNT;
  if (!m_vb[m_vb_idx])
    return;
  /* Igual que las subidas: el Lock no espera a la GPU. */
  if (m_vb_fence_ok[m_vb_idx] && m_dev->IsFencePending(m_vb_fence[m_vb_idx]))
    m_dev->BlockOnFence(m_vb_fence[m_vb_idx]);
  m_vb[m_vb_idx]->Lock(0, MAX_BATCH_VERTICES * sizeof(HWVertex), &p, 0);
  m_vb_data = static_cast<HWVertex*>(p);
  m_vb_used = 0;
}

/* La EDRAM desde la copia de lectura: color tal cual y profundidad desde el
 * bit de mascara (1 si lo tiene).  Es UpdateDepthBufferFromMaskBit de
 * upstream con las escrituras de color encendidas. */
void GPU_HW_D3D9_Backend::RestoreEDRAM()
{
  HW_PF_BEGIN(rst);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
  m_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
  m_dev->SetTexture(0, m_read_tex);
  m_dev->SetPixelShader(m_ps_depth);
  DrawUtilRect(0.0f, 0.0f, (float)VRAM_WIDTH, (float)VRAM_HEIGHT, (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_depth_counter = 1;
  m_batch_valid = false;
  m_pipe_valid = false;
  m_scissor_valid = false;
  m_tp_key_valid = false;
  m_edram_valid = true;
  HW_PF_END(rst, PF_RESTORE);
}
#endif /* _XBOX */

IDirect3DVertexShader9* GPU_HW_D3D9_Backend::CompileVS(const char* src)
{
  ID3DXBuffer* code = 0;
  ID3DXBuffer* err = 0;
  IDirect3DVertexShader9* vs = 0;
  HRESULT hr = D3DXCompileShader(src, (UINT)std::strlen(src), NULL, NULL, "main", "vs_3_0", 0, &code, &err, NULL);
  if (FAILED(hr))
  {
    hw_log(3, "[DUCK-HW] VS (%s): %s\n", (src == s_vs_batch_src) ? "lotes" : "util",
           err ? static_cast<const char*>(err->GetBufferPointer()) : "?");
    SAFE_RELEASE(err);
    return 0;
  }
  hr = m_dev->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &vs);
  if (FAILED(hr) || !vs)
    hw_log(3, "[DUCK-HW] CreateVertexShader fallo 0x%08X\n", (unsigned)hr);
  SAFE_RELEASE(code);
  SAFE_RELEASE(err);
  return vs;
}

IDirect3DPixelShader9* GPU_HW_D3D9_Backend::CompilePS(const char* src, const char* texmode, const char* raw,
                                                     int true_color, int filter)
{
  static const char* digits[4] = { "0", "1", "2", "3" };
  D3DXMACRO macros[5];
  ID3DXBuffer* code = 0;
  ID3DXBuffer* err = 0;
  IDirect3DPixelShader9* ps = 0;
  HRESULT hr;

  macros[0].Name = "TEXMODE"; macros[0].Definition = texmode ? texmode : "0";
  macros[1].Name = "RAW";     macros[1].Definition = raw ? raw : "0";
  macros[2].Name = "TRUECOLOR"; macros[2].Definition = true_color ? "1" : "0";
  macros[3].Name = "FILTER";  macros[3].Definition = digits[(filter >= 0 && filter <= 3) ? filter : 0];
  macros[4].Name = NULL;      macros[4].Definition = NULL;

  hr = D3DXCompileShader(src, (UINT)std::strlen(src), macros, NULL, "main", "ps_3_0", 0, &code, &err, NULL);
  if (FAILED(hr))
  {
    hw_log(3, "[DUCK-HW] PS (TEXMODE=%s RAW=%s TRUECOLOR=%s FILTER=%s): %s\n", macros[0].Definition,
           macros[1].Definition, macros[2].Definition, macros[3].Definition,
           err ? static_cast<const char*>(err->GetBufferPointer()) : "?");
    SAFE_RELEASE(err);
    return 0;
  }
  hr = m_dev->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &ps);
  if (FAILED(hr) || !ps)
    hw_log(3, "[DUCK-HW] CreatePixelShader fallo 0x%08X (TEXMODE=%s RAW=%s)\n", (unsigned)hr, macros[0].Definition,
           macros[1].Definition);
  SAFE_RELEASE(code);
  SAFE_RELEASE(err);
  return ps;
}

bool GPU_HW_D3D9_Backend::CompileShaders()
{
  m_vs_batch = CompileVS(s_vs_batch_src);
  m_vs_util  = CompileVS(s_vs_util_src);
  if (!m_vs_batch || !m_vs_util)
    return false;

  /* Lotes con las opciones vigentes; si un filtro no compila se sigue sin
   * filtro, y si true color tampoco, sin true color: nunca se queda el
   * renderizador sin arrancar por una opcion. */
  m_req_tc = g_duck_hw_true_color ? 1 : 0;
  m_req_filter = (g_duck_hw_texture_filter >= 0 && g_duck_hw_texture_filter <= 3) ? g_duck_hw_texture_filter : 0;
  if (!CompileBatchShaders(m_req_tc, m_req_filter))
    return false;

  m_ps_upload = CompilePS(s_ps_upload_src, 0, 0);
  m_ps_copy   = CompilePS(s_ps_copy_src, 0, 0);
  m_ps_depth  = CompilePS(s_ps_depth_src, 0, 0);
  m_ps_disp15 = CompilePS(s_ps_disp15_src, 0, 0);
  m_ps_disp24 = CompilePS(s_ps_disp24_src, 0, 0);
  m_ps_fill   = CompilePS(s_ps_fill_src, 0, 0);
  if (!(m_ps_upload && m_ps_copy && m_ps_depth && m_ps_disp15 && m_ps_disp24 && m_ps_fill))
  {
    hw_log(3, "[DUCK-HW] shaders de utilidad: upload=%d copy=%d depth=%d disp15=%d disp24=%d (1 = bien)\n",
           m_ps_upload ? 1 : 0, m_ps_copy ? 1 : 0, m_ps_depth ? 1 : 0, m_ps_disp15 ? 1 : 0, m_ps_disp24 ? 1 : 0);
    return false;
  }
  return true;
}

void GPU_HW_D3D9_Backend::DestroyBatchShaders()
{
  int t, r;
  for (t = 0; t < 2; t++)
    for (r = 0; r < 2; r++)
      SAFE_RELEASE(m_ps_batch[t][r]);
}

/* Las 3 variantes de lote (sin textura + con textura x raw) para un true color y
 * un filtro.  Con fallo: se prueba sin filtro y luego sin true color. */
bool GPU_HW_D3D9_Backend::CompileBatchShaders(int true_color, int filter)
{
  static const char* texmodes[2] = { "0", "4" };   /* solo sin textura / todo en uno (modo y raw en el vertice) */
  static const char* raws[2] = { "0", "1" };
  static const char* fnames[4] = { "nearest", "bilineal", "JINC2", "xBR" };
  int t, r;
  bool ok = true;
#if DUCK_HW_PERF
  const u64 t0 = hw_now();
#endif

  if (filter < 0 || filter > 3)
    filter = 0;
  DestroyBatchShaders();
  for (t = 0; t < 2 && ok; t++)
  {
    for (r = 0; r < 2 && ok; r++)
    {
      if (r == 1)
        continue;   /* "raw" va en el vertice: solo [0][0] y [1][0] */
      /* Sin textura el filtro no cambia nada: siempre la variante 0. */
      m_ps_batch[t][r] = CompilePS(s_ps_batch_src, texmodes[t], raws[r], true_color, (t == 0) ? 0 : filter);
      if (!m_ps_batch[t][r])
        ok = false;
    }
  }
  if (!ok)
  {
    DestroyBatchShaders();
    if (filter != 0)
    {
      hw_log(3, "[DUCK-HW] el filtro %s no compila (ver arriba): se sigue sin filtro\n", fnames[filter]);
      return CompileBatchShaders(true_color, 0);
    }
    if (true_color)
    {
      hw_log(3, "[DUCK-HW] true color no compila (ver arriba): se sigue a 15 bits\n");
      return CompileBatchShaders(0, 0);
    }
    return false;
  }
  m_tc = true_color;
  m_filter = filter;
  m_cur_ps = 0;
  m_pipe_valid = false;
#if DUCK_HW_PERF
  {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    hw_log(1, "[DUCK-HW] lotes: true color=%d filtro=%s (compilados en %u ms)\n", m_tc, fnames[m_filter],
           (unsigned)(((hw_now() - t0) * 1000u) / static_cast<u64>(f.QuadPart ? f.QuadPart : 1)));
  }
#else
  hw_log(1, "[DUCK-HW] lotes: true color=%d filtro=%s\n", m_tc, fnames[m_filter]);
#endif
  return true;
}

/* Cambio de opciones en caliente: al empezar un frame, si true color o el
 * filtro ya no coinciden con lo compilado.  Los intentos fallidos no se
 * repiten en cada frame: se recuerda lo pedido. */
void GPU_HW_D3D9_Backend::ApplyRenderOptions()
{
  const int want_tc = g_duck_hw_true_color ? 1 : 0;
  const int want_filter = (g_duck_hw_texture_filter >= 0 && g_duck_hw_texture_filter <= 3) ? g_duck_hw_texture_filter : 0;

  if (!m_dev || (want_tc == m_req_tc && want_filter == m_req_filter))
    return;
  m_req_tc = want_tc;
  m_req_filter = want_filter;
  if (want_tc == m_tc && want_filter == m_filter)
    return;
  if (m_bound)
    FlushRender();
  if (!CompileBatchShaders(want_tc, want_filter))
  {
    /* Ni siquiera la variante basica: reintentar lo de antes. */
    hw_log(3, "[DUCK-HW] no se pudieron recompilar los lotes; se vuelve a true color=%d filtro=%d\n", m_tc, m_filter);
    CompileBatchShaders(m_tc, m_filter);
  }
}

void GPU_HW_D3D9_Backend::DestroyShaders()
{
  DestroyBatchShaders();
  SAFE_RELEASE(m_ps_upload);
  SAFE_RELEASE(m_ps_copy);
  SAFE_RELEASE(m_ps_depth);
  SAFE_RELEASE(m_ps_disp15);
  SAFE_RELEASE(m_ps_disp24);
  SAFE_RELEASE(m_ps_fill);
  SAFE_RELEASE(m_vs_batch);
  SAFE_RELEASE(m_vs_util);
}

/* ======================================================================
 * Ligar y desligar el device del frontend
 * ==================================================================== */

void GPU_HW_D3D9_Backend::Bind()
{
  int i;
  if (m_bound || !m_dev)
    return;

  /* [traza] Todo el trabajo D3D9 tiene que ir en el hilo de retro_run.  Si
   * llega desde otro (el hilo de GPU de libpcsxcore/gpu.c), se cruza con el
   * frontend: pantalla negra o basura. */
  /* [2026-10-31] Con el hilo de GPU el dueno va cambiando: se compara con el
   * que tiene el device AHORA (s_hw_owner_tid), no con el que lo creo. */
  hw_check_owner("Bind", m_owner_thread);

  /* Las texturas de salida viejas: el frontend ya tiene la nueva (env 30003
   * se llamo al final del frame anterior). */
  for (i = 0; i < 4; i++)
  {
    if (m_out_graveyard[i])
    {
#if defined(_XBOX)
      /* En la Xbox Release no espera a la GPU, y el frontend la ha
       * muestreado hace un frame.  Pasa solo al cambiar de resolucion. */
      m_dev->BlockUntilIdle();
#endif
      SAFE_RELEASE(m_out_graveyard[i]);
    }
  }

#if defined(_XBOX)
  /* Sin state block en la consola: el frontend rehace su pipeline en cada
   * frame (XBOX_SetupQuadState) y aqui solo se guarda el destino. */
  m_dev->GetRenderTarget(0, &m_saved_rt);
  if (FAILED(m_dev->GetDepthStencilSurface(&m_saved_ds)))
    m_saved_ds = 0;
  m_in_scene = false;
#else
  /* Todo el estado del frontend (shaders, texturas, samplers, render states,
   * viewport) en un state block; el render target y la profundidad aparte,
   * porque un state block no los guarda. */
  if (!m_sb)
    m_dev->CreateStateBlock(D3DSBT_ALL, &m_sb);
  else
    m_sb->Capture();
  m_dev->GetRenderTarget(0, &m_saved_rt);
  m_dev->GetDepthStencilSurface(&m_saved_ds);

  /* El D3D9 de PC solo dibuja entre BeginScene y EndScene.  Si alguien ya
   * esta dentro de una escena, no se cierra la suya. */
  m_in_scene = SUCCEEDED(m_dev->BeginScene());
#endif

  m_bound = true;
  m_batch_valid = false;
  m_pipe_valid = false;
  m_scissor_valid = false;
  SetVRAMTarget();
  ApplyFixedStates();
#if defined(_XBOX)
  /* Lo que haya en la EDRAM no es nuestro: repintarla desde la copia. */
  if (!m_edram_valid)
    RestoreEDRAM();
#endif
}

void GPU_HW_D3D9_Backend::Unbind()
{
  if (!m_bound)
    return;

  m_flush_reason = BRK_END;
  FlushRender();

#if defined(_XBOX)
  /* La copia de lectura pasa a ser la unica verdad: lo pendiente de la EDRAM
   * se resuelve ya, y el proximo Bind la repinta. */
  RefreshReadTexture();
  m_edram_valid = false;
  RestoreFrontendStates();
  if (m_saved_rt)
    m_dev->SetRenderTarget(0, m_saved_rt);
  m_dev->SetDepthStencilSurface(m_saved_ds);
#else
  if (m_in_scene)
    m_dev->EndScene();
  m_in_scene = false;

  m_dev->SetRenderTarget(0, m_saved_rt);
  m_dev->SetDepthStencilSurface(m_saved_ds);
  if (m_sb)
    m_sb->Apply();
#endif
  SAFE_RELEASE(m_saved_rt);
  SAFE_RELEASE(m_saved_ds);
  m_bound = false;
}

#if defined(_XBOX)
/* Lo que XBOX_SetupQuadState NO rehace y nosotros cambiamos.  Su overlay
 * mezcla con SRCALPHA/INVSRCALPHA dando por hecho BLENDOP ADD; sus LUT van en
 * s1..s3; sin Z ligada, ZENABLE tiene que ir apagado. */
void GPU_HW_D3D9_Backend::RestoreFrontendStates()
{
  int s;
  m_dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  m_dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  m_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  m_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
  m_dev->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
  m_dev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
  m_dev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
  m_dev->SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
  m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
  m_dev->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC);
  m_dev->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC);
  for (s = 0; s < 2; s++)
  {
    m_dev->SetTexture(s, NULL);
    m_dev->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_dev->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_dev->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_dev->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  }
  m_dev->SetPixelShader(NULL);
  m_dev->SetVertexShader(NULL);
}
#endif

void GPU_HW_D3D9_Backend::EndOutsideFrame()
{
  if (!m_in_frame)
    Unbind();
}

void GPU_HW_D3D9_Backend::FrameBegin()
{
#if DUCK_HW_PERF
  const u64 t = hw_now();
  if (m_pf_frame_end)
    m_pf[PF_GAP] += t - m_pf_frame_end;
  m_pf_frame_begin = t;
#endif
  m_in_frame = true;
  m_frame_ready = false;
  ApplyRenderOptions();

  /* [traza] Volcado de UN frame (L3+R3, libretro_core.cpp): VRAM antes, cada
   * comando de GPU al log ([DUCK-DUMP]) y VRAM despues (en FrameEnd). */
  /* [MGS] Ahora CUATRO frames seguidos: el juego va a 25/30 fps con doble
   * buffer, y en los volcados de un frame siempre salia el buffer bueno.
   * Solo el primero liga y vuelca al empezar (el Bind adelantado podria
   * tapar el fallo); los demas solo al acabar. */
  if (m_dump_armed)
  {
    m_dump_armed = false;
    m_dump_left = 4;
    m_dump_active = true;
    m_dump_lines = 0;
    m_dump_seq++;
    hw_log(1, "[DUCK-DUMP] inicio del volcado %u\n", m_dump_seq);
    Bind();
    DumpVRAM("a");
  }
  else if (m_dump_left > 0)
  {
    m_dump_active = true;
    m_dump_lines = 0;
    m_dump_seq++;
    hw_log(1, "[DUCK-DUMP] inicio del volcado %u (seguido)\n", m_dump_seq);
  }
}

bool GPU_HW_D3D9_Backend::Dumping()
{
  if (!m_dump_active)
    return false;
  if (m_dump_lines >= 4000)
    return false;
  m_dump_lines++;
  return true;
}

void GPU_HW_D3D9_Backend::FrameEnd()
{
  if (m_dump_active)
  {
    Bind();
    DumpVRAM("d");
    m_dump_active = false;
    if (m_dump_left > 0)
      m_dump_left--;
    hw_log(1, "[DUCK-DUMP] fin del volcado %u (%u lineas)\n", m_dump_seq, m_dump_lines);
  }
  Unbind();
  m_in_frame = false;
  ++m_st_frames;
#if DUCK_HW_PERF
  m_pf_frame_end = hw_now();
  if (m_pf_frame_begin)
    m_pf[PF_RUN] += m_pf_frame_end - m_pf_frame_begin;
  if (m_st_frames >= 120)
  {
    LogPerf(m_st_frames);
    LogStats();   /* los contadores van con los tiempos */
  }
#else
  if (m_st_frames >= 600)
    LogStats();
#endif
}

/* [traza] Tiempos medios por frame en centesimas de ms. */
void GPU_HW_D3D9_Backend::LogPerf(u32 frames)
{
#if DUCK_HW_PERF
  LARGE_INTEGER f;
  u32 v[PF_COUNT];
  int i;
  QueryPerformanceFrequency(&f);
  for (i = 0; i < PF_COUNT; i++)
  {
    v[i] = (f.QuadPart > 0 && frames)
             ? static_cast<u32>((m_pf[i] * 100000ull / static_cast<u64>(f.QuadPart)) / frames)
             : 0u;
    m_pf[i] = 0;
  }
  {
    const u32 total = v[PF_RUN] + v[PF_GAP];
    const u32 fps10 = total ? (100000u * 10u / total) : 0u;
    hw_log(1, "[DUCK-HW/PERF] ms/fr run=%u.%02u gap=%u.%02u fps=%u.%u | flush=%u.%02u vblock=%u.%02u\n",
           v[PF_RUN] / 100, v[PF_RUN] % 100, v[PF_GAP] / 100, v[PF_GAP] % 100, fps10 / 10, fps10 % 10,
           v[PF_FLUSH] / 100, v[PF_FLUSH] % 100, v[PF_VBLOCK] / 100, v[PF_VBLOCK] % 100);
    hw_log(1, "[DUCK-HW/PERF] upload=%u.%02u uplock=%u.%02u resolve=%u.%02u present=%u.%02u\n",
           v[PF_UPLOAD] / 100, v[PF_UPLOAD] % 100, v[PF_UPLOCK] / 100, v[PF_UPLOCK] % 100,
           v[PF_RESOLVE] / 100, v[PF_RESOLVE] % 100, v[PF_PRESENT] / 100, v[PF_PRESENT] % 100);
    hw_log(1, "[DUCK-HW/PERF] restore=%u.%02u readback=%u.%02u idle=%u.%02u prims=%u.%02u\n",
           v[PF_RESTORE] / 100, v[PF_RESTORE] % 100, v[PF_READBACK] / 100, v[PF_READBACK] % 100,
           v[PF_IDLE] / 100, v[PF_IDLE] % 100, v[PF_PRIM] / 100, v[PF_PRIM] % 100);
    /* [2026-10-31] Con hilo de GPU: prims/flush son tiempo del hilo de GPU (en
     * paralelo con la emulacion) y own = cambios de dueno del device por frame
     * (2 por cada vuelta principal->GPU->principal). */
    if (s_hw_owner_tid != 0 || s_hw_acquires != 0)
    {
      const u32 own10 = frames ? (s_hw_acquires * 10u / frames) : 0u;
      s_hw_acquires = 0;
      hw_log(1, "[DUCK-HW/PERF] hilo-gpu own=%u.%u/fr\n", own10 / 10, own10 % 10);
    }
  }
#else
  UNREFERENCED_VARIABLE(frames);
#endif
}

/* ----------------------------------------------------------------------
 * [traza] Volcado de la VRAM a BMP (L3+R3).  Dos ficheros por llamada:
 * duckhw_<n>_<cuando>_color.bmp (la VRAM tal cual, 1024x512) y
 * duckhw_<n>_<cuando>_mascara.bmp (blanco = bit 15 puesto: mascara /
 * semitransparencia del texel).  Directorio: el primero escribible de
 * game:\ devkit:\ cache:\ (Xbox) o el del ejecutable (PC), como el
 * TEXBMP de xbox64 (xenon/gx_d3d9.cpp).
 * -------------------------------------------------------------------- */
static void bmp_put32(unsigned char* p, unsigned int v)
{
  /* BMP es little-endian y la consola big-endian: byte a byte. */
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
  p[2] = (unsigned char)((v >> 16) & 0xFF);
  p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static const char* dump_dir()
{
#if defined(_XBOX)
  static const char* cand[] = { "game:\\", "devkit:\\", "cache:\\" };
#else
  static const char* cand[] = { ".\\" };
#endif
  static const char* found = 0;
  static bool searched = false;
  int i;
  if (searched)
    return found;
  searched = true;
  for (i = 0; i < (int)(sizeof(cand) / sizeof(cand[0])); i++)
  {
    char path[64];
    FILE* f;
    _snprintf(path, sizeof(path) - 1, "%sduckhw.tmp", cand[i]);
    path[sizeof(path) - 1] = 0;
    f = std::fopen(path, "wb");
    if (f)
    {
      std::fclose(f);
      found = cand[i];
      hw_log(1, "[DUCK-DUMP] BMP en %s\n", found);
      return found;
    }
  }
  hw_log(3, "[DUCK-DUMP] ningun directorio escribible para los BMP\n");
  return 0;
}

static void write_bmp(const char* name, const D3DCOLOR* px, u32 w, u32 h, bool mask)
{
  const char* dir = dump_dir();
  unsigned char hdr[54];
  unsigned char* row;
  char path[128];
  FILE* f;
  u32 x, y;
  if (!dir)
    return;
  _snprintf(path, sizeof(path) - 1, "%s%s.bmp", dir, name);
  path[sizeof(path) - 1] = 0;
  f = std::fopen(path, "wb");
  if (!f)
  {
    hw_log(3, "[DUCK-DUMP] no se puede escribir %s\n", path);
    return;
  }
  std::memset(hdr, 0, sizeof(hdr));
  hdr[0] = 'B'; hdr[1] = 'M';
  bmp_put32(hdr + 2, 54u + w * h * 4u);
  bmp_put32(hdr + 10, 54u);
  bmp_put32(hdr + 14, 40u);
  bmp_put32(hdr + 18, w);
  bmp_put32(hdr + 22, (unsigned int)(-(int)h));   /* de arriba abajo */
  hdr[26] = 1;
  hdr[28] = 32;
  bmp_put32(hdr + 34, w * h * 4u);
  std::fwrite(hdr, 1, sizeof(hdr), f);
  row = static_cast<unsigned char*>(std::malloc(w * 4u));
  if (row)
  {
    for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
      {
        const D3DCOLOR p = px[y * w + x];
        if (mask)
        {
          const unsigned char m = ((p >> 24) & 0xFF) >= 128u ? 255 : 0;
          row[x * 4 + 0] = m; row[x * 4 + 1] = m; row[x * 4 + 2] = m;
        }
        else
        {
          row[x * 4 + 0] = (unsigned char)(p & 0xFF);           /* B */
          row[x * 4 + 1] = (unsigned char)((p >> 8) & 0xFF);    /* G */
          row[x * 4 + 2] = (unsigned char)((p >> 16) & 0xFF);   /* R */
        }
        row[x * 4 + 3] = 255;
      }
      std::fwrite(row, 1, w * 4u, f);
    }
    std::free(row);
  }
  std::fclose(f);
  hw_log(1, "[DUCK-DUMP] %s\n", path);
}

/* La VRAM entera a CPU (A8R8G8B8, la PSX roja en .r) y a dos BMP. */
void GPU_HW_D3D9_Backend::DumpVRAM(const char* when)
{
  D3DLOCKED_RECT lr;
  D3DCOLOR* buf;
  char name[64];
  u32 y;
  bool ok = false;

  buf = static_cast<D3DCOLOR*>(std::malloc(VRAM_WIDTH * VRAM_HEIGHT * sizeof(D3DCOLOR)));
  if (!buf)
    return;
  FlushRender();
#if defined(_XBOX)
  m_dirty.valid = true;   /* resolver TODO: la copia es la que se lee */
  m_dirty.left = 0; m_dirty.top = 0; m_dirty.right = VRAM_WIDTH; m_dirty.bottom = VRAM_HEIGHT;
  RefreshReadTexture();
  m_dev->BlockUntilIdle();
  {
    IDirect3DSurface9* dst = 0;
    if (SUCCEEDED(m_sys_tex->GetSurfaceLevel(0, &dst)) && dst)
    {
      if (SUCCEEDED(D3DXLoadSurfaceFromSurface(dst, NULL, NULL, m_read_surf, NULL, NULL, D3DX_FILTER_NONE, 0)) &&
          SUCCEEDED(dst->LockRect(&lr, NULL, D3DLOCK_READONLY)))
      {
        for (y = 0; y < VRAM_HEIGHT; y++)
          std::memcpy(&buf[y * VRAM_WIDTH], static_cast<const u8*>(lr.pBits) + y * lr.Pitch,
                      VRAM_WIDTH * sizeof(D3DCOLOR));
        dst->UnlockRect();
        ok = true;
      }
      dst->Release();
    }
  }
#else
  if (SUCCEEDED(m_dev->GetRenderTargetData(m_vram_surf, m_sys_surf)) &&
      SUCCEEDED(m_sys_surf->LockRect(&lr, NULL, D3DLOCK_READONLY)))
  {
    for (y = 0; y < VRAM_HEIGHT; y++)
      std::memcpy(&buf[y * VRAM_WIDTH], static_cast<const u8*>(lr.pBits) + y * lr.Pitch,
                  VRAM_WIDTH * sizeof(D3DCOLOR));
    m_sys_surf->UnlockRect();
    ok = true;
  }
#endif
  if (ok)
  {
    _snprintf(name, sizeof(name) - 1, "duckhw_%u_%s_color", m_dump_seq, when);
    name[sizeof(name) - 1] = 0;
    write_bmp(name, buf, VRAM_WIDTH, VRAM_HEIGHT, false);
    _snprintf(name, sizeof(name) - 1, "duckhw_%u_%s_mascara", m_dump_seq, when);
    name[sizeof(name) - 1] = 0;
    write_bmp(name, buf, VRAM_WIDTH, VRAM_HEIGHT, true);
  }
  else
    hw_log(3, "[DUCK-DUMP] no se pudo leer la VRAM\n");

#if !defined(_XBOX)
  /* Y la COPIA DE LECTURA tal cual, sin refrescarla: es lo que muestrean los
   * lotes.  Si difiere de la VRAM en una pagina o una CLUT en uso, el
   * texturizado lee datos viejos.  (En la Xbox la copia ya es lo que se
   * vuelca arriba.) */
  if (SUCCEEDED(m_dev->GetRenderTargetData(m_read_surf, m_sys_surf)) &&
      SUCCEEDED(m_sys_surf->LockRect(&lr, NULL, D3DLOCK_READONLY)))
  {
    for (y = 0; y < VRAM_HEIGHT; y++)
      std::memcpy(&buf[y * VRAM_WIDTH], static_cast<const u8*>(lr.pBits) + y * lr.Pitch,
                  VRAM_WIDTH * sizeof(D3DCOLOR));
    m_sys_surf->UnlockRect();
    _snprintf(name, sizeof(name) - 1, "duckhw_%u_%s_lectura", m_dump_seq, when);
    name[sizeof(name) - 1] = 0;
    write_bmp(name, buf, VRAM_WIDTH, VRAM_HEIGHT, false);
    _snprintf(name, sizeof(name) - 1, "duckhw_%u_%s_lectura_mascara", m_dump_seq, when);
    name[sizeof(name) - 1] = 0;
    write_bmp(name, buf, VRAM_WIDTH, VRAM_HEIGHT, true);
  }
  hw_log(1, "[DUCK-DUMP] sucio %s: %d (%u,%u)-(%u,%u) pagina=%d (%u,%u) clut=(%u,%u) clave=%d\n", when,
         m_dirty.valid ? 1 : 0, m_dirty.left, m_dirty.top, m_dirty.right, m_dirty.bottom, m_tp_mode, m_tp_x, m_tp_y,
         m_clut_x, m_clut_y, m_tp_key_valid ? 1 : 0);
#endif
  std::free(buf);
}

/* [traza] Resumen cada 600 frames de retro_run.  prims/verts = lo que entra;
 * draws = DrawPrimitiveUP; refresh = copias a la textura de lectura. */
void GPU_HW_D3D9_Backend::LogStats()
{
  if (DUCK_HW_STATS || DUCK_HW_PERF)
  {
    hw_log(1, "[DUCK-HW] %u fr: prims=%u verts=%u draws=%u presents=%u salida=%ux%u\n", m_st_frames, m_st_prims,
           m_st_verts, m_st_draws, m_st_presents, m_out_w, m_out_h);
    hw_log(1, "[DUCK-HW] %u fr: fills=%u copies=%u uploads=%u (esperas %u) readbacks=%u refresh=%u\n", m_st_frames,
           m_st_fills, m_st_copies, m_st_uploads, m_st_upload_waits, m_st_readbacks, m_st_refresh);
    m_st_upload_waits = 0;
    /* Por que se corto cada lote (solo los que tenian vertices). */
    hw_log(1, "[DUCK-HW] cortes: tex=%u tr=%u bf=%u dith=%u mask=%u ilace=%u win=%u op=%u\n", m_brk[BRK_TEX],
           m_brk[BRK_TRANSP], m_brk[BRK_BF], m_brk[BRK_DITH], m_brk[BRK_MASK], m_brk[BRK_ILACE], m_brk[BRK_WIN],
           m_brk[BRK_OP]);
    hw_log(1, "[DUCK-HW] cortes: scissor=%u pagina=%u lleno=%u fin=%u otro=%u\n", m_brk[BRK_SCISSOR],
           m_brk[BRK_PAGE], m_brk[BRK_FULL], m_brk[BRK_END], m_brk[BRK_OTHER]);
    std::memset(m_brk, 0, sizeof(m_brk));
    hw_log(1, "[DUCK-HW] prims: sintex=%u t4=%u t8=%u t15=%u | semi m0=%u m1=%u m2=%u m3=%u | raw=%u dith=%u\n",
           m_ct[CT_TEX0], m_ct[CT_TEX0 + 1], m_ct[CT_TEX0 + 2], m_ct[CT_TEX0 + 3], m_ct[CT_SEMI0], m_ct[CT_SEMI0 + 1],
           m_ct[CT_SEMI0 + 2], m_ct[CT_SEMI0 + 3], m_ct[CT_RAW], m_ct[CT_DITH]);
    hw_log(1, "[DUCK-HW] prims: rect=%u (finos %u) lineas=%u ventana=%u mask=%u/%u ilace=%u copiasfinas=%u\n",
           m_ct[CT_RECT], m_ct[CT_RECT_THIN], m_ct[CT_LINE], m_ct[CT_WIN], m_ct[CT_SETMASK], m_ct[CT_CHKMASK],
           m_ct[CT_ILACE], m_ct[CT_COPY_THIN]);
  }
  m_st_upload_waits = 0;
  std::memset(m_brk, 0, sizeof(m_brk));
  std::memset(m_ct, 0, sizeof(m_ct));
  m_st_frames = m_st_prims = m_st_verts = m_st_draws = m_st_presents = 0;
  m_st_uploads = m_st_readbacks = m_st_refresh = m_st_fills = m_st_copies = 0;
}

void GPU_HW_D3D9_Backend::SetVRAMTarget()
{
  D3DVIEWPORT9 vp;
  m_dev->SetRenderTarget(0, m_vram_surf);
  m_dev->SetDepthStencilSurface(m_depth);
  vp.X = 0; vp.Y = 0; vp.Width = VRAM_WIDTH; vp.Height = VRAM_HEIGHT; vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
  m_dev->SetViewport(&vp);
  m_scissor_valid = false;
}

void GPU_HW_D3D9_Backend::ApplyFixedStates()
{
  int s;
  m_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
#if !defined(_XBOX)
  m_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif
  m_dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  m_dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  m_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
  m_dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
#if defined(_XBOX)
  /* El frontend lo deja a FALSE (su quad va ya transformado); sin el las
   * posiciones de recorte se tomarian como pixeles.  Y sin Hi-Z: la
   * profundidad la escribe la shader (oDepth), y la prueba de mascara tiene
   * que ser exacta pixel a pixel.  Xenos mezcla el alfa SIEMPRE con sus
   * propios factores (no hay SEPARATEALPHABLENDENABLE). */
  m_dev->SetRenderState(D3DRS_VIEWPORTENABLE, TRUE);
  m_dev->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_DISABLE);
  m_dev->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_DISABLE);
#else
  m_dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
  m_dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_DITHERENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
  m_dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
#endif
  m_dev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
  m_dev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
  m_dev->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
  m_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  m_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR);

  for (s = 0; s < 2; s++)
  {
    m_dev->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_dev->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    m_dev->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_dev->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    m_dev->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
#if !defined(_XBOX)
    m_dev->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, FALSE);
    m_dev->SetSamplerState(s, D3DSAMP_MAXMIPLEVEL, 0);
#endif
  }
  for (s = 2; s < 8; s++)
    m_dev->SetTexture(s, NULL);
}

/* ======================================================================
 * Rectangulo sucio y textura de lectura (gpu_hw.cpp:1153, 1343)
 * ==================================================================== */

void GPU_HW_D3D9_Backend::IncludeDirty(u32 left, u32 top, u32 right, u32 bottom)
{
  if (right > VRAM_WIDTH)   right = VRAM_WIDTH;
  if (bottom > VRAM_HEIGHT) bottom = VRAM_HEIGHT;
  if (left >= right || top >= bottom)
    return;
  /* IncludeVRAMDirtyRectangle de upstream: si se escribe en la pagina o la
   * CLUT vigentes, el siguiente texturizado tiene que volver a mirar. */
  if (m_tp_key_valid && TouchesTexturePage(left, top, right, bottom))
    m_tp_key_valid = false;
  if (!m_dirty.valid)
  {
    m_dirty.left = left; m_dirty.top = top; m_dirty.right = right; m_dirty.bottom = bottom;
    m_dirty.valid = true;
    return;
  }
  if (left < m_dirty.left)     m_dirty.left = left;
  if (top < m_dirty.top)       m_dirty.top = top;
  if (right > m_dirty.right)   m_dirty.right = right;
  if (bottom > m_dirty.bottom) m_dirty.bottom = bottom;
}

static bool rects_intersect(u32 l0, u32 t0, u32 r0, u32 b0, u32 l1, u32 t1, u32 r1, u32 b1)
{
  return (l0 < r1 && l1 < r0 && t0 < b1 && t1 < b0);
}

void GPU_HW_D3D9_Backend::RefreshReadTexture()
{
  if (!m_dirty.valid)
    return;
#if defined(_XBOX)
  {
    /* Resolve de la EDRAM a la copia, en el mismo sitio.  Rect redondeado
     * hacia fuera a 8 (la EDRAM tiene la verdad entera mientras estamos
     * ligados, asi que resolver de mas no estropea nada). */
    D3DRECT r;
    D3DPOINT pt;
    HRESULT hr;
    r.x1 = (LONG)(m_dirty.left & ~7u);
    r.y1 = (LONG)(m_dirty.top & ~7u);
    r.x2 = (LONG)hw_min((m_dirty.right + 7u) & ~7u, static_cast<u32>(VRAM_WIDTH));
    r.y2 = (LONG)hw_min((m_dirty.bottom + 7u) & ~7u, static_cast<u32>(VRAM_HEIGHT));
    pt.x = r.x1;
    pt.y = r.y1;
    HW_PF_BEGIN(res);
    hr = m_dev->Resolve(D3DRESOLVE_RENDERTARGET0, &r, m_read_tex, &pt, 0, 0, NULL, 0.0f, 0, NULL);
    HW_PF_END(res, PF_RESOLVE);
    if (FAILED(hr) && !m_warned_draw)
    {
      m_warned_draw = true;
      hw_log(3, "[DUCK-HW] Resolve (lectura) fallo 0x%08X\n", (unsigned)hr);
    }
  }
#else
  {
    RECT r;
    HRESULT hr;
    r.left = (LONG)m_dirty.left; r.top = (LONG)m_dirty.top;
    r.right = (LONG)m_dirty.right; r.bottom = (LONG)m_dirty.bottom;
    HW_PF_BEGIN(res);
    hr = m_dev->StretchRect(m_vram_surf, &r, m_read_surf, &r, D3DTEXF_NONE);
    HW_PF_END(res, PF_RESOLVE);
    if (FAILED(hr) && !m_warned_draw)
    {
      m_warned_draw = true;
      hw_log(3, "[DUCK-HW] StretchRect (lectura) fallo 0x%08X\n", (unsigned)hr);
    }
  }
#endif
  m_st_refresh++;
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] refresco (%u,%u)-(%u,%u)\n", m_dirty.left, m_dirty.top, m_dirty.right, m_dirty.bottom);
  m_dirty.valid = false;
  m_pipe_valid = false;   /* volver a ligar la copia recien escrita */
}

/* Upstream solo mira el rectangulo sucio cuando cambia la pagina de textura
 * (DispatchRenderCommand), y marca la pagina como cambiada cuando CUALQUIER
 * escritura de VRAM toca la pagina o la CLUT vigentes
 * (IncludeVRAMDirtyRectangle, gpu_hw.cpp:1153).  Aqui: la clave de
 * pagina+CLUT+modo; las operaciones de VRAM la invalidan, y IncludeDirty la
 * invalida tambien si el dibujo cae dentro de la pagina o la CLUT vigentes.
 *
 * [MGS] Esa segunda parte faltaba: un dibujo que escribe en la pagina que se
 * esta usando como textura (render a textura: caras del codec, sombras,
 * fundidos) no refrescaba la copia de lectura, y el siguiente texturizado
 * leia texels viejos.  Segun por donde cayera el refresco siguiente, unos
 * frames salian bien y otros no: parpadeo. */
bool GPU_HW_D3D9_Backend::TouchesTexturePage(u32 left, u32 top, u32 right, u32 bottom) const
{
  static const u32 widths[4] = { 0, 64, 128, 256 };
  const u32 w = widths[m_tp_mode];
  bool hit;
  if (m_tp_mode == 0)
    return false;
  hit = rects_intersect(m_tp_x, m_tp_y, m_tp_x + w, m_tp_y + 256, left, top, right, bottom);
  if (!hit && m_tp_x + w > VRAM_WIDTH)
    hit = rects_intersect(0, m_tp_y, m_tp_x + w - VRAM_WIDTH, m_tp_y + 256, left, top, right, bottom);
  if (!hit && m_tp_mode <= 2)
  {
    const u32 cw = (m_tp_mode == 1) ? 16 : 256;
    hit = rects_intersect(m_clut_x, m_clut_y, m_clut_x + cw, m_clut_y + 1, left, top, right, bottom);
    if (!hit && m_clut_x + cw > VRAM_WIDTH)
      hit = rects_intersect(0, m_clut_y, m_clut_x + cw - VRAM_WIDTH, m_clut_y + 1, left, top, right, bottom);
  }
  return hit;
}

void GPU_HW_D3D9_Backend::CheckTexturePage(int texmode, u32 tpx, u32 tpy, u32 clutx, u32 cluty)
{
  const u32 key = (tpx >> 6) | ((tpy >> 8) << 4) | (static_cast<u32>(texmode) << 5) | ((clutx >> 4) << 8) |
                  (cluty << 14);

  if (m_tp_key_valid && key == m_last_tp_key)
    return;
  m_last_tp_key = key;
  m_tp_key_valid = true;
  m_tp_mode = texmode;
  m_tp_x = tpx;
  m_tp_y = tpy;
  m_clut_x = clutx;
  m_clut_y = cluty;

  if (!m_dirty.valid)
    return;

  if (TouchesTexturePage(m_dirty.left, m_dirty.top, m_dirty.right, m_dirty.bottom))
  {
    m_flush_reason = BRK_PAGE;
    FlushRender();
    RefreshReadTexture();
  }
}

/* ======================================================================
 * Profundidad (bit de mascara)
 * ==================================================================== */

float GPU_HW_D3D9_Backend::NextMaskedDepth(bool check_mask)
{
  /* gpu_hw.cpp:676 -- cada primitivo que COMPRUEBA mascara baja un escalon. */
  if (check_mask)
  {
    if (m_depth_counter + 1 > MAX_DEPTH_COUNTER)
      ResetDepthCounter();
    m_depth_counter++;
  }
  return CurrentDepth();
}

void GPU_HW_D3D9_Backend::ResetDepthCounter()
{
  /* gpu_hw.cpp:1201 + gpu_hw_d3d11.cpp:2126: reconstruir la profundidad desde
   * el alfa de la VRAM y empezar de nuevo. */
  FlushRender();
  m_dirty.valid = true;
  m_dirty.left = 0; m_dirty.top = 0; m_dirty.right = VRAM_WIDTH; m_dirty.bottom = VRAM_HEIGHT;
  RefreshReadTexture();
  m_tp_key_valid = false;

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
  m_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
  m_dev->SetTexture(0, m_read_tex);
  m_dev->SetPixelShader(m_ps_depth);
  DrawUtilRect(0.0f, 0.0f, (float)VRAM_WIDTH, (float)VRAM_HEIGHT, (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
  m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_batch_valid = false;
  m_pipe_valid = false;
  m_scissor_valid = false;

  m_depth_counter = 1;
}

/* ======================================================================
 * Lotes
 * ==================================================================== */

void GPU_HW_D3D9_Backend::UpdateScissor()
{
  RECT r;
  const Common::Rectangle<u32>& a = m_drawing_area;
  r.left = (LONG)a.left;
  r.top = (LONG)a.top;
  r.right = (LONG)a.right + 1;
  r.bottom = (LONG)a.bottom + 1;
  if (m_scissor_valid && r.left == m_scissor.left && r.top == m_scissor.top && r.right == m_scissor.right &&
      r.bottom == m_scissor.bottom)
    return;
  m_flush_reason = BRK_SCISSOR;
  FlushRender();
  m_scissor = r;
  m_scissor_valid = true;
  m_dev->SetScissorRect(&r);
}

float GPU_HW_D3D9_Backend::BeginPrimitive(const GPUBackendDrawCommand* cmd, bool textured, int texmode, bool raw,
                                          int transparency, bool dithering)
{
  BatchKey k;

  Bind();
  m_last_params = cmd->params;

  std::memset(&k, 0, sizeof(k));
  /* [rendimiento] El modo (4/8 bits/directa) va en el vertice: en la clave
   * solo "con textura o sin textura".  GT2/Crash cortaban 150-200 lotes por
   * frame solo por esto, y en la Xbox cada lote es una llamada cara. */
  /* Con transparencia NO se mezclan con y sin textura en el mismo lote: el
   * lote con textura va en dos pasadas (opacos y luego semitransparentes) y
   * un poligono sin textura semitransparente acabaria pintado DESPUES de un
   * opaco con textura que el juego mando detras (sombras sobre los coches).
   * Sin transparencia es una sola pasada y el orden se respeta. */
  k.texmode = (transparency < 4 && textured) ? 1 : 0;
  k.raw = 0;
  m_vmode = textured ? static_cast<float>(texmode + ((raw) ? 4 : 0)) : 0.0f;
  k.transparency = transparency;
  k.dithering = dithering ? 1 : 0;
  k.set_mask = cmd->params.set_mask_while_drawing ? 1 : 0;
  k.check_mask = cmd->params.check_mask_before_draw ? 1 : 0;
  k.interlaced = cmd->params.interlaced_rendering ? 1 : 0;
  k.field = k.interlaced ? static_cast<int>(cmd->params.active_line_lsb) : 0;
  /* La ventana tambien en los primitivos sin textura (no la usan): asi no
   * cortan un lote de texturas con ventana. */
  {
    k.win_and_x = cmd->window.and_x;
    k.win_and_y = cmd->window.and_y;
    k.win_or_x = cmd->window.or_x;
    k.win_or_y = cmd->window.or_y;
  }

  /* [traza] Desglose de lo que dibuja el juego (cada 120 frames, con el
   * medidor): para saber que usa una escena que sale mal. */
  m_ct[CT_TEX0 + (textured ? texmode : 0)]++;
  if (k.transparency < 4) m_ct[CT_SEMI0 + k.transparency]++;
  if (textured && raw) m_ct[CT_RAW]++;
  if (k.dithering) m_ct[CT_DITH]++;
  if (k.set_mask) m_ct[CT_SETMASK]++;
  if (k.check_mask) m_ct[CT_CHKMASK]++;
  if (k.interlaced) m_ct[CT_ILACE]++;
  if (textured && (k.win_and_x != 0xFF || k.win_and_y != 0xFF || k.win_or_x != 0 || k.win_or_y != 0))
    m_ct[CT_WIN]++;

  /* gpu_hw.cpp:1375 -- cambio de estado que exige un lote nuevo.  B-F
   * siempre vacia (upstream lo hace igual: el orden importa). */
  if (!m_batch_valid || std::memcmp(&k, &m_batch, sizeof(k)) != 0 || transparency == 2)
  {
    if (!m_batch_valid)                                   m_flush_reason = BRK_OP;
    else if (k.texmode != m_batch.texmode || k.raw != m_batch.raw) m_flush_reason = BRK_TEX;
    else if (k.transparency != m_batch.transparency)      m_flush_reason = BRK_TRANSP;
    else if (k.dithering != m_batch.dithering)            m_flush_reason = BRK_DITH;
    else if (k.set_mask != m_batch.set_mask || k.check_mask != m_batch.check_mask) m_flush_reason = BRK_MASK;
    else if (k.interlaced != m_batch.interlaced || k.field != m_batch.field) m_flush_reason = BRK_ILACE;
    else if (std::memcmp(&k, &m_batch, sizeof(k)) != 0)   m_flush_reason = BRK_WIN;
    else                                                  m_flush_reason = BRK_BF;
    FlushRender();
    m_batch = k;
    m_batch_valid = true;
  }

  UpdateScissor();

  if (textured)
  {
    CheckTexturePage(texmode, cmd->draw_mode.GetTexturePageBaseX(), cmd->draw_mode.GetTexturePageBaseY(),
                     cmd->palette.GetXBase(), cmd->palette.GetYBase());
  }

  return NextMaskedDepth(cmd->params.check_mask_before_draw);
}

void GPU_HW_D3D9_Backend::EnsureSpace(u32 nverts)
{
  if (m_nverts + nverts > MAX_BATCH_VERTICES)
  {
    m_flush_reason = BRK_FULL;
    FlushRender();
  }
}

void GPU_HW_D3D9_Backend::AddVertex(float x, float y, float z, u32 color, float u, float v, const float* tp, float w)
{
  HWVertex* o = &m_verts[m_nverts++];
  o->x = x;
  o->y = y;
  o->z = z;
  o->w = w;   /* [PGXP] 1 salvo poligonos con perspectiva */
  /* PSX 0x00BBGGRR -> D3DCOLOR (ARGB) */
  o->color = D3DCOLOR_ARGB(0, color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
  o->u = u;
  o->v = v;
  o->tp_x = tp[0];
  o->tp_y = tp[1];
  o->clut_x = tp[2];
  o->clut_y = tp[3];
  o->lim_u0 = m_uvlim[0];
  o->lim_v0 = m_uvlim[1];
  o->lim_u1 = m_uvlim[2];
  o->lim_v1 = m_uvlim[3];
  o->mode = m_vmode;
  if (m_vmode != 0.0f)
    m_batch_tex = true;   /* el lote necesita el shader con textura (y las dos pasadas) */
}

bool GPU_HW_D3D9_Backend::DitherOn(const GPUBackendDrawCommand* cmd) const
{
  return (m_tc == 0) && (g_pcsxr_dithering != 0) && cmd->IsDitheringEnabled();
}

/* gpu_hw.cpp:503 ComputePolygonUVLimits: el rango de u/v de los vertices,
 * con el maximo un texel por dentro (el borde del poligono no se muestrea).
 * Solo lo usan los filtros (para no leer texels de fuera del sprite). */
void GPU_HW_D3D9_Backend::SetUVLimits(u32 min_u, u32 max_u, u32 min_v, u32 max_v)
{
  m_uvlim[0] = static_cast<float>(min_u);
  m_uvlim[1] = static_cast<float>(min_v);
  m_uvlim[2] = static_cast<float>(max_u);
  m_uvlim[3] = static_cast<float>(max_v);
}

static void texpage_floats(const GPUBackendDrawCommand* cmd, float* tp)
{
  tp[0] = static_cast<float>(cmd->draw_mode.GetTexturePageBaseX());
  tp[1] = static_cast<float>(cmd->draw_mode.GetTexturePageBaseY());
  tp[2] = static_cast<float>(cmd->palette.GetXBase());
  tp[3] = static_cast<float>(cmd->palette.GetYBase());
}

static int texmode_of(const GPUBackendDrawCommand* cmd)
{
  switch (static_cast<int>(cmd->draw_mode.texture_mode.GetValue()))
  {
    case GPUTextureMode::Palette4Bit: return 1;
    case GPUTextureMode::Palette8Bit: return 2;
    default:                          return 3;
  }
}

void GPU_HW_D3D9_Backend::DrawPolygon(const GPUBackendDrawPolygonCommand* cmd)
{
  HW_PF_SCOPE(PF_PRIM);
  const GPURenderCommand rc = cmd->rc;
  const bool textured = rc.texture_enable;
  const bool shaded = rc.shading_enable;
  const int transparency = rc.transparency_enable ? static_cast<int>(cmd->draw_mode.transparency_mode.GetValue()) : 4;
  const u32 nv = (cmd->num_vertices >= 4) ? 4u : 3u;
  const Common::Rectangle<u32>& da = m_drawing_area;
  float tp[4];
  float depth;
  u32 col[4];
  s32 px[4], py[4];
  float fx[4], fy[4], fw[4];
  u32 i;

  if (da.left > da.right || da.top > da.bottom)
    return;

  depth = BeginPrimitive(cmd, textured, texmode_of(cmd), rc.raw_texture_enable, transparency, DitherOn(cmd));
  m_st_prims++;
  if (DUCK_HW_DIAG && m_presents_total >= 600 && m_log_polys < 8)
  {
    m_log_polys++;
    hw_log(1, "[DUCK-HW] poly n=%u tex=%d tr=%d (%d,%d) (%d,%d) (%d,%d) c=%06X\n", cmd->num_vertices,
           textured ? texmode_of(cmd) : 0, transparency, cmd->vertices[0].x, cmd->vertices[0].y,
           cmd->vertices[1].x, cmd->vertices[1].y, cmd->vertices[2].x, cmd->vertices[2].y,
           cmd->vertices[0].color & 0xFFFFFFu);
    hw_log(1, "[DUCK-HW] poly area=(%u,%u)-(%u,%u) z=%.5f mask=%d/%d\n", da.left, da.top, da.right, da.bottom,
           depth, (int)cmd->params.set_mask_while_drawing, (int)cmd->params.check_mask_before_draw);
  }
  texpage_floats(cmd, tp);
  EnsureSpace(6);
  if (textured)
  {
    u32 mnu = cmd->vertices[0].texcoord & 0xFF, mxu = mnu;
    u32 mnv = cmd->vertices[0].texcoord >> 8, mxv = mnv;
    for (i = 1; i < nv; i++)
    {
      const u32 tu = cmd->vertices[i].texcoord & 0xFF, tv = (cmd->vertices[i].texcoord >> 8) & 0xFF;
      mnu = hw_min(mnu, tu); mxu = hw_max(mxu, tu);
      mnv = hw_min(mnv, tv); mxv = hw_max(mxv, tv);
    }
    if (mnu != mxu) mxu--;
    if (mnv != mxv) mxv--;
    SetUVLimits(mnu, mxu, mnv, mxv);
  }
  if (Dumping())
  {
    /* Cabecera y luego un renglon por vertice: x,y, color, u,v. */
    u32 dv;
    hw_log(1, "[DUCK-DUMP] P%u t%d r%d tr%d d%d g%d m%d%d pg(%u,%u) cl(%u,%u)\n", cmd->num_vertices,
           textured ? texmode_of(cmd) : 0, rc.raw_texture_enable ? 1 : 0, transparency,
           DitherOn(cmd) ? 1 : 0, shaded ? 1 : 0,
           (int)cmd->params.set_mask_while_drawing, (int)cmd->params.check_mask_before_draw, (unsigned)tp[0],
           (unsigned)tp[1], (unsigned)tp[2], (unsigned)tp[3]);
    for (dv = 0; dv < nv; dv++)
    {
      hw_log(1, "[DUCK-DUMP]   v%u (%d,%d) c%06X uv(%u,%u)\n", dv, cmd->vertices[dv].x, cmd->vertices[dv].y,
             cmd->vertices[dv].color & 0xFFFFFFu, (unsigned)(cmd->vertices[dv].texcoord & 0xFF),
             (unsigned)(cmd->vertices[dv].texcoord >> 8));
      if (cmd->precise)
        hw_log(1, "[DUCK-DUMP]     pgxp (%.3f,%.3f) w%.4f%s\n", cmd->vertices[dv].px, cmd->vertices[dv].py,
               cmd->vertices[dv].pw, cmd->precise_w ? "" : " (w no)");
    }
  }

  for (i = 0; i < nv; i++)
  {
    px[i] = cmd->vertices[i].x;
    py[i] = cmd->vertices[i].y;
    col[i] = shaded ? (cmd->vertices[i].color & 0x00FFFFFFu) : (rc.bits & 0x00FFFFFFu);
    /* [PGXP] Lo que se dibuja: con decimales y w si las hay.  Los descartes
     * y el rectangulo sucio siguen con las enteras (gpu_hw.cpp:741). */
    if (cmd->precise)
    {
      fx[i] = cmd->vertices[i].px;
      fy[i] = cmd->vertices[i].py;
      fw[i] = cmd->precise_w ? cmd->vertices[i].pw : 1.0f;
    }
    else
    {
      fx[i] = static_cast<float>(px[i]);
      fy[i] = static_cast<float>(py[i]);
      fw[i] = 1.0f;
    }
  }

  /* Dos triangulos como gpu_hw.cpp:735-805: (0,1,2) y (2,1,3), cada uno
   * descartado si excede MAX_PRIMITIVE_WIDTH/HEIGHT. */
  {
    static const u32 tris[2][3] = { { 0, 1, 2 }, { 2, 1, 3 } };
    const u32 ntris = (nv == 4) ? 2u : 1u;
    u32 t;
    for (t = 0; t < ntris; t++)
    {
      const u32 a = tris[t][0], b = tris[t][1], c = tris[t][2];
      s32 minx = px[a], maxx = px[a], miny = py[a], maxy = py[a];
      if (px[b] < minx) minx = px[b]; if (px[b] > maxx) maxx = px[b];
      if (px[c] < minx) minx = px[c]; if (px[c] > maxx) maxx = px[c];
      if (py[b] < miny) miny = py[b]; if (py[b] > maxy) maxy = py[b];
      if (py[c] < miny) miny = py[c]; if (py[c] > maxy) maxy = py[c];
      if ((maxx - minx) >= MAX_PRIMITIVE_WIDTH || (maxy - miny) >= MAX_PRIMITIVE_HEIGHT)
        continue;

      {
        /* Con decimales el borde puede pisar un pixel mas alla de la entera. */
        const s32 m = cmd->precise ? 1 : 0;
        const s32 cl = DUCK_CLAMP(minx - m, static_cast<s32>(da.left), static_cast<s32>(da.right));
        const s32 cr = DUCK_CLAMP(maxx + m, static_cast<s32>(da.left), static_cast<s32>(da.right)) + 1;
        const s32 ct = DUCK_CLAMP(miny - m, static_cast<s32>(da.top), static_cast<s32>(da.bottom));
        const s32 cb = DUCK_CLAMP(maxy + m, static_cast<s32>(da.top), static_cast<s32>(da.bottom)) + 1;
        IncludeDirty(static_cast<u32>(cl), static_cast<u32>(ct), static_cast<u32>(cr), static_cast<u32>(cb));
      }

      {
        const u32 idx[3] = { a, b, c };
        u32 k;
        for (k = 0; k < 3; k++)
        {
          const GPUBackendDrawPolygonCommand::Vertex& v = cmd->vertices[idx[k]];
          AddVertex(fx[idx[k]], fy[idx[k]], depth, col[idx[k]], static_cast<float>(v.texcoord & 0xFF),
                    static_cast<float>(v.texcoord >> 8), tp, fw[idx[k]]);
        }
      }
    }
  }

  if (!m_in_frame)
    EndOutsideFrame();
}

void GPU_HW_D3D9_Backend::DrawRectangle(const GPUBackendDrawRectangleCommand* cmd)
{
  HW_PF_SCOPE(PF_PRIM);
  const GPURenderCommand rc = cmd->rc;
  const bool textured = rc.texture_enable;
  const int transparency = rc.transparency_enable ? static_cast<int>(cmd->draw_mode.transparency_mode.GetValue()) : 4;
  const Common::Rectangle<u32>& da = m_drawing_area;
  const s32 pos_x = cmd->x;
  const s32 pos_y = cmd->y;
  const s32 rw = cmd->width;
  const s32 rh = cmd->height;
  const u32 color = cmd->color & 0x00FFFFFFu;
  const u16 orig_left = static_cast<u16>(cmd->texcoord & 0xFF);
  const u16 orig_top = static_cast<u16>(cmd->texcoord >> 8);
  float tp[4];
  float depth;
  u16 tex_top;
  s32 y_off;

  if (rw <= 0 || rh <= 0 || rw >= MAX_PRIMITIVE_WIDTH || rh >= MAX_PRIMITIVE_HEIGHT)
    return;
  if (da.left > da.right || da.top > da.bottom)
    return;

  depth = BeginPrimitive(cmd, textured, texmode_of(cmd), rc.raw_texture_enable, transparency, false);
  m_st_prims++;
  m_ct[CT_RECT]++;
  if (rw == 1 || rh == 1) m_ct[CT_RECT_THIN]++;
  texpage_floats(cmd, tp);
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] R t%d r%d tr%d m%d%d pg(%u,%u) cl(%u,%u) (%d,%d) %dx%d c%06X uv(%u,%u)\n",
           textured ? texmode_of(cmd) : 0, rc.raw_texture_enable ? 1 : 0, transparency,
           (int)cmd->params.set_mask_while_drawing, (int)cmd->params.check_mask_before_draw, (unsigned)tp[0],
           (unsigned)tp[1], (unsigned)tp[2], (unsigned)tp[3], pos_x, pos_y, rw, rh, color, (unsigned)(cmd->texcoord & 0xFF),
           (unsigned)(cmd->texcoord >> 8));

  /* gpu_hw.cpp:858: partir en trozos que no crucen la pagina de 256. */
  tex_top = orig_top;
  for (y_off = 0; y_off < rh;)
  {
    const s32 qh = hw_min(rh - y_off, static_cast<s32>(TEXTURE_PAGE_HEIGHT) - static_cast<s32>(tex_top));
    const float y0 = static_cast<float>(pos_y + y_off);
    const float y1 = y0 + static_cast<float>(qh);
    const float v0 = static_cast<float>(tex_top);
    const float v1 = static_cast<float>(tex_top + qh);
    u16 tex_left = orig_left;
    s32 x_off;
    for (x_off = 0; x_off < rw;)
    {
      const s32 qw = hw_min(rw - x_off, static_cast<s32>(TEXTURE_PAGE_WIDTH) - static_cast<s32>(tex_left));
      const float x0 = static_cast<float>(pos_x + x_off);
      const float x1 = x0 + static_cast<float>(qw);
      const float u0 = static_cast<float>(tex_left);
      const float u1 = static_cast<float>(tex_left + qw);
      EnsureSpace(6);
      /* gpu_hw.cpp:888: el rango de cada trozo */
      SetUVLimits(tex_left, tex_left + qw - 1, tex_top, tex_top + qh - 1);
      AddVertex(x0, y0, depth, color, u0, v0, tp);
      AddVertex(x1, y0, depth, color, u1, v0, tp);
      AddVertex(x0, y1, depth, color, u0, v1, tp);
      AddVertex(x0, y1, depth, color, u0, v1, tp);
      AddVertex(x1, y0, depth, color, u1, v0, tp);
      AddVertex(x1, y1, depth, color, u1, v1, tp);
      x_off += qw;
      tex_left = 0;
    }
    y_off += qh;
    tex_top = 0;
  }

  {
    const s32 cl = DUCK_CLAMP(pos_x, static_cast<s32>(da.left), static_cast<s32>(da.right));
    const s32 cr = DUCK_CLAMP(pos_x + rw, static_cast<s32>(da.left), static_cast<s32>(da.right)) + 1;
    const s32 ct = DUCK_CLAMP(pos_y, static_cast<s32>(da.top), static_cast<s32>(da.bottom));
    const s32 cb = DUCK_CLAMP(pos_y + rh, static_cast<s32>(da.top), static_cast<s32>(da.bottom)) + 1;
    IncludeDirty(static_cast<u32>(cl), static_cast<u32>(ct), static_cast<u32>(cr), static_cast<u32>(cb));
  }

  if (!m_in_frame)
    EndOutsideFrame();
}

/* gpu_hw.cpp:585 -- una linea es un quad de 1 pixel de grosor en el eje
 * menor, con el extremo final incluido. */
void GPU_HW_D3D9_Backend::DrawLineQuad(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float depth)
{
  static const float notp[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  float ax[4], ay[4];
  u32 ac[4];

  if (dx == 0.0f && dy == 0.0f)
  {
    ax[0] = x0;        ay[0] = y0;        ac[0] = c0;
    ax[1] = x0 + 1.0f; ay[1] = y0;        ac[1] = c0;
    ax[2] = x1;        ay[2] = y1 + 1.0f; ac[2] = c0;
    ax[3] = x1 + 1.0f; ay[3] = y1 + 1.0f; ac[3] = c0;
  }
  else
  {
    const float abs_dx = (dx < 0.0f) ? -dx : dx;
    const float abs_dy = (dy < 0.0f) ? -dy : dy;
    float fill_dx, fill_dy, dxdk, dydk;
    float pad_x0 = 0.0f, pad_x1 = 0.0f, pad_y0 = 0.0f, pad_y1 = 0.0f;
    if (abs_dx > abs_dy)
    {
      fill_dx = 0.0f; fill_dy = 1.0f; dxdk = 1.0f; dydk = dy / abs_dx;
      if (dx > 0.0f) { pad_x1 = 1.0f; pad_y1 = dydk; }
      else           { pad_x0 = 1.0f; pad_y0 = -dydk; }
    }
    else
    {
      fill_dx = 1.0f; fill_dy = 0.0f; dydk = 1.0f; dxdk = dx / abs_dy;
      if (dy > 0.0f) { pad_y1 = 1.0f; pad_x1 = dxdk; }
      else           { pad_y0 = 1.0f; pad_x0 = -dxdk; }
    }
    UNREFERENCED_VARIABLE(dxdk);
    UNREFERENCED_VARIABLE(dydk);
    {
      const float ox0 = x0 + pad_x0, oy0 = y0 + pad_y0, ox1 = x1 + pad_x1, oy1 = y1 + pad_y1;
      ax[0] = ox0;           ay[0] = oy0;           ac[0] = c0;
      ax[1] = ox0 + fill_dx; ay[1] = oy0 + fill_dy; ac[1] = c0;
      ax[2] = ox1;           ay[2] = oy1;           ac[2] = c1;
      ax[3] = ox1 + fill_dx; ay[3] = oy1 + fill_dy; ac[3] = c1;
    }
  }

  EnsureSpace(6);
  AddVertex(ax[0], ay[0], depth, ac[0], 0.0f, 0.0f, notp);
  AddVertex(ax[1], ay[1], depth, ac[1], 0.0f, 0.0f, notp);
  AddVertex(ax[2], ay[2], depth, ac[2], 0.0f, 0.0f, notp);
  AddVertex(ax[3], ay[3], depth, ac[3], 0.0f, 0.0f, notp);
  AddVertex(ax[2], ay[2], depth, ac[2], 0.0f, 0.0f, notp);
  AddVertex(ax[1], ay[1], depth, ac[1], 0.0f, 0.0f, notp);
}

void GPU_HW_D3D9_Backend::DrawLine(const GPUBackendDrawLineCommand* cmd)
{
  HW_PF_SCOPE(PF_PRIM);
  const GPURenderCommand rc = cmd->rc;
  const bool shaded = rc.shading_enable;
  const int transparency = rc.transparency_enable ? static_cast<int>(cmd->draw_mode.transparency_mode.GetValue()) : 4;
  const Common::Rectangle<u32>& da = m_drawing_area;
  float depth;
  u32 i;

  if (cmd->num_vertices < 2)
    return;
  if (da.left > da.right || da.top > da.bottom)
    return;

  depth = BeginPrimitive(cmd, false, 0, false, transparency, DitherOn(cmd));
  m_st_prims++;
  m_ct[CT_LINE]++;
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] L%u tr%d m%d%d (%d,%d)->(%d,%d) c%06X\n", cmd->num_vertices, transparency,
           (int)cmd->params.set_mask_while_drawing, (int)cmd->params.check_mask_before_draw, cmd->vertices[0].x,
           cmd->vertices[0].y, cmd->vertices[1].x, cmd->vertices[1].y, cmd->vertices[0].color & 0xFFFFFFu);

  for (i = 0; i + 1 < cmd->num_vertices; i++)
  {
    const s32 sx = cmd->vertices[i].x, sy = cmd->vertices[i].y;
    const s32 ex = cmd->vertices[i + 1].x, ey = cmd->vertices[i + 1].y;
    const u32 sc = shaded ? (cmd->vertices[i].color & 0x00FFFFFFu) : (rc.bits & 0x00FFFFFFu);
    const u32 ec = shaded ? (cmd->vertices[i + 1].color & 0x00FFFFFFu) : sc;
    const s32 minx = hw_min(sx, ex), maxx = hw_max(sx, ex);
    const s32 miny = hw_min(sy, ey), maxy = hw_max(sy, ey);
    if ((maxx - minx) >= MAX_PRIMITIVE_WIDTH || (maxy - miny) >= MAX_PRIMITIVE_HEIGHT)
      continue;
    {
      const s32 cl = DUCK_CLAMP(minx, static_cast<s32>(da.left), static_cast<s32>(da.right));
      const s32 cr = DUCK_CLAMP(maxx, static_cast<s32>(da.left), static_cast<s32>(da.right)) + 1;
      const s32 ct = DUCK_CLAMP(miny, static_cast<s32>(da.top), static_cast<s32>(da.bottom));
      const s32 cb = DUCK_CLAMP(maxy, static_cast<s32>(da.top), static_cast<s32>(da.bottom)) + 1;
      IncludeDirty(static_cast<u32>(cl), static_cast<u32>(ct), static_cast<u32>(cr), static_cast<u32>(cb));
    }
    DrawLineQuad(static_cast<float>(sx), static_cast<float>(sy), sc, static_cast<float>(ex), static_cast<float>(ey),
                 ec, depth);
  }

  if (!m_in_frame)
    EndOutsideFrame();
}

void GPU_HW_D3D9_Backend::DrawBatch(int render_mode)
{
  /* Factores de transparencia (gpu_hw.cpp:1398): origen, destino. */
  static const float factors[4][2] = { { 0.5f, 0.5f }, { 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.25f, 1.0f } };
  const BatchKey& k = m_batch;
  const bool blend = (render_mode == 1 || render_mode == 3);
  float c[5][4];

  std::memset(c, 0, sizeof(c));
  if (k.transparency < 4)
    c[0][0] = factors[k.transparency][0];
  c[0][1] = static_cast<float>(render_mode);
  c[0][2] = k.set_mask ? 1.0f : 0.0f;
  c[0][3] = k.dithering ? 1.0f : 0.0f;
  c[1][0] = k.interlaced ? 1.0f : 0.0f;
  c[1][1] = static_cast<float>(k.field);
  c[1][3] = g_pgxp_enabled ? 1.0f : 0.0f;   /* limitar u/v al poligono */

  if (m_batch_tex && (k.win_and_x != 0xFF || k.win_and_y != 0xFF || k.win_or_x != 0 || k.win_or_y != 0))
  {
    /* and = ~(mascara * 8): los bits 3..7 que FALTAN son la mascara. */
    const u32 mx = (~static_cast<u32>(k.win_and_x) >> 3) & 0x1F;
    const u32 my = (~static_cast<u32>(k.win_and_y) >> 3) & 0x1F;
    int b;
    c[1][2] = 1.0f;
    for (b = 0; b < 4; b++)
    {
      c[2][b] = static_cast<float>((mx >> b) & 1);
      c[3][b] = static_cast<float>((my >> b) & 1);
    }
    c[4][0] = static_cast<float>((mx >> 4) & 1);
    c[4][1] = static_cast<float>((my >> 4) & 1);
    c[4][2] = static_cast<float>(k.win_or_x);
    c[4][3] = static_cast<float>(k.win_or_y);
  }

#if !DUCK_HW_STATE_CACHE
  m_cur_psc_valid = false;
  m_cur_blend = m_cur_bop = m_cur_zfunc = 0xFFFFFFFFu;
  m_cur_bf = HW_BF_NONE;
#endif
  if (!m_cur_psc_valid || std::memcmp(m_cur_psc, c, sizeof(c)) != 0)
  {
    m_dev->SetPixelShaderConstantF(0, &c[0][0], 5);
    std::memcpy(m_cur_psc, c, sizeof(c));
    m_cur_psc_valid = true;
  }

  {
    const u32 want_blend = blend ? 1u : 0u;
    if (m_cur_blend != want_blend)
    {
      m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, blend ? TRUE : FALSE);
      m_cur_blend = want_blend;
    }
  }
  if (blend)
  {
    const u32 f = (factors[k.transparency][1] >= 1.0f) ? 255u : 128u;
    const u32 bf = D3DCOLOR_ARGB(f, f, f, f);
    const u32 bop = (k.transparency == 2) ? static_cast<u32>(D3DBLENDOP_REVSUBTRACT) : static_cast<u32>(D3DBLENDOP_ADD);
    if (m_cur_bf != bf)
    {
      m_dev->SetRenderState(D3DRS_BLENDFACTOR, bf);
      m_cur_bf = bf;
    }
    if (m_cur_bop != bop)
    {
      m_dev->SetRenderState(D3DRS_BLENDOP, bop);
      m_cur_bop = bop;
    }
  }
  {
    const u32 zf = k.check_mask ? static_cast<u32>(D3DCMP_GREATEREQUAL) : static_cast<u32>(D3DCMP_ALWAYS);
    if (m_cur_zfunc != zf)
    {
      m_dev->SetRenderState(D3DRS_ZFUNC, zf);
      m_cur_zfunc = zf;
    }
  }

  {
#if defined(_XBOX)
    const HRESULT hr = m_dev->DrawPrimitive(D3DPT_TRIANGLELIST, m_vb_start, m_nverts / 3);
#else
    const HRESULT hr = m_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_nverts / 3, m_verts, sizeof(HWVertex));
#endif
    if (FAILED(hr) && !m_warned_draw)
    {
      m_warned_draw = true;
      hw_log(3, "[DUCK-HW] dibujo de lote fallo 0x%08X (%u vertices)\n", (unsigned)hr, m_nverts);
    }
  }
  m_st_draws++;
}

void GPU_HW_D3D9_Backend::FlushRender()
{
  const BatchKey& k = m_batch;
  if (m_nverts == 0 || !m_bound)
  {
    m_nverts = 0;
    m_batch_tex = false;
    m_flush_reason = BRK_OTHER;
    return;
  }

  hw_check_owner("FlushRender", m_owner_thread);
  m_brk[m_flush_reason]++;
  m_flush_reason = BRK_OTHER;
#if !DUCK_HW_STATE_CACHE
  m_pipe_valid = false;
#endif

  HW_PF_BEGIN(flush);
#if defined(_XBOX)
  /* Al buffer de vertices del anillo (las dos pasadas dibujan de la misma
   * copia).  El stream se religa siempre: DrawPrimitiveUP lo suelta. */
  if (!m_vb_data || m_vb_used + m_nverts > MAX_BATCH_VERTICES)
  {
    HW_PF_BEGIN(vbl);
    RotateVB();
    HW_PF_END(vbl, PF_VBLOCK);
  }
  if (!m_vb_data)
  {
    m_nverts = 0;
    m_batch_tex = false;
    return;
  }
  std::memcpy(m_vb_data + m_vb_used, m_verts, m_nverts * sizeof(HWVertex));
  m_vb_start = m_vb_used;
  m_vb_used += m_nverts;
#endif
  /* [rendimiento] Con cientos de lotes por frame (GT2 en carrera: ~1.400
   * dibujos/frame, flush=3-5 ms) lo caro es la llamada, no el dibujo.  El
   * pipeline de los lotes se pone una vez y solo se repite lo que cambia;
   * cualquier operacion que lo toque (subida, copia, salida, Resolve...)
   * pone m_pipe_valid = false. */
  if (!m_pipe_valid)
  {
    const float vsc[4] = { 2.0f / VRAM_WIDTH, 2.0f / VRAM_HEIGHT, 0.5f + HALF_PIXEL, 0.0f };
    m_dev->SetVertexDeclaration(m_decl_batch);
    m_dev->SetVertexShader(m_vs_batch);
    m_dev->SetVertexShaderConstantF(0, vsc, 1);
    m_dev->SetTexture(0, m_read_tex);
    m_dev->SetTexture(1, m_dither_tex);
    m_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    m_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR);
    m_cur_ps = 0;
    m_cur_vb = 0;
    m_cur_psc_valid = false;
    m_cur_blend = 0xFFFFFFFFu;
    m_cur_bf = HW_BF_NONE;
    m_cur_bop = 0xFFFFFFFFu;
    m_cur_zfunc = 0xFFFFFFFFu;
    m_pipe_valid = true;
  }
#if defined(_XBOX)
  if (m_cur_vb != m_vb[m_vb_idx])
  {
    m_dev->SetStreamSource(0, m_vb[m_vb_idx], 0, sizeof(HWVertex));
    m_cur_vb = m_vb[m_vb_idx];
  }
#endif
  if (m_cur_ps != m_ps_batch[m_batch_tex ? 1 : 0][0])
  {
    m_cur_ps = m_ps_batch[m_batch_tex ? 1 : 0][0];
    m_dev->SetPixelShader(m_cur_ps);
  }

  if (k.transparency == 4)
    DrawBatch(0);
  else if (m_batch_tex)
  {
    /* gpu_hw.h:372 sin dual-source: opacos sin mezclar, luego los
     * semitransparentes mezclados. */
    DrawBatch(2);
    DrawBatch(3);
  }
  else
    DrawBatch(1);

  m_st_verts += m_nverts;
  m_nverts = 0;
  m_batch_tex = false;
  HW_PF_END(flush, PF_FLUSH);
}

/* ======================================================================
 * Operaciones de VRAM
 * ==================================================================== */

void GPU_HW_D3D9_Backend::DrawUtilRect(float x0, float y0, float x1, float y1, float rt_w, float rt_h)
{
  float v[4][4];
  const float vsc[4] = { 2.0f / rt_w, 2.0f / rt_h, HALF_PIXEL, 0.0f };
  v[0][0] = x0; v[0][1] = y0; v[1][0] = x1; v[1][1] = y0;
  v[2][0] = x0; v[2][1] = y1; v[3][0] = x1; v[3][1] = y1;
  for (int i = 0; i < 4; i++) { v[i][2] = 0.0f; v[i][3] = 1.0f; }
  m_dev->SetVertexDeclaration(m_decl_util);
  m_dev->SetVertexShader(m_vs_util);
  m_dev->SetVertexShaderConstantF(0, vsc, 1);
  {
    const HRESULT hr = m_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
    if (FAILED(hr) && !m_warned_util)
    {
      m_warned_util = true;
      hw_log(3, "[DUCK-HW] DrawUtilRect fallo 0x%08X\n", (unsigned)hr);
    }
  }
}

/* [traza] Lee la VRAM y la textura de salida a la CPU y cuenta pixeles que no
 * son negros: dice si el problema esta antes de la salida (VRAM vacia), en la
 * pasada de salida (VRAM con imagen, salida negra) o en el frontend (salida
 * con imagen).  Solo unas pocas veces: cada lectura para la GPU. */
void GPU_HW_D3D9_Backend::DiagDump(IDirect3DSurface9* out_surf, s32 vram_x, s32 vram_y, s32 cw, s32 ch)
{
#if defined(_XBOX)
  /* Solo PC (GetRenderTargetData).  En la Xbox no hay diagnostico de lectura. */
  UNREFERENCED_VARIABLE(out_surf); UNREFERENCED_VARIABLE(vram_x); UNREFERENCED_VARIABLE(vram_y);
  UNREFERENCED_VARIABLE(cw); UNREFERENCED_VARIABLE(ch);
#else
  D3DLOCKED_RECT lr;
  HRESULT hr;
  u32 x, y, nz_all = 0, nz_area = 0, mask = 0, cpu_nz = 0;
  u32 cells[8][16];
  D3DCOLOR center = 0;
  const u32 ax = (u32)vram_x & VRAM_WIDTH_MASK, ay = (u32)vram_y & VRAM_HEIGHT_MASK;
  const u32 aw = (cw > 0) ? (u32)cw : 0, ah = (ch > 0) ? (u32)ch : 0;

  std::memset(cells, 0, sizeof(cells));
  hr = m_dev->GetRenderTargetData(m_vram_surf, m_sys_surf);
  if (FAILED(hr))
    hw_log(3, "[DUCK-HW] diag: GetRenderTargetData(VRAM) 0x%08X\n", (unsigned)hr);
  else if (SUCCEEDED(hr = m_sys_surf->LockRect(&lr, NULL, D3DLOCK_READONLY)))
  {
    for (y = 0; y < VRAM_HEIGHT; y++)
    {
      const D3DCOLOR* row = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) + y * lr.Pitch);
      for (x = 0; x < VRAM_WIDTH; x++)
      {
        const D3DCOLOR p = row[x];
        if (p & 0x00FFFFFFu)
        {
          nz_all++;
          cells[y >> 6][x >> 6]++;
          if (x - ax < aw && y - ay < ah)
            nz_area++;
        }
        if (p & 0xFF000000u)
          mask++;
      }
    }
    if (aw && ah)
      center = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) +
                                                 ((ay + ah / 2) & VRAM_HEIGHT_MASK) * lr.Pitch)[(ax + aw / 2) & VRAM_WIDTH_MASK];
    m_sys_surf->UnlockRect();
  }
  else
    hw_log(3, "[DUCK-HW] diag: LockRect(VRAM) 0x%08X\n", (unsigned)hr);

  for (y = 0; y < ah && y < VRAM_HEIGHT; y++)
    for (x = 0; x < aw && x < VRAM_WIDTH; x++)
      if (m_vram_ptr[((ay + y) & VRAM_HEIGHT_MASK) * VRAM_WIDTH + ((ax + x) & VRAM_WIDTH_MASK)] & 0x7FFF)
        cpu_nz++;

  hw_log(1, "[DUCK-HW] diag #%u vram: total=%u visible=%u mask=%u\n", m_presents_total, nz_all, nz_area, mask);
  hw_log(1, "[DUCK-HW] diag #%u area (%u,%u) %ux%u centro=%08X cpu=%u\n", m_presents_total, ax, ay, aw, ah,
         (unsigned)center, cpu_nz);
  /* Mapa de la VRAM en celdas de 64x64: . vacia, 1-9 decimas llenas, # llena. */
  for (y = 0; y < 8; y++)
  {
    char line[17];
    for (x = 0; x < 16; x++)
    {
      const u32 n = cells[y][x];
      line[x] = (n == 0) ? '.' : (n >= 4096u) ? '#' : static_cast<char>('1' + hw_min(n * 10u / 4096u, 8u));
    }
    line[16] = 0;
    hw_log(1, "[DUCK-HW] map y=%3u |%s|\n", y * 64u, line);
  }

  {
    IDirect3DSurface9* sys = 0;
    u32 nz_out = 0;
    D3DCOLOR oc = 0;
    hr = m_dev->CreateOffscreenPlainSurface(m_out_w, m_out_h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL);
    if (FAILED(hr))
    {
      hw_log(3, "[DUCK-HW] diag: superficie de salida 0x%08X\n", (unsigned)hr);
      return;
    }
    hr = m_dev->GetRenderTargetData(out_surf, sys);
    if (SUCCEEDED(hr) && SUCCEEDED(hr = sys->LockRect(&lr, NULL, D3DLOCK_READONLY)))
    {
      for (y = 0; y < m_out_h; y++)
      {
        const D3DCOLOR* row = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) + y * lr.Pitch);
        for (x = 0; x < m_out_w; x++)
          if (row[x] & 0x00FFFFFFu)
            nz_out++;
      }
      oc = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) + (m_out_h / 2) * lr.Pitch)[m_out_w / 2];
      sys->UnlockRect();
      hw_log(1, "[DUCK-HW] diag #%u salida %ux%u: no-negros=%u centro=%08X\n", m_presents_total, m_out_w, m_out_h,
             nz_out, (unsigned)oc);
    }
    else
      hw_log(3, "[DUCK-HW] diag: lectura de la salida 0x%08X\n", (unsigned)hr);
    sys->Release();
  }
#endif
}

static u8 c5to8(u32 c5) { return static_cast<u8>((c5 * 255u + 15u) / 31u); }

void GPU_HW_D3D9_Backend::FillVRAM(u32 x, u32 y, u32 width, u32 height, u32 color,
                                   GPUBackendCommandParameters params)
{
  D3DRECT rects[4];
  DWORD n = 0;
  const u32 r5 = (color & 0xFFu) >> 3, g5 = ((color >> 8) & 0xFFu) >> 3, b5 = ((color >> 16) & 0xFFu) >> 3;
  /* gpu_hw.cpp:1072: a 15 bits se pierde la precision; con true color, el
   * color de 24 bits tal cual. */
  const D3DCOLOR c = m_tc ? D3DCOLOR_ARGB(0, color & 0xFFu, (color >> 8) & 0xFFu, (color >> 16) & 0xFFu)
                          : D3DCOLOR_ARGB(0, c5to8(r5), c5to8(g5), c5to8(b5));
  u32 w1, h1;

  UNREFERENCED_VARIABLE(params);
  if (width == 0 || height == 0)
    return;
  x &= VRAM_WIDTH_MASK;
  y &= VRAM_HEIGHT_MASK;
  if (width > VRAM_WIDTH) width = VRAM_WIDTH;
  if (height > VRAM_HEIGHT) height = VRAM_HEIGHT;

  Bind();
  FlushRender();

  /* Con envoltura: hasta cuatro trozos. */
  w1 = hw_min(width, static_cast<u32>(VRAM_WIDTH) - x);
  h1 = hw_min(height, static_cast<u32>(VRAM_HEIGHT) - y);
  rects[n].x1 = x; rects[n].y1 = y; rects[n].x2 = x + w1; rects[n].y2 = y + h1; n++;
  if (w1 < width)
  { rects[n].x1 = 0; rects[n].y1 = y; rects[n].x2 = width - w1; rects[n].y2 = y + h1; n++; }
  if (h1 < height)
  { rects[n].x1 = x; rects[n].y1 = 0; rects[n].x2 = x + w1; rects[n].y2 = height - h1; n++; }
  if (w1 < width && h1 < height)
  { rects[n].x1 = 0; rects[n].y1 = 0; rects[n].x2 = width - w1; rects[n].y2 = height - h1; n++; }

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  /* [Xbox / MGS] El relleno NO se mezcla con nada.  Antes iba con Clear() y
   * con la mezcla que hubiera dejado el ultimo lote: en PC Clear la ignora,
   * en la Xbox no.  Desde el arreglo del BLENDFACTOR los lotes dejan la
   * mezcla buena puesta (B-F, B/2+F/2...), y los rellenos de la Xbox salian
   * mezclados con lo que hubiera debajo: el mar morado/rojo de Crash, los
   * negros que no se borraban (azul en pantalla, parpadeando segun el
   * frame).  Upstream rellena con un quad y su propio estado sin mezcla
   * (gpu_hw_d3d11.cpp FillVRAM + VRAMFill); en la Xbox se hace igual. */
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
#if defined(_XBOX)
  {
    float fc[4];
    DWORD i;
    fc[0] = static_cast<float>((c >> 16) & 0xFFu) / 255.0f;
    fc[1] = static_cast<float>((c >> 8) & 0xFFu) / 255.0f;
    fc[2] = static_cast<float>(c & 0xFFu) / 255.0f;
    fc[3] = 0.0f;
    m_dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    m_dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    m_dev->SetPixelShader(m_ps_fill);
    m_dev->SetPixelShaderConstantF(0, fc, 1);
    for (i = 0; i < n; i++)
      DrawUtilRect((float)rects[i].x1, (float)rects[i].y1, (float)rects[i].x2, (float)rects[i].y2, (float)VRAM_WIDTH,
                   (float)VRAM_HEIGHT);
  }
#else
  m_dev->Clear(n, rects, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, c, 0.0f, 0);
#endif
  m_pipe_valid = false;   /* mezcla, shader y constantes: los lotes los vuelven a poner */
  m_st_fills++;
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] F (%u,%u) %ux%u c%06X\n", x, y, width, height, color & 0xFFFFFFu);
  if (DUCK_HW_DIAG && m_presents_total >= 600 && m_log_fills < 6)
  {
    m_log_fills++;
    hw_log(1, "[DUCK-HW] fill (%u,%u) %ux%u color=%06X\n", x, y, width, height, color & 0xFFFFFFu);
  }
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

  for (DWORD i = 0; i < n; i++)
    IncludeDirty(rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
  m_tp_key_valid = false;

  if (!m_in_frame)
    EndOutsideFrame();
}

void GPU_HW_D3D9_Backend::UpdateVRAM(u32 x, u32 y, u32 width, u32 height, const void* data,
                                     GPUBackendCommandParameters params)
{
  /* El driver no la usa hoy (LoadImage va por xbox_soft); si llegara, se
   * copia a psxVuw y se sube como una carga normal. */
  const u16* src = static_cast<const u16*>(data);
  u32 r, cidx;
  if (!src || width == 0 || height == 0)
    return;
  for (r = 0; r < height; r++)
    for (cidx = 0; cidx < width; cidx++)
      m_vram_ptr[((y + r) & VRAM_HEIGHT_MASK) * VRAM_WIDTH + ((x + cidx) & VRAM_WIDTH_MASK)] = src[r * width + cidx];
  m_last_params = params;
  UploadFromCPU(x, y, width, height, true);
}

void GPU_HW_D3D9_Backend::CopyVRAM(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 width, u32 height,
                                   GPUBackendCommandParameters params)
{
  u32 w1, h1;
  float c[4];
  float depth;

  if (width == 0 || height == 0)
    return;
  src_x &= VRAM_WIDTH_MASK; src_y &= VRAM_HEIGHT_MASK;
  dst_x &= VRAM_WIDTH_MASK; dst_y &= VRAM_HEIGHT_MASK;
  if (width > VRAM_WIDTH) width = VRAM_WIDTH;
  if (height > VRAM_HEIGHT) height = VRAM_HEIGHT;

  Bind();
  FlushRender();
  m_last_params = params;

  /* El origen tiene que estar al dia en la copia de lectura. */
  RefreshReadTexture();
  depth = NextMaskedDepth(params.check_mask_before_draw);
  m_st_copies++;
  if (width <= 2 || height <= 2) m_ct[CT_COPY_THIN]++;
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] C (%u,%u)->(%u,%u) %ux%u m%d%d\n", src_x, src_y, dst_x, dst_y, width, height,
           (int)params.set_mask_while_drawing, (int)params.check_mask_before_draw);
  if (DUCK_HW_DIAG && m_log_copies < 24)
  {
    m_log_copies++;
    hw_log(1, "[DUCK-HW] copia (%u,%u)->(%u,%u) %ux%u mask=%d/%d\n", src_x, src_y, dst_x, dst_y, width, height,
           (int)params.set_mask_while_drawing, (int)params.check_mask_before_draw);
  }

  c[0] = static_cast<float>(static_cast<s32>(src_x) - static_cast<s32>(dst_x));
  c[1] = static_cast<float>(static_cast<s32>(src_y) - static_cast<s32>(dst_y));
  c[2] = params.set_mask_while_drawing ? 1.0f : 0.0f;
  c[3] = depth;

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ZFUNC, params.check_mask_before_draw ? D3DCMP_GREATEREQUAL : D3DCMP_ALWAYS);
  m_dev->SetTexture(0, m_read_tex);
  m_dev->SetPixelShader(m_ps_copy);

  /* El desplazamiento (origen - destino) vale para todos los trozos: cuando
   * el destino da la vuelta, el origen correspondiente esta 1024 (o 512) mas
   * alla, y el muestreador en WRAP lo devuelve al mismo sitio. */
  m_dev->SetPixelShaderConstantF(0, c, 1);
  w1 = hw_min(width, static_cast<u32>(VRAM_WIDTH) - dst_x);
  h1 = hw_min(height, static_cast<u32>(VRAM_HEIGHT) - dst_y);
  DrawUtilRect((float)dst_x, (float)dst_y, (float)(dst_x + w1), (float)(dst_y + h1), (float)VRAM_WIDTH,
               (float)VRAM_HEIGHT);
  IncludeDirty(dst_x, dst_y, dst_x + w1, dst_y + h1);
  if (w1 < width)
  {
    DrawUtilRect(0.0f, (float)dst_y, (float)(width - w1), (float)(dst_y + h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(0, dst_y, width - w1, dst_y + h1);
  }
  if (h1 < height)
  {
    DrawUtilRect((float)dst_x, 0.0f, (float)(dst_x + w1), (float)(height - h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(dst_x, 0, dst_x + w1, height - h1);
  }
  if (w1 < width && h1 < height)
  {
    DrawUtilRect(0.0f, 0.0f, (float)(width - w1), (float)(height - h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(0, 0, width - w1, height - h1);
  }

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_batch_valid = false;
  m_pipe_valid = false;
  m_tp_key_valid = false;

  if (!m_in_frame)
    EndOutsideFrame();
}

void GPU_HW_D3D9_Backend::UploadRegion(u32 x, u32 y, u32 w, u32 h, bool set_mask, bool check_mask, float depth)
{
  /* psxVuw -> textura de subida (A1R5G5B5, mismos bytes) en la MISMA
   * posicion de la VRAM, envuelta; luego un rectangulo por trozo. */
  D3DLOCKED_RECT lr;
  u32 r;
  float c[4];
  u32 w1, h1;

#if defined(_XBOX)
  /* Siguiente del anillo: la anterior puede seguir en uso por la GPU.
   * [MGS, caras del codec] Y ESTA TAMBIEN, si en este frame ya se han hecho
   * mas subidas que texturas tiene el anillo: en la Xbox LockRect NO espera
   * a la GPU (xbox64 por eso no reutiliza nunca una textura en vuelo: crea
   * otra, d3dq.cpp "almacen", o espera a una fence).  Sin esto, la subida
   * nueva pisaba los texels de otra que la GPU aun no habia dibujado. */
  SAFE_RELEASE(m_upload_tex);
  m_upload_idx = (m_upload_idx + 1) % HW_UPLOAD_RING;
  m_upload_tex = m_upload_ring[m_upload_idx];
  m_upload_tex->AddRef();
  if (m_upload_fence_ok[m_upload_idx] && m_dev->IsFencePending(m_upload_fence[m_upload_idx]))
  {
    m_st_upload_waits++;
    m_dev->BlockOnFence(m_upload_fence[m_upload_idx]);
  }
#endif
  {
    HRESULT hr;
    HW_PF_BEGIN(upl);
    hr = m_upload_tex->LockRect(0, &lr, NULL, HW_LOCK_CPU);
    HW_PF_END(upl, PF_UPLOCK);
    if (FAILED(hr))
      return;
  }
  for (r = 0; r < h; r++)
  {
    const u32 vy = (y + r) & VRAM_HEIGHT_MASK;
    u16* dst = reinterpret_cast<u16*>(static_cast<u8*>(lr.pBits) + vy * lr.Pitch);
    const u16* src = &m_vram_ptr[vy * VRAM_WIDTH];
    const u32 part = hw_min(w, static_cast<u32>(VRAM_WIDTH) - x);
#if defined(_XBOX)
    /* psxVuw es little-endian; la textura de 16 bits, del orden nativo. */
    u32 k;
    for (k = 0; k < part; k++)
      dst[x + k] = hw_le16(src[x + k]);
    for (k = 0; k < w - part; k++)
      dst[k] = hw_le16(src[k]);
#else
    std::memcpy(&dst[x], &src[x], part * sizeof(u16));
    if (part < w)
      std::memcpy(&dst[0], &src[0], (w - part) * sizeof(u16));
#endif
  }
  m_upload_tex->UnlockRect(0);

  c[0] = set_mask ? 1.0f : 0.0f;
  c[1] = depth;
  c[2] = 0.0f;
  c[3] = 0.0f;

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ZFUNC, check_mask ? D3DCMP_GREATEREQUAL : D3DCMP_ALWAYS);
  m_dev->SetTexture(0, m_upload_tex);
  m_dev->SetPixelShader(m_ps_upload);
  m_dev->SetPixelShaderConstantF(0, c, 1);

  w1 = hw_min(w, static_cast<u32>(VRAM_WIDTH) - x);
  h1 = hw_min(h, static_cast<u32>(VRAM_HEIGHT) - y);
  DrawUtilRect((float)x, (float)y, (float)(x + w1), (float)(y + h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
  IncludeDirty(x, y, x + w1, y + h1);
  if (w1 < w)
  {
    DrawUtilRect(0.0f, (float)y, (float)(w - w1), (float)(y + h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(0, y, w - w1, y + h1);
  }
  if (h1 < h)
  {
    DrawUtilRect((float)x, 0.0f, (float)(x + w1), (float)(h - h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(x, 0, x + w1, h - h1);
  }
  if (w1 < w && h1 < h)
  {
    DrawUtilRect(0.0f, 0.0f, (float)(w - w1), (float)(h - h1), (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    IncludeDirty(0, 0, w - w1, h - h1);
  }

  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_dev->SetTexture(0, m_read_tex);
#if defined(_XBOX)
  /* Cuando la GPU pase de aqui, esta textura del anillo vuelve a estar libre. */
  m_upload_fence[m_upload_idx] = m_dev->InsertFence();
  m_upload_fence_ok[m_upload_idx] = true;
#endif
  m_batch_valid = false;
  m_pipe_valid = false;
  m_tp_key_valid = false;
}

void GPU_HW_D3D9_Backend::UploadFromCPU(u32 x, u32 y, u32 w, u32 h, bool use_mask)
{
  bool set_mask = false, check_mask = false;
  float depth;

  if (!m_dev || w == 0 || h == 0)
    return;
  x &= VRAM_WIDTH_MASK;
  y &= VRAM_HEIGHT_MASK;
  if (w > VRAM_WIDTH) w = VRAM_WIDTH;
  if (h > VRAM_HEIGHT) h = VRAM_HEIGHT;

  Bind();
  FlushRender();
  /* [MGS] Las subidas CPU->VRAM van SIN mascara, como en el resto de este
   * core: en los renderizadores software las escribe xbox_soft directamente
   * en psxVuw (FinishedVRAMWrite) y GP0(E6) no se aplica nunca.  Upstream si
   * la aplica (gpu_commands.cpp:496), pero el resultado que hay que igualar
   * es el del software de este core, y MGS lo deja claro: con "poner
   * mascara" activo al subir una CLUT o una textura, el negro 0x0000
   * (transparente) pasaba a 0x8000 (se dibuja): el cuadrado oscuro alrededor
   * de la luz, el vaho de Snake y, con el estado viejo de la ultima
   * primitiva, las caras del codec.  `use_mask` se queda en la API por si
   * algun juego lo necesitara. */
  UNREFERENCED_VARIABLE(use_mask);
  depth = NextMaskedDepth(check_mask);
  {
    HW_PF_BEGIN(up);
    UploadRegion(x, y, w, h, set_mask, check_mask, depth);
    HW_PF_END(up, PF_UPLOAD);
  }
  m_st_uploads++;
  if (Dumping())
  {
    const GPUBackendCommandParameters cur = duck_get_current_params_for_hw();
    hw_log(1, "[DUCK-DUMP] U (%u,%u) %ux%u E6=%d%d\n", x, y, w, h, (int)cur.set_mask_while_drawing,
           (int)cur.check_mask_before_draw);
  }
  if (DUCK_HW_DIAG && m_presents_total >= 600 && m_log_uploads < 6)
  {
    m_log_uploads++;
    hw_log(1, "[DUCK-HW] upload (%u,%u) %ux%u mask=%d\n", x, y, w, h, use_mask ? 1 : 0);
  }

  if (!m_in_frame)
    EndOutsideFrame();
}

void GPU_HW_D3D9_Backend::ReadbackToCPU(u32 x, u32 y, u32 w, u32 h)
{
  D3DLOCKED_RECT lr;
  u32 r, c;
  bool ok = false;

  if (!m_dev || w == 0 || h == 0)
    return;
  if (w > VRAM_WIDTH) w = VRAM_WIDTH;
  if (h > VRAM_HEIGHT) h = VRAM_HEIGHT;

  HW_PF_BEGIN(rb);
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] S (%u,%u) %ux%u\n", x, y, w, h);
  if (DUCK_HW_DIAG && m_log_readbacks < 24)
  {
    m_log_readbacks++;
    hw_log(1, "[DUCK-HW] lectura a CPU (%u,%u) %ux%u\n", x, y, w, h);
  }
  Bind();
  FlushRender();
  m_st_readbacks++;

#if defined(_XBOX)
  /* Resolve de lo pendiente a la copia (tileada), esperar a la GPU y
   * destilarla con D3DX a la LINEAL, solo el rect pedido (si envuelve, toda).
   * Memoria de GPU leida por CPU: lento, pero StoreImage es raro y pequeno. */
  RefreshReadTexture();
  {
    HW_PF_BEGIN(idl);
    m_dev->BlockUntilIdle();
    HW_PF_END(idl, PF_IDLE);
  }
  {
    IDirect3DSurface9* dst = 0;
    RECT rc;
    if (x + w <= VRAM_WIDTH && y + h <= VRAM_HEIGHT)
    {
      rc.left = (LONG)x; rc.top = (LONG)y; rc.right = (LONG)(x + w); rc.bottom = (LONG)(y + h);
    }
    else
    {
      rc.left = 0; rc.top = 0; rc.right = VRAM_WIDTH; rc.bottom = VRAM_HEIGHT;
    }
    if (SUCCEEDED(m_sys_tex->GetSurfaceLevel(0, &dst)) && dst)
    {
      if (SUCCEEDED(D3DXLoadSurfaceFromSurface(dst, NULL, &rc, m_read_surf, NULL, &rc, D3DX_FILTER_NONE, 0)) &&
          SUCCEEDED(dst->LockRect(&lr, NULL, D3DLOCK_READONLY)))
      {
        ok = true;
        for (r = 0; r < h; r++)
        {
          const u32 vy = (y + r) & VRAM_HEIGHT_MASK;
          const D3DCOLOR* row = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) + vy * lr.Pitch);
          for (c = 0; c < w; c++)
          {
            const u32 vx = (x + c) & VRAM_WIDTH_MASK;
            m_vram_ptr[vy * VRAM_WIDTH + vx] = hw_le16(ArgbTo1555(row[vx]));
          }
        }
        dst->UnlockRect();
      }
      dst->Release();
    }
  }
#else
  if (SUCCEEDED(m_dev->GetRenderTargetData(m_vram_surf, m_sys_surf)) &&
      SUCCEEDED(m_sys_surf->LockRect(&lr, NULL, D3DLOCK_READONLY)))
  {
    ok = true;
    for (r = 0; r < h; r++)
    {
      const u32 vy = (y + r) & VRAM_HEIGHT_MASK;
      const D3DCOLOR* row = reinterpret_cast<const D3DCOLOR*>(static_cast<const u8*>(lr.pBits) + vy * lr.Pitch);
      for (c = 0; c < w; c++)
      {
        const u32 vx = (x + c) & VRAM_WIDTH_MASK;
        m_vram_ptr[vy * VRAM_WIDTH + vx] = ArgbTo1555(row[vx]);
      }
    }
    m_sys_surf->UnlockRect();
  }
#endif
  HW_PF_END(rb, PF_READBACK);
  if (!ok)
    hw_log(2, "[DUCK-HW] lectura de VRAM a CPU fallo\n");

  if (!m_in_frame)
    EndOutsideFrame();
}

/* Inversa exacta de c5 -> round(c5 * 255 / 31); el alfa es el bit 15. */
u16 GPU_HW_D3D9_Backend::ArgbTo1555(D3DCOLOR p)
{
  const u32 r5 = (((p >> 16) & 0xFF) * 31u + 127u) / 255u;
  const u32 g5 = (((p >> 8) & 0xFF) * 31u + 127u) / 255u;
  const u32 b5 = ((p & 0xFF) * 31u + 127u) / 255u;
  const u32 m = ((p >> 24) & 0xFF) >= 128u ? 1u : 0u;
  return static_cast<u16>(r5 | (g5 << 5) | (b5 << 10) | (m << 15));
}

/* ======================================================================
 * Salida a pantalla (DoBufferSwap de xbox_soft)
 * ==================================================================== */

void GPU_HW_D3D9_Backend::Present(s32 vram_x, s32 vram_y, s32 cw, s32 ch, s32 left, s32 top, s32 out_w, s32 out_h,
                                  bool rgb24)
{
  IDirect3DSurface9* out_surf = 0;
  D3DVIEWPORT9 vp;
  float c[4];

  if (!m_dev || out_w <= 0 || out_h <= 0)
    return;
#if defined(_XBOX)
  if (out_w > HW_OUT_MAX_W) out_w = HW_OUT_MAX_W;
  if (out_h > HW_OUT_MAX_H) out_h = HW_OUT_MAX_H;
#else
  if (out_w > 1024) out_w = 1024;
  if (out_h > 1024) out_h = 1024;
#endif

  HW_PF_BEGIN(pres);
  if (Dumping())
    hw_log(1, "[DUCK-DUMP] V vram(%d,%d) %dx%d en (%d,%d) de %dx%d\n", vram_x, vram_y, cw, ch, left, top, out_w, out_h);
  Bind();
  FlushRender();
#if defined(_XBOX)
  /* La EDRAM no se muestrea: la salida lee la copia, que se pone al dia. */
  RefreshReadTexture();
#endif
  m_st_presents++;
  /* [traza] solo con DUCK_HW_DIAG (la geometria de salida ya esta bien) */
  if (DUCK_HW_DIAG && (m_presents_logged < 4 ||
      (m_presents_logged < 200 &&
       (cw != m_lp[2] || ch != m_lp[3] || left != m_lp[4] ||
        top != m_lp[5] || out_w != m_lp[6] || out_h != m_lp[7] || (rgb24 ? 1 : 0) != m_lp[8]))))
  {
    /* [traza] Las 4 primeras y luego cada CAMBIO de geometria (hasta 200; la
     * posicion en VRAM no cuenta: con doble buffer cambia en cada frame):
     * para los margenes (barra a la derecha en el menu de GT2). */
    m_lp[0] = vram_x; m_lp[1] = vram_y; m_lp[2] = cw; m_lp[3] = ch; m_lp[4] = left;
    m_lp[5] = top; m_lp[6] = out_w; m_lp[7] = out_h; m_lp[8] = rgb24 ? 1 : 0;
    m_presents_logged++;
    hw_log(1, "[DUCK-HW] present: vram=(%d,%d) contenido=%dx%d en (%d,%d) imagen=%dx%d rgb24=%d\n",
           vram_x, vram_y, cw, ch, left, top, out_w, out_h, rgb24 ? 1 : 0);
  }

  /* Textura de salida del tamano exacto de la imagen: el quad de Salvia la
   * muestrea entera.  La vieja no se suelta hasta el siguiente Bind (el
   * frontend todavia la tiene ligada hasta que retro_run le de la nueva). */
  if (!m_out_tex || m_out_w != static_cast<u32>(out_w) || m_out_h != static_cast<u32>(out_h))
  {
    IDirect3DTexture9* t = 0;
    if (FAILED(m_dev->CreateTexture(out_w, out_h, 1, HW_USAGE_OUT, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, NULL)))
    {
      hw_log(3, "[DUCK-HW] salida %dx%d: CreateTexture fallo\n", out_w, out_h);
      if (!m_in_frame)
        EndOutsideFrame();
      return;
    }
    if (m_out_tex)
    {
      int i;
      for (i = 0; i < 4 && m_out_graveyard[i]; i++) {}
      if (i < 4) m_out_graveyard[i] = m_out_tex;
      else m_out_tex->Release();
    }
    m_out_tex = t;
    m_out_w = static_cast<u32>(out_w);
    m_out_h = static_cast<u32>(out_h);
    m_out_changed = true;
    hw_log(1, "[DUCK-HW] salida %dx%d\n", out_w, out_h);
  }

#if defined(_XBOX)
  /* Se pinta en la superficie de salida de la EDRAM y se resuelve a la
   * textura (abajo). */
  out_surf = m_out_rt;
  out_surf->AddRef();
#else
  m_out_tex->GetSurfaceLevel(0, &out_surf);
#endif
  m_dev->SetRenderTarget(0, out_surf);
  m_dev->SetDepthStencilSurface(NULL);
  vp.X = 0; vp.Y = 0; vp.Width = m_out_w; vp.Height = m_out_h; vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
  m_dev->SetViewport(&vp);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  m_dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  /* [GT2 / MGS] SIN Clear: los margenes (izquierdo Range.x0, derecho
   * [Range.x0+dx .. ancho), arriba/abajo) los pinta de negro la propia
   * pasada, que cubre la imagen ENTERA.  Es el arreglo del "ruido a la
   * derecha" del blit software (draw_ok.c, BlitScreen32: TexZero del margen
   * derecho; memoria project_pcsxr360_right_margin_noise), aqui dentro del
   * shader.  Con el Clear, en la Xbox se veia en el margen lo que otro habia
   * dejado en esa EDRAM (barra verde a la derecha del menu de GT2, otra que
   * parpadeaba a la izquierda). */
  if (cw > out_w - left) cw = out_w - left;
  if (ch > out_h - top)  ch = out_h - top;
  if (cw < 0) cw = 0;
  if (ch < 0) ch = 0;
  {
    float d1[4];
    c[0] = static_cast<float>(vram_x & VRAM_WIDTH_MASK);
    c[1] = static_cast<float>(vram_y & VRAM_HEIGHT_MASK);
    c[2] = static_cast<float>(left);
    c[3] = static_cast<float>(top);
    d1[0] = static_cast<float>(cw);
    d1[1] = static_cast<float>(ch);
    d1[2] = 0.0f;
    d1[3] = 0.0f;
#if defined(_XBOX)
    m_dev->SetTexture(0, m_read_tex);
#else
    m_dev->SetTexture(0, m_vram_tex);
#endif
    m_dev->SetPixelShader(rgb24 ? m_ps_disp24 : m_ps_disp15);
    m_dev->SetPixelShaderConstantF(0, c, 1);
    m_dev->SetPixelShaderConstantF(1, d1, 1);
    DrawUtilRect(0.0f, 0.0f, (float)m_out_w, (float)m_out_h, (float)m_out_w, (float)m_out_h);
  }

  m_presents_total++;
#if defined(_XBOX)
  {
    D3DRECT r;
    HRESULT hr;
    r.x1 = 0; r.y1 = 0; r.x2 = (LONG)m_out_w; r.y2 = (LONG)m_out_h;
    HW_PF_BEGIN(res);
    hr = m_dev->Resolve(D3DRESOLVE_RENDERTARGET0, &r, m_out_tex, NULL, 0, 0, NULL, 0.0f, 0, NULL);
    HW_PF_END(res, PF_RESOLVE);
    if (FAILED(hr) && !m_warned_draw)
    {
      m_warned_draw = true;
      hw_log(3, "[DUCK-HW] Resolve (salida) fallo 0x%08X\n", (unsigned)hr);
    }
  }
#endif
  if (DUCK_HW_DIAG && (m_presents_total == 30 || m_presents_total == 300 || m_presents_total == 600 ||
                       m_presents_total == 1200 || m_presents_total == 3000))
    DiagDump(out_surf, vram_x, vram_y, cw, ch);

  SAFE_RELEASE(out_surf);
  m_dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  m_dev->SetTexture(0, m_read_tex);
  SetVRAMTarget();
  m_batch_valid = false;
  m_pipe_valid = false;
  m_frame_ready = true;
  HW_PF_END(pres, PF_PRESENT);

  if (!m_in_frame)
    EndOutsideFrame();
}

bool GPU_HW_D3D9_Backend::TakeFrame(IDirect3DTexture9** tex, u32* w, u32* h, bool* changed)
{
  const bool ready = m_frame_ready && m_out_tex;
  *tex = m_out_tex;
  *w = m_out_w;
  *h = m_out_h;
  *changed = m_out_changed;
  m_out_changed = false;
  m_frame_ready = false;
  return ready;
}

void GPU_HW_D3D9_Backend::ReleaseOutput()
{
#if defined(_XBOX)
  if (m_out_tex && m_dev)
    m_dev->BlockUntilIdle();   /* el frontend la acaba de muestrear */
#endif
  SAFE_RELEASE(m_out_tex);
  m_out_w = 0;
  m_out_h = 0;
  m_out_changed = true;
  m_frame_ready = false;
}

void GPU_HW_D3D9_Backend::Reset(bool clear_vram)
{
  if (!m_dev)
    return;
  Bind();
  m_nverts = 0;
  m_batch_tex = false;
  m_batch_valid = false;
  m_pipe_valid = false;
  m_tp_key_valid = false;
  m_depth_counter = 1;
  if (clear_vram)
  {
    m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0, 0, 0, 0), 0.0f, 0);
    m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    m_dirty.valid = true;
    m_dirty.left = 0; m_dirty.top = 0; m_dirty.right = VRAM_WIDTH; m_dirty.bottom = VRAM_HEIGHT;
    RefreshReadTexture();
  }
  if (!m_in_frame)
    EndOutsideFrame();
}

static GPU_HW_D3D9_Backend* hw_backend()
{
  return static_cast<GPU_HW_D3D9_Backend*>(duck_get_backend_for_hw());
}

#endif /* DUCK_HW_D3D9 */

/* ======================================================================
 * API C (gpu_duck_c_api.h).  En las plataformas sin backend todo es no-op y
 * duck_hw_available() devuelve 0.
 * ==================================================================== */
extern "C" {

int duck_hw_enabled = 0;

void duck_hw_set_logger(void (*fn)(int level, const char* msg))
{
  s_hw_log = fn;
}

#if DUCK_HW_D3D9

int duck_hw_available(void) { return 1; }

void duck_hw_set_device(void* device)
{
  s_hw_device = static_cast<IDirect3DDevice9*>(device);
}

/* [2026-10-31] Propiedad del device con el hilo de GPU (libpcsxcore/gpu.c).
 * Xbox: el device es del hilo que lo creo (el de Salvia/retro_run) y solo ese
 * puede llamarlo; para usarlo desde otro hay que soltarlo en uno
 * (ReleaseThreadOwnership) y cogerlo en el otro (AcquireThreadOwnership).
 * PC: el device de Salvia es D3DCREATE_MULTITHREADED, no hace falta nada; solo
 * se apunta el hilo para la traza. */
void duck_hw_thread_acquire(int call_d3d)
{
#if defined(_XBOX)
  if (call_d3d && s_hw_device)
    s_hw_device->AcquireThreadOwnership();
#endif
  if (call_d3d)
    s_hw_acquires++;
  s_hw_owner_tid = GetCurrentThreadId();
}

void duck_hw_thread_release(void)
{
  s_hw_owner_tid = 0;
#if defined(_XBOX)
  if (s_hw_device)
    s_hw_device->ReleaseThreadOwnership();
#endif
}

void duck_hw_frame_begin(void)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->FrameBegin();
}

void duck_hw_frame_end(void)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->FrameEnd();
}

int duck_hw_take_frame(void** texture, unsigned* width, unsigned* height, int* changed)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  IDirect3DTexture9* t = 0;
  u32 w = 0, h = 0;
  bool ch = false, ready;
  if (!b)
    return 0;
  ready = b->TakeFrame(&t, &w, &h, &ch);
  *texture = t;
  *width = w;
  *height = h;
  *changed = ch ? 1 : 0;
  return ready ? 1 : 0;
}

void duck_hw_release_output(void)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->ReleaseOutput();
}

void duck_hw_arm_dump(void)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->ArmDump();
}

void duck_hw_upload_rect(unsigned x, unsigned y, unsigned w, unsigned h, int use_mask)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->UploadFromCPU(x, y, w, h, use_mask != 0);
}

void duck_hw_readback_rect(unsigned x, unsigned y, unsigned w, unsigned h)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (b) b->ReadbackToCPU(x, y, w, h);
}

int duck_hw_present(int vram_x, int vram_y, int content_w, int content_h, int left, int top, int out_w, int out_h,
                    int rgb24)
{
  GPU_HW_D3D9_Backend* b = duck_hw_enabled ? hw_backend() : 0;
  if (!b)
    return 0;
  b->Present(vram_x, vram_y, content_w, content_h, left, top, out_w, out_h, rgb24 != 0);
  return 1;
}

#else /* !DUCK_HW_D3D9 */

int  duck_hw_available(void) { return 0; }
void duck_hw_set_device(void* device) { UNREFERENCED_VARIABLE(device); }
void duck_hw_thread_acquire(int call_d3d) { UNREFERENCED_VARIABLE(call_d3d); }
void duck_hw_thread_release(void) {}
void duck_hw_frame_begin(void) {}
void duck_hw_frame_end(void) {}
int  duck_hw_take_frame(void** texture, unsigned* width, unsigned* height, int* changed)
{
  *texture = 0; *width = 0; *height = 0; *changed = 0;
  return 0;
}
void duck_hw_release_output(void) {}
void duck_hw_arm_dump(void) {}
void duck_hw_upload_rect(unsigned x, unsigned y, unsigned w, unsigned h, int use_mask)
{
  UNREFERENCED_VARIABLE(x); UNREFERENCED_VARIABLE(y); UNREFERENCED_VARIABLE(w); UNREFERENCED_VARIABLE(h);
  UNREFERENCED_VARIABLE(use_mask);
}
void duck_hw_readback_rect(unsigned x, unsigned y, unsigned w, unsigned h)
{
  UNREFERENCED_VARIABLE(x); UNREFERENCED_VARIABLE(y); UNREFERENCED_VARIABLE(w); UNREFERENCED_VARIABLE(h);
}
int duck_hw_present(int vram_x, int vram_y, int content_w, int content_h, int left, int top, int out_w, int out_h,
                    int rgb24)
{
  UNREFERENCED_VARIABLE(vram_x); UNREFERENCED_VARIABLE(vram_y); UNREFERENCED_VARIABLE(content_w);
  UNREFERENCED_VARIABLE(content_h); UNREFERENCED_VARIABLE(left); UNREFERENCED_VARIABLE(top);
  UNREFERENCED_VARIABLE(out_w); UNREFERENCED_VARIABLE(out_h); UNREFERENCED_VARIABLE(rgb24);
  return 0;
}

#endif /* DUCK_HW_D3D9 */

} /* extern "C" */
