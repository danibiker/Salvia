#pragma once

/* HLSLBackground: fondo animado fullscreen compilado como pixel shader HLSL.
 *
 * Compila un ps_3_0 que genera un efecto procedural (adaptado de GLSL) y lo
 * dibuja como un quad fullscreen sin necesidad de textura.
 *
 * API: init(device) -> draw() -> shutdown()
 * - init(): compila los shaders y crea el VB (llamar una vez con el device).
 * - draw(): dibuja el quad fullscreen (llamar cada frame que se necesite).
 * - shutdown(): libera recursos GPU.
 *
 * En Windows (C++), la implementacion esta en win_d3d9.cpp y se accede
 * a traves de Engine::hlslBkg.
 * En Xbox 360 (C), la implementacion esta embebida en SDL_xboxvideo.c. */

/* --- Shader source strings (shared between Win and Xbox) --- */

static const char* g_strHLSLBackground =
	" float gTime : register(c0);                        \n"
	" float2 gResolution : register(c1);                 \n"
	"                                                    \n"
	" struct PS_IN { float4 Position : POSITION;          \n"
	"                float2 TexCoord : TEXCOORD0; };      \n"
	"                                                    \n"
	" float4 main(PS_IN In) : COLOR0                     \n"
	" {                                                  \n"
	"     float2 r = gResolution;                        \n"
	"     float  t = gTime;                              \n"
	"     float4 o = 0;                                  \n"
	"                                                    \n"
	"     float2 p = (In.TexCoord * 2.0 - 1.0);         \n"
	"     p.x *= r.x / r.y;                             \n"
	"                                                    \n"
	"     float l = 0.0;                                 \n"
	"     l += abs(0.7 - dot(p, p));                     \n"
	"     float2 v = p * (1.0 - l) / 0.2;               \n"
	"                                                    \n"
	"     for (float i = 1.0; i <= 8.0; i += 1.0)        \n"
	"     {                                              \n"
	"         o += (sin(float4(v.x, v.y, v.y, v.x))     \n"
	"               + 1.0)                               \n"
	"               * abs(v.x - v.y) * 0.2;             \n"
	"         v += cos(float2(v.y, v.x) * i              \n"
	"               + float2(0, i) + t)                  \n"
	"               / i + 0.7;                           \n"
	"     }                                              \n"
	"                                                    \n"
	"     o = tanh(exp(p.y * float4(1,-1,-2,0))         \n"
	"              * exp(-4.0 * l) / o);                 \n"
	"     return o;                                      \n"
	" }                                                                   \n";

//static const char* g_strHLSLBackground2 =
//	" float gTime : register(c0);                                         \n"
//	" float2 gResolution : register(c1);                                  \n"
//	"                                                                     \n"
//	" struct PS_IN { float4 Position : POSITION;                           \n"
//	"                float2 TexCoord : TEXCOORD0; };                       \n"
//	"                                                                     \n"
//	" float4 main(PS_IN In) : COLOR0                                      \n"
//	" {                                                                   \n"
//	"     float2 r = gResolution;                                         \n"
//	"     float  t = gTime;                                               \n"
//	"     float4 o = 0;                                                   \n"
//	"                                                                     \n"
//	"     float3 p;                                                       \n"
//	"     float z = 0.0;                                                  \n"
//	"     float d;                                                        \n"
//	"                                                                     \n"
//	"     for (float i = 0.0; i < 100.0; i += 1.0)                        \n"
//	"     {                                                               \n"
//	"         p = z * normalize(float3((In.TexCoord * 2.0 - 1.0) * r,    \n"
//	"                                  -r.y));                            \n"
//	"         p.z -= t;                                                   \n"
//	"         float2x2 m = float2x2(                                      \n"
//	"             cos(z * 0.2 + t * 0.1),                                 \n"
//	"             cos(z * 0.2 + t * 0.1 + 11.0),                         \n"
//	"             cos(z * 0.2 + t * 0.1 + 33.0),                         \n"
//	"             cos(z * 0.2 + t * 0.1));                                \n"
//	"         p.xy = mul(m, p.xy);                                        \n"
//	"         d = length(cos(p + cos(p.yzx + p.z                         \n"
//	"                         - t * 0.2)).xy) / 6.0;                     \n"
//	"         z += d;                                                     \n"
//	"         o += (sin(p.x + t + float4(0, 2, 3, 0))                    \n"
//	"               + 1.0) / d;                                           \n"
//	"     }                                                               \n"
//	"                                                                     \n"
//	"     o = tanh(o / 5000.0);                                           \n"
//	"     return o;                                                       \n"
//	" }                                                                   \n";

