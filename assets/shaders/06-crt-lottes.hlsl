/* === CRT-Lottes: port directo de crt-lottes.slang. ============================ *
 * Author: Timothy Lottes (public domain, autor del paper original sobre         *
 * shading CRT en GPUs y posterior empleado de NVIDIA/AMD).                       *
 *                                                                                *
 * Look distintivo: scanlines mas suaves que CRT-Geom, shadow mask en vez de    *
 * aperture grille (default mask type 3 = stretched VGA), warp barrel ligero.   *
 * El bloom multi-pasa del slang esta deshabilitado (#define DO_BLOOM 0):       *
 * meter Bloom anade 31 tex2D extra y se sale del presupuesto de PS3.0.         *
 *                                                                                *
 * Coste sin bloom: 11 tex2D (Horz3 + Horz5 + Horz3 = 3+5+3) + ~50 ALU.         *
 *                                                                                *
 * Diferencias forzadas vs .slang:                                                *
 *   - SIMPLE_LINEAR_GAMMA path (pow 2.2): la version sRGB completa con if/else *
 *     mete branches dinamicos que en PS3.0 son caros; el resultado visual es  *
 *     practicamente identico.                                                   *
 *   - DO_BLOOM deshabilitado.                                                   *
 *   - Mask uses vpos (VPOS semantic) en vez de vTexCoord/OutputSize.zw.       */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 /* Parametros (defaults del .slang). */
 #define hardScan     -8.0    /* nitidez de la scanline en Y (mas neg=
                                  scanlines mas marcadas). Rango -20..0. */
 #define hardPix      -10.0    /* nitidez del pixel en X. Rango -20..0.   */
 #define warpX         0.031  /* curvatura barrel horizontal. 0=plano.   */
 #define warpY         0.041  /* curvatura barrel vertical. 0=plano.     */
 #define maskDark      0.5    /* atenuacion subpixel apagado en la mask. */
 #define maskLight     1.5    /* boost subpixel encendido. >1 satura.    */
 #define brightBoost   1.1    /* multiplicador de brillo global pre-
                                  linearizacion. Rango 0.5..2.            */
 #define shape         2.0    /* exponente del kernel Gaussiano.         */

 /* Esquinas redondeadas del bisel (portado del antiguo crt-geom fiel).
    El borde del tubo CRT no era un rectangulo perfecto, sino que tenia
    las esquinas redondeadas.  Reemplaza la mask rectangular inScreen.   */
 #define cornersize       0.03   /* radio del fillet de esquina en
                                    coords [0,1].  0.0 = sin redondeo.
                                    Rango tipico 0.01-0.05.              */
 #define cornersmooth     1000.0 /* nitidez del corte. Alto = corte duro
                                    (look mask).  Bajo = transicion
                                    suave estilo vineta. 80-2000.        */
 static const float2 cornerAspect = float2(1.0, 0.75);

 float3 ToLinear(float3 c) {
     return pow(saturate(c) * brightBoost, float3(2.2, 2.2, 2.2));
 }
 float3 ToSrgb(float3 c) {
     return pow(saturate(c), float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
 }

 /* Fetch en posicion nearest emulada con offset entero. */
 float3 Fetch(float2 pos, float2 off) {
     pos = (floor(pos * textureDims + off) + 0.5) / textureDims;
     return ToLinear(tex2D(detail, pos).rgb);
 }

 float2 Dist(float2 pos) {
     pos = pos * textureDims;
     return -((pos - floor(pos)) - 0.5);
 }

 float Gaus(float pos, float scale) {
     return exp2(scale * pow(abs(pos), shape));
 }

 /* Filtro 3-tap Gaussiano horizontal. */
 float3 Horz3(float2 pos, float off) {
     float3 b = Fetch(pos, float2(-1.0, off));
     float3 c = Fetch(pos, float2( 0.0, off));
     float3 d = Fetch(pos, float2( 1.0, off));
     float dst = Dist(pos).x;
     float wb = Gaus(dst - 1.0, hardPix);
     float wc = Gaus(dst + 0.0, hardPix);
     float wd = Gaus(dst + 1.0, hardPix);
     return (b*wb + c*wc + d*wd) / (wb + wc + wd);
 }

 /* Filtro 5-tap Gaussiano horizontal (linea central). */
 float3 Horz5(float2 pos, float off) {
     float3 a = Fetch(pos, float2(-2.0, off));
     float3 b = Fetch(pos, float2(-1.0, off));
     float3 c = Fetch(pos, float2( 0.0, off));
     float3 d = Fetch(pos, float2( 1.0, off));
     float3 e = Fetch(pos, float2( 2.0, off));
     float dst = Dist(pos).x;
     float wa = Gaus(dst - 2.0, hardPix);
     float wb = Gaus(dst - 1.0, hardPix);
     float wc = Gaus(dst + 0.0, hardPix);
     float wd = Gaus(dst + 1.0, hardPix);
     float we = Gaus(dst + 2.0, hardPix);
     return (a*wa + b*wb + c*wc + d*wd + e*we)
             / (wa + wb + wc + wd + we);
 }

 /* Peso de la scanline vertical. */
 float Scan(float2 pos, float off) {
     float dst = Dist(pos).y;
     return Gaus(dst + off, hardScan);
 }

 /* Combina las 3 scanlines cercanas. */
 float3 Tri(float2 pos) {
     float3 a = Horz3(pos, -1.0);
     float3 b = Horz5(pos,  0.0);
     float3 c = Horz3(pos,  1.0);
     float wa = Scan(pos, -1.0);
     float wb = Scan(pos,  0.0);
     float wc = Scan(pos,  1.0);
     return a*wa + b*wb + c*wc;
 }

 /* Warp barrel y descarte off-screen. */
 float2 Warp(float2 pos) {
     pos = pos * 2.0 - 1.0;
     pos *= float2(1.0 + (pos.y * pos.y) * warpX,
                   1.0 + (pos.x * pos.x) * warpY);
     return pos * 0.5 + 0.5;
 }

 /* Mascara de esquina redondeada (portado de crt-geom).
    Devuelve 1.0 en el centro del tubo, 0.0 fuera del fillet, con
    transicion suave en el borde del bisel.  Llamado con las UVs ya
    transformadas por Warp. */
 float corner_lottes(float2 coord) {
     coord = min(coord, 1.0 - coord) * cornerAspect;
     float2 cdist = float2(cornersize, cornersize);
     coord = cdist - min(coord, cdist);
     float dist = sqrt(dot(coord, coord));
     return saturate((cornersize - dist) * cornersmooth);
 }

 /* Shadow mask 'stretched VGA' (mask_type 3, default del slang). */
 float3 Mask(float2 pos) {
     float3 mask = float3(maskDark, maskDark, maskDark);
     pos.x += pos.y * 3.0;
     pos.x = frac(pos.x * 0.166666666);
     if      (pos.x < 0.333) mask.r = maskLight;
     else if (pos.x < 0.666) mask.g = maskLight;
     else                    mask.b = maskLight;
     return mask;
 }

 float4 main(PS_IN In, float2 vpos : VPOS) : COLOR0
 {
     float2 pos = Warp(In.TexCoord);
     float3 outColor = Tri(pos);
     outColor *= Mask(vpos);

     /* Esquinas redondeadas + descarte off-screen.  La funcion devuelve
        0.0 fuera del bisel, 1.0 en el centro, con transicion suave en
        el fillet.  Reemplaza el step() rectangular original.            */
     outColor *= corner_lottes(pos);

     return float4(ToSrgb(outColor), 1.0);
 }

