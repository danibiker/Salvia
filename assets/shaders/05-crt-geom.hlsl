/* CRT-Geom (fake-crt-geom): hunterk / DariusG - port de fake-crt-geom.slangp. *
 * "Simple scanlines with curvature and mask effects lifted from crt-geom".    *
 *                                                                             *
 * SUSTITUYE al port fiel de crt-geom.slang (cgwg/Themaister) que habia aqui.  *
 * Da practicamente el mismo look con ~la mitad del coste: el crt-geom real    *
 * gastaba 8 tex2D + raycasting esfera/plano (5 llamadas a bkwtrans_/fwtrans_  *
 * con acos/asin/sin/cos/sqrt solo para calcular la curvatura) + 6 llamadas a  *
 * scanlineWeights_ (cada una con pow(color,4) y exp(-pow(...))) + 11 pow()    *
 * para las gammas. Aqui la curvatura es una parabola (4 mul), la scanline es  *
 * UN sin(), la mascara otro sin(), y las gammas son cuadrado/raiz.            *
 *                                                                             *
 * El .slangp original son 2 pasadas:                                          *
 *   pass0 = crt-consumer/linearize.slang -> FragColor = res*res  (de-gamma 2) *
 *   pass1 = fake-crt-geom.slang          -> ... sqrt(res) al final (re-gamma) *
 * Como aqui solo tenemos una pasada, el de-gamma se hace dentro de TEX2D_     *
 * (t*t al muestrear) y el re-gamma en el sqrt final: matematicamente el mismo *
 * pipeline lineal, sin render target intermedio.                              *
 *                                                                             *
 * Lo que hace:                                                                *
 *   - Curvatura barrel aproximada (Warp_): pos *= 1 + pos^2*warp. warpx=0.03, *
 *     warpy=0.04 igual que los defaults del .slang.                           *
 *   - Lanczos2 horizontal de 4 taps sobre UNA sola scanline (el crt-geom real *
 *     hacia 4 taps x 2 scanlines = 8 fetches).                                *
 *   - Scanline: sin() sobre la fila de texel, con profundidad interpolada     *
 *     entre scanl (0.5) en oscuros y scanh (0.35) en claros, mas la vineta.   *
 *   - Mascara de fosforo: sin() a cadencia de pixel OUTPUT (tipo fine).       *
 *   - Vineta horizontal, saturacion y bright boost (dark 1.45 / bright 1.05), *
 *     que el port de crt-geom no tenia (por eso este se ve menos apagado).    *
 *   - Corner cutoff suave (a_corner=0.03, bsmooth=250).                       *
 *                                                                             *
 * Diferencias respecto al .slang (forzadas por la infraestructura):           *
 *   - Los valores del VS (ps, maskpos) se calculan en el PS: libSDLx360       *
 *     comparte un unico vertex shader. maskpos usa vpos.x (VPOS de ps_3_0),   *
 *     que es exactamente vTexCoord.x*OutputSize.x del original.               *
 *   - scale = SourceSize/OriginalSize = 1.0 (una sola pasada, sin reescalado  *
 *     previo), asi que se elimina de las cuentas.                             *
 *   - Sin interlacing: no tenemos FrameCount como uniforme (solo c1 =         *
 *     textureDims). Se conserva el /2 vertical para fuentes >400 lineas, que  *
 *     es lo que evita el moire en 480i/480p.                                  *
 *   - saturate() antes del sqrt final: con scanl=0.5 y vineta el factor de    *
 *     scanline puede bajar de 0 y el sqrt daria NaN. En el rango visible el   *
 *     resultado es identico (sqrt(saturate(x)) == saturate(sqrt(x)) si x>=0). *
 *                                                                             *
 * Coste: 4 tex2D + ~45 ALU + 4 sin + 1 sqrt. Aprox. la mitad de fetches y     *
 * 3-4x menos ALU/transcendentales que el crt-geom fiel anterior.              */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 /* === Parametros (hardcoded a los defaults del fake-crt-geom.slang) === */

 /* Color. */
 #define a_col_temp   0.0   /* temperatura de color. >0 tira a calido
                               (rojo), <0 a frio (azul). Rango -0.15..0.15,
                               ~200K por cada 0.01.                       */
 #define a_sat        1.0   /* saturacion. 0 = blanco y negro, 1 = tal
                               cual, 2 = colores exagerados.              */
 #define a_boostd     1.45  /* bright boost en zonas OSCURAS. Compensa
                               lo que se come la scanline. Rango 1.0-2.0. */
 #define a_boostb     1.05  /* bright boost en zonas CLARAS. Bajo a
                               proposito para no quemar los blancos.      */

 /* Scanlines y mascara. */
 #define scanl        0.5   /* profundidad de la scanline en zonas
                               oscuras. 0 = sin scanline, 0.5 = maxima.   */
 #define scanh        0.35  /* profundidad en zonas claras (menor, para
                               que los brillos engorden como en un CRT).  */
 #define a_MTYPE      0.0   /* tipo de mascara: 0 = fina (cadencia de
                               pixel de pantalla), 1 = gruesa, 2 = LCD
                               (cadencia de pixel FUENTE).                */
 #define a_MSIZE      1.0   /* tamano de la mascara: 1 o 2 pixeles.       */
 #define a_MASK       0.2   /* intensidad de la mascara. 0 = sin mascara,
                               0.5 = maxima (oscurece bastante).          */

 /* Geometria. */
 #define warpx        0.03  /* curvatura horizontal. 0 = plano. 0.2 = ojo
                               de pez. Rango util 0.0-0.06.              */
 #define warpy        0.04  /* curvatura vertical, mismo rango.          */
 #define a_corner     0.03  /* radio del redondeo de esquina en coords
                               [0,1]. 0 = esquinas cuadradas.            */
 #define bsmooth      250.0 /* dureza del corte del borde. Alto = corte
                               nitido, bajo = degradado. Rango 100-1000. */
 #define a_vignette   1.0   /* 1 = vineta ON (oscurece los laterales).    */
 #define a_vigstr     0.5   /* fuerza de la vineta. Rango 0.0-1.0.        */

 #define PI_          3.141592

 /* Muestreo en espacio lineal: equivale a la pasada linearize.slang
    (FragColor = res*res) del .slangp original. */
 float4 TEX2D_(float2 c) { float4 t = tex2D(detail, c); return t*t; }

 /* FIX macro: evita la division por cero en el kernel de Lanczos. */
 float4 FIX4_(float4 c) { return max(abs(c), float4(1e-5,1e-5,1e-5,1e-5)); }

 /* Curvatura barrel aproximada: en vez del raycasting esfera/plano del
    crt-geom real, desplaza cada eje proporcionalmente al cuadrado del
    otro. Visualmente casi indistinguible a estas intensidades. */
 float2 Warp_(float2 pos) {
     pos = pos*2.0 - 1.0;
     pos *= float2(1.0 + pos.y*pos.y*warpx, 1.0 + pos.x*pos.x*warpy);
     return pos*0.5 + 0.5;
 }

 /* Redondeo de esquinas del bisel. */
 float corner_(float2 coord) {
     coord = min(coord, 1.0 - coord);
     float2 cdist = float2(a_corner, a_corner);
     coord = cdist - min(coord, cdist);
     float dist = sqrt(dot(coord, coord));
     return saturate((cdist.x - dist) * bsmooth);
 }

 float4 main(PS_IN In, float2 vpos : VPOS) : COLOR0
 {
     float2 ps  = 1.0 / textureDims;

     /* El VS del original hace vTexCoord = TexCoord*1.0001 (evita que el
        borde derecho/inferior muestree fuera por redondeo). */
     float2 cpos = Warp_(In.TexCoord * 1.0001);
     float2 ogl2pos = cpos * textureDims;

     float2 ratio_scale = ogl2pos - 0.5;
     float2 uv_ratio    = frac(ratio_scale);
     float2 xy = (floor(ratio_scale) + 0.5) * ps;

     /* Lanczos2 horizontal: 4 taps sobre la scanline actual. */
     float4 coeffs = PI_ * float4(1.0 + uv_ratio.x, uv_ratio.x,
                                  1.0 - uv_ratio.x, 2.0 - uv_ratio.x);
     coeffs = FIX4_(coeffs);
     coeffs = 2.0 * sin(coeffs) * sin(coeffs*0.5) / (coeffs*coeffs);
     coeffs /= dot(coeffs, float4(1.0,1.0,1.0,1.0));

     float4 res = saturate(mul(coeffs, float4x4(
         TEX2D_(xy + float2(-ps.x, 0.0)),
         TEX2D_(xy),
         TEX2D_(xy + float2( ps.x, 0.0)),
         TEX2D_(xy + float2(2.0*ps.x, 0.0)))));

     /* Luma aproximada: decide la profundidad del beam (los brillos
        engordan y tapan mas la scanline, como en un CRT real). */
     float w = dot(float3(0.33,0.33,0.33), res.rgb);

     /* Temperatura de color (aproximacion barata). */
     res.rgb *= float3(1.0 + a_col_temp, 1.0 - a_col_temp*0.2,
                       1.0 - a_col_temp);

     float scan = lerp(scanl, scanh, w);

     /* Mascara de fosforo. maskpos del VS original =
        vTexCoord.x*OutputSize.x/a_MSIZE*PI, que es exactamente
        vpos.x*PI/a_MSIZE (VPOS = coords de pixel de pantalla). */
     float sz  = (a_MTYPE == 1.0) ? 0.6666 : 1.0;
     float m_m = (a_MTYPE == 2.0) ? (ogl2pos.x * 2.0 * PI_)
                                  : (vpos.x * PI_ / a_MSIZE);
     res *= a_MASK*sin(m_m*sz) + 1.0 - a_MASK;

     /* Vineta horizontal (se suma a la profundidad de scanline). */
     float vig = 0.0;
     if (a_vignette == 1.0) {
         vig = cpos.x - 0.5;
         vig = vig*vig*a_vigstr;
     }

     /* Fuentes de mas de 400 lineas (480i/480p): media cadencia de
        scanline, si no el patron degenera en moire. El original alterna
        ademas el campo con FrameCount; aqui no hay contador de frame. */
     ogl2pos.y *= (textureDims.y > 400.0) ? 0.5 : 1.0;

     res *= (scan+vig)*sin((ogl2pos.y + 0.25)*2.0*PI_) + (1.0-scan-vig);

     /* Saturacion y bright boost. */
     float l = dot(res.rgb, float3(0.3,0.6,0.1));
     res.rgb = lerp(float3(l,l,l), res.rgb, a_sat);
     res.rgb *= lerp(a_boostd, a_boostb, l);

     /* sqrt = vuelta a gamma de monitor (re-gamma de la pasada final
        del .slangp). saturate previo: ver nota de cabecera. */
     return float4(sqrt(saturate(res.rgb)) * corner_(cpos), 1.0);
 }

