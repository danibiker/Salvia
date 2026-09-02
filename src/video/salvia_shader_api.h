#ifndef SALVIA_SHADER_API_H
#define SALVIA_SHADER_API_H

/* salvia_shader_api.h - frontera entre la capa de aplicacion y los backends
 * de video para los shaders de post-proceso cargados de disco.
 *
 * Los shaders ya no viven embebidos en SDL_shaders_src.h: se descubren en
 * <appDir>\assets\shaders\*.hlslp al arrancar (formato de preset estilo
 * RetroArch) y se cargan en runtime. El trabajo pesado -enumerar el
 * directorio, parsear el ini, leer el .hlsl, decodificar el PNG de la LUT-
 * lo hace la capa de aplicacion (src/video/shaderpreset.cpp), que puede usar
 * dirutil/Fileio/SDL_image. Los backends de video solo reciben la tabla ya
 * masticada y se encargan de compilar y de aplicar estado D3D.
 *
 * Esta cabecera es NEUTRA a proposito: C puro, sin STL y sin d3d9.h, para que
 * la incluyan por igual:
 *   - src/video/shaderpreset.cpp                      (C++, productor)
 *   - src/video/win_d3d9.cpp                          (C++, consumidor)
 *   - libs/libSDLx360/SDL/src/video/xbox/SDL_xboxvideo.c  (C, consumidor)
 * El mismo patron que ya usa SDL_xboxvideo.c para incluir HLSLBackground.h.
 *
 * ORDEN DE LLAMADA (importante): initShaders() se invoca desde dentro de
 * SDL_SetVideoMode / WinD3D9_Init, asi que SalviaShader_SetTable() tiene que
 * haberse llamado ANTES. Si la tabla llega tarde, el backend arranca sin
 * ningun filtro (solo el passthrough integrado). */

#ifdef __cplusplus
extern "C" {
#endif

#define SALVIA_SHADER_MAX_PASSES    4    /* multi-pasada: hoy solo se usa la 0 */
#define SALVIA_SHADER_MAX_PRESETS  64
#define SALVIA_SHADER_MAX_LUTS      3    /* por pasada, a los samplers s1..s3 */
#define SALVIA_SHADER_ID_MAX       64

typedef enum {
    SALVIA_FILTER_NEAREST = 0,
    SALVIA_FILTER_LINEAR  = 1
} SalviaShaderFilter;

typedef enum {
    SALVIA_WRAP_CLAMP  = 0,
    SALVIA_WRAP_REPEAT = 1,
    SALVIA_WRAP_MIRROR = 2
} SalviaShaderWrap;

/* Flags de compilacion propios; el backend los traduce a D3DXSHADER_*.
 * 0 = precision parcial (half), que es el PS_FLAGS_DEFAULT de siempre. */
#define SALVIA_PS_FULL_PRECISION  0x0001

/* Tabla de consulta ya decodificada.
 *
 * CONTRATO DE FORMATO: pixels apunta a words de 32 bits 0xAARRGGBB en
 * ENDIANNESS NATIVA. En x86 eso son bytes B,G,R,A (lo que quiere
 * D3DFMT_A8R8G8B8) y en Xenon bytes A,R,G,B (lo que quiere
 * D3DFMT_LIN_A8R8G8B8). Definirlo asi hace que las dos plataformas suban la
 * textura con un memcpy por fila, sin byte-swap en ninguna. */
typedef struct SalviaShaderLut {
    const unsigned char* pixels;
    int                  width;
    int                  height;
    int                  pitch;    /* bytes por fila; puede ser > width*4 */
    SalviaShaderFilter   filter;
    SalviaShaderWrap     wrap;
    int                  sampler;  /* registro s<N> destino, 1..SALVIA_SHADER_MAX_LUTS */
} SalviaShaderLut;

typedef struct SalviaShaderPass {
    const char*        source;    /* HLSL ps_3_0 NUL-terminado.
                                     NULL = usar el passthrough integrado
                                     (asi se representan Nearest y Bilinear,
                                     que solo cambian el estado del sampler). */
    unsigned int       psFlags;   /* combinacion de SALVIA_PS_* */
    SalviaShaderFilter filter;    /* sampler s0 */
    SalviaShaderWrap   wrap;      /* sampler s0 */
    int                lutCount;
    SalviaShaderLut    luts[SALVIA_SHADER_MAX_LUTS];
} SalviaShaderPass;

typedef struct SalviaShaderPreset {
    char             id[SALVIA_SHADER_ID_MAX];  /* nombre de fichero sin extension */
    int              passCount;                 /* del `shaders = N` del preset */
    int              activePass;                /* hoy siempre 0 */
    SalviaShaderPass passes[SALVIA_SHADER_MAX_PASSES];
} SalviaShaderPreset;

/* Registra la tabla de presets. La aplicacion conserva la PROPIEDAD de los
 * buffers apuntados (source, pixels) y garantiza que siguen vivos hasta que
 * SalviaShader_LutsUploaded() devuelva 1 (para los pixeles) o hasta
 * SalviaShader_Release() (para los sources, que se recompilan tras un cambio
 * de modo de video).
 * Devuelve 1 si la tabla se acepto, 0 si no. */
int  SalviaShader_SetTable(const SalviaShaderPreset* presets, int count);

/* Numero de presets disponibles en el backend; 0 antes del registro. La app
 * lo usa como tope del menu y del ciclado con la hotkey. */
int  SalviaShader_GetCount(void);

/* 1 cuando initShaders() ya ha subido las LUT a la GPU. A partir de ese
 * momento la app puede liberar los buffers de pixeles: las texturas
 * resultantes sobreviven al device lost/reset en ambas plataformas. */
int  SalviaShader_LutsUploaded(void);

/* Olvida la tabla. La llama destroyShaders(). */
void SalviaShader_Release(void);

#ifdef __cplusplus
}
#endif

#endif /* SALVIA_SHADER_API_H */
