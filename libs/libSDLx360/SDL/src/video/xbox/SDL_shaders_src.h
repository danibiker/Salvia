#ifndef _SDL_shaders_src_h
#define _SDL_shaders_src_h

/* Passthrough: el UNICO shader que sigue viviendo embebido en el binario.
 *
 * Todos los demas (Sharp-Bilinear, LCD3x, Scanlines, CRT-Geom, CRT-Lottes,
 * CRT-Easymode, HQ2x/3x/4x, xBR...) se han sacado a ficheros de texto en
 * <appDir>\assets\shaders, en formato de preset estilo RetroArch (.hlslp +
 * .hlsl), y se cargan y compilan en runtime. Ver:
 *   - src/video/salvia_shader_api.h  (frontera app <-> backend de video)
 *   - src/video/shaderpreset.cpp     (descubrimiento, parser, LUTs)
 *   - utils/extract_shaders.py       (script que genero los assets iniciales)
 *
 * Este se queda aqui a proposito, como RED DE SEGURIDAD: en Xenon no hay
 * pipeline de funcion fija, asi que enganchar un pixel shader NULL da PANTALLA
 * NEGRA. initShaders() lo compila lo primero y lo usa siempre que un preset no
 * exista, no compile, o solo describa estado de sampler (Nearest y Bilinear,
 * que se diferencian unicamente en filter_linear0). */
const static char* g_strShaderNormalSource =
    " float fFilterType : register(c0);            "
    "                                              "
    " struct PS_IN                                 "
    " {                                            "
    "     float2 TexCoord : TEXCOORD0;             "
    " };                                           "
    "                                              "
    " sampler2D detail : register(s0);             "
    "                                              "
    " float4 main( PS_IN In ) : COLOR0             "
    " {                                            "
    "     float4 color = tex2D( detail, In.TexCoord ); "
    "     return color;                            "
    " }     ";

#endif
