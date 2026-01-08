#include <algorithm>

#if defined(WIN)
	#include <emmintrin.h> // SSE2
	#include <immintrin.h> // Intrínsecos x86 (SSE/AVX)	
#endif

/**
* This is the fastest function, but not safe because it doesn't have overflow checkpoints
*/
inline void scale_software_fixed_point(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = spitch >> 1;
    int dst_stride = dpitch >> 1;
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    for (int y = 0; y < out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int x = 0;
        // Procesar de 8 en 8 píxeles
        for (; x <= out_w - 8; x += 8) {
            int x0 = (x * inv_scale_x_fp);
            
            line_dst[x + 0] = line_src[(x0 + 0 * inv_scale_x_fp) >> 16];
            line_dst[x + 1] = line_src[(x0 + 1 * inv_scale_x_fp) >> 16];
            line_dst[x + 2] = line_src[(x0 + 2 * inv_scale_x_fp) >> 16];
            line_dst[x + 3] = line_src[(x0 + 3 * inv_scale_x_fp) >> 16];
            line_dst[x + 4] = line_src[(x0 + 4 * inv_scale_x_fp) >> 16];
            line_dst[x + 5] = line_src[(x0 + 5 * inv_scale_x_fp) >> 16];
            line_dst[x + 6] = line_src[(x0 + 6 * inv_scale_x_fp) >> 16];
            line_dst[x + 7] = line_src[(x0 + 7 * inv_scale_x_fp) >> 16];
        }

        int curr_src_x_fp = x * inv_scale_x_fp;
        for (; x < out_w; x++) {
            line_dst[x] = line_src[curr_src_x_fp >> 16];
            curr_src_x_fp += inv_scale_x_fp;
        }
    }
}

inline void scale_software_fixed_point_simple(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = spitch >> 1;
    int dst_stride = dpitch >> 1;

    // Centrado dinámico (si es modo estirado, estos offsets serán 0)
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    for (int y = 0; y < out_h; y++) {
        // Cálculo de la línea de origen en punto fijo
        int src_y = (y * inv_scale_y_fp) >> 16;
        
        // Protección de límites para evitar lecturas fuera de memoria
        if (src_y >= sh) src_y = sh - 1;

        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int curr_src_x_fp = 0; 
        for (int x = 0; x < out_w; x++) {
            int src_x = curr_src_x_fp >> 16;
            
            // Protección de límites X
            if (src_x >= sw) src_x = sw - 1;

            line_dst[x] = line_src[src_x];
            curr_src_x_fp += inv_scale_x_fp;
        }
    }
}

inline void scale_software_fixed_point_simple_safe(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);
    uint16_t* dst_start = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // 2. AJUSTE DE RANGO (Bounding)
    // Calculamos el límite de 'y' que es 100% seguro sin comprobaciones
    int safe_out_h = out_h;
    while (safe_out_h > 0 && (((safe_out_h - 1) * inv_scale_y_fp) >> 16) >= sh) {
        safe_out_h--;
    }

    // Calculamos el límite de 'x' que es 100% seguro para el bloque de 8 píxeles
    int safe_out_w = out_w;
    while (safe_out_w > 0 && (((safe_out_w + 7) * inv_scale_x_fp) >> 16) >= sw) {
        safe_out_w--;
    }

    // Pre-calculo de saltos para el bucle de 8 (Elimina dependencias de datos)
    const int s1 = inv_scale_x_fp;
    const int s2 = s1 * 2; const int s3 = s1 * 3; const int s4 = s1 * 4;
    const int s5 = s1 * 5; const int s6 = s1 * 6; const int s7 = s1 * 7;
    const int s8 = s1 * 8;

    // 3. BUCLE PRINCIPAL: Zona Segura (Máximo Rendimiento)
    for (int y = 0; y < safe_out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst_start + (y * dst_stride);

        int curr_x_fp = 0;
        int x = 0;

        // Procesar de 8 en 8 sin IFs ni riesgos de memoria
        for (; x <= safe_out_w - 8; x += 8) {
            line_dst[x + 0] = line_src[(curr_x_fp     ) >> 16];
            line_dst[x + 1] = line_src[(curr_x_fp + s1) >> 16];
            line_dst[x + 2] = line_src[(curr_x_fp + s2) >> 16];
            line_dst[x + 3] = line_src[(curr_x_fp + s3) >> 16];
            line_dst[x + 4] = line_src[(curr_x_fp + s4) >> 16];
            line_dst[x + 5] = line_src[(curr_x_fp + s5) >> 16];
            line_dst[x + 6] = line_src[(curr_x_fp + s6) >> 16];
            line_dst[x + 7] = line_src[(curr_x_fp + s7) >> 16];
            curr_x_fp += s8;
        }

        // Limpieza de píxeles restantes en línea segura (solo copia el último píxel de origen si es necesario)
        for (; x < out_w; x++) {
            int sx = curr_x_fp >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
            curr_x_fp += s1;
        }
    }

    // 4. ZONA DE RIESGO: Filas finales (Normalmente solo la última línea)
    // Se procesa fuera del bucle de alto rendimiento para no ralentizar el 99% de la imagen
    for (int y = safe_out_h; y < out_h; y++) {
        uint16_t* line_src = src + ((sh - 1) * src_stride); // Forzamos última línea válida
        uint16_t* line_dst = dst_start + (y * dst_stride);
        
        int curr_x_fp = 0;
        for (int x = 0; x < out_w; x++) {
            int sx = curr_x_fp >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
            curr_x_fp += s1;
        }
    }
}

