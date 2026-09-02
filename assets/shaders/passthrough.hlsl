/* Passthrough: devuelve el texel tal cual.
 * Es el shader que el backend lleva EMBEBIDO como fallback de
 * emergencia; se publica aqui como plantilla de partida. */
 float fFilterType : register(c0);
 struct PS_IN{
	float2 TexCoord : TEXCOORD0;
 };                    
 sampler2D detail : register(s0);
 float4 main( PS_IN In ) : COLOR0 {
	float4 color = tex2D( detail, In.TexCoord );
	return color;
 }
