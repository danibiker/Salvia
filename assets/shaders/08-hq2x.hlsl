/* HQ2x: Real Maxim Stepin algorithm with LUT - single pass.                 *
 * Original by Maxim Stepin, Cameron Zemek, Jules Blok - LGPL 2.1            *
 * Adapted for Xbox 360 D3D9 pipeline.                                       *
 * s0 = game texture, s1 = LUT texture (256x64), c1 = textureDims            */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 sampler2D lutTex : register(s1);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 static const float3 yuv_threshold = float3(48.0/255.0, 7.0/255.0, 6.0/255.0);
 static const float3 yuv_offset = float3(0.0, 0.5, 0.5);

 float3 toYUV(float3 c) {
     float3 yuv;
     yuv.x = dot(c, float3( 0.299, 0.587, 0.114));
     yuv.y = dot(c, float3(-0.169,-0.331, 0.5  ));
     yuv.z = dot(c, float3( 0.5,  -0.419,-0.081));
     return yuv;
 }

 float isDiff(float3 a, float3 b) {
     float3 d = abs((a + yuv_offset) - (b + yuv_offset));
     float3 cmp = step(yuv_threshold + 0.0001, d);
     return saturate(cmp.x + cmp.y + cmp.z);
 }

 float4 main(PS_IN In) : COLOR0
 {
     float2 texel = 1.0 / textureDims;
     float2 uv = In.TexCoord;
     float dx = texel.x;
     float dy = texel.y;

     float2 fp = frac(uv * textureDims);
     float2 quad = sign(-0.5 + fp);

     float3 p1 = tex2D(detail, uv).rgb;
     float3 p2 = tex2D(detail, uv + float2(dx,dy) * quad).rgb;
     float3 p3 = tex2D(detail, uv + float2(dx, 0) * quad).rgb;
     float3 p4 = tex2D(detail, uv + float2( 0,dy) * quad).rgb;

     float3 w1 = toYUV(tex2D(detail, uv+float2(-dx,-dy)).rgb);
     float3 w2 = toYUV(tex2D(detail, uv+float2(  0,-dy)).rgb);
     float3 w3 = toYUV(tex2D(detail, uv+float2( dx,-dy)).rgb);
     float3 w4 = toYUV(tex2D(detail, uv+float2(-dx,  0)).rgb);
     float3 w5 = toYUV(p1);
     float3 w6 = toYUV(tex2D(detail, uv+float2( dx,  0)).rgb);
     float3 w7 = toYUV(tex2D(detail, uv+float2(-dx, dy)).rgb);
     float3 w8 = toYUV(tex2D(detail, uv+float2(  0, dy)).rgb);
     float3 w9 = toYUV(tex2D(detail, uv+float2( dx, dy)).rgb);

     float pattern = isDiff(w5,w1)*1.0   + isDiff(w5,w2)*2.0
                   + isDiff(w5,w3)*4.0   + isDiff(w5,w4)*8.0
                   + isDiff(w5,w6)*16.0  + isDiff(w5,w7)*32.0
                   + isDiff(w5,w8)*64.0  + isDiff(w5,w9)*128.0;

     float cross_val = isDiff(w4,w2)*1.0 + isDiff(w2,w6)*2.0
                     + isDiff(w8,w4)*4.0 + isDiff(w6,w8)*8.0;

     float2 index;
     index.x = pattern;
     index.y = cross_val * 4.0
             + floor(fp.x * 2.0) + floor(fp.y * 2.0) * 2.0;

     float2 lutStep = float2(1.0/256.0, 1.0/64.0);
     float2 lutOff  = lutStep * 0.5;
     float4 weights = tex2D(lutTex, index * lutStep + lutOff);
     float wsum = dot(weights, float4(1,1,1,1));
     float4 nw = weights / max(wsum, 0.001);
     float3 res = p1*nw.x + p2*nw.y + p3*nw.z + p4*nw.w;

     return float4(res, 1.0);
 }

