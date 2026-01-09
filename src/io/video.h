#pragma once

#include <SDL.h>
#include <SDL_thread.h>
#include "const/constant.h"
#include "io/xbrz/xbrz.h"
#include <stdint.h>

//Todas las funciones de escalado, tienen que tener esta interfaz
typedef void (*ScalerFunc)(uint16_t* __restrict , uint16_t* __restrict , int, int, std::size_t, int, int, std::size_t, int, float);

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
	src_stride = spitch >> 1;   // Pitch en uint16_t
    dst_stride = dpitch >> 1;   // Pitch de pantalla en uint16_t

    // Ajustar el puntero de destino al punto de centrado (X, Y)
    dst += (start_y * dst_stride) + start_x;
}

inline void no_video(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
	return;
}

inline void fast_video_blit(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
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
/**
*
*/
inline void calcDestDimFromRatio(int sw, int sh, int dw, int dh, float ratio, int& out_w, int& out_h, int& inv_scale_x_fp, int& inv_scale_y_fp) {
    if (ratio <= 0.0f) {
        out_w = dw;
        out_h = dh;
    } else {
        out_w = dw;
        out_h = (int)((float)dw / ratio);

        if (out_h > dh) {
            out_h = dh;
            out_w = (int)((float)dh * ratio);
        }
    }

    // Usamos float para el cálculo intermedio y convertimos a punto fijo 16.16
    inv_scale_x_fp = (int)(((float)sw / (float)out_w) * 65536.0f);
    inv_scale_y_fp = (int)(((float)sh / (float)out_h) * 65536.0f);
}

/**
* The best choice for speed and safeness
*/
inline void scale_software_fixed_point_safe2(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // SEGURIDAD: Limitar out_w para que nunca pueda leer sw
    // Restamos un pequeño margen para asegurar que el acumulador no desborde sw-1
    int safe_out_w = out_w;
    if (safe_out_w > 0 && (((safe_out_w * inv_scale_x_fp) >> 16) >= sw)) safe_out_w--;

    for (int y = 0; y < out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        if (src_y >= sh) src_y = sh - 1;

        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int curr_x_fp = 0;
        int x = 0;

        // BUCLE MAESTRO: Volvemos al acumulador simple (lo más rápido)
        // Procesamos de 8 en 8. Sin multiplicaciones, solo sumas.
        for (; x <= safe_out_w - 8; x += 8) {
            line_dst[x + 0] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 1] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 2] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 3] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 4] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 5] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 6] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
            line_dst[x + 7] = line_src[curr_x_fp >> 16]; curr_x_fp += inv_scale_x_fp;
        }

        // Limpieza final rápida
        for (; x < out_w; x++) {
            int sx = curr_x_fp >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
            curr_x_fp += inv_scale_x_fp;
        }
    }
}

