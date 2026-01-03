#pragma once

#include "const\constant.h"
#include <stdint.h>

inline void check_center(uint16_t* src, uint16_t*& dst, int sw, int sh, std::size_t spitch, 
						int dw, int dh, std::size_t dpitch, 
						int scale, int &src_stride, int &dst_stride){
	// 1. Configuración de dimensiones
    const int out_w = sw * scale;
    const int out_h = sh * scale;
	src_stride = 0;
	dst_stride = 0;

	// 2. Comprobación de límites (Safety Check)
    // Si el resultado 3x es más grande que la resolución de pantalla actual, abortamos
    if (out_w > dw || out_h > dh) {
        // Opcional: Podrías hacer un fallback a un Blit 1:1 aquí
        return; 
    }

    // 3. Calcular el offset de centrado
    int start_x = (dw - out_w) / 2;
    int start_y = (dh - out_h) / 2;

	//spitch y dpitch indican el numero de bytes(8Bits) que hay en cada fila.
	//Como src_stride y dst_stride representan el numero de elementos de 16Bits(porque viene de un puntero uint16_t)
	//tenemos que dividir por 2, lo que se consigue con la operacion de desplazamiento de 1 bit (>> 1)
	src_stride = spitch >> 1;          // Pitch en uint16_t
    dst_stride = dpitch >> 1;   // Pitch de pantalla en uint16_t

    // Ajustar el puntero de destino al punto de centrado (X, Y)
    dst += (start_y * dst_stride) + start_x;
}

inline void fast_video_blit(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    int src_stride = 0, dst_stride = 0;

    // 1. Usar check_center (escala 1 ya que no hay escalado manual aquí)
    // Esto ajustará el puntero 'dst' al punto exacto de centrado.
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 1, src_stride, dst_stride);

    // 2. El ancho a copiar en bytes (cada píxel uint16_t son 2 bytes)
    const std::size_t bytes_per_line = sw * sizeof(uint16_t);

    // 3. Bucle de copiado
    for (int y = 0; y < sh; y++) {
        // Usamos memcpy para máxima velocidad por línea
        // Destino: puntero centrado + salto de línea (en píxeles)
        // Origen: puntero base + salto de línea (en píxeles)
        memcpy(dst + (y * dst_stride), src + (y * src_stride), bytes_per_line);
    }
}

// Escalador 2x manual para RGB565
inline void scale2x_software(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {

	int src_stride = 0, dst_stride = 0;
	// 1. Usar check_center (escala 2 ya que no hay escalado manual aquí)
    // Esto ajustará el puntero 'dst' al punto exacto de centrado.
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 2, src_stride, dst_stride);

	for (int y = 0; y < sh; y++) {
        uint16_t* line_src = src + (y * src_stride);
        uint16_t* line_dst1 = dst + ((y * 2) * dst_stride);
        uint16_t* line_dst2 = dst + ((y * 2 + 1) * dst_stride);

        for (int x = 0; x < sw; x++) {
            uint16_t pixel = line_src[x];
            // Duplicamos el píxel horizontalmente
            line_dst1[x * 2] = pixel;
            line_dst1[x * 2 + 1] = pixel;
            // Duplicamos la línea completa verticalmente
            line_dst2[x * 2] = pixel;
            line_dst2[x * 2 + 1] = pixel;
        }
    }
}

