/* === CRT-Easymode (Salvia variant): clon estructural de CRT-Lottes. =========== *
 * El port literal del crt-easymode.slang nunca llego a verse correctamente en   *
 * la Xbox 360 (mascara con bandas anchas de color sin importar el approach:    *
 * VPOS, OutputSize uniform, ddx-derivadas). Como CRT-Lottes SI funciona en el  *
 * hardware, esta variante se construye reciclando 1:1 la estructura de Lottes  *
 * (Fetch/Dist/Gaus/Horz3/Horz5/Scan/Tri/Mask) pero ajustada para emular el     *
 * look de Easymode:                                                              *
 *   - SIN warp: CRT plano (Easymode no curva la pantalla por defecto).         *
 *   - Mask APERTURE GRILLE (Lottes shadowMask=2): RGB en columnas, sin stagger.*
 *   - maskDark/maskLight a 0.7/1.0 (Easymode usa MASK_STRENGTH=0.3 y NO        *
 *     amplifica subpixeles por encima de 1.0, a diferencia de Lottes con 0.5/ *
 *     1.5). Eso reduce el contraste de la rejilla y da un look mas 'plano'.   *
 *   - hardScan -10.0 / hardPix -3.0: scanlines mas marcadas que Lottes default *
 *     (-8.0) para parecerse al look fino de Easymode.                          *
 *   - brightBoost 1.1: ligero refuerzo (Easymode usa BRIGHT_BOOST=1.2 pero     *
 *     aqui aplicamos antes de la mask asi que 1.1 da brillo similar).         *
 *                                                                                *
 * Coste: 11 tex2D (igual que Lottes sin bloom). Visualmente: CRT plano con    *
 * aperture grille fina + scanlines limpias.                                    */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 /* Parametros (ajustados respecto a Lottes para el look Easymode). */
 #define hardScan     -10.0   /* scanlines mas finas que Lottes (-8). */
 #define hardPix       -10.0   /* sharpness horizontal.                  */
 #define maskDark      0.7    /* MASK_STRENGTH 0.3 -> 1-0.3 = 0.7.     */
 #define maskLight     1.2    /* sin amplificacion, fiel a Easymode.    */
 #define brightBoost   1.1    /* refuerzo ligero antes de la mask.     */
 #define shape         2.0    /* exponente del kernel Gaussiano.         */

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

 /* Mask: STRETCHED VGA SHADOW MASK (Lottes shadowMask=3).
    Es la misma que CRT-Lottes usa por defecto. La clave es la linea
    `pos.x += pos.y * 3.0` que mete stagger vertical: las columnas R/G/B
    se desplazan cada fila Y, asi que cruzadas con las scanlines forman
    celdas casi cuadradas (en vez de rectangulos altos como daria una
    aperture grille pura). El periodo es 6 (de ahi el 1/6 = 0.16666). */
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
     /* Sin Warp: Easymode es CRT plano. */
     float3 outColor = Tri(In.TexCoord);
     outColor *= Mask(vpos);
     return float4(ToSrgb(outColor), 1.0);
 }