inline void scale_software_fixed_point_notif(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = spitch >> 1;
    int dst_stride = dpitch >> 1;

    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // --- OPTIMIZACIÓN: Cálculo de límites seguros ---
    // Calculamos hasta qué 'y' y 'x' podemos llegar sin pasarnos de (sh-1) y (sw-1)
    int safe_h = out_h;
    if (((safe_h - 1) * inv_scale_y_fp) >> 16 >= sh) safe_h--;

    int safe_w = out_w;
    if (((safe_w - 1) * inv_scale_x_fp) >> 16 >= sw) safe_w--;

    // 1. Bucle principal de filas seguras (Sin IFs)
    for (int y = 0; y < safe_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int curr_src_x_fp = 0; 
        for (int x = 0; x < safe_w; x++) {
            line_dst[x] = line_src[curr_src_x_fp >> 16];
            curr_src_x_fp += inv_scale_x_fp;
        }
        
        // Manejar el último píxel de la línea solo si es necesario
        if (safe_w < out_w) {
            line_dst[safe_w] = line_src[sw - 1];
        }
    }

    // 2. Manejar las filas restantes (si las hay) por error de redondeo
    for (int y = safe_h; y < out_h; y++) {
        uint16_t* line_src = src + ((sh - 1) * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);
        for (int x = 0; x < out_w; x++) {
            line_dst[x] = line_src[(x * inv_scale_x_fp >> 16) >= sw ? sw-1 : (x * inv_scale_x_fp >> 16)];
        }
    }
}

inline void scale_software_float(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    // 1. Calcular factores de escala
    float scaleX = (float)dw / sw;
    float scaleY = (float)dh / sh;

    if (scaleMode != 0) { // Mantener Aspecto
        float s = (scaleX < scaleY) ? scaleX : scaleY;
        scaleX = s;
        scaleY = s;
    }

    int out_w = (int)(sw * scaleX);
    int out_h = (int)(sh * scaleY);
    
    // Factores inversos para mapear del destino al origen
    float inv_scale_x = 1.0f / scaleX;
    float inv_scale_y = 1.0f / scaleY;

    int src_stride = (int)(spitch / sizeof(uint16_t));
    int dst_stride = (int)(dpitch / sizeof(uint16_t));

    // Puntero de destino ajustado para centrar la imagen
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // 2. Bucle de reescalado
    for (int y = 0; y < out_h; y++) {
        // Calculamos la fila de origen usando float
        int src_y = (int)(y * inv_scale_y);
        
        // Protección básica de límites
        if (src_y >= sh) src_y = sh - 1;

        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        for (int x = 0; x < out_w; x++) {
            // Calculamos la columna de origen usando float
            int src_x = (int)(x * inv_scale_x);
            
            if (src_x >= sw) src_x = sw - 1;

            line_dst[x] = line_src[src_x];
        }
    }
}