static const char* g_strHLSLBackground2 =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"                                                                     \n"
	"     // 1) Reconstruir fragCoord de pixeles                             \n"
	"     float2 fragCoord = In.TexCoord * r;                             \n"
	"                                                                     \n"
	"     // 2) Coordenadas normalizadas y centradas (Corregido eje Y)    \n"
	"     float2 p = (fragCoord * 2.0 - r.x) / r.y;                       \n"
	"     p.y = -p.y;                                                     \n"
	"                                                                     \n"
	"     // 3) Distancia al borde del circulo                            \n"
	"     float l = 1.0 - length(p);                                      \n"
	"                                                                     \n"
	"     // 4) Onda de color RGB (Frecuencia identica a Shadertoy)      \n"
	"     float4 colorShift = float4(0.0, 2.0, 4.0, 0.0);                 \n"
	"     float4 col = 1.2 + sin(p.x + t + colorShift);                   \n"
	"                                                                     \n"
	"     // 5) Calculo de intensidad inversa                             \n"
	"     // Se anade un epsilon (0.0001) para blindar el cero matematico \n"
	"     float4 intensity = (col * 0.1) / max(l / 0.1, -l + 0.0001);     \n"
	"                                                                     \n"
	"     // 6) Tonemapping Tanh ultraestable para Xbox 360               \n"
	"     // Formula algebraica: x / (1.0 + abs(x))                       \n"
	"     // Evita por completo desbordamientos por exponenciales.        \n"
	"     float4 o = intensity / (1.0 + abs(intensity));                  \n"
	"                                                                     \n"
	"     o.w = 1.0;                                                      \n"
	"     return o;                                                       \n"
	" }                                                                   \n";


static const char* g_strHLSLBackground3 =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float snoise2D(float2 p)                                            \n"
	" {                                                                   \n"
	"     return frac(sin(dot(p, float2(127.1, 311.7)))                   \n"
	"                * 43758.5453) * 2.0 - 1.0;                          \n"
	" }                                                                   \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"     float4 o = 0;                                                   \n"
	"                                                                     \n"
	"     float2 p = mul((float2(In.TexCoord.x, 1.0 - In.TexCoord.y)    \n"
	"                    * r - r * 0.5) / r.y,                          \n"
	"                    float2x2(8, 6, -6, 8));                          \n"
	"     float2 v;                                                       \n"
	"     float f = 3.0 + snoise2D(p + float2(t * 7.0, 0.0));            \n"
	"                                                                     \n"
	"     for (float i = 0.0; i < 50.0; i += 1.0)                        \n"
	"     {                                                               \n"
	"         v = p + cos(i * i + (t + p.x * 0.1) * 0.03                 \n"
	"                   + i * float2(11, 9)) * 5.0;                       \n"
	"         o += (cos(sin(i) * float4(1, 2, 3, 1))                     \n"
	"               + 1.0)                                                \n"
	"               * exp(sin(i * i + t))                                 \n"
	"               / length(max(v,                                       \n"
	"                   float2(v.x * f * 0.02, v.y)));                    \n"
	"     }                                                               \n"
	"                                                                     \n"
	"     o = tanh(pow(max(o / 100.0, 0.0), float4(1.5, 1.5, 1.5, 1.5)));\n"
	"     return o;                                                       \n"
	" }                                                                   \n";