inline void scale_software_fixed_point_xbox_final(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    const int src_stride = (int)(spitch >> 1);
    const int dst_stride = (int)(dpitch >> 1);
    uint16_t* __restrict dst_ptr = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // Bounding de seguridad (calculado una sola vez por frame)
    int safe_out_w = out_w;
    if (safe_out_w > 0 && (((safe_out_w * inv_scale_x_fp) >> 16) >= sw)) safe_out_w--;

    // Pre-calculo de offsets para evitar multiplicaciones en el bucle
    const int s1 = inv_scale_x_fp;
    const int s2 = s1 * 2; const int s3 = s1 * 3; const int s4 = s1 * 4;
    const int s5 = s1 * 5; const int s6 = s1 * 6; const int s7 = s1 * 7;
    const int s8 = s1 * 8;

    for (int y = 0; y < out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        if (src_y >= sh) src_y = sh - 1;

        const uint16_t* __restrict line_src = src + (src_y * src_stride);
        uint16_t* __restrict line_dst = dst_ptr + (y * dst_stride);

        int curr_x_fp = 0;
        int x = 0;

        // BUCLE GANADOR (Máximos FPS en Xenon)
        // Usamos variables locales para que el compilador use los registros GPR (General Purpose Registers)
        for (; x <= safe_out_w - 8; x += 8) {
            // La CPU Xenon puede lanzar estas lecturas casi en paralelo si no hay IFs
            uint16_t p0 = line_src[curr_x_fp >> 16];
            uint16_t p1 = line_src[(curr_x_fp + s1) >> 16];
            uint16_t p2 = line_src[(curr_x_fp + s2) >> 16];
            uint16_t p3 = line_src[(curr_x_fp + s3) >> 16];
            uint16_t p4 = line_src[(curr_x_fp + s4) >> 16];
            uint16_t p5 = line_src[(curr_x_fp + s5) >> 16];
            uint16_t p6 = line_src[(curr_x_fp + s6) >> 16];
            uint16_t p7 = line_src[(curr_x_fp + s7) >> 16];

            line_dst[x + 0] = p0;
            line_dst[x + 1] = p1;
            line_dst[x + 2] = p2;
            line_dst[x + 3] = p3;
            line_dst[x + 4] = p4;
            line_dst[x + 5] = p5;
            line_dst[x + 6] = p6;
            line_dst[x + 7] = p7;

            curr_x_fp += s8;
        }

        // Cleanup final
        for (; x < out_w; x++) {
            int sx = curr_x_fp >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
            curr_x_fp += s1;
        }
    }
}



