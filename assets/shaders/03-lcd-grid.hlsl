/* === LCD3x: port de Gigaherz' LCD3x.cg (public domain). =====================  *
 * Simulacion sinusoidal de la rejilla LCD para handhelds (GB/GBC/GBA/DS, etc). *
 * Reemplaza al port anterior basado en lcd-grid-v2.slang (cgwg, GPL) con:      *
 *                                                                              *
 *   - Modulacion sin() de scanlines verticales (Y) + subpixeles RGB (X).      *
 *   - 3 offsets de fase a 120 grados para R/G/B - patron clasico LCD a tira. *
 *   - 1 sola muestra de textura (vs 4 en LCD-Grid-v2).                        *
 *   - Sin integrales de smear filter ni pow(): ~20 ALU + 4 sin() en total.    *
 *                                                                              *
 * Diferencia visual vs lcd-grid-v2:                                            *
 *   - lcd-grid-v2: subpixeles RECTANGULARES discretos con gaps negros y       *
 *     pre-filter integral. Aspecto "rejilla matriz" tipo GBA.                  *
 *   - LCD3x: modulacion suave sinusoidal continua. Aspecto "phosphors" mas    *
 *     proximo al tube simulation, sin gaps duros.                              *
 *                                                                              *
 * Coste estimado en Xenos:                                                     *
 *   - 1 tex2D + 4 sin() + ~15 mul/add. ~3-4x mas barato que lcd-grid-v2.      *
 *                                                                              *
 * Configurabilidad:                                                            *
 *   - brighten_scanlines (16): controla la profundidad del oscurecimiento en *
 *     scanlines. Mas alto = menos efecto, imagen mas brillante.                *
 *   - brighten_lcd (4): igual para los subpixeles RGB. Mas alto = colores    *
 *     menos saturados pero pantalla mas brillante.                             *
 *                                                                              *
 * El vertex shader original precomputaba `omega = 2*PI*texture_size`. En el   *
 * port PS-only lo calculamos inline (una multiplicacion, despreciable).        */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 /* Parametros (constantes del original Gigaherz). Mas alto = imagen
    mas brillante pero efecto menos pronunciado. */
 #define BRIGHTEN_SCANLINES 16.0
 #define BRIGHTEN_LCD        4.0

 static const float PI = 3.141592654;

 /* Phase offsets de los 3 subpixeles RGB:
      R: PI * (1/2)         =  PI/2  (peak en mitad del ciclo)
      G: PI * (1/2 - 2/3)   = -PI/6  (120 grados desfasado)
      B: PI * (1/2 - 4/3)   = -5PI/6 (240 grados desfasado)
    Resultado: cada x del texel ilumina principalmente un canal RGB.    */
 static const float3 OFFSETS =
     float3(PI * 0.5, PI * (0.5 - 2.0/3.0), PI * (0.5 - 4.0/3.0));

 float4 main(PS_IN In) : COLOR0
 {
     float3 res = tex2D(detail, In.TexCoord).rgb;

     /* omega: frecuencia angular del patron, un ciclo por texel.       */
     float2 omega = 2.0 * PI * textureDims;
     float2 angle = In.TexCoord * omega;

     /* yfactor: oscurecimiento por scanline (en [N/(N+1), 1]).          */
     float yfactor =
         (BRIGHTEN_SCANLINES + sin(angle.y)) / (BRIGHTEN_SCANLINES + 1.0);

     /* xfactors: per-channel weighting con phase shifts R/G/B.          */
     float3 xfactors =
         (BRIGHTEN_LCD + sin(angle.x + OFFSETS)) / (BRIGHTEN_LCD + 1.0);

     float3 color = yfactor * xfactors * res;
     return float4(color, 1.0);
 }