static const char* g_strHLSLBackgroundVortex =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"                                                                     \n"
	"     // 1) Reconstruccion exacta de fragCoord de Shadertoy          \n"
	"     float2 fragCoord = In.TexCoord * r;                             \n"
	"                                                                     \n"
	"     // 2) Vector de direccion del rayo original (Eje Y corregido)  \n"
	"     float3 rayDir = normalize(float3(fragCoord * 2.0 - r, -r.y));   \n"
	"                                                                     \n"
	"     // 3) Semilla de ruido (Dither) original para volumen           \n"
	"     float z = frac(dot(fragCoord, sin(fragCoord)));                 \n"
	"                                                                     \n"
	"     float4 O = float4(0.0, 0.0, 0.0, 0.0);                          \n"
	"     float d = 1.0;                                                  \n"
	"                                                                     \n"
	"     // 4) Bucle principal de Raymarching (Fiel a las 100 iteraciones)\n"
	"     for(int i = 0; i < 100; i++)                                    \n"
	"     {                                                               \n"
	"         float3 p = z * rayDir;                                      \n"
	"         p.z += 6.0; // Desplazamiento de camara original            \n"
	"                                                                     \n"
	"         // 5) Bucle de turbulencia original (Convertido de d/=0.8 a d*=1.25)\n"
	"         for(d = 1.0; d < 9.0; d *= 1.25)                            \n"
	"         {                                                           \n"
	"             p += cos(p.yzx * d - t) / d;                            \n"
	"         }                                                           \n"
	"                                                                     \n"
	"         // 6) Campo de distancia de esfera hueca exacto             \n"
	"         d = 0.002 + abs(length(p) - 0.5) / 40.0;                    \n"
	"         z += d;                                                     \n"
	"                                                                     \n"
	"         // 7) Acumulacion de color y resplandor original            \n"
	"         O += (sin(z + float4(6.0, 2.0, 4.0, 0.0)) + 1.5) / d;       \n"
	"     }                                                               \n"
	"                                                                     \n"
	"     // 8) Tonemapping Tanh algebraico ultraestable                  \n"
	"     float4 intensity = O / 7000.0;                                  \n"
	"     float4 finalColor = intensity / (1.0 + abs(intensity));         \n"
	"                                                                     \n"
	"     finalColor.w = 1.0;                                             \n"
	"     return finalColor;                                              \n"
	" }                                                                   \n";


static const char* g_strHLSLBackgroundWormholes =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"                                                                     \n"
	"     // 1) Reconstruir coordenadas de pantalla tradicionales        \n"
	"     float2 fragCoord = In.TexCoord * r;                             \n"
	"                                                                     \n"
	"     float4 O = float4(0.0, 0.0, 0.0, 0.0);                          \n"
	"                                                                     \n"
	"     // 2) Bucle de 40 iteraciones (Reducido de 50 para optimizar)   \n"
	"     [unroll]                                                        \n"
	"     for(float i = 1.0; i <= 20.0; i += 1.0)                         \n"
	"     {                                                               \n"
	"         // 3) Calculo de la distancia del anillo (Profundidad)       \n"
	"         // mod(a, b) en HLSL se calcula como: a - b * floor(a / b)   \n"
	"         float dArg = (i - t);                                       \n"
	"         float d = (dArg - 20.0 * floor(dArg / 20.0)) + 0.01;        \n"
	"                                                                     \n"
	"         // 4) Desplazamiento ondulatorio del centro del gusano       \n"
	"         // Optimizamos calculando cos de un angulo escalar          \n"
	"         float2 centerShift = cos(float2(i, i * 1.3)) * (r.y / d);   \n"
	"         float2 p = fragCoord - (r * 0.5) + centerShift + (d / 0.4); \n"
	"                                                                     \n"
	"         // 5) Formula del grosor y atenuacion del anillo             \n"
	"         float ringRadius = (length(p) / r.y) * d - 0.2;             \n"
	"         float ringGlow = abs(ringRadius) + (8.0 / r.y);             \n"
	"                                                                     \n"
	"         // 6) Paleta de colores RGB basada en cos(i*i)              \n"
	"         float4 colorWave = cos(i * i + float4(6.0, 7.0, 8.0, 0.0)) + 1.0;\n"
	"                                                                     \n"
	"         // 7) Acumulacion final por cada capa con factor de brillo   \n"
	"         O += (colorWave / ringGlow) * min(d, 1.0) / ((d + 1.0) * 15.0);\n"
	"     }                                                               \n"
	"                                                                     \n"
	"     // 8) Tonemapping Tanh algebraico fluido                        \n"
	"     float4 finalColor = O / (1.0 + abs(O));                         \n"
	"                                                                     \n"
	"     finalColor.w = 1.0;                                             \n"
	"     return finalColor;                                              \n"
	" }                                                                   \n";



