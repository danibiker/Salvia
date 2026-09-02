/* Sharp-Bilinear-Simple: port directo del sharp-bilinear-simple.slang.        *
 * Author: rsn8887 / TheMaister (public domain).                                *
 *                                                                              *
 * Que hace: hace nearest-neighbour DENTRO de cada pixel-fuente, y solo deja   *
 * que el filtro hardware bilinear interpole en una rampa estrecha en las     *
 * transiciones entre pixeles. Resultado: imagen tan nitida como nearest pero *
 * sin "pixel wobble" (variacion de tamano entre pixeles) en escalas no       *
 * enteras. Es el upscaler recomendado oficialmente por libretro para         *
 * handhelds y para cualquier emulador con escala fraccional.                 *
 *                                                                              *
 * Cost: 1 tex2D + ~10 ALU + 2 ddx/ddy. Esencialmente gratis.                  *
 *                                                                              *
 * Diferencias respecto al .slang:                                             *
 *   - precalc_scale (calculado en VS en el original) lo calculamos en PS    *
 *     con ddx/ddy de las coordenadas de texel. ddx(uv.x) = 1/OutputSize.x   *
 *     asi que ddx(texel.x) = SourceSize.x/OutputSize.x = 1/scale.x. Es la   *
 *     misma operacion en cada pixel de un mismo 2x2 quad, asi que el coste *
 *     de las derivadas es despreciable.                                     *
 *   - El sampler debe estar en LINEAR (lo configura XBOX_SelectEffect).    */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 float4 main(PS_IN In) : COLOR0
 {
     float2 texel = In.TexCoord * textureDims;

     /* scale = OutputSize / SourceSize, derivado por ddx/ddy. */
     float2 dxy = float2(abs(ddx(texel.x)), abs(ddy(texel.y)));
     float2 scale = max(floor(1.0 / max(dxy, float2(1e-6,1e-6))),
                        float2(1.0, 1.0));

     float2 texel_floored = floor(texel);
     float2 s = frac(texel);
     float2 region_range = 0.5 - 0.5 / scale;

     /* Punto de muestreo: dentro de la region central usa nearest;
        en la rampa de transicion la hardware bilinear hace su parte. */
     float2 center_dist = s - 0.5;
     float2 f = (center_dist - clamp(center_dist, -region_range, region_range))
                  * scale + 0.5;

     float2 mod_texel = texel_floored + f;
     return tex2D(detail, mod_texel / textureDims);
 }