inline void scale3x_software(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
	int src_stride = 0, dst_stride = 0;
	check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 3, src_stride, dst_stride);

    // 5. Bucle de Escalado 3x Manual Optimizado
    for (int y = 0; y < sh; y++) {
        uint16_t* line_src = src + (y * src_stride);
        
        // Calculamos las 3 líneas de destino que corresponden a esta línea de origen
        uint16_t* line_dst1 = dst + ((y * 3) * dst_stride);
        uint16_t* line_dst2 = line_dst1 + dst_stride;
        uint16_t* line_dst3 = line_dst2 + dst_stride;

        for (int x = 0; x < sw; x++) {
            uint16_t pixel = line_src[x];
            int x3 = x * 3;

            // Escribir 3 píxeles horizontalmente en las 3 líneas verticales
            line_dst1[x3] = line_dst1[x3+1] = line_dst1[x3+2] = pixel;
            line_dst2[x3] = line_dst2[x3+1] = line_dst2[x3+2] = pixel;
            line_dst3[x3] = line_dst3[x3+1] = line_dst3[x3+2] = pixel;
        }
    }
}

inline void scale4x_software(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    int src_stride = 0, dst_stride = 0;
    
    // Llamada para centrar la pantalla (factor 4)
    // Nota: Asegúrate de que tu check_center asigne a src_stride el valor de spitch/2 
    // y a dst_stride el valor de dpitch/2 si trabajas con punteros uint16_t.
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 4, src_stride, dst_stride);

    for (int y = 0; y < sh; y++) {
        // Puntero a la línea de origen actual
        uint16_t* line_src = src + (y * src_stride);
        
        // Calculamos las 4 líneas de destino que corresponden a esta línea de origen
        uint16_t* line_dst1 = dst + ((y * 4) * dst_stride);
        uint16_t* line_dst2 = line_dst1 + dst_stride;
        uint16_t* line_dst3 = line_dst2 + dst_stride;
        uint16_t* line_dst4 = line_dst3 + dst_stride;

        for (int x = 0; x < sw; x++) {
            uint16_t pixel = line_src[x];
            int x4 = x * 4;

            // Fila 1
            line_dst1[x4] = line_dst1[x4+1] = line_dst1[x4+2] = line_dst1[x4+3] = pixel;
            // Fila 2
            line_dst2[x4] = line_dst2[x4+1] = line_dst2[x4+2] = line_dst2[x4+3] = pixel;
            // Fila 3
            line_dst3[x4] = line_dst3[x4+1] = line_dst3[x4+2] = line_dst3[x4+3] = pixel;
            // Fila 4
            line_dst4[x4] = line_dst4[x4+1] = line_dst4[x4+2] = line_dst4[x4+3] = pixel;
        }
    }
}

inline void scale3x_advance(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    int src_stride = 0, dst_stride = 0;
    
    // Llamada obligatoria para centrar según tu estructura
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 3, src_stride, dst_stride);

    // Ajuste de punteros base (sumamos el offset calculado en píxeles)
    uint16_t* s_ptr = src + src_stride;
    uint16_t* d_ptr = dst + dst_stride;

    const int s_gap = spitch / sizeof(uint16_t);
    const int d_gap = dpitch / sizeof(uint16_t);

    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            /* 
               Matriz de vecinos:
               A B C
               D E F
               G H I
            */
            uint16_t E = s_ptr[y * s_gap + x];
            uint16_t B = (y > 0) ? s_ptr[(y - 1) * s_gap + x] : E;
            uint16_t D = (x > 0) ? s_ptr[y * s_gap + (x - 1)] : E;
            uint16_t F = (x < sw - 1) ? s_ptr[y * s_gap + (x + 1)] : E;
            uint16_t H = (y < sh - 1) ? s_ptr[(y + 1) * s_gap + x] : E;

            uint16_t A = (y > 0 && x > 0) ? s_ptr[(y - 1) * s_gap + (x - 1)] : E;
            uint16_t C = (y > 0 && x < sw - 1) ? s_ptr[(y - 1) * s_gap + (x + 1)] : E;
            uint16_t G = (y < sh - 1 && x > 0) ? s_ptr[(y + 1) * s_gap + (x - 1)] : E;
            uint16_t I = (y < sh - 1 && x < sw - 1) ? s_ptr[(y + 1) * s_gap + (x + 1)] : E;

            // Puntero al inicio del bloque 3x3 de salida
            uint16_t* out = &d_ptr[(y * 3) * d_gap + (x * 3)];

            if (B != H && D != F) {
                // Fila superior del bloque 3x3
                out[0] = (D == B) ? B : E;                                     // E0
                out[1] = ((D == B && E != C) || (B == F && E != A)) ? B : E;   // E1
                out[2] = (B == F) ? F : E;                                     // E2

                // Fila central del bloque 3x3
                out[d_gap + 0] = ((D == B && E != G) || (D == H && E != A)) ? D : E; // E3
                out[d_gap + 1] = E;                                                  // E4 (Centro siempre es E)
                out[d_gap + 2] = ((B == F && E != I) || (H == F && E != C)) ? F : E; // E5

                // Fila inferior del bloque 3x3
                out[2 * d_gap + 0] = (D == H) ? H : E;                               // E6
                out[2 * d_gap + 1] = ((D == H && E != I) || (H == F && E != G)) ? H : E; // E7
                out[2 * d_gap + 2] = (H == F) ? F : E;                               // E8
            } else {
                // Si no se detectan bordes, el bloque de 3x3 es idéntico al original
                for (int j = 0; j < 3; ++j) {
                    for (int i = 0; i < 3; ++i) {
                        out[j * d_gap + i] = E;
                    }
                }
            }
        }
    }
}