static const char* g_strHLSLBackgroundEther =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"                                                                     \n"
	"     // 1) Coordenadas de pantalla centradas (Corregido aspecto y eje Y)\n"
	"     float2 uv = In.TexCoord;                                        \n"
	"     float2 pCoords = float2(uv.x * (r.x / r.y), (1.0 - uv.y)) - float2(0.9, 0.5);\n"
	"                                                                     \n"
	"     // 2) PRE-CALCULO TRIGONOMETRICO (Ahorrate decenas de sin/cos por pixel)\n"
	"     float cosXZ, sinXZ, cosXY, sinXY;                               \n"
	"     sincos(t * 0.4, sinXZ, cosXZ);                                  \n"
	"     sincos(t * 0.3, sinXY, cosXY);                                  \n"
	"     float sinT07 = sin(t * 0.7);                                    \n"
	"                                                                     \n"
	"     float3 cl = float3(0.0, 0.0, 0.0);                              \n"
	"     float d = 2.5;                                                  \n"
	"     float3 rayDir = normalize(float3(pCoords, -1.0));               \n"
	"                                                                     \n"
	"     // 3) Bucle principal de Raymarching (Solo 6 iteraciones nativas)\n"
	"     [unroll]                                                        \n"
	"     for(int i = 0; i <= 5; i++)                                     \n"
	"     {                                                               \n"
	"         float3 p = float3(0.0, 0.0, 5.0) + rayDir * d;              \n"
	"                                                                     \n"
	"         // --- MAP PASO 1 (Posicion original) ---                   \n"
	"         float3 p1 = p;                                              \n"
	"         // Rotacion XZ manual                                       \n"
	"         float2 xz1 = float2(p1.x * cosXZ - p1.z * sinXZ, p1.x * sinXZ + p1.z * cosXZ);\n"
	"         p1.xz = xz1;                                                \n"
	"         // Rotacion XY manual                                       \n"
	"         float2 xy1 = float2(p1.x * cosXY - p1.y * sinXY, p1.x * sinXY + p1.y * cosXY);\n"
	"         p1.xy = xy1;                                                \n"
	"         float3 q1 = p1 * 2.0 + t;                                   \n"
	"         float rz = length(p1 + float3(sinT07, sinT07, sinT07)) * log(length(p1) + 1.0) + sin(q1.x + sin(q1.z + sin(q1.y))) * 0.5 - 1.0;\n"
	"                                                                     \n"
	"         // --- MAP PASO 2 (Posicion desplazada para iluminacixn) --- \n"
	"         float3 p2 = p + 0.1;                                        \n"
	"         // Rotacion XZ manual                                       \n"
	"         float2 xz2 = float2(p2.x * cosXZ - p2.z * sinXZ, p2.x * sinXZ + p2.z * cosXZ);\n"
	"         p2.xz = xz2;                                                \n"
	"         // Rotacion XY manual                                       \n"
	"         float2 xy2 = float2(p2.x * cosXY - p2.y * sinXY, p2.x * sinXY + p2.y * cosXY);\n"
	"         p2.xy = xy2;                                                \n"
	"         float3 q2 = p2 * 2.0 + t;                                   \n"
	"         float rz2 = length(p2 + float3(sinT07, sinT07, sinT07)) * log(length(p2) + 1.0) + sin(q2.x + sin(q2.z + sin(q2.y))) * 0.5 - 1.0;\n"
	"                                                                     \n"
	"         // 4) Iluminacion y acumulacixn de color estables           \n"
	"         float f = clamp((rz - rz2) * 0.5, -0.1, 1.0);               \n"
	"         float3 l = float3(0.1, 0.3, 0.4) + float3(5.0, 2.5, 3.0) * f;\n"
	"         cl = cl * l + smoothstep(2.5, 0.0, rz) * 0.7 * l;           \n"
	"                                                                     \n"
	"         // 5) Avance del rayo                                       \n"
	"         d += min(rz, 1.0);                                          \n"
	"     }                                                               \n"
	"                                                                     \n"
	"     return float4(cl, 1.0);                                         \n"
	" }                                                                   \n";


