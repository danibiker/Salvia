/* Scanlines: Simulates CRT scanline effect.                                  *
 * Darkens alternating lines aligned to source pixel rows using sin().        *
 * s0 = game texture, c1 = textureDims                                       *
 * Cost: 1 tex2D + 4 ALU - extremely lightweight.                            */
 float2 textureDims : register(c1);
 sampler2D detail : register(s0);
 struct PS_IN { float2 TexCoord : TEXCOORD0; };

 static const float PI = 3.14159265;

 float4 main(PS_IN In) : COLOR0
 {
     float4 color = tex2D(detail, In.TexCoord);
     float scanline = sin(In.TexCoord.y * textureDims.y * PI);
     float mask = 1.0 - 0.35 * (1.0 - scanline * scanline);
     return float4(color.rgb * mask, color.a);
 }