#ifdef WIN

inline void scale_software_float_sse(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    // 1. Cálculos de escala (igual que la versión simple)
    float scaleX = (float)dw / sw;
    float scaleY = (float)dh / sh;
    if (scaleMode != 0) {
        float s = (scaleX < scaleY) ? scaleX : scaleY;
        scaleX = s; scaleY = s;
    }

    int out_w = (int)(sw * scaleX);
    int out_h = (int)(sh * scaleY);
    float inv_scale_x = 1.0f / scaleX;
    float inv_scale_y = 1.0f / scaleY;

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // Preparar constantes SSE
    __m128 v_inv_scale_x = _mm_set1_ps(inv_scale_x);
    __m128 v_offsets = _mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f);
    __m128 v_step4 = _mm_set1_ps(inv_scale_x * 4.0f);

    for (int y = 0; y < out_h; y++) {
        int src_y = (int)(y * inv_scale_y);
        if (src_y >= sh) src_y = sh - 1;

        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        __m128 v_curr_x = _mm_mul_ps(_mm_set1_ps((float)0), v_inv_scale_x); // Inicializar
        // Para mayor precisión en el bucle, calculamos el x base float
        v_curr_x = _mm_mul_ps(_mm_add_ps(_mm_set1_ps(0.0f), v_offsets), v_inv_scale_x);

        int x = 0;
        for (; x <= out_w - 4; x += 4) {
            // Convertir de float a int con truncamiento (vectorial)
            __m128i v_idx = _mm_cvttps_epi32(v_curr_x);

            // Gather manual (extraer índices)
            // Nota: Se usa almacenamiento temporal alineado para evitar fallos en CPUs antiguas
            __declspec(align(16)) uint32_t idx[4];
            _mm_store_si128((__m128i*)idx, v_idx);

            // Cargar píxeles y guardar
            line_dst[x + 0] = line_src[idx[0] >= sw ? sw - 1 : idx[0]];
            line_dst[x + 1] = line_src[idx[1] >= sw ? sw - 1 : idx[1]];
            line_dst[x + 2] = line_src[idx[2] >= sw ? sw - 1 : idx[2]];
            line_dst[x + 3] = line_src[idx[3] >= sw ? sw - 1 : idx[3]];

            // Avanzar el vector de floats
            v_curr_x = _mm_add_ps(v_curr_x, v_step4);
        }

        // Limpieza final
        for (; x < out_w; x++) {
            int src_x = (int)(x * inv_scale_x);
            line_dst[x] = line_src[src_x >= sw ? sw - 1 : src_x];
        }
    }
}