static const char* g_strHLSLBackgroundShootingStars =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"                                                                     \n"
	"     // 1) Coordenadas de pantalla identicas a Shadertoy            \n"
	"     float2 fragCoord = In.TexCoord * r;                             \n"
	"                                                                     \n"
	"     float4 O = float4(0.0, 0.0, 0.0, 0.0);                          \n"
	"     float2 b = float2(0.0, 0.2);                                    \n"
	"                                                                     \n"
	"     // 2) Bucle principal estatico de 20 iteraciones                 \n"
	"     [unroll]                                                        \n"
	"     for(float i = 1.0; i <= 20.0; i += 1.0)                         \n"
	"     {                                                               \n"
	"         // 3) Reconstruccion de la matriz de rotacixn para DirectX   \n"
	"         float sinR, cosR;                                           \n"
	"         sincos(i, sinR, cosR);                                      \n"
	"                                                                     \n"
	"         float2x2 R = float2x2(cosR, -sinR, sinR, cosR);             \n"
	"         float2x2 R_inv = float2x2(cosR, sinR, -sinR, cosR);         \n"
	"                                                                     \n"
	"         // 4) Animacixn y repeticixn del espacio (fract)            \n"
	"         float2 uvShift = (fragCoord / r.y * i * 0.1) - (t * b);     \n"
	"                                                                     \n"
	"         // Aplicamos la rotacixn simulando el movimiento oblicuo    \n"
	"         float2 rotatedUV = mul(uvShift, R);                         \n"
	"         float2 fractUV = frac(rotatedUV) - 0.5;                     \n"
	"                                                                     \n"
	"         // 5) Calculo de la caja de la linea de la estrella (Box SDF)\n"
	"         float2 p = mul(fractUV, R_inv);                             \n"
	"         float2 clampedP = clamp(p, -b, b);                          \n"
	"                                                                     \n"
	"         // Distancia a la linea con proteccion contra NaN           \n"
	"         float dist = length(clampedP - p) + 0.001;                  \n"
	"                                                                     \n"
	"         // 6) Paleta cromatica original con el gradiente invertido   \n"
	"         // CORRECCIxN: Invertimos el signo de p.y para emparejar el color\n"
	"         float4 colorWave = cos(-p.y / 0.1 + float4(0.0, 1.0, 2.0, 3.0)) + 1.0;\n"
	"                                                                     \n"
	"         // Acumulacion del brillo de la estela                     \n"
	"         O += (0.001 / dist) * colorWave;                            \n"
	"     }                                                               \n"
	"                                                                     \n"
	"     // 7) Tonemapping Tanh algebraico para suavizar el brillo extremo\n"
	"     float4 finalColor = O / (1.0 + abs(O));                         \n"
	"     finalColor.w = 1.0;                                             \n"
	"                                                                     \n"
	"     return finalColor;                                              \n"
	" }                                                                   \n";


