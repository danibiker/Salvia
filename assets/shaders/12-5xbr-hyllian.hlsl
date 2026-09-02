/* === 5xBR-v3.8a (Hyllian rounded) ============================================ *
 * Port de "Hyllian's 5xBR v3.8a (rounded)" shader (GPL, Sergio "Hyllian" Diaz).  *
 * Evolucion directa de v3.7a: mismo algoritmo de edge detection (5x5,            *
 * weighted_distance con outer ring, inecuaciones de linea a 30/45/60), pero      *
 * con OUTPUT SUAVIZADO mediante smoothstep + lerp en lugar de pixel substitution.*
 *                                                                                *
 * Cambios clave vs v3.7a:                                                        *
 *   - fx/fx_left/fx_up se calculan SIN step() boolean, mantienen el valor raw   *
 *     (Ao*fp.y + Bo*fp.x).                                                       *
 *   - fx45 = smoothstep(Co - 0.2, Co + 0.2, fx) (idem fx30, fx60).               *
 *     Genera transiciones suaves de ancho 0.4 alrededor del threshold.           *
 *   - 3 finales separados (final45/30/60) y `maximo = max(...)` como factor de  *
 *     blend final en lugar de hard substitution.                                 *
 *   - Output: res = lerp(E, pix, maximo).                                        *
 *                                                                                *
 * Resultado: bordes con AA en las transiciones de subpixel - elimina el         *
 * "staircase" visible que v3.7a generaba en algunas zonas. Mismo coste GPU      *
 * dentro del margen de error (~5 ALU mas por los smoothstep + dot products).    *
 *                                                                                *
 * Estructura del algoritmo (igual que v3.7a):                                    *
 *   - 5x5 (21 tex2D): 9 muestras del 3x3 interior + 12 del anillo exterior.    *
 *   - weighted_distance usa h5/f4/i4/i5 para distinguir diagonales largas de   *
 *     esquinas falsas.                                                           *
 *   - Edge detection con threshold step (epsilon 0.001) - mismo trade-off       *
 *     dithering-robustness que la v3.7a port.                                   *
 *                                                                                *
 * Color space:                                                                   *
 *   - El original v3.8a declara matriz YUV completa pero solo usa la fila Y    *
 *     (mismas weights NTSC que v3.7a: 14.352, 28.176, 5.472). Las componentes  *
 *     U/V quedan declaradas pero no se aplican en este shader (probable        *
 *     preparacion para xBR-lv3 que si las usa).                                 *
 *                                                                                *
 * Coste (estimado Xenos):                                                        *
 *   - 21 tex2D + ~115 ALU (vs ~110 en v3.7a). Diferencia despreciable.         *
 *                                                                                *
 * Conversion Cg -> HLSL D3D9:                                                    *
 *   - half3/half4 -> float3/float4.                                              *
 *   - mul(float4x3(A,B,C,D), yuv_weighted[0]) -> 4x dot(_, yuv) per-row.         *
 *   - bool4 + && + dot(bool, fx) collapsed a productos float-coded.             *
 *   - if-else chain de salida -> lerp chain en orden inverso (priority).        *
 *   - 3x render target scale (comparte infraestructura HQ3x).                   */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 static const float  coef = 2.0;
 /* YUV NTSC weights, equivalente a 48*(0.299, 0.587, 0.114) del original.   */
 static const float3 yuv  = float3(14.352, 28.176, 5.472);
 /* delta del smoothstep: ancho de la zona de transicion alrededor del
    threshold de cada angulo.  0.2 = +/-0.2 -> banda de 0.4 unidades.        */
 static const float4 delta = float4(0.2, 0.2, 0.2, 0.2);

 float4 df(float4 A, float4 B) { return abs(A - B); }

 /* weighted_distance: 4*df(g,h) + df(a,b)+df(a,c)+df(d,e)+df(d,f).          */
 float4 wdist(float4 a, float4 b, float4 c, float4 d,
              float4 e, float4 f, float4 g, float4 h)
 {
     return df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h);
 }

 float4 main(PS_IN In) : COLOR0
 {
     float2 ps = 1.0 / textureDims;
     float  dx = ps.x;
     float  dy = ps.y;
     float2 uv = In.TexCoord;
     float2 fp = frac(uv * textureDims);

     /* === 3x3 inner neighborhood ===
          A B C
          D E F
          G H I                                                                */
     float3 A = tex2D(detail, uv + float2(-dx, -dy)).rgb;
     float3 B = tex2D(detail, uv + float2(  0, -dy)).rgb;
     float3 C = tex2D(detail, uv + float2( dx, -dy)).rgb;
     float3 D = tex2D(detail, uv + float2(-dx,   0)).rgb;
     float3 E = tex2D(detail, uv + float2(  0,   0)).rgb;
     float3 F = tex2D(detail, uv + float2( dx,   0)).rgb;
     float3 G = tex2D(detail, uv + float2(-dx,  dy)).rgb;
     float3 H = tex2D(detail, uv + float2(  0,  dy)).rgb;
     float3 I = tex2D(detail, uv + float2( dx,  dy)).rgb;

     /* === 5x5 outer ring (12 muestras extra) ===                            */
     float3 A1 = tex2D(detail, uv + float2(-dx,    -2.0*dy)).rgb;
     float3 C1 = tex2D(detail, uv + float2( dx,    -2.0*dy)).rgb;
     float3 A0 = tex2D(detail, uv + float2(-2.0*dx, -dy   )).rgb;
     float3 G0 = tex2D(detail, uv + float2(-2.0*dx,  dy   )).rgb;
     float3 C4 = tex2D(detail, uv + float2( 2.0*dx, -dy   )).rgb;
     float3 I4 = tex2D(detail, uv + float2( 2.0*dx,  dy   )).rgb;
     float3 G5 = tex2D(detail, uv + float2(-dx,     2.0*dy)).rgb;
     float3 I5 = tex2D(detail, uv + float2( dx,     2.0*dy)).rgb;
     float3 B1 = tex2D(detail, uv + float2(  0,    -2.0*dy)).rgb;
     float3 D0 = tex2D(detail, uv + float2(-2.0*dx,  0    )).rgb;
     float3 H5 = tex2D(detail, uv + float2(  0,     2.0*dy)).rgb;
     float3 F4 = tex2D(detail, uv + float2( 2.0*dx,  0    )).rgb;

     /* === Lumas empaquetadas (4 rotaciones simultaneas) ===                 */
     float4 b = float4(dot(B, yuv), dot(D, yuv), dot(H, yuv), dot(F, yuv));
     float4 c = float4(dot(C, yuv), dot(A, yuv), dot(G, yuv), dot(I, yuv));
     float  E_lum = dot(E, yuv);
     float4 e = float4(E_lum, E_lum, E_lum, E_lum);
     float4 d = b.yzwx;
     float4 f = b.wxyz;
     float4 g = c.zwxy;
     float4 h = b.zwxy;
     float4 i = c.wxyz;

     float4 i4 = float4(dot(I4, yuv), dot(C1, yuv), dot(A0, yuv), dot(G5, yuv));
     float4 i5 = float4(dot(I5, yuv), dot(C4, yuv), dot(A1, yuv), dot(G0, yuv));
     float4 h5 = float4(dot(H5, yuv), dot(F4, yuv), dot(B1, yuv), dot(D0, yuv));
     float4 f4 = h5.yzwx;

     /* === Inecuaciones de linea (RAW, sin step) ===
        En v3.8a guardamos el valor sin clipear para que el smoothstep
        produzca la transicion suave.                                         */
     float4 Ao = float4( 1.0, -1.0, -1.0,  1.0);
     float4 Bo = float4( 1.0,  1.0, -1.0, -1.0);
     float4 Co = float4( 1.5,  0.5, -0.5,  0.5);
     float4 Bx = float4( 0.5,  2.0, -0.5, -2.0);
     float4 Cx = float4( 1.0,  1.0, -0.5,  0.0);
     float4 By = float4( 2.0,  0.5, -2.0, -0.5);
     float4 Cy = float4( 2.0,  0.0, -1.0,  0.5);

     float4 fx      = Ao*fp.y + Bo*fp.x;
     float4 fx_left = Ao*fp.y + Bx*fp.x;
     float4 fx_up   = Ao*fp.y + By*fp.x;

     /* smoothstep produce el blend factor por cada angulo en [0,1].
        Reemplaza el step() boolean del v3.7a por una rampa de ancho 0.4.    */
     float4 fx45 = smoothstep(Co - delta, Co + delta, fx);
     float4 fx30 = smoothstep(Cx - delta, Cx + delta, fx_left);
     float4 fx60 = smoothstep(Cy - delta, Cy + delta, fx_up);

     /* Interpolation restrictions: e!=f && e!=h, etc. Float-coded con
        epsilon 0.001 (igual que v3.7a).                                      */
     float4 ir_lv1      = step(0.001, df(e, f)) * step(0.001, df(e, h));
     float4 ir_lv2_left = step(0.001, df(e, g)) * step(0.001, df(d, g));
     float4 ir_lv2_up   = step(0.001, df(e, c)) * step(0.001, df(b, c));

     /* Weighted distance comparisons.                                        */
     float4 w1 = wdist(e, c, g, i, h5, f4, h, f);
     float4 w2 = wdist(h, d, i5, f, i4, b, e, i);

     float4 edr      = step(w1 + 0.0001, w2)            * ir_lv1;
     float4 edr_left = step(coef * df(f, g), df(h, c))   * ir_lv2_left;
     float4 edr_up   = step(coef * df(h, c), df(f, g))   * ir_lv2_up;

     /* px.X: elige entre las dos alternativas de cada rotacion.              */
     float4 px = step(df(e, f), df(e, h));

     /* final45/30/60: smoothstep value de la rotacion activa, ponderado
        por su edge detection mask.  dot() en lugar de seleccion explicita
        porque normalmente solo una rotacion esta activa por pixel.           */
     float final45 = dot(edr,            fx45);
     float final30 = dot(edr * edr_left, fx30);
     float final60 = dot(edr * edr_up,   fx60);
     float maximo  = max(max(final30, final60), final45);

     /* Mascaras binarias para la seleccion de pix (que rotacion gana).
        Solo nos importa si ESTA activa (>epsilon), no su magnitud.          */
     float4 active = edr * saturate(fx45 + edr_left*fx30 + edr_up*fx60);
     float4 nc     = step(0.001, active);

     /* Reverse-order lerp chain para preservar la prioridad del if-else:
        nc.x es la prioridad maxima, asi que la aplicamos al final
        (sobrescribe).                                                        */
     float3 pix = E;
     pix = lerp(pix, lerp(D, H, px.w), nc.w);
     pix = lerp(pix, lerp(B, D, px.z), nc.z);
     pix = lerp(pix, lerp(F, B, px.y), nc.y);
     pix = lerp(pix, lerp(H, F, px.x), nc.x);

     /* Smooth blend final: maximo es 0 si no hay edge, sino el smoothstep
        de la rotacion activa.  Genera la transicion suave en los bordes
        de subpixel.                                                          */
     float3 res = lerp(E, pix, saturate(maximo));
     return float4(res, 1.0);
 }

