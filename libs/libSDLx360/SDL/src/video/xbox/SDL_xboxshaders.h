#ifndef _SDL_xboxshaders_h
#define _SDL_xboxshaders_h

#include "SDL.h"
#include <xtl.h>
#include <d3dx9.h>
#include <XGraphics.h>

void initShaders();
void destroyShaders();
void XBOX_SelectEffect(int effectID);

const static char* g_strShaderNormalSource =
    " float fFilterType : register(c0);            " // El parámetro vive aquí
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
    "     return color;                            " // Tipo 0: Original
    " }     ";

const static char* g_strShaderGraySource =
    " float fFilterType : register(c0);            " // El parámetro vive aquí
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
    "         float gray = dot(color.rgb, float3(0.299, 0.587, 0.114)); "
    "         return float4(gray, gray, gray, color.a); "
    " }     ";

const static char* g_strShaderSepiaSource =
    " float fFilterType : register(c0);            " // El parámetro vive aquí
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
    "         float r = (color.r * 0.393) + (color.g * 0.769) + (color.b * 0.189); "
    "         float g = (color.r * 0.349) + (color.g * 0.686) + (color.b * 0.168); "
    "         float b = (color.r * 0.272) + (color.g * 0.534) + (color.b * 0.131); "
    "         return float4(r, g, b, color.a);      "
    " }     ";

const static char* g_strShaderFullscreen = 
"float4 vParams : register(c0);											\n"
"float4 vRes    : register(c1);                                         \n"
"                                                                       \n"
"struct VS_IN {                                                         \n"
"    float4 Pos : POSITION;                                             \n"
"    float2 UV  : TEXCOORD0;                                            \n"
"};                                                                     \n"
"                                                                       \n"
"struct VS_OUT {                                                        \n"
"    float4 Pos : POSITION0;                                            \n"
"    float2 UV  : TEXCOORD0;                                            \n"
"};                                                                     \n"
"                                                                       \n"
"VS_OUT main(VS_IN In) {                                                \n"
"    VS_OUT Out;                                                        \n"
"    float2 screenRes = vRes.xy;                                        \n"
"    float2 imgRes    = vRes.zw;                                        \n"
"    float ratio      = vParams.x;                                      \n"
"    float forceFS    = vParams.y;                                      \n"
"                                                                       \n"
"    float2 scale;                                                      \n"
"    if (forceFS > 0.5) {                                               \n"
"        float screenRatio = screenRes.x / screenRes.y;                 \n"
"        if (ratio > screenRatio)                                       \n"
"            scale = float2(1.0, screenRatio / ratio);                  \n"
"        else                                                           \n"
"            scale = float2(ratio / screenRatio, 1.0);                  \n"
"    } else {                                                           \n"
"        scale = (imgRes / screenRes) * float2(ratio, 1.0);             \n"
"    }                                                                  \n"
"                                                                       \n"
"    Out.Pos.x = In.Pos.x * scale.x;                                    \n"
"    Out.Pos.y = In.Pos.y * scale.y;                                    \n"
"    Out.Pos.z = 0.0f;                                                  \n"
"    Out.Pos.w = 1.0f;                                                  \n"
"                                                                       \n"
"    Out.UV = In.UV;                                                    \n"
"    return Out;                                                        \n"
"}																		\n";	

#endif