// Escalador 2x manual para RGB565
inline void scale2x_software(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {

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

inline void scale3x_software(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
	if (sw * 3 > dw || sh * 3 > dh)
		return;

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


inline void scale4x_software(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
    if (sw * 4 > dw || sh * 4 > dh)
		return;
	
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

/**
 * AdvMAME2x (Scale2x) para RGB565 con centrado de pantalla
 * Optimizado para Xbox 360 / x86
 */
inline void scale2x_advance(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
    // Verificación de límites de seguridad para escalado 2x
    if (!src || !dst || (sw * 2 > dw) || (sh * 2 > dh)) {
        return;
    }

    int src_stride = 0, dst_stride = 0;
    
    // Llamada obligatoria para centrar (Factor 2 para Scale2x)
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, 2, src_stride, dst_stride);

    // Ajuste de punteros base (sumamos el offset calculado en píxeles)
    uint16_t* s_ptr_base = src + src_stride;
    uint16_t* d_ptr_base = dst + dst_stride;

    const int s_gap = spitch / sizeof(uint16_t);
    const int d_gap = dpitch / sizeof(uint16_t);

    for (int y = 0; y < sh; ++y) {
        // Punteros a las líneas de origen: previa, actual y siguiente
        const uint16_t* s_curr = &s_ptr_base[y * s_gap];
        const uint16_t* s_prev = (y > 0) ? &s_ptr_base[(y - 1) * s_gap] : s_curr;
        const uint16_t* s_next = (y < sh - 1) ? &s_ptr_base[(y + 1) * s_gap] : s_curr;

        // Cada línea de origen genera dos líneas de destino
        uint16_t* d_top = &d_ptr_base[(y * 2) * d_gap];
        uint16_t* d_bot = &d_ptr_base[(y * 2 + 1) * d_gap];

        for (int x = 0; x < sw; ++x) {
            // Píxel central y sus vecinos inmediatos
            uint16_t E = s_curr[x];
            uint16_t B = s_prev[x];
            uint16_t D = (x > 0) ? s_curr[x - 1] : E;
            uint16_t F = (x < sw - 1) ? s_curr[x + 1] : E;
            uint16_t H = s_next[x];

            /*
               Matriz de salida 2x2 para el píxel E:
               [p0][p1]  <- d_top
               [p2][p3]  <- d_bot
            */

            if (B != H && D != F) {
                // Aplicación de reglas AdvMAME2x
                d_top[x * 2]     = (D == B) ? B : E; // p0
                d_top[x * 2 + 1] = (B == F) ? F : E; // p1
                d_bot[x * 2]     = (D == H) ? H : E; // p2
                d_bot[x * 2 + 1] = (H == F) ? F : E; // p3
            } else {
                // Relleno simple para zonas sin diagonales
                d_top[x * 2]     = E;
                d_top[x * 2 + 1] = E;
                d_bot[x * 2]     = E;
                d_bot[x * 2 + 1] = E;
            }
        }
    }
}

inline void scale3x_advance(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
     if (sw * 3 > dw || sh * 3 > dh)
		return;

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
inline void scale4x_advance(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {
    if (sw * 4 > dw || sh * 4 > dh)
		return;
	
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



inline void convertRGB565ToARGB8888(const uint16_t* src, int sw, int sh, std::size_t spitch, uint32_t* dst, std::size_t dpitch) {
    for (int y = 0; y < sh; ++y) {
        // Calculamos el inicio de cada línea usando el pitch (bytes)
        const uint16_t* srcLine = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(src) + (y * spitch));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(dst) + (y * dpitch));

        for (int x = 0; x < sw; ++x) {
            uint16_t p = srcLine[x];
            // Conversión directa por registro (Endian-safe)
            uint32_t r = ((p >> 11) & 0x1F);
            uint32_t g = ((p >> 5) & 0x3F);
            uint32_t b = (p & 0x1F);

            // Replicación de bits para color pleno (0x1F -> 0xFF)
            dstLine[x] = (0xFF000000) |           // Alpha
                         ((r << 3 | r >> 2) << 16) | // Red
                         ((g << 2 | g >> 4) << 8)  | // Green
                         (b << 3 | b >> 2);          // Blue
        }
    }
}

/**
 * Convierte ARGB8888 a RGB565 de forma eficiente.
 * @param src      Puntero a los datos de origen (32 bits).
 * @param sw       Ancho de la imagen.
 * @param sh       Alto de la imagen.
 * @param spitch   Pitch (bytes por línea) de la superficie origen (32 bits).
 * @param dst      Puntero al destino (16 bits).
 * @param dpitch   Pitch (bytes por línea) de la superficie destino (16 bits).
 */
inline void convertARGB8888ToRGB565(const uint32_t* src, int sw, int sh, size_t spitch,
                             uint16_t* dst, size_t dpitch) {
    
    const uint8_t* srcLine = reinterpret_cast<const uint8_t*>(src);
    uint8_t* dstLine = reinterpret_cast<uint8_t*>(dst);

    for (int y = 0; y < sh; ++y) {
        const uint32_t* srcPtr = reinterpret_cast<const uint32_t*>(srcLine);
        uint16_t* dstPtr = reinterpret_cast<uint16_t*>(dstLine);

        for (int x = 0; x < sw; ++x) {
            uint32_t pixel = srcPtr[x];

            // Extraer canales (asumiendo 0xAARRGGBB en registro)
            // Descartamos los bits bajos para quedarnos con 5-6-5
            uint16_t r = (uint16_t)((pixel >> 19) & 0x1F); // Rojo: de bits 16-23 a 5 bits
            uint16_t g = (uint16_t)((pixel >> 10) & 0x3F); // Verde: de bits 8-15 a 6 bits
            uint16_t b = (uint16_t)((pixel >> 3)  & 0x1F); // Azul: de bits 0-7 a 5 bits

            // Empaquetar en RGB565: RRRRRGGGGGGBBBBB
            dstPtr[x] = (r << 11) | (g << 5) | b;
        }

        srcLine += spitch;
        dstLine += dpitch;
    }
}

inline SDL_Surface* crearSuperficie16Bits(int ancho, int alto) {
    Uint32 rmask, gmask, bmask, amask;

    /* Configuración de máscaras según el Endianness */
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        /* Caso Xbox 360 / PowerPC: El byte más significativo va primero */
        rmask = 0xf800; // 5 bits Red
        gmask = 0x07e0; // 6 bits Green
        bmask = 0x001f; // 5 bits Blue
        amask = 0x0000;
    #else
        /* Caso PC / x86: Little Endian */
        rmask = 0xf800;
        gmask = 0x07e0;
        bmask = 0x001f;
        amask = 0x0000;
        /* Nota: En SDL 1.2 sobre Little Endian, a veces se requiere 
           reordenar los bytes dependiendo de cómo se vuelquen los datos. */
    #endif

    // Crear la superficie en memoria de sistema (recomendado para emuladores)
    SDL_Surface* superficie = SDL_CreateRGBSurface(SDL_SWSURFACE, 
                                                   ancho, 
                                                   alto, 
                                                   16, 
                                                   rmask, gmask, bmask, amask);

    if (superficie == NULL) {
        fprintf(stderr, "Error creando superficie: %s\n", SDL_GetError());
        return NULL;
    }

    return superficie;
}

struct XBRZJob {
    int scale;
    uint32_t* src;
    uint32_t* dst;
    int w, h;
    xbrz::ScalerCfg* cfg;
    int yFirst, yLast;
};

inline int xbrz_thread_func(void* data) {
    XBRZJob* job = (XBRZJob*)data;

	xbrz::scale(job->scale,
        job->src,
        job->dst,
        job->w,
        job->h,
		*job->cfg,
        job->yFirst,
        job->yLast);

    return 0;
}

inline void xbrz_scale_multithread(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {

	int src_stride = 0, dst_stride = 0;
	// 1. Usar check_center (escala 2 ya que no hay escalado manual aquí)
    // Esto ajustará el puntero 'dst' al punto exacto de centrado.
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, scale, src_stride, dst_stride);

	Uint32 init = SDL_GetTicks();
	Uint32 time = init;
	
	SDL_Surface *src32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw, sh, 32, rmask, gmask, bmask, amask);
	SDL_Surface *dst32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw * scale, sh * scale, 32, rmask, gmask, bmask, amask);

	/*if (src32->w != sw || src32->h != sh){
		SDL_FreeSurface(src32);
		src32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw, sh, 32, rmask, gmask, bmask, amask);
	}

	if (dst32->w != dw || dst32->h != dh){
		SDL_FreeSurface(dst32);
		dst32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw * scale, sh * scale, 32, rmask, gmask, bmask, amask);
	}*/

	Uint32 timeSurfaces = SDL_GetTicks() - time;

	time = SDL_GetTicks();
	convertRGB565ToARGB8888(src, sw, sh, spitch, (uint32_t*)src32->pixels, src32->pitch);
	Uint32 timeConv8888 = SDL_GetTicks() - time;
	time = SDL_GetTicks();

	/*static int frame = 0;
	if (frame % (60*3) == 0){
		std::string file = "D:\\develop\\Github\\xbox360\\project\\Salvia\\Debug\\Win32\\imgs\\frame_" + Constant::intToString(frame) + ".bmp";
		SDL_SaveBMP(src32, file.c_str());
	}
	frame++;*/

	// 3. Llamar a xBRZ usando directamente los píxeles de SDL
    auto* srcPixels = reinterpret_cast<uint32_t*>(src32->pixels);
    auto* dstPixels = reinterpret_cast<uint32_t*>(dst32->pixels);

    // xBRZ asume pitch en número de píxeles, no en bytes (en la API clásica tipo xbrzscale)
    int srcPitchPixels = src32->pitch / 4;
    int dstPitchPixels = dst32->pitch / 4;

    SDL_Thread* threads[16];
    XBRZJob jobs[16];

	int numThreads = 2;
    int slice = sh / numThreads;
	xbrz::ScalerCfg cfg;  // o tu config

    for (int i = 0; i < numThreads; i++) {
        jobs[i].scale = scale;
        jobs[i].src = srcPixels;
        jobs[i].dst = dstPixels;
        jobs[i].w = sw;
        jobs[i].h = sh;
        jobs[i].cfg = &cfg;

        jobs[i].yFirst = i * slice;
        jobs[i].yLast  = (i == numThreads - 1) ? sh : (i + 1) * slice;

        threads[i] = SDL_CreateThread(xbrz_thread_func, &jobs[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        SDL_WaitThread(threads[i], NULL);
    }

	Uint32 timeScaler = SDL_GetTicks() - time;
	time = SDL_GetTicks();

	SDL_FreeSurface(src32);
	convertARGB8888ToRGB565((uint32_t*)dst32->pixels, dst32->w, dst32->h, dst32->pitch, dst, dpitch);
	SDL_FreeSurface(dst32);
	Uint32 timeConv565 = SDL_GetTicks() - time;

	Uint32 end = SDL_GetTicks() - init;
	LOG_DEBUG("New surf. %d = %.0f%%, conv32: %d = %.0f%%, scale: %d = %.0f%%, conv16: %d = %.0f%%\n", 
		timeSurfaces, (timeSurfaces / (float)end) * 100.0, 
		timeConv8888, (timeConv8888 / (float)end) * 100.0,
		timeScaler, (timeScaler / (float)end) * 100.0,
		timeConv565, (timeConv565 / (float)end) * 100.0);
}


// Escalador 2x manual para RGB565
inline void scale_xBRZ_nx(uint16_t* __restrict src, uint16_t* __restrict dst, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scale, float ratio) {

	int src_stride = 0, dst_stride = 0;
	// 1. Usar check_center (escala 2 ya que no hay escalado manual aquí)
    // Esto ajustará el puntero 'dst' al punto exacto de centrado.
    check_center(src, dst, sw, sh, spitch, dw, dh, dpitch, scale, src_stride, dst_stride);

	Uint32 init = SDL_GetTicks();
	Uint32 time = init;
	
	SDL_Surface *src32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw, sh, 32, rmask, gmask, bmask, amask);
	SDL_Surface *dst32 = SDL_CreateRGBSurface(SDL_SWSURFACE, sw * scale, sh * scale, 32, rmask, gmask, bmask, amask);
	Uint32 timeSurfaces = SDL_GetTicks() - time;

	time = SDL_GetTicks();
	convertRGB565ToARGB8888(src, sw, sh, spitch, (uint32_t*)src32->pixels, src32->pitch);
	Uint32 timeConv8888 = SDL_GetTicks() - time;
	time = SDL_GetTicks();

	/*static int frame = 0;
	if (frame % (60*3) == 0){
		std::string file = "D:\\develop\\Github\\xbox360\\project\\Salvia\\Debug\\Win32\\imgs\\frame_" + Constant::intToString(frame) + ".bmp";
		SDL_SaveBMP(src32, file.c_str());
	}
	frame++;*/

	// 3. Llamar a xBRZ usando directamente los píxeles de SDL
    auto* srcPixels = reinterpret_cast<uint32_t*>(src32->pixels);
    auto* dstPixels = reinterpret_cast<uint32_t*>(dst32->pixels);

    // xBRZ asume pitch en número de píxeles, no en bytes (en la API clásica tipo xbrzscale)
    int srcPitchPixels = src32->pitch / 4;
    int dstPitchPixels = dst32->pitch / 4;

    xbrz::ScalerCfg cfg;  // o tu config
    xbrz::scale(scale,srcPixels, dstPixels,sw, sh, cfg, 0, sh);

	Uint32 timeScaler = SDL_GetTicks() - time;
	time = SDL_GetTicks();

	SDL_FreeSurface(src32);
	convertARGB8888ToRGB565((uint32_t*)dst32->pixels, dst32->w, dst32->h, dst32->pitch, dst, dpitch);
	SDL_FreeSurface(dst32);
	Uint32 timeConv565 = SDL_GetTicks() - time;

	Uint32 end = SDL_GetTicks() - init;
	LOG_INFO("New surf. %d = %.0f%%, conv32: %d = %.0f%%, scale: %d = %.0f%%, conv16: %d = %.0f%%\n", 
		timeSurfaces, (timeSurfaces / (float)end) * 100.0, 
		timeConv8888, (timeConv8888 / (float)end) * 100.0,
		timeScaler, (timeScaler / (float)end) * 100.0,
		timeConv565, (timeConv565 / (float)end) * 100.0);
}