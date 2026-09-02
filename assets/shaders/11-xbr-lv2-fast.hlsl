/* xBR-lv2-fast: Hyllian's xBR Level 2 "fast" single-pass upscaler.           *
 * Original .cg by Hyllian (sergiogdb@gmail.com), 2011-2015 - MIT license.    *
 * Adapted to Salvia's D3D9 pattern (single PS, sampler s0, register c1).    *
 *                                                                            *
 * Version simplificada de xBR-lv2 con muestreo 3x3 (vs 5x5 de lv3), sin     *
 * calculo de los angulos a 15/75 y sin la logica de level-3 restrictions. *
 * El resultado tiene calidad similar a lv2 estandar en la mayoria de pixel  *
 * art (sprites NES/SNES/MD), pero a ~1/3 del coste de xBR-lv3.               *
 *                                                                            *
 * Cost (estimado en Xenos):                                                  *
 *   - 9 tex2D (3x3 neighborhood, sin outer ring).                            *
 *   - ~50 ALU operations.                                                    *
 *   => Considerablemente mas rapido que xBR-lv3 (21 tex + ~120 ALU). Deberia *
 *     mantener 60fps incluso a resoluciones tipo Saturn/PSX (320-512 px).   *
 *                                                                            *
 * Scale-invariant: el algoritmo usa `fp = frac(uv*texDims)` (subpixel       *
 * position) para escoger el color interpolado. Renderizamos a 3x con la    *
 * misma infraestructura que HQ3x.                                            *
 *                                                                            *
 * Conversion notes Cg -> HLSL D3D9 (identicas a xBR-lv3):                     *
 *   - bool4 logical ops (&&, ||) reemplazadas por arithmetic float-coded.   *
 *   - (e!=f) "exact" inequality -> nexact() con epsilon 0.0001.              *
 *   - !eq(a,b) threshold-based -> ne_f() con XBR_EQ_THRESHOLD.               *
 *   - mul(float4x3, vec3) -> per-row dot products en float4.                  *
 *   - VS-precomputed texcoords -> calculados en PS (Xenos unified absorbe la *
 *     ALU extra y no hay overhead de interpoladores adicionales).            *
 *   - CORNER_C activo (default) + SMOOTH_TIPS habilitado (fx45i incluido).  */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 /* Defines de algoritmo. XBR_SCALE = 3 para que delta/deltaL/deltaU coincidan
    con la escala del render target (Salvia case 7 -> 3x via HQ3x infra).      */
 #define XBR_SCALE         3.0
 #define XBR_Y_WEIGHT     48.0
 #define XBR_EQ_TH        15.0
 #define XBR_LV2_COEF      2.0
 #define EPS_EXACT       0.0001

 /* Constantes file-scope -> van al constant pool, no consumen temp registers.*/
 static const float3 Y     = float3(0.2126, 0.7152, 0.0722);
 static const float4 Ao    = float4(1.0,-1.0,-1.0, 1.0);
 static const float4 Bo    = float4(1.0, 1.0,-1.0,-1.0);
 static const float4 Co    = float4(1.5, 0.5,-0.5, 0.5);
 static const float4 Bx    = float4(0.5, 2.0,-0.5,-2.0);
 static const float4 Cx    = float4(1.0, 1.0,-0.5, 0.0);
 static const float4 By    = float4(2.0, 0.5,-2.0,-0.5);
 static const float4 Cy    = float4(2.0, 0.0,-1.0, 0.5);
 static const float4 Ci    = float4(0.25, 0.25, 0.25, 0.25);
 static const float4 delta  = float4(1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE);
 static const float4 deltaL = float4(0.5/XBR_SCALE, 1.0/XBR_SCALE, 0.5/XBR_SCALE, 1.0/XBR_SCALE);
 static const float4 deltaU = float4(1.0/XBR_SCALE, 0.5/XBR_SCALE, 1.0/XBR_SCALE, 0.5/XBR_SCALE);

 /* Helpers float-codificados: 1.0 = true, 0.0 = false. */
 float4 df_(float4 A, float4 B)    { return abs(A - B); }
 float  c_df_(float3 a, float3 b)  { return abs(a.r-b.r)+abs(a.g-b.g)+abs(a.b-b.b); }
 float4 eq_f(float4 A, float4 B)   { return 1.0 - step(XBR_EQ_TH, df_(A,B)); }
 float4 ne_f(float4 A, float4 B)   { return       step(XBR_EQ_TH, df_(A,B)); }
 float4 nexact(float4 A, float4 B) { return       step(EPS_EXACT, df_(A,B)); }

 float4 main(PS_IN In) : COLOR0
 {
     float2 ps = 1.0 / textureDims;
     float  dx = ps.x;
     float  dy = ps.y;
     float2 uv = In.TexCoord;

     /* Solo vecindario 3x3:
          A B C
          D E F
          G H I       (9 samples - vs 21 de xBR-lv3).                          */
     float3 A = tex2D(detail, uv + float2(-dx, -dy)).rgb;
     float3 B = tex2D(detail, uv + float2(  0, -dy)).rgb;
     float3 C = tex2D(detail, uv + float2( dx, -dy)).rgb;
     float3 D = tex2D(detail, uv + float2(-dx,   0)).rgb;
     float3 E = tex2D(detail, uv + float2(  0,   0)).rgb;
     float3 F = tex2D(detail, uv + float2( dx,   0)).rgb;
     float3 G = tex2D(detail, uv + float2(-dx,  dy)).rgb;
     float3 H = tex2D(detail, uv + float2(  0,  dy)).rgb;
     float3 I = tex2D(detail, uv + float2( dx,  dy)).rgb;

     float2 fp = frac(uv * textureDims);

     /* Empaquetamos lumas en float4 con las 4 orientaciones rotacionales:
          b = (B, D, H, F)        c = (C, A, G, I)        e broadcast(E)
        a = c.yzwx, d = b.yzwx, f = b.wxyz,
        g = c.zwxy, h = b.zwxy, i = c.wxyz   (swizzles in-line, sin temps).  */
     float4 b = float4(dot(B, Y), dot(D, Y), dot(H, Y), dot(F, Y)) * XBR_Y_WEIGHT;
     float4 c = float4(dot(C, Y), dot(A, Y), dot(G, Y), dot(I, Y)) * XBR_Y_WEIGHT;
     float  E_lum = dot(E, Y) * XBR_Y_WEIGHT;
     float4 e = float4(E_lum, E_lum, E_lum, E_lum);

     /* === Interpolation restrictions ===
        lv0 = (e != f) && (e != h) - usa exact inequality
        lv1 = lv0 && (!eq(f,b) && !eq(f,c)
                       || !eq(h,d) && !eq(h,g)
                       || eq(e,g) || eq(e,c))   - usa threshold-based eq.    */
     float4 interp_lv0 = nexact(e, b.wxyz) * nexact(e, b.zwxy);
     float4 interp_lv1 = interp_lv0 * saturate(
           ne_f(b.wxyz, b)      * ne_f(b.wxyz, c)
         + ne_f(b.zwxy, b.yzwx) * ne_f(b.zwxy, c.zwxy)
         + eq_f(e, c.zwxy)
         + eq_f(e, c));
     float4 interp_lv2_left = nexact(e, c.zwxy) * nexact(b.yzwx, c.zwxy);
     float4 interp_lv2_up   = nexact(e, c)      * nexact(b, c);

     /* === Weighted distances inlined ===
        wd1 = weighted_distance(d, b, g, e, e, c, h, f)
            = df(g, e) + df(e, c) + 3*df(h, f)
        wd2 = weighted_distance(a, e, b, f, d, h, e, i)
            = df(b, f) + df(d, h) + 3*df(e, i)                                */
     float4 wd1 = df_(c.zwxy, e) + df_(e, c) + 3.0 * df_(b.zwxy, b.wxyz);
     float4 wd2 = df_(b, b.wxyz) + df_(b.yzwx, b.zwxy) + 3.0 * df_(e, c.wxyz);

     /* edri usa lv0 + wd1<=wd2 (sirve para SMOOTH_TIPS),
        edr usa lv1 + wd1<wd2 (step ~= ambos comportamientos cuando flotan). */
     float4 wd_cmp   = step(wd1, wd2);
     float4 edri     = wd_cmp * interp_lv0;
     float4 edr      = wd_cmp * interp_lv1;
     float4 edr_left = step(XBR_LV2_COEF*df_(b.wxyz, c.zwxy),
                            df_(b.zwxy, c))
                       * interp_lv2_left * edr;
     float4 edr_up   = step(XBR_LV2_COEF*df_(b.zwxy, c),
                            df_(b.wxyz, c.zwxy))
                       * interp_lv2_up * edr;

     /* Inecuaciones de linea y los smoothstep linealizados.                  */
     float4 fx       = Ao*fp.y + Bo*fp.x;
     float4 fx_left  = Ao*fp.y + Bx*fp.x;
     float4 fx_up    = Ao*fp.y + By*fp.x;

     float4 fx45i = saturate((fx      + delta  - Co - Ci) / (2.0 * delta));
     float4 fx45  = saturate((fx      + delta  - Co)      / (2.0 * delta));
     float4 fx30  = saturate((fx_left + deltaL - Cx)      / (2.0 * deltaL));
     float4 fx60  = saturate((fx_up   + deltaU - Cy)      / (2.0 * deltaU));

     fx45  *= edr;
     fx30  *= edr_left;
     fx60  *= edr_up;
     fx45i *= edri;

     float4 px = step(df_(e, b.wxyz), df_(e, b.zwxy));

     /* SMOOTH_TIPS (CORNER_C default): incluye fx45i. */
     float4 maximos = max(max(fx30, fx60), max(fx45, fx45i));

     /* Blending secuencial con lerp - sin if/else if chains que disparan
        el uso de temps. Cada lerp solo anade el aporte de una orientacion;
        si maximos.X es 0, no hay efecto.                                     */
     float3 res1 = E;
     res1 = lerp(res1, lerp(H, F, px.x), maximos.x);
     res1 = lerp(res1, lerp(B, D, px.z), maximos.z);

     float3 res2 = E;
     res2 = lerp(res2, lerp(F, B, px.y), maximos.y);
     res2 = lerp(res2, lerp(D, H, px.w), maximos.w);

     float3 res  = lerp(res1, res2, step(c_df_(E, res1), c_df_(E, res2)));

     return float4(res, 1.0);
 }