/**
 * Scale4x para 16 bits (RGB565/555)
 * Expande cada píxel en un bloque de 4x4 analizando sus vecinos.
 */
inline void scale4x_advance(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    int src_stride = 0, dst_stride = 0;
    
    // Llamada para centrar la imagen y obtener los offsets de inicio
    // El factor de escala es 4
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 4, src_stride, dst_stride);

    // Ajuste de punteros base según el cálculo de check_center
    uint16_t* s_ptr = src + src_stride;
    uint16_t* d_ptr = dst + dst_stride;

    // Convertimos pitch de bytes a número de elementos uint16_t
    const int s_gap = spitch / sizeof(uint16_t);
    const int d_gap = dpitch / sizeof(uint16_t);

    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            // Píxel central (E) y vecinos cardinales
            uint16_t E = s_ptr[y * s_gap + x];
            uint16_t B = (y > 0) ? s_ptr[(y - 1) * s_gap + x] : E;
            uint16_t D = (x > 0) ? s_ptr[y * s_gap + (x - 1)] : E;
            uint16_t F = (x < sw - 1) ? s_ptr[y * s_gap + (x + 1)] : E;
            uint16_t H = (y < sh - 1) ? s_ptr[(y + 1) * s_gap + x] : E;

            // Puntero al inicio del bloque 4x4 en el destino
            uint16_t* out = &d_ptr[(y * 4) * d_gap + (x * 4)];

            if (B != H && D != F) {
                // El algoritmo Scale4x es efectivamente Scale2x aplicado sobre el resultado de un Scale2x.
                // Aquí aplicamos la lógica simplificada para los 16 sub-píxeles:
                
                // Fila 0
                out[0] = (D == B) ? B : E;
                out[1] = (D == B) ? B : E;
                out[2] = (B == F) ? F : E;
                out[3] = (B == F) ? F : E;

                // Fila 1
                out[d_gap + 0] = (D == B) ? B : E;
                out[d_gap + 1] = E; // Centro-Arriba-Izquierda
                out[d_gap + 2] = E; // Centro-Arriba-Derecha
                out[d_gap + 3] = (B == F) ? F : E;

                // Fila 2
                out[2 * d_gap + 0] = (D == H) ? H : E;
                out[2 * d_gap + 1] = E; // Centro-Abajo-Izquierda
                out[2 * d_gap + 2] = E; // Centro-Abajo-Derecha
                out[2 * d_gap + 3] = (H == F) ? F : E;

                // Fila 3
                out[3 * d_gap + 0] = (D == H) ? H : E;
                out[3 * d_gap + 1] = (D == H) ? H : E;
                out[3 * d_gap + 2] = (H == F) ? F : E;
                out[3 * d_gap + 3] = (H == F) ? F : E;
            } else {
                // Si no hay variación, se rellena el bloque 4x4 con el color original
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        out[j * d_gap + i] = E;
                    }
                }
            }
        }
    }
}