inline void scale_software_fixed_point_x86_simd(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);
    uint16_t* dst_start = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // Bounding: Límites seguros
    int safe_out_h = out_h;
    while (safe_out_h > 0 && (((safe_out_h - 1) * inv_scale_y_fp) >> 16) >= sh) safe_out_h--;
    int safe_out_w = out_w;
    while (safe_out_w > 0 && (((safe_out_w + 7) * inv_scale_x_fp) >> 16) >= sw) safe_out_w--;

    // PREPARACIÓN DE REGISTROS SSE
    // Vector de pasos: [0, 1, 2, 3] * inv_scale_x_fp
    __m128i v_step = _mm_setr_epi32(0, inv_scale_x_fp, inv_scale_x_fp * 2, inv_scale_x_fp * 3);
    __m128i v_inv_x_step4 = _mm_set1_epi32(inv_scale_x_fp * 4);

    for (int y = 0; y < safe_out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst_start + (y * dst_stride);

        int x = 0;
        int curr_x_fp = 0;

        // BUCLE SIMD: Procesa de 8 en 8 (dos bloques de 4 con SSE)
        for (; x <= safe_out_w - 8; x += 8) {
            // BLOQUE 1 (Píxeles 0-3)
            __m128i v_curr_fp = _mm_add_epi32(_mm_set1_epi32(curr_x_fp), v_step);
            __m128i v_idx = _mm_srli_epi32(v_curr_fp, 16); // Shift para obtener parte entera
            
            // Extraer índices y leer (Gather manual)
            uint32_t i0 = (uint32_t)_mm_extract_epi32(v_idx, 0);
            uint32_t i1 = (uint32_t)_mm_extract_epi32(v_idx, 1);
            uint32_t i2 = (uint32_t)_mm_extract_epi32(v_idx, 2);
            uint32_t i3 = (uint32_t)_mm_extract_epi32(v_idx, 3);
            
            // Escritura agrupada de 64 bits (4 píxeles de 16 bits)
            *((uint64_t*)&line_dst[x]) = (uint64_t)line_src[i0] | ((uint64_t)line_src[i1] << 16) | 
                                         ((uint64_t)line_src[i2] << 32) | ((uint64_t)line_src[i3] << 48);

            // BLOQUE 2 (Píxeles 4-7)
            int next_x_fp = curr_x_fp + (inv_scale_x_fp * 4);
            v_curr_fp = _mm_add_epi32(_mm_set1_epi32(next_x_fp), v_step);
            v_idx = _mm_srli_epi32(v_curr_fp, 16);

            uint32_t i4 = (uint32_t)_mm_extract_epi32(v_idx, 0);
            uint32_t i5 = (uint32_t)_mm_extract_epi32(v_idx, 1);
            uint32_t i6 = (uint32_t)_mm_extract_epi32(v_idx, 2);
            uint32_t i7 = (uint32_t)_mm_extract_epi32(v_idx, 3);

            *((uint64_t*)&line_dst[x + 4]) = (uint64_t)line_src[i4] | ((uint64_t)line_src[i5] << 16) | 
                                             ((uint64_t)line_src[i6] << 32) | ((uint64_t)line_src[i7] << 48);

            curr_x_fp += inv_scale_x_fp * 8;
        }

        // Limpieza final (mismo que antes)
        for (; x < out_w; x++) {
            int sx = (x * inv_scale_x_fp) >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
        }
    }

    // Filas finales (Zona de riesgo)
    for (int y = safe_out_h; y < out_h; y++) {
        uint16_t* line_src = src + ((sh - 1) * src_stride);
        uint16_t* line_dst = dst_start + (y * dst_stride);
        for (int x = 0; x < out_w; x++) {
            int sx = (x * inv_scale_x_fp) >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
        }
    }
}

/**
 * Reescalador por software optimizado con Intrínsecos x86 (SSE4.1)
 * Soporta cualquier ratio (4:3, 16:9, etc.) mediante scaleMode.
 * scaleMode: 0 = Pantalla Completa (Stretched), 1 = Mantener Aspecto (Fit)
 */
inline void scale_software_fixed_point_noif_x86(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    if (!src || !dst_base || sw <= 0 || sh <= 0) return;

    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);

    // Centrado de la imagen en el buffer de destino
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    // 2. Límites seguros para evitar IFs dentro de los bucles
    int safe_h = out_h;
    if (safe_h > 0 && ((safe_h - 1) * inv_scale_y_fp >> 16) >= sh) safe_h--;

    int safe_w = out_w;
    if (safe_w > 0 && ((safe_w - 1) * inv_scale_x_fp >> 16) >= sw) safe_w--;

    // Preparar constantes SSE
    __m128i v_step_x = _mm_setr_epi32(0, 1, 2, 3);
    __m128i v_inv_scale_x = _mm_set1_epi32(inv_scale_x_fp);
    __m128i v_inv_scale_x4 = _mm_set1_epi32(inv_scale_x_fp * 4);

    // 3. Bucle Principal (Eje Y)
    for (int y = 0; y < safe_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int x = 0;
        int curr_src_x_fp = 0;

        // --- BUCLE VECTORIZADO X (4 píxeles por iteración) ---
        for (; x <= safe_w - 4; x += 4) {
            // Calcular 4 índices de origen simultáneamente: {curr, curr+s, curr+2s, curr+3s}
            __m128i v_curr_fp = _mm_add_epi32(_mm_set1_epi32(curr_src_x_fp), 
                                             _mm_mullo_epi32(v_inv_scale_x, v_step_x));
            
            // Shift a la derecha para obtener los índices reales (>> 16)
            __m128i v_idx = _mm_srli_epi32(v_curr_fp, 16);

            // Gather manual (Extracción de índices del registro SSE)
            // SSE4.1 no tiene gather nativo, pero el cálculo de índices en paralelo ya ahorra mucho
            __declspec(align(16)) uint32_t idx[4];
            _mm_store_si128((__m128i*)idx, v_idx);

            // Lectura de píxeles (4x 16-bit)
            uint16_t p0 = line_src[idx[0]];
            uint16_t p1 = line_src[idx[1]];
            uint16_t p2 = line_src[idx[2]];
            uint16_t p3 = line_src[idx[3]];

            // Escritura de 64 bits (4 píxeles de una vez)
            // Esto es mucho más rápido que 4 escrituras de 16 bits
            *((uint64_t*)&line_dst[x]) = (uint64_t)p0 | ((uint64_t)p1 << 16) | 
                                         ((uint64_t)p2 << 32) | ((uint64_t)p3 << 48);

            curr_src_x_fp += inv_scale_x_fp * 4;
        }

        // Cleanup X: Píxeles restantes en la línea
        for (; x < out_w; x++) {
            int sx = (x * inv_scale_x_fp) >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
        }
    }

    // 4. Cleanup Y: Filas finales (si las hay por redondeo)
    for (int y = safe_h; y < out_h; y++) {
        uint16_t* line_src = src + ((sh - 1) * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);
        for (int x = 0; x < out_w; x++) {
            int sx = (x * inv_scale_x_fp) >> 16;
            line_dst[x] = line_src[sx >= sw ? sw - 1 : sx];
        }
    }
}