static const char* g_strHLSLBackgroundNeonBars =
	" float gTime : register(c0);                                         \n"
	" float2 gResolution : register(c1);                                  \n"
	"                                                                     \n"
	" struct PS_IN { float4 Position : POSITION;                           \n"
	"                float2 TexCoord : TEXCOORD0; };                       \n"
	"                                                                     \n"
	" float4 main(PS_IN In) : COLOR0                                      \n"
	" {                                                                   \n"
	"     float2 r = gResolution;                                         \n"
	"     float  t = gTime;                                               \n"
	"     float2 I = float2(In.TexCoord.x, 1.0 - In.TexCoord.y) * r;      \n"
	"                                                                     \n"
	"     float4 O = 0;                                                   \n"
	"     float  i0 = frac(-t);                                           \n"
	"                                                                     \n"
	"     [unroll]                                                        \n"
	"     for (int k = 0; k < 20; k++)                                    \n"
	"     {                                                               \n"
	"         float i = i0 + 0.5 * (float)k;                              \n"
	"         float mask = (i < 10.0) ? 1.0 : 0.0;                        \n"
	"                                                                     \n"
	"         // Coordenadas de la barra (centro de la barra)              \n"
	"         float2 o = (I + I - r) / r.y * i                            \n"
	"                    + cos(i * float2(0.8, 0.5) + t);                 \n"
	"                                                                     \n"
	"         // Punto de la linea de referencia (solo componente x de o) \n"
	"         float2 lineP = float2(clamp(o.x, -4.0, 4.0),                \n"
	"                                i + o.x * sin(i) * 0.1 - 4.0);        \n"
	"         float dist = length(o - lineP);                             \n"
	"                                                                     \n"
	"         // Evitar division por i=0 exacto (t entero)                \n"
	"         float iSafe = max(i, 1e-4);                                 \n"
	"         float denom = iSafe / 1e3 + dist / iSafe;                   \n"
	"                                                                     \n"
	"         float4 col = (cos(i + float4(0, 2, 4, 0)) + 1.0)            \n"
	"                      / max(i * i, 5.0) * 0.1 / denom;               \n"
	"                                                                     \n"
	"         O += col * mask;                                            \n"
	"     }                                                               \n"
	"                                                                     \n"
	"     return O;                                                       \n"
	" }                                                                   \n";








/* --- Background shader array --- */
static const char* g_strHLSLAlphaFix =
	" sampler s0 : register(s0);                              \n"
	" float4 main(float2 uv : TEXCOORD0) : COLOR0             \n"
	" {                                                       \n"
	"     float4 c = tex2D(s0, uv);                           \n"
	"     if (dot(c.rgb, 1) > 0.001 && c.a < 0.01) c.a = 1.0;\n"
	"     return c;                                           \n"
	" }                                                       \n";

static const char* g_strHLSLBackgrounds[3];

#define HLSL_BG_COUNT (sizeof(g_strHLSLBackgrounds) / sizeof(g_strHLSLBackgrounds[0]))

/* --- Platform-specific API ------------------------------------------------ *
 * setActive(n): 0=off, 1..HLSL_BG_COUNT = shader index                     *
 * getActive():  returns current active shader index                          */

#ifdef WIN
	/* Windows: clase C++ con implementacion en win_d3d9.cpp */
	class HLSLBackground {
	public:
		HLSLBackground();
		void init(struct IDirect3DDevice9* dev);
		void draw();
		void shutdown();
		struct IDirect3DPixelShader9* alphaFixShader() { return m_psAlphaFix; }
	private:
		struct IDirect3DDevice9*       m_dev;
		struct IDirect3DPixelShader9*  m_psBg[HLSL_BG_COUNT];
		struct IDirect3DPixelShader9*  m_psAlphaFix;
		struct IDirect3DVertexBuffer9* m_vb;
		bool m_inited;
	};
	void HLSLBackground_setActive(int n);
#elif defined(_XBOX)
	/* Xbox 360: wrappers C (implementacion en SDL_xboxvideo.c) */
	#ifdef __cplusplus
	extern "C" {
	#endif
	void HLSLBackground_init(struct IDirect3DDevice9* dev);
	void HLSLBackground_draw(struct IDirect3DDevice9* dev);
	void HLSLBackground_shutdown(void);
	void HLSLBackground_setActive(int n);
	int HLSLBackground_getActive();
	struct IDirect3DPixelShader9* HLSLBackground_getAlphaFixShader(void);
	#ifdef __cplusplus
	}
	#endif
#endif