// Función auxiliar: Distancia de color para detectar bordes con precisión
inline bool color_dist(uint16_t c1, uint16_t c2) {
    if (c1 == c2) return false;
    // Extraer canales RGB565
    int r = ((c1 >> 11) & 0x1F) - ((c2 >> 11) & 0x1F);
    int g = ((c1 >> 5) & 0x3F) - ((c2 >> 5) & 0x3F);
    int b = (c1 & 0x1F) - (c2 & 0x1F);
    // Umbral de luminancia para decidir si hay un borde real
    return (r*r + g*g + b*b) > 200; 
}

inline void scale4x_xbrz_software(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    int src_stride = 0, dst_stride = 0;
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 4, src_stride, dst_stride);

    const int s_gap = spitch / sizeof(uint16_t);
    const int d_gap = dpitch / sizeof(uint16_t);

    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            // Píxel actual (P) y sus vecinos cardinales
            uint16_t P = src[y * s_gap + x];
            uint16_t U = (y > 0) ? src[(y - 1) * s_gap + x] : P;
            uint16_t D = (y < sh - 1) ? src[(y + 1) * s_gap + x] : P;
            uint16_t L = (x > 0) ? src[y * s_gap + (x - 1)] : P;
            uint16_t R = (x < sw - 1) ? src[y * s_gap + (x + 1)] : P;

            uint16_t* out = dst + (y * 4 * d_gap) + (x * 4);

            // Lógica simplificada de xBRZ: 
            // Si detecta una diagonal (ej. arriba y derecha coinciden), suaviza la esquina.
            // Si no, mantiene el bloque nítido.
            
            for (int dy = 0; dy < 4; dy++) {
                for (int dx = 0; dx < 4; dx++) {
                    uint16_t res = P;
                    
                    // Ejemplo de regla xBRZ para esquinas:
                    if (dx > 2 && dy < 1 && !color_dist(U, R)) res = R; // Esquina superior derecha
                    if (dx < 1 && dy > 2 && !color_dist(D, L)) res = L; // Esquina inferior izquierda
                    if (dx > 2 && dy > 2 && !color_dist(D, R)) res = R; // Esquina inferior derecha
                    if (dx < 1 && dy < 1 && !color_dist(U, L)) res = L; // Esquina superior izquierda

                    out[dy * d_gap + dx] = res;
                }
            }
        }
    }
}

/**
 * Escalador genérico para cualquier factor entero (1x, 2x, 3x, 4x, etc.)
 * scale: el factor de multiplicación (ej: 2 para 2x)
 */
inline void scale_generic_software(uint16_t* src, uint16_t* dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale) {
    int src_stride = 0, dst_stride = 0;
    
    // Centrar la imagen según el factor de escala solicitado
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, scale, src_stride, dst_stride);

    uint16_t* s_ptr = src + src_stride;
    uint16_t* d_ptr = dst + dst_stride;

    const int s_gap = spitch / sizeof(uint16_t);
    const int d_gap = dpitch / sizeof(uint16_t);

    // Caso especial 1x para máxima velocidad (es un simple blit/copia)
    if (scale == 1) {
        for (int y = 0; y < sh; ++y) {
            memcpy(d_ptr + (y * d_gap), s_ptr + (y * s_gap), sw * sizeof(uint16_t));
        }
        return;
    }

    // Escalado genérico para scale > 1
    for (int y = 0; y < sh; ++y) {
        uint16_t* src_row = &s_ptr[y * s_gap];
        uint16_t* dst_row_base = &d_ptr[(y * scale) * d_gap];

        for (int x = 0; x < sw; ++x) {
            uint16_t color = src_row[x];
            uint16_t* out = &dst_row_base[x * scale];

            // Rellenar el bloque scale * scale
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    out[dy * d_gap + dx] = color;
                }
            }
        }
    }
}