inline void scale_software_fixed_point_sse2_safe(uint16_t* __restrict src, uint16_t* __restrict dst_base, int sw, int sh, std::size_t spitch, int dw, int dh, std::size_t dpitch, int scaleMode, float ratio) {
    int out_w, out_h, inv_scale_x_fp, inv_scale_y_fp;
    calcDestDimFromRatio(sw, sh, dw, dh, ratio, out_w, out_h, inv_scale_x_fp, inv_scale_y_fp);

    int src_stride = (int)(spitch >> 1);
    int dst_stride = (int)(dpitch >> 1);
    uint16_t* dst = dst_base + (((dh - out_h) / 2) * dst_stride) + ((dw - out_w) / 2);

    for (int y = 0; y < out_h; y++) {
        int src_y = (y * inv_scale_y_fp) >> 16;
        
        // SEGURIDAD Y: Evitar leer filas fuera de rango
        if (src_y >= sh) src_y = sh - 1;

        uint16_t* line_src = src + (src_y * src_stride);
        uint16_t* line_dst = dst + (y * dst_stride);

        int x = 0;
        // Procesar de 8 en 8 píxeles (Bucle Rápido sin comprobaciones)
        // Dejamos un margen de seguridad de 8 píxeles para el bucle de limpieza
        for (; x <= out_w - 8; x += 8) {
            int x0 = (x * inv_scale_x_fp);
            
            line_dst[x + 0] = line_src[(x0 + 0 * inv_scale_x_fp) >> 16];
            line_dst[x + 1] = line_src[(x0 + 1 * inv_scale_x_fp) >> 16];
            line_dst[x + 2] = line_src[(x0 + 2 * inv_scale_x_fp) >> 16];
            line_dst[x + 3] = line_src[(x0 + 3 * inv_scale_x_fp) >> 16];
            line_dst[x + 4] = line_src[(x0 + 4 * inv_scale_x_fp) >> 16];
            line_dst[x + 5] = line_src[(x0 + 5 * inv_scale_x_fp) >> 16];
            line_dst[x + 6] = line_src[(x0 + 6 * inv_scale_x_fp) >> 16];
            line_dst[x + 7] = line_src[(x0 + 7 * inv_scale_x_fp) >> 16];
        }

        // Bucle de limpieza con SEGURIDAD X
        // Aquí manejamos los últimos píxeles donde el redondeo podría fallar
        int curr_src_x_fp = x * inv_scale_x_fp;
        for (; x < out_w; x++) {
            int sx = curr_src_x_fp >> 16;
            if (sx >= sw) sx = sw - 1; // Solo se evalúa en los últimos píxeles
            line_dst[x] = line_src[sx];
            curr_src_x_fp += inv_scale_x_fp;
        }
    }
}


#endif