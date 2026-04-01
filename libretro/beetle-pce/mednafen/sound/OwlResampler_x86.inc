//
// Don't use vzeroall in AVX code unless we mark ymm8-ymm15/xmm8-xmm15 as clobbers on x86_64
// (Also, remember to mark xmm* instead of ymm* as clobbers if __AVX__ isn't defined!)

#if defined(__x86_64__) && !defined(__ILP32__)
#define X86_REGC "r"
#define X86_REGAT ""
#else
#define X86_REGC "e"
#define X86_REGAT "l"
#endif

static INLINE void DoMAC_AVX_32X(float* wave, float* coeffs, int32 count, int32* accum_output)
{
    // Inicializar registros a cero (vxorps)
    __m256 ymm3 = _mm256_setzero_ps();
    __m256 ymm4 = _mm256_setzero_ps();
    __m256 ymm5 = _mm256_setzero_ps();
    __m256 ymm6 = _mm256_setzero_ps();
    __m256 ymm7 = _mm256_setzero_ps();

    int loop_count = count >> 5; // count / 32
    float* di = wave;
    float* si = coeffs;

    for (int i = 0; i < loop_count; i++)
    {
        // Carga de datos (vmovups)
        __m256 ymm0 = _mm256_loadu_ps(di);
        __m256 ymm1 = _mm256_loadu_ps(di + 8);  // 32 bytes = 8 floats
        
        // Multiplicación y acumulación 1
        ymm0 = _mm256_mul_ps(ymm0, _mm256_loadu_ps(si));
        ymm7 = _mm256_add_ps(ymm7, ymm3);

        __m256 ymm2 = _mm256_loadu_ps(di + 16);
        ymm1 = _mm256_mul_ps(ymm1, _mm256_loadu_ps(si + 8));
        ymm4 = _mm256_add_ps(ymm4, ymm0);

        ymm3 = _mm256_loadu_ps(di + 24);
        ymm2 = _mm256_mul_ps(ymm2, _mm256_loadu_ps(si + 16));
        ymm5 = _mm256_add_ps(ymm5, ymm1);

        // Preparar ymm0 para la siguiente iteración
        ymm3 = _mm256_mul_ps(ymm3, _mm256_loadu_ps(si + 24));
        ymm6 = _mm256_add_ps(ymm6, ymm2);

        di += 32;
        si += 32;
    }

    ymm7 = _mm256_add_ps(ymm7, ymm3);

    // Sumar los registros acumuladores
    ymm5 = _mm256_add_ps(ymm4, ymm5);
    ymm7 = _mm256_add_ps(ymm6, ymm7);
    ymm7 = _mm256_add_ps(ymm5, ymm7);

    // Suma horizontal (vhaddps)
    ymm7 = _mm256_hadd_ps(ymm7, ymm7);
    ymm7 = _mm256_hadd_ps(ymm7, ymm7);

    // Extraer la parte alta (vextractf128) y sumar
    __m128 xmm7_low  = _mm256_castps256_ps128(ymm7);
    __m128 xmm7_high = _mm256_extractf128_ps(ymm7, 1);
    __m128 xmm_res   = _mm_add_ss(xmm7_low, xmm7_high);

    // Convertir a entero (vcvtss2si) y devolver
    *accum_output = _mm_cvtss_si32(xmm_res);
}
static INLINE void DoMAC_AVX_32X_P16(float* wave, float* coeffs, int32 count, int32* accum_output)
{
    // vxorps - Inicialización a cero
    __m256 ymm3 = _mm256_setzero_ps();
    __m256 ymm4 = _mm256_setzero_ps();
    __m256 ymm5 = _mm256_setzero_ps();
    __m256 ymm6 = _mm256_setzero_ps();
    __m256 ymm7 = _mm256_setzero_ps();

    int loop_count = count >> 5; // count / 32
    float* di = wave;
    float* si = coeffs;

    // Primer elemento fuera del bucle para replicar "vmovups 0(di), ymm0"
    __m256 ymm0 = _mm256_loadu_ps(di);

    // Bucle principal (1b)
    for (int i = 0; i < loop_count; i++)
    {
        __m256 ymm1 = _mm256_loadu_ps(di + 8);
        ymm0 = _mm256_mul_ps(ymm0, _mm256_loadu_ps(si));
        ymm7 = _mm256_add_ps(ymm7, ymm3);

        __m256 ymm2 = _mm256_loadu_ps(di + 16);
        ymm1 = _mm256_mul_ps(ymm1, _mm256_loadu_ps(si + 8));
        ymm4 = _mm256_add_ps(ymm4, ymm0);

        ymm3 = _mm256_loadu_ps(di + 24);
        ymm2 = _mm256_mul_ps(ymm2, _mm256_loadu_ps(si + 16));
        ymm5 = _mm256_add_ps(ymm5, ymm1);

        ymm0 = _mm256_loadu_ps(di + 32); // vmovups 128(di)
        ymm3 = _mm256_mul_ps(ymm3, _mm256_loadu_ps(si + 24));
        ymm6 = _mm256_add_ps(ymm6, ymm2);

        di += 32; // add 128 (4 bytes * 32)
        si += 32;
    }

    // --- BLOQUE P16 (El código extra tras el jnz) ---
    __m256 ymm1_p16 = _mm256_loadu_ps(di + 8); 
    ymm0 = _mm256_mul_ps(ymm0, _mm256_loadu_ps(si));
    ymm7 = _mm256_add_ps(ymm7, ymm3);
    ymm1_p16 = _mm256_mul_ps(ymm1_p16, _mm256_loadu_ps(si + 8));
    ymm4 = _mm256_add_ps(ymm4, ymm0);
    ymm5 = _mm256_add_ps(ymm1_p16, ymm5);

    // --- Reducción de registros ---
    ymm5 = _mm256_add_ps(ymm4, ymm5);
    ymm7 = _mm256_add_ps(ymm6, ymm7);
    ymm7 = _mm256_add_ps(ymm5, ymm7);

    // Sumas horizontales (vhaddps)
    ymm7 = _mm256_hadd_ps(ymm7, ymm7);
    ymm7 = _mm256_hadd_ps(ymm7, ymm7);

    // Extracción de 128 bits y suma final (vextractf128 + vaddss)
    __m128 xmm7_low  = _mm256_castps256_ps128(ymm7);
    __m128 xmm6_high = _mm256_extractf128_ps(ymm7, 1);
    __m128 xmm_final = _mm_add_ss(xmm7_low, xmm6_high);

    // vcvtss2si
    *accum_output = _mm_cvtss_si32(xmm_final);
}