inline void scale_software_fixed_point(uint16_t* src, uint16_t* dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch) {
    float scale = (float)dw / sw;
    if ((float)dh / sh < scale) scale = (float)dh / sh;

    int out_w = (int)(sw * scale);
    int out_h = (int)(sh * scale);
    
    // Convertir el factor inverso a punto fijo (entero que representa decimales)
    // Usamos un desplazamiento de 16 bits (65536)
    int inv_scale_fp = (int)((1.0f / scale) * 65536.0f);

    int src_stride = spitch >> 1;
    int dst_stride = dpitch >> 1;
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    for (int y = 0; y < out_h; y++) {
        int src_y = (y * inv_scale_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int curr_src_x_fp = 0; // Acumulador de punto fijo
        for (int x = 0; x < out_w; x++) {
            line_dst[x] = line_src[curr_src_x_fp >> 16];
            curr_src_x_fp += inv_scale_fp;
        }
    }
}

static inline void scale_bilinear_fast(uint16_t* src, uint16_t* dst, 
									   int sw, int sh, std::size_t spitch,
									   int dw, int dh, std::size_t dpitch) {
    float f_scale = (float)dw / sw;
    if ((float)dh / sh < f_scale) f_scale = (float)dh / sh;

    int out_w = (int)(sw * f_scale);
    int out_h = (int)(sh * f_scale);
    int src_stride, dst_stride;

    check_center(src, dst, out_w, out_h, spitch, dw, dh, dpitch, 1, src_stride, dst_stride);

    int inv_scale_fp = (int)((1.0f / f_scale) * 65536.0f);

    for (int y = 0; y < out_h; y++) {
        int fp_y = y * inv_scale_fp;
        int src_y = fp_y >> 16;
        uint32_t y_fract = (fp_y >> 11) & 0x1F; // 5 bits de precisión son suficientes

        uint16_t* s0 = src + (src_y * src_stride);
        uint16_t* s1 = s0 + (src_y < (int)sh - 1 ? src_stride : 0);
        uint16_t* line_dst = dst + (y * dst_stride);

        for (int x = 0; x < out_w; x++) {
            int fp_x = x * inv_scale_fp;
            int src_x = fp_x >> 16;
            uint32_t x_fract = (fp_x >> 11) & 0x1F;

            uint32_t p00 = s0[src_x];
            uint32_t p01 = s0[src_x + 1];
            uint32_t p10 = s1[src_x];
            uint32_t p11 = s1[src_x + 1];

            // TRUCO DE BITS: Interpolar RB y G por separado
            // Rojo y Azul (RB) se procesan juntos
            uint32_t rb00 = p00 & 0xF81F;
            uint32_t rb01 = p01 & 0xF81F;
            uint32_t rb10 = p10 & 0xF81F;
            uint32_t rb11 = p11 & 0xF81F;
            
            uint32_t rb = rb00 + (((rb01 - rb00) * x_fract) >> 5); // Simplificado para ejemplo
            // En una implementación real, aquí harías el lerp completo 2D en 3 pasos
            // Pero para máxima velocidad, el "Bilinear Aproximado" es mejor:
            
            // Si el rendimiento sigue bajo, usa Nearest Neighbor pero con Scanline Dimming
            line_dst[x] = (uint16_t)p00; // Fallback a Nearest si buscas 60fps constantes
        }
    }
}

static uint8_t LUT_R50[32][32];
static uint8_t LUT_G50[64][64];
static uint8_t LUT_B50[32][32];

static uint8_t LUT_R75[32][32];
static uint8_t LUT_G75[64][64];
static uint8_t LUT_B75[32][32];

static bool lut_initialized = false;

static uint16_t LUT_BLEND50[1 << 16];
static uint16_t LUT_BLEND75[1 << 16];

static inline uint8_t r5(uint16_t c) { return (c >> 11) & 0x1F; }
static inline uint8_t g6(uint16_t c) { return (c >> 5) & 0x3F; }
static inline uint8_t b5(uint16_t c) { return c & 0x1F; }

static inline uint16_t make565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((r << 11) | (g << 5) | b);
}


static void init_luts()
{
    if (lut_initialized) return;
    lut_initialized = true;

    for (int a = 0; a < 32; a++)
    for (int b = 0; b < 32; b++)
    {
        LUT_R50[a][b] = (a + b) >> 1;
        LUT_B50[a][b] = (a + b) >> 1;

        LUT_R75[a][b] = (a * 3 + b) >> 2;
        LUT_B75[a][b] = (a * 3 + b) >> 2;
    }

    for (int a = 0; a < 64; a++)
    for (int b = 0; b < 64; b++)
    {
        LUT_G50[a][b] = (a + b) >> 1;
        LUT_G75[a][b] = (a * 3 + b) >> 2;
    }
}

static inline uint16_t blend50(uint16_t a, uint16_t b)
{
    return make565(
        LUT_R50[r5(a)][r5(b)],
        LUT_G50[g6(a)][g6(b)],
        LUT_B50[b5(a)][b5(b)]
    );
}

static inline uint16_t blend75(uint16_t a, uint16_t b)
{
    return make565(
        LUT_R75[r5(a)][r5(b)],
        LUT_G75[g6(a)][g6(b)],
        LUT_B75[b5(a)][b5(b)]
    );
}


void scale_xBRZ_3x(uint16_t* src, uint16_t* dst,
                   int sw, int sh, std::size_t spitch,
                   int dw, int dh, std::size_t dpitch)
{
    init_luts();

    const int scale = 3;

    int src_stride, dst_stride;
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, scale, src_stride, dst_stride);

    if (sw * 3 > dw || sh * 3 > dh)
        return;

    for (int y = 0; y < sh; y++)
    {
        uint16_t* rowM = src + y * src_stride;
        uint16_t* rowU = src + (y > 0 ? y - 1 : y) * src_stride;
        uint16_t* rowD = src + (y < sh - 1 ? y + 1 : y) * src_stride;

        for (int x = 0; x < sw; x++)
        {
            uint16_t C = rowM[x];
            uint16_t A = rowU[x];
            uint16_t B = rowM[x < sw - 1 ? x + 1 : x];
            uint16_t D = rowD[x];
            uint16_t E = rowM[x > 0 ? x - 1 : x];

            uint16_t* d0 = dst + (y * 3 + 0) * dst_stride + x * 3;
            uint16_t* d1 = dst + (y * 3 + 1) * dst_stride + x * 3;
            uint16_t* d2 = dst + (y * 3 + 2) * dst_stride + x * 3;

            d0[0] = d0[1] = d0[2] =
            d1[0] = d1[1] = d1[2] =
            d2[0] = d2[1] = d2[2] = C;

            bool h = (E != B);
            bool v = (A != D);

            if (h || v)
            {
                if ((A != B) && (E != D))
                {
                    d0[1] = blend75(C, A);
                    d1[2] = blend75(C, B);
                    d1[1] = blend75(C, blend50(A, B));
                }
                else if ((A != E) && (B != D))
                {
                    d0[1] = blend75(C, A);
                    d1[0] = blend75(C, E);
                    d1[1] = blend75(C, blend50(A, E));
                }
                else if (h)
                {
                    d1[0] = blend75(C, E);
                    d1[2] = blend75(C, B);
                }
                else if (v)
                {
                    d0[1] = blend75(C, A);
                    d2[1] = blend75(C, D);
                }
            }
        }
    }
}