static INLINE void DoMAC_SSE_16X(float *wave, float *coeffs, int32 count, int32 *accum_output)
{
    // Inicialización de registros (xorps)
    __m128 xmm3 = _mm_setzero_ps();
    __m128 xmm4 = _mm_setzero_ps();
    __m128 xmm5 = _mm_setzero_ps();
    __m128 xmm6 = _mm_setzero_ps();
    __m128 xmm7 = _mm_setzero_ps();

    int loop_count = count >> 4;
    float* di = wave;   // Equivalente a EDI/RDI
    float* si = coeffs; // Equivalente a ESI/RSI

    // Bucle principal (16 coeficientes por iteración)
    for (int i = 0; i < loop_count; i++)
    {
        // Carga y procesamiento similar al bloque asm
        __m128 xmm0 = _mm_loadu_ps(di);      // movups 0(di)
        __m128 xmm1 = _mm_loadu_ps(di + 4);  // movups 16(di)
        
        xmm0 = _mm_mul_ps(xmm0, _mm_loadu_ps(si)); // mulps 0(si)
        xmm7 = _mm_add_ps(xmm7, xmm3);             // addps xmm3, xmm7

        __m128 xmm2 = _mm_loadu_ps(di + 8);  // movups 32(di)
        xmm1 = _mm_mul_ps(xmm1, _mm_loadu_ps(si + 4)); // mulps 16(si)
        xmm4 = _mm_add_ps(xmm4, xmm0);             // addps xmm0, xmm4

        xmm3 = _mm_loadu_ps(di + 12);        // movups 48(di)
        xmm2 = _mm_mul_ps(xmm2, _mm_loadu_ps(si + 8)); // mulps 32(si)
        xmm5 = _mm_add_ps(xmm5, xmm1);             // addps xmm1, xmm5

        // Preparar xmm0 para la siguiente vuelta (pipeline)
        __m128 xmm0_next = _mm_loadu_ps(di + 16); // movups 64(di)
        xmm3 = _mm_mul_ps(xmm3, _mm_loadu_ps(si + 12)); // mulps 48(si)
        xmm6 = _mm_add_ps(xmm6, xmm2);             // addps xmm2, xmm6

        di += 16;
        si += 16;
        xmm0 = xmm0_next; // Reasignar para el inicio del bucle
    }

    xmm7 = _mm_add_ps(xmm7, xmm3);

    // Suma de los cuatro registros acumuladores
    xmm4 = _mm_add_ps(xmm4, xmm5);
    xmm7 = _mm_add_ps(xmm7, xmm6);
    xmm7 = _mm_add_ps(xmm7, xmm4);

    // Suma horizontal (shufps $27...)
    __m128 xmm4_shuf = _mm_shuffle_ps(xmm7, xmm7, _MM_SHUFFLE(0, 1, 2, 3));
    xmm7 = _mm_add_ps(xmm7, xmm4_shuf);

    // Suma horizontal final (shufps $1...)
    xmm4_shuf = _mm_shuffle_ps(xmm7, xmm7, _MM_SHUFFLE(0, 0, 0, 1));
    xmm7 = _mm_add_ss(xmm7, xmm4_shuf);

    // Convertir a entero y guardar (cvtss2si)
    *accum_output = _mm_cvtss_si32(xmm7);
}

