/*
 * win_d3d9.cpp - Implementacion de la capa de video D3D9 para Windows.
 * Ver win_d3d9.h para la vision general. Port directo de la logica de
 * libs/libSDLx360/SDL/src/video/xbox/SDL_xboxvideo.c, adaptado a D3D9 PC:
 *
 *   - Formatos D3DFMT_* normales en vez de D3DFMT_LIN_* (Xenon).
 *   - La textura del juego es DYNAMIC + DEFAULT y se sube por frame
 *     (1 memcpy) en lugar del zero-copy con lock permanente de Xenon.
 *   - Vertices con la regla del medio pixel de D3D9 PC (-0.5 en x/y).
 *   - Manejo basico de device-lost (reset de recursos DEFAULT).
 *
 * Los shaders (HLSL ps_3_0) son identicos a los de Xbox.
 */
#ifdef WIN

#include "win_d3d9.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Passthrough embebido: unico shader que sigue viviendo en el binario, como
   fallback de emergencia. El resto se cargan de assets\shaders en runtime. */
#include "../../libs/libSDLx360/SDL/src/video/xbox/SDL_shaders_src.h"
#include "HLSLBackground.h"
/* Tabla de presets publicada por src/video/shaderpreset.cpp. */
#include "salvia_shader_api.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* La lista de efectos ya no es fija: la publica la capa de aplicacion desde
   assets\shaders (ver salvia_shader_api.h). El indice de efecto es la posicion
   en esa tabla. */

/* Flags de compilacion (mismos presets que en Xbox). */
#define PS_FLAGS_DEFAULT         (D3DXSHADER_PARTIALPRECISION | D3DXSHADER_PREFER_FLOW_CONTROL)
#define PS_FLAGS_FULL_PRECISION  (D3DXSHADER_PREFER_FLOW_CONTROL)

/* Vertice pretransformado (screen-space, rhw) + 1 set de UV.
   Usamos pipeline de vertices fijo (FVF) + pixel shader programable:
   en D3D9 PC es legal y nos ahorra el vertex shader. VPOS le llega al
   ps_3_0 igualmente (lo genera el rasterizador). */
typedef struct { float x, y, z, rhw; float u, v; } VTX;
#define VTX_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

/* ---- Estado del modulo ---- */
static LPDIRECT3D9            g_d3d        = NULL;
static LPDIRECT3DDEVICE9      g_dev        = NULL;
static D3DPRESENT_PARAMETERS  g_pp;
static int                    g_bbw = 0, g_bbh = 0;     /* backbuffer */

static LPDIRECT3DTEXTURE9     g_game_tex   = NULL;       /* DYNAMIC, DEFAULT */
static SDL_Surface*           g_game_surf  = NULL;       /* fuente CPU (core res) */
static int                    g_tex_w = 0, g_tex_h = 0;
static D3DFORMAT              g_tex_fmt    = D3DFMT_X8R8G8B8;
static int                    g_tex_bpp    = 32;

static LPDIRECT3DVERTEXBUFFER9 g_vb        = NULL;       /* 3 verts: triangulo fullscreen */
static VTX                    g_verts[3];

/* ---- Tabla de shaders publicada por la app (assets\shaders) ---- */
static const SalviaShaderPreset* g_presets = NULL;
static int                    g_presetCount = 0;
static LPDIRECT3DPIXELSHADER9* g_compiled  = NULL;       /* [preset], NULL = passthrough */
static LPDIRECT3DTEXTURE9**   g_lutTex     = NULL;       /* [preset][lut], MANAGED */
static int                    g_lutsUploaded = 0;

/* Passthrough embebido. Es la red de seguridad: NUNCA se engancha un pixel
   shader NULL, porque en Xenon eso es pantalla negra (sin pipeline de funcion
   fija) y aqui replicamos el mismo modelo. */
static LPDIRECT3DPIXELSHADER9 g_fallbackPS = NULL;
/* Shader realmente enganchado ahora mismo. El overlay lo usa para restaurar
   sin volver a indexar por g_current_effect. */
static LPDIRECT3DPIXELSHADER9 g_activePS   = NULL;

static int                    g_current_effect = 0;
static D3DTEXTUREFILTERTYPE   g_current_filter = D3DTEXF_LINEAR;
static float                  g_aspect     = 0.0f;       /* 0 = ratio nativo */
static int                    g_fullscreen = 1;          /* 1 = fill, 0 = pixel-perfect */
static int                    g_overflow   = 0;          /* 1 = integer scale puede salirse de pantalla */
static int                    g_scale_type = 0;          /* 0=reduce,1=increase,2..6=escala fija 1x..5x */
static int                    g_rotation   = 0;          /* 0..3 (libretro) */
static RECT                   g_visible    = { 0, 0, 0, 0 };
static int                    g_vsync      = 1;          /* 1 = vsync on (DEFAULT), 0 = vsync off */

/* Overlay ARGB (1 capa sobre el quad del juego). */
static LPDIRECT3DTEXTURE9      g_ovl_tex    = NULL;       /* DYNAMIC, DEFAULT */
static LPDIRECT3DVERTEXBUFFER9 g_ovl_vb     = NULL;
static SDL_Surface*            g_ovl_surf   = NULL;       /* fuente CPU (bb res) */
static int                     g_ovl_enabled = 0;
static int                     g_ovl_overscan_x = 0;   /* pixeles de overscan (positivo = reduce area) */
static int                     g_ovl_overscan_y = 0;

static CRITICAL_SECTION        g_cs;
static int                     g_cs_init    = 0;

/* Instance global de HLSLBackground (fondo animado por shader). */
static HLSLBackground          g_hlslBkg;
int                     g_hlslBkg_active = 0;

IDirect3DDevice9* WinD3D9_GetDevice(void) { return g_dev; }

void HLSLBackground_setActive(int n) {
    g_hlslBkg_active = n;
}

int HLSLBackground_getActive(){
	return g_hlslBkg_active;
}

/* =====================================================================
 * HLSLBackground — implementacion Windows (clase C++)
 * =================================================================== */

typedef struct { float x, y, z, rhw; float u, v; } HLSL_BG_VTX;
#define HLSL_BG_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

/* HLSL pixel shader (ps_3_0) — adaptado de GLSL (one-liner demo).
 * Original GLSL:
 *   vec2 p=(FC.xy*2.-r)/r.y,l,v=p*(1.-(l+=abs(.7-dot(p,p))))/.2;
 *   for(float i;i++<8.;o+=(sin(v.xyyx)+1.)*abs(v.x-v.y)*.2)
 *     v+=cos(v.yx*i+vec2(0,i)+t)/i+.7;
 *   o=tanh(exp(p.y*vec4(1,-1,-2,0))*exp(-4.*l.x)/o);
 *
 * Uniforms: c0.x = time, c1.xy = resolution */

/* =====================================================================
 * Shader cache — DJB2 hash + disk load/save
 * =================================================================== */

static unsigned long Win_HashShaderSource(const char* str, DWORD flags) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    hash = ((hash << 5) + hash) + (unsigned long)flags;
    return hash;
}

static DWORD* Win_LoadCachedShader(unsigned long hash, DWORD* outSize) {
    char path[256];
    HANDLE hFile;
    DWORD fileSize, bytesRead;
    DWORD* buffer;

    sprintf(path, "shadercache\\%08lX.pso", hash);
    hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }

    buffer = (DWORD*)malloc(fileSize);
    if (!buffer) { CloseHandle(hFile); return NULL; }

    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        free(buffer);
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);
    *outSize = fileSize;
    return buffer;
}

static void Win_SaveCachedShader(unsigned long hash, const void* bytecode, DWORD size) {
    char path[256];
    HANDLE hFile;
    DWORD bytesWritten;

    CreateDirectoryA("shadercache", NULL);

    sprintf(path, "shadercache\\%08lX.pso", hash);
    hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    WriteFile(hFile, bytecode, size, &bytesWritten, NULL);
    CloseHandle(hFile);
}

HLSLBackground::HLSLBackground() : m_dev(NULL), m_psAlphaFix(NULL), m_vb(NULL), m_inited(false) {
	for (int i = 0; i < HLSL_BG_COUNT; i++) m_psBg[i] = NULL;
}

void HLSLBackground::init(IDirect3DDevice9* dev) {
	if (m_inited || !dev) return;
	m_dev = dev;

	g_strHLSLBackgrounds[0] = g_strHLSLBackground;
	g_strHLSLBackgrounds[1] = g_strHLSLBackgroundEther;
	g_strHLSLBackgrounds[2] = g_strHLSLBackgroundWormholes;
	//g_strHLSLBackgrounds[3] = g_strHLSLBackgroundShootingStars;
	//g_strHLSLBackgrounds[4] = g_strHLSLBackgroundNeonBars;

	ID3DXBuffer* code = NULL;
	ID3DXBuffer* errs = NULL;

	/* Compile all background shaders (with disk cache) */
	for (int i = 0; i < HLSL_BG_COUNT; i++) {
		unsigned long hash = Win_HashShaderSource(g_strHLSLBackgrounds[i], D3DXSHADER_PARTIALPRECISION | D3DXSHADER_PREFER_FLOW_CONTROL);
		DWORD cachedSize = 0;
		DWORD* cachedCode = Win_LoadCachedShader(hash, &cachedSize);
		if (cachedCode) {
			m_dev->CreatePixelShader(cachedCode, &m_psBg[i]);
			free(cachedCode);
		} else {
			if (SUCCEEDED(D3DXCompileShader(g_strHLSLBackgrounds[i], (UINT)strlen(g_strHLSLBackgrounds[i]),
					NULL, NULL, "main", "ps_3_0",
					D3DXSHADER_PARTIALPRECISION | D3DXSHADER_PREFER_FLOW_CONTROL,
					&code, &errs, NULL))) {
				Win_SaveCachedShader(hash, code->GetBufferPointer(), code->GetBufferSize());
				m_dev->CreatePixelShader((const DWORD*)code->GetBufferPointer(), &m_psBg[i]);
				code->Release();
			}
			if (errs) { OutputDebugStringA((const char*)errs->GetBufferPointer()); errs->Release(); errs = NULL; }
		}
	}

	/* Alpha-fixup shader para overlay cuando HLSL background esta activo */
	{
		unsigned long hash = Win_HashShaderSource(g_strHLSLAlphaFix, 0);
		DWORD cachedSize = 0;
		DWORD* cachedCode = Win_LoadCachedShader(hash, &cachedSize);
		if (cachedCode) {
			m_dev->CreatePixelShader(cachedCode, &m_psAlphaFix);
			free(cachedCode);
		} else {
			if (SUCCEEDED(D3DXCompileShader(g_strHLSLAlphaFix, (UINT)strlen(g_strHLSLAlphaFix),
					NULL, NULL, "main", "ps_3_0", 0, &code, &errs, NULL))) {
				Win_SaveCachedShader(hash, code->GetBufferPointer(), code->GetBufferSize());
				m_dev->CreatePixelShader((const DWORD*)code->GetBufferPointer(), &m_psAlphaFix);
				code->Release();
			}
			if (errs) { errs->Release(); }
		}
	}

	m_dev->CreateVertexBuffer(sizeof(HLSL_BG_VTX), D3DUSAGE_WRITEONLY,
		HLSL_BG_FVF, D3DPOOL_DEFAULT, &m_vb, NULL);
	m_inited = true;
}

void HLSLBackground::draw() {
	if (!m_dev || !m_psBg[0] || !m_vb) return;

	D3DVIEWPORT9 vp;
	m_dev->GetViewport(&vp);
	float w = (float)vp.Width;
	float h = (float)vp.Height;

	struct QuadVTX { float x, y, z, rhw; float u, v; };
	QuadVTX* vtx;
	m_vb->Lock(0, 0, (void**)&vtx, 0);
	vtx[0].x = -0.5f;    vtx[0].y = h - 0.5f; vtx[0].z = 0; vtx[0].rhw = 1; vtx[0].u = 0; vtx[0].v = 1;
	vtx[1].x = -0.5f;    vtx[1].y = -0.5f;    vtx[1].z = 0; vtx[1].rhw = 1; vtx[1].u = 0; vtx[1].v = 0;
	vtx[2].x = w - 0.5f; vtx[2].y = h - 0.5f; vtx[2].z = 0; vtx[2].rhw = 1; vtx[2].u = 1; vtx[2].v = 1;
	vtx[3].x = w - 0.5f; vtx[3].y = -0.5f;    vtx[3].z = 0; vtx[3].rhw = 1; vtx[3].u = 1; vtx[3].v = 0;
	m_vb->Unlock();

	float t = SDL_GetTicks() / 1000.0f;
	float cTime[4] = { t, 0, 0, 0 };
	float cRes[4]  = { w, h, 0, 0 };

	m_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	m_dev->SetTexture(0, NULL);
	m_dev->SetStreamSource(0, m_vb, 0, sizeof(HLSL_BG_VTX));
	m_dev->SetFVF(HLSL_BG_FVF);
	m_dev->SetPixelShaderConstantF(0, cTime, 1);
	m_dev->SetPixelShaderConstantF(1, cRes, 1);
	int idx = (g_hlslBkg_active >= 1 && g_hlslBkg_active <= HLSL_BG_COUNT) ? g_hlslBkg_active - 1 : 0;
	m_dev->SetPixelShader(m_psBg[idx]);
	m_dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	m_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

	/* Restaurar c1 = dims de la textura del juego. Los shaders del juego (HQ2X,
	 * xBR, CRT, Sharp-Bilinear...) leen c1 como textureDims; si dejamos aqui la
	 * resolucion del backbuffer, al volver del menu el efecto muestrea con
	 * offsets erroneos y "desaparece". El fondo debe ser transparente al
	 * pipeline del juego. */
	if (g_tex_w > 0) {
		float cGameDims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
		m_dev->SetPixelShaderConstantF(1, cGameDims, 1);
	}
}

void HLSLBackground::shutdown() {
	if (m_vb) { m_vb->Release(); m_vb = NULL; }
	for (int i = 0; i < HLSL_BG_COUNT; i++) {
		if (m_psBg[i]) { m_psBg[i]->Release(); m_psBg[i] = NULL; }
	}
	if (m_psAlphaFix) { m_psAlphaFix->Release(); m_psAlphaFix = NULL; }
	m_dev = NULL;
	m_inited = false;
}

/* UVs del "single fullscreen triangle" por rotacion (identico a Xbox). */
static const float g_uv_rot[4][6] = {
    /* 0:   0 deg  */ { 0.0f, 0.0f,  2.0f, 0.0f,  0.0f, 2.0f },
    /* 1:  90 CCW  */ { 1.0f, 0.0f,  1.0f, 2.0f, -1.0f, 0.0f },
    /* 2: 180      */ { 1.0f, 1.0f, -1.0f, 1.0f,  1.0f,-1.0f },
    /* 3: 270 CCW  */ { 0.0f, 1.0f,  0.0f,-1.0f,  2.0f, 1.0f },
};

static HRESULT CreateShader(const char* src, LPDIRECT3DPIXELSHADER9* target, DWORD flags)
{
    ID3DXBuffer* code  = NULL;
    ID3DXBuffer* errs  = NULL;
    HRESULT hr;
    unsigned long hash;
    DWORD cachedSize = 0;
    DWORD* cachedCode;

    if (!src || !target) return E_INVALIDARG;
    *target = NULL;

    hash = Win_HashShaderSource(src, flags);
    cachedCode = Win_LoadCachedShader(hash, &cachedSize);
    if (cachedCode) {
        hr = g_dev->CreatePixelShader(cachedCode, target);
        free(cachedCode);
        return hr;
    }

    hr = D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL, "main", "ps_3_0",
                           flags, &code, &errs, NULL);
    if (FAILED(hr)) {
        if (errs) {
            OutputDebugStringA((const char*)errs->GetBufferPointer());
            errs->Release();
        }
        return hr;
    }

    Win_SaveCachedShader(hash, code->GetBufferPointer(), code->GetBufferSize());

    hr = g_dev->CreatePixelShader((const DWORD*)code->GetBufferPointer(), target);
    code->Release();
    if (errs) errs->Release();
    return hr;
}

/* Sube una LUT ya decodificada a una textura MANAGED (sobrevive al reset).
   El contrato de salvia_shader_api.h es "words 0xAARRGGBB nativos", que en
   x86 son bytes B,G,R,A = exactamente el layout de D3DFMT_A8R8G8B8 -> memcpy
   por fila, sin reordenar nada. */
static LPDIRECT3DTEXTURE9 CreateLUT(const SalviaShaderLut* src)
{
    LPDIRECT3DTEXTURE9 tex = NULL;
    D3DLOCKED_RECT lr;
    int y;

    if (!src || !src->pixels || src->width <= 0 || src->height <= 0) return NULL;

    if (FAILED(g_dev->CreateTexture(src->width, src->height, 1, 0, D3DFMT_A8R8G8B8,
                                    D3DPOOL_MANAGED, &tex, NULL)) || !tex)
        return NULL;

    if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return NULL; }
    for (y = 0; y < src->height; y++)
        memcpy((unsigned char*)lr.pBits + y * lr.Pitch,
               src->pixels + (size_t)y * src->pitch, (size_t)src->width * 4);
    tex->UnlockRect(0);
    return tex;
}

static D3DTEXTUREADDRESS MapWrap(SalviaShaderWrap w)
{
    if (w == SALVIA_WRAP_REPEAT) return D3DTADDRESS_WRAP;
    if (w == SALVIA_WRAP_MIRROR) return D3DTADDRESS_MIRROR;
    return D3DTADDRESS_CLAMP;
}

static void DestroyShaders(void)
{
    int i, l;

    if (g_compiled) {
        for (i = 0; i < g_presetCount; i++)
            if (g_compiled[i]) { g_compiled[i]->Release(); g_compiled[i] = NULL; }
        free(g_compiled);
        g_compiled = NULL;
    }
    if (g_lutTex) {
        for (i = 0; i < g_presetCount; i++) {
            if (!g_lutTex[i]) continue;
            for (l = 0; l < SALVIA_SHADER_MAX_LUTS; l++)
                if (g_lutTex[i][l]) { g_lutTex[i][l]->Release(); g_lutTex[i][l] = NULL; }
            free(g_lutTex[i]);
        }
        free(g_lutTex);
        g_lutTex = NULL;
    }
    if (g_fallbackPS) { g_fallbackPS->Release(); g_fallbackPS = NULL; }
    g_activePS = NULL;
    g_lutsUploaded = 0;
    SalviaShader_Release();
}

static void InitShaders(void)
{
    int i, l;

    if (g_fallbackPS != NULL) return; /* ya compilados */

    /* El passthrough va PRIMERO y sin condiciones: es lo que evita que un
       preset roto -o la ausencia total de assets\shaders- deje el pipeline
       con un pixel shader NULL. */
    CreateShader(g_strShaderNormalSource, &g_fallbackPS, PS_FLAGS_DEFAULT);
    if (!g_fallbackPS) {
        OutputDebugStringA("ERROR: no se pudo compilar el passthrough integrado\n");
        return;
    }
    g_activePS = g_fallbackPS;

    if (g_presetCount <= 0 || !g_presets) {
        OutputDebugStringA("Shaders: no hay tabla registrada; solo passthrough\n");
        return;
    }

    g_compiled = (LPDIRECT3DPIXELSHADER9*)calloc(g_presetCount, sizeof(LPDIRECT3DPIXELSHADER9));
    g_lutTex   = (LPDIRECT3DTEXTURE9**)calloc(g_presetCount, sizeof(LPDIRECT3DTEXTURE9*));
    if (!g_compiled || !g_lutTex) return;

    for (i = 0; i < g_presetCount; i++) {
        const SalviaShaderPass* pass = &g_presets[i].passes[g_presets[i].activePass];

        /* Sin source el preset solo describe estado de sampler (Nearest,
           Bilinear): se queda en NULL y SelectEffect usa el passthrough. */
        if (pass->source) {
            DWORD flags = (pass->psFlags & SALVIA_PS_FULL_PRECISION)
                        ? PS_FLAGS_FULL_PRECISION : PS_FLAGS_DEFAULT;
            CreateShader(pass->source, &g_compiled[i], flags);
            if (!g_compiled[i]) {
                char msg[160];
                sprintf(msg, "ERROR: el preset '%s' no compila; cae a passthrough\n",
                        g_presets[i].id);
                OutputDebugStringA(msg);
            }
        }

        g_lutTex[i] = (LPDIRECT3DTEXTURE9*)calloc(SALVIA_SHADER_MAX_LUTS,
                                                  sizeof(LPDIRECT3DTEXTURE9));
        if (!g_lutTex[i]) continue;
        for (l = 0; l < pass->lutCount && l < SALVIA_SHADER_MAX_LUTS; l++)
            g_lutTex[i][l] = CreateLUT(&pass->luts[l]);
    }

    g_lutsUploaded = 1;
}

static void SetSampler0Filter(D3DTEXTUREFILTERTYPE f)
{
    g_current_filter = f;
    g_dev->SetSamplerState(0, D3DSAMP_MINFILTER, f);
    g_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, f);
}

/* Factor de escala del efecto (para el modo pixel-perfect). */
static int EffectScale(void)
{
//    switch (g_current_effect) {
//		case 8:  return 2; /* HQ2x */
//		case 9:  return 3; /* HQ3x */
//		case 10: return 4; /* HQ4x */
//		case 11: return 3; /* xBR-lv2-fast */
//		case 12: return 3; /* 5xBR-Hyllian (rendered at 3x via HQ3x infra) */
//		default: return 1; /* 0=Nearest, 1=Sharp-Bilinear, 2=Bilinear, 3=LCD-Grid-v2,
//		                      4=Scanlines, 5=CRT-Geom, 6=CRT-Lottes, 7=CRT-Easymode */
//    }
	return 1;
}

/* =====================================================================
 * Quad principal (aspect ratio / rotacion / pixel-perfect) - port de
 * XBOX_UpdateVertexBuffer. La unica diferencia con Xenon es el -0.5 en
 * x/y por la regla del medio pixel de D3D9 PC.
 * =================================================================== */
static void UpdateVertexBuffer(int tex_w, int tex_h, float aspect_ratio)
{
    void* locked;
    float display_w, display_h, offset_x, offset_y;
    float bbw = (float)g_bbw;
    float bbh = (float)g_bbh;
    float bb_ratio;

    if (!g_vb) return;

    if (g_rotation & 1) { int t = tex_w; tex_w = tex_h; tex_h = t; }

    if (g_fullscreen) {
        bb_ratio = bbw / bbh;
        if (aspect_ratio <= 0.0f)
            aspect_ratio = (float)tex_w / (float)tex_h;
        if (aspect_ratio > bb_ratio) { display_w = bbw; display_h = bbw / aspect_ratio; }
        else                         { display_h = bbh; display_w = bbh * aspect_ratio; }
    } else {
        int scale = EffectScale();
        /* Modos de escala entera FIJA 1x..5x (g_scale_type 2..6): factor fijo, se salta
           el auto-calculo y puede salirse de pantalla (g_overflow ya es true). */
        int fixed = (g_scale_type >= 2) ? (g_scale_type - 1) : 0;
        if (fixed > 0) scale = fixed;
        if (aspect_ratio > 0.0f && tex_w > 0 && tex_h > 0) {
            if (fixed == 0 && (scale == 1 || g_overflow)) {
                int mh = (int)floor(bbh / (float)tex_h);
                int mw = (int)floor(bbw / ((float)tex_h * aspect_ratio));
                int ms = g_overflow ? max(mh, mw) : min(mh, mw);
                if (ms > 1) scale = ms;
            }
            display_h = (float)(tex_h * scale);
            display_w = (float)floor(display_h * aspect_ratio);
        } else {
            if (fixed == 0 && (scale == 1 || g_overflow) && tex_w > 0 && tex_h > 0) {
                int mw = (int)floor(bbw / (float)tex_w);
                int mh = (int)floor(bbh / (float)tex_h);
                int ms = g_overflow ? max(mw, mh) : min(mw, mh);
                if (ms > 1) scale = ms;
            }
            display_w = (float)(tex_w * scale);
            display_h = (float)(tex_h * scale);
        }
        if (!g_overflow && (display_w > bbw || display_h > bbh)) {
            float cr = aspect_ratio;
            if (cr <= 0.0f) cr = (float)tex_w / (float)tex_h;
            bb_ratio = bbw / bbh;
            if (cr > bb_ratio) { display_w = bbw; display_h = (float)floor(bbw / cr); }
            else               { display_h = bbh; display_w = (float)floor(bbh * cr); }
        }
    }

    offset_x = (float)floor((bbw - display_w) * 0.5f);
    offset_y = (float)floor((bbh - display_h) * 0.5f);

    g_visible.left   = (LONG)offset_x;
    g_visible.top    = (LONG)offset_y;
    g_visible.right  = (LONG)(offset_x + display_w);
    g_visible.bottom = (LONG)(offset_y + display_h);

    {
        const float* uv = g_uv_rot[g_rotation & 3];
        const float hp = 0.5f; /* medio pixel D3D9 PC */

        g_verts[0].x = offset_x - hp;               g_verts[0].y = offset_y - hp;
        g_verts[0].z = 0; g_verts[0].rhw = 1;       g_verts[0].u = uv[0]; g_verts[0].v = uv[1];

        g_verts[1].x = offset_x + 2.0f*display_w - hp; g_verts[1].y = offset_y - hp;
        g_verts[1].z = 0; g_verts[1].rhw = 1;       g_verts[1].u = uv[2]; g_verts[1].v = uv[3];

        g_verts[2].x = offset_x - hp;               g_verts[2].y = offset_y + 2.0f*display_h - hp;
        g_verts[2].z = 0; g_verts[2].rhw = 1;       g_verts[2].u = uv[4]; g_verts[2].v = uv[5];
    }

    if (SUCCEEDED(g_vb->Lock(0, 0, (void**)&locked, 0))) {
        memcpy(locked, g_verts, sizeof(g_verts));
        g_vb->Unlock();
    }
}

static void DrawMainQuad(void)
{
    g_dev->SetScissorRect(&g_visible);
    g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    g_dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

/* =====================================================================
 * Seleccion de efecto - port de XBOX_SelectEffect (D3D9 identico).
 * =================================================================== */
void XBOX_SelectEffect(int effectID)
{
    const SalviaShaderPass* pass;
    LPDIRECT3DPIXELSHADER9 ps;
    float dims[4];
    int i;

    if (!g_dev || !g_fallbackPS) return;
    if (g_presetCount <= 0 || !g_presets) {
        /* Sin tabla: passthrough puro, para no dejar el pipeline sin shader. */
        g_activePS = g_fallbackPS;
        g_dev->SetPixelShader(g_activePS);
        SetSampler0Filter(D3DTEXF_POINT);
        return;
    }
    if (effectID < 0 || effectID >= g_presetCount) effectID = 0;
    g_current_effect = effectID;

    pass = &g_presets[effectID].passes[g_presets[effectID].activePass];

    /* 1. Shader. Cachear el puntero (y no indexar por g_current_effect desde
          el bucle de dibujado) es lo que garantiza que jamas se engancha un
          NULL: un preset que no compilo cae aqui al passthrough. */
    ps = (g_compiled && g_compiled[effectID]) ? g_compiled[effectID] : g_fallbackPS;
    g_activePS = ps;
    g_dev->SetPixelShader(ps);

    /* 2. c1 = textureDims, SIEMPRE. Antes habia efectos que no lo escribian;
          ponerlo de mas es inocuo (cuesta 4 floats por cambio de efecto, no
          por frame) y elimina la casuistica. */
    dims[0] = (float)g_tex_w; dims[1] = (float)g_tex_h; dims[2] = 0; dims[3] = 0;
    g_dev->SetPixelShaderConstantF(1, dims, 1);

    /* 3. Sampler s0: filtro y wrap SIEMPRE. Escribir el wrap incondicionalmente
          arregla de paso una fuga de estado que habia antes: los efectos con
          CLAMP explicito (Lottes, Easymode, HQx) nunca lo devolvian a su sitio,
          asi que el siguiente efecto heredaba su modo de direccionamiento. */
    SetSampler0Filter(pass->filter == SALVIA_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    g_dev->SetSamplerState(0, D3DSAMP_ADDRESSU, MapWrap(pass->wrap));
    g_dev->SetSamplerState(0, D3DSAMP_ADDRESSV, MapWrap(pass->wrap));

    /* 4. LUTs: desvincular s1..sN y enganchar las de este preset. */
    for (i = 1; i <= SALVIA_SHADER_MAX_LUTS; i++)
        g_dev->SetTexture(i, NULL);

    for (i = 0; i < pass->lutCount && i < SALVIA_SHADER_MAX_LUTS; i++) {
        int s = pass->luts[i].sampler;
        LPDIRECT3DTEXTURE9 tex = (g_lutTex && g_lutTex[effectID]) ? g_lutTex[effectID][i] : NULL;
        D3DTEXTUREFILTERTYPE f = (pass->luts[i].filter == SALVIA_FILTER_LINEAR)
                               ? D3DTEXF_LINEAR : D3DTEXF_POINT;
        if (!tex || s < 1 || s > SALVIA_SHADER_MAX_LUTS) continue;
        g_dev->SetTexture(s, tex);
        g_dev->SetSamplerState(s, D3DSAMP_MINFILTER, f);
        g_dev->SetSamplerState(s, D3DSAMP_MAGFILTER, f);
        g_dev->SetSamplerState(s, D3DSAMP_ADDRESSU, MapWrap(pass->luts[i].wrap));
        g_dev->SetSamplerState(s, D3DSAMP_ADDRESSV, MapWrap(pass->luts[i].wrap));
    }

    if (!g_fullscreen && g_tex_w > 0)
        UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
}

/* =====================================================================
 * API de registro de la tabla de shaders (salvia_shader_api.h).
 * =================================================================== */
extern "C" int SalviaShader_SetTable(const SalviaShaderPreset* presets, int count)
{
    if (!presets || count <= 0) return 0;
    if (count > SALVIA_SHADER_MAX_PRESETS) count = SALVIA_SHADER_MAX_PRESETS;
    g_presets = presets;
    g_presetCount = count;
    return 1;
}

extern "C" int SalviaShader_GetCount(void)     { return g_presetCount; }
extern "C" int SalviaShader_LutsUploaded(void) { return g_lutsUploaded; }

extern "C" void SalviaShader_Release(void)
{
    g_presets = NULL;
    g_presetCount = 0;
}


/* =====================================================================
 * Overlay (capa ARGB sobre el quad del juego) - port de XBOX_*Overlay.
 * =================================================================== */
/* Aplica los valores actuales de overscan al vertex buffer del overlay.
   Llamar tras crear el VB y cada vez que cambien g_ovl_overscan_x/y. */
static void UpdateOverlayVertices(void)
{
    float bbw = (float)g_bbw, bbh = (float)g_bbh;
    float ox  = (float)g_ovl_overscan_x;
    float oy  = (float)g_ovl_overscan_y;
    void* locked;

    if (!g_ovl_vb) return;

    VTX ov[4];
    ov[0].x = -0.5f + ox;      ov[0].y = bbh - 0.5f - oy; ov[0].z = 0; ov[0].rhw = 1; ov[0].u = 0; ov[0].v = 1;
    ov[1].x = -0.5f + ox;      ov[1].y = -0.5f + oy;      ov[1].z = 0; ov[1].rhw = 1; ov[1].u = 0; ov[1].v = 0;
    ov[2].x = bbw - 0.5f - ox; ov[2].y = bbh - 0.5f - oy; ov[2].z = 0; ov[2].rhw = 1; ov[2].u = 1; ov[2].v = 1;
    ov[3].x = bbw - 0.5f - ox; ov[3].y = -0.5f + oy;      ov[3].z = 0; ov[3].rhw = 1; ov[3].u = 1; ov[3].v = 0;

    if (SUCCEEDED(g_ovl_vb->Lock(0, 0, (void**)&locked, 0))) {
        memcpy(locked, ov, sizeof(ov));
        g_ovl_vb->Unlock();
    }
}

/* Idempotente por recurso: crea solo lo que falte. El surface CPU del
   overlay persiste a traves de device-lost; la textura/VB (DEFAULT) se
   recrean en el reset llamando otra vez aqui. */
static void InitOverlay(void)
{
    D3DLOCKED_RECT lr;
    float bbw = (float)g_bbw, bbh = (float)g_bbh;

    /* Surface CPU ARGB (lo que dibuja el menu). Mismo formato que en Xbox. */
    if (!g_ovl_surf) {
        g_ovl_surf = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, (int)bbw, (int)bbh, 32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        if (!g_ovl_surf) return;
        SDL_FillRect(g_ovl_surf, NULL, 0x00000000); /* transparente */
    }

    /* Textura GPU (DYNAMIC/DEFAULT). */
    if (!g_ovl_tex) {
        if (FAILED(g_dev->CreateTexture((UINT)bbw, (UINT)bbh, 1, D3DUSAGE_DYNAMIC,
                                        D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_ovl_tex, NULL)))
            return;
        if (SUCCEEDED(g_ovl_tex->LockRect(0, &lr, NULL, D3DLOCK_DISCARD))) {
            int y;
            for (y = 0; y < (int)bbh; y++)
                memset((unsigned char*)lr.pBits + y * lr.Pitch, 0, (int)bbw * 4);
            g_ovl_tex->UnlockRect(0);
        }
    }

    /* Vertex buffer del quad fullscreen (TRIANGLESTRIP de 4 verts, medio pixel). */
    if (!g_ovl_vb) {
        if (FAILED(g_dev->CreateVertexBuffer(sizeof(VTX) * 4, D3DUSAGE_WRITEONLY, VTX_FVF,
                                             D3DPOOL_DEFAULT, &g_ovl_vb, NULL)))
            return;
    }
    UpdateOverlayVertices();  /* llena el VB con el overscan actual */
}

static void DestroyOverlay(void)
{
    if (g_ovl_surf) { SDL_FreeSurface(g_ovl_surf); g_ovl_surf = NULL; }
    if (g_ovl_tex)  { g_ovl_tex->Release();  g_ovl_tex = NULL; }
    if (g_ovl_vb)   { g_ovl_vb->Release();   g_ovl_vb  = NULL; }
    g_ovl_enabled = 0;
}

/* Sube el surface del overlay a su textura (solo si hay algo que mostrar). */
static void UploadOverlay(void)
{
    D3DLOCKED_RECT lr;
    int y, rb;
    if (!g_ovl_tex || !g_ovl_surf) return;
    if (FAILED(g_ovl_tex->LockRect(0, &lr, NULL, D3DLOCK_DISCARD))) return;
    rb = g_ovl_surf->w * 4;
    if (rb > lr.Pitch) rb = lr.Pitch;
    for (y = 0; y < g_ovl_surf->h; y++)
        memcpy((unsigned char*)lr.pBits + y * lr.Pitch,
               (unsigned char*)g_ovl_surf->pixels + y * g_ovl_surf->pitch, rb);
    g_ovl_tex->UnlockRect(0);
}

static void DrawOverlay(void)
{
    if (!g_ovl_enabled || !g_ovl_tex || !g_ovl_vb) return;

    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    g_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    g_dev->SetTexture(0, g_ovl_tex);
    g_dev->SetStreamSource(0, g_ovl_vb, 0, sizeof(VTX));
    /* When HLSL background is active, use alpha-fixup shader instead of normal
     * passthrough.  Forces alpha=1 for any non-black pixel on GPU, replacing
     * the slow CPU loop that iterates every pixel. */
    g_dev->SetPixelShader((g_hlslBkg_active && g_hlslBkg.alphaFixShader())
        ? g_hlslBkg.alphaFixShader() : g_fallbackPS);
    g_dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    g_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    /* Restaurar lo que toco el overlay (textura+vb+shader+filtro del s0).
     * Se usa g_activePS -el puntero ya validado por SelectEffect- y NO
     * g_compiled[g_current_effect]: esto corre cada frame con el menu abierto,
     * y un NULL aqui seria pantalla negra en Xenon. */
    g_dev->SetTexture(0, g_game_tex);
    g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
    g_dev->SetPixelShader(g_activePS ? g_activePS : g_fallbackPS);
    g_dev->SetSamplerState(0, D3DSAMP_MINFILTER, g_current_filter);
    g_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, g_current_filter);
}

/* =====================================================================
 * Textura del juego + surface fuente
 * =================================================================== */
static void DestroyGameTexture(void)
{
    if (g_game_surf) { SDL_FreeSurface(g_game_surf); g_game_surf = NULL; }
    if (g_game_tex)  { g_game_tex->Release(); g_game_tex = NULL; }
}

static int CreateGameTexture(int width, int height, int bpp)
{
    Uint32 Rmask, Gmask, Bmask;
    int pitch_bpp;

    DestroyGameTexture();

    switch (bpp) {
        case 8:
        case 16:
            g_tex_fmt = D3DFMT_R5G6B5; pitch_bpp = 16;
            Rmask = 0x0000F800; Gmask = 0x000007E0; Bmask = 0x0000001F;
            break;
        case 24:
        case 32:
            g_tex_fmt = D3DFMT_X8R8G8B8; pitch_bpp = 32;
            Rmask = 0x00FF0000; Gmask = 0x0000FF00; Bmask = 0x000000FF;
            break;
        default:
            return 0;
    }

    if (FAILED(g_dev->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC,
                                    g_tex_fmt, D3DPOOL_DEFAULT, &g_game_tex, NULL)))
        return 0;

    g_game_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, pitch_bpp,
                                       Rmask, Gmask, Bmask, 0);
    if (!g_game_surf) { g_game_tex->Release(); g_game_tex = NULL; return 0; }

    g_tex_w = width; g_tex_h = height; g_tex_bpp = pitch_bpp;
    return 1;
}

SDL_Surface* WinD3D9_SetGameMode(int width, int height, int bpp)
{
    if (!g_dev) return NULL;

    if (!CreateGameTexture(width, height, bpp))
        return NULL;

    UpdateVertexBuffer(width, height, g_aspect);

    g_dev->SetTexture(0, g_game_tex);
    g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
    g_dev->SetFVF(VTX_FVF);

    /* Deja shader+sampler+constants coherentes desde el primer frame. */
    XBOX_SelectEffect(g_current_effect);

    return g_game_surf;
}

/* =====================================================================
 * Ajustes en caliente (mismos nombres que en Xbox).
 * =================================================================== */
void SDL_XBOX_SetDisplaySize(float aspect_ratio)
{
    g_aspect = aspect_ratio;
    UpdateVertexBuffer(g_tex_w, g_tex_h, aspect_ratio);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
}

void SDL_XBOX_SetDisplayFullscreen(int fullscreen)
{
    g_fullscreen = fullscreen;
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
}

void SDL_XBOX_SetDisplayOverflow(int type)
{
    g_scale_type = type;
    g_overflow   = (type != 0);   /* reduce(0) recorta a pantalla; increase y 1x-5x pueden salirse */
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
}

void SDL_XBOX_SetRotation(int rotation)
{
    g_rotation = (rotation >= 0 && rotation <= 3) ? rotation : 0;
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
}


/* Rectangulo donde se dibuja la imagen del JUEGO, expresado en PIXELES DEL
 * OVERLAY.  Lo necesita quien dibuje sobre el overlay algo que tenga que
 * alinearse con el contenido del juego (la reticula del lightgun).
 *
 * Son dos quads distintos y hay que componer sus geometrias:
 *
 *  - El quad del JUEGO ocupa g_visible_rect en pixeles de backbuffer: el
 *    aspect del core lo deja pillarboxed (4:3 en 16:9 -> 960 de 1280) o no,
 *    segun lo que reporte el core (el hack de widescreen cambia esto).
 *  - El quad del OVERLAY cubre el backbuffer ENTERO menos el overscan, y NO
 *    aplica el aspect.  Asi que el pixel px del overlay aparece en pantalla en
 *    ox + px*(bbw - 2*ox)/bbw, y hay que invertir esa relacion.
 *
 * Sin esto, la reticula se reparte sobre toda la pantalla mientras el disparo
 * se reparte sobre la imagen: coinciden en el centro y se separan hacia los
 * bordes, cada uno hacia su lado. */
void SDL_XBOX_GetGameRectOnOverlay(int *x, int *y, int *w, int *h)
{
    float bbw = (float)g_pp.BackBufferWidth;
    float bbh = (float)g_pp.BackBufferHeight;
    float ox  = (float)g_ovl_overscan_x;
    float oy  = (float)g_ovl_overscan_y;
    float span_x = bbw - 2.0f * ox;
    float span_y = bbh - 2.0f * oy;
    float left = (float)g_visible.left,  top    = (float)g_visible.top;
    float right= (float)g_visible.right, bottom = (float)g_visible.bottom;

    /* Sin juego cargado el rect esta a cero: devolver el overlay entero. */
    if (right <= left || bottom <= top) {
        *x = 0; *y = 0; *w = (int)bbw; *h = (int)bbh;
        return;
    }

    if (span_x < 1.0f || span_y < 1.0f) {   /* overscan absurdo: sin inset */
        *x = (int)left;  *y = (int)top;
        *w = (int)(right - left);
        *h = (int)(bottom - top);
        return;
    }

    *x = (int)((left   - ox) * bbw / span_x);
    *y = (int)((top    - oy) * bbh / span_y);
    *w = (int)((right  - left) * bbw / span_x);
    *h = (int)((bottom - top ) * bbh / span_y);
}

SDL_Surface* SDL_XBOX_GetOverlay(void)
{
    if (!g_ovl_surf) InitOverlay();
    return g_ovl_surf;
}

void SDL_XBOX_SetOverlayEnabled(int enabled)
{
    if (enabled && !g_ovl_surf) InitOverlay();
    g_ovl_enabled = enabled;
}

void SDL_XBOX_SetOverscan(int x, int y)
{
    g_ovl_overscan_x = x;
    g_ovl_overscan_y = y;
    if (g_ovl_vb) UpdateOverlayVertices();
}

/* =====================================================================
 * Device-lost (reset de recursos DEFAULT). Los shaders y las LUT
 * (MANAGED) sobreviven al reset.
 * =================================================================== */
static void ReleaseDefaultResources(void)
{
    g_hlslBkg.shutdown();
    if (g_game_tex) { g_game_tex->Release(); g_game_tex = NULL; }
    if (g_ovl_tex)  { g_ovl_tex->Release();  g_ovl_tex  = NULL; }
    if (g_ovl_vb)   { g_ovl_vb->Release();   g_ovl_vb   = NULL; }
    if (g_vb)       { g_vb->Release();       g_vb       = NULL; }
}

static int RecreateDefaultResources(void)
{
    /* Vertex buffer del quad principal. */
    if (FAILED(g_dev->CreateVertexBuffer(sizeof(g_verts), D3DUSAGE_WRITEONLY, VTX_FVF,
                                         D3DPOOL_DEFAULT, &g_vb, NULL)))
        return 0;

    /* Recrear SOLO la textura GPU del juego (D3DPOOL_DEFAULT). La surface CPU
       g_game_surf se CONSERVA: gameMenu->gameScreen apunta a ella y el core
       escribe ahi cada frame. NO usar CreateGameTexture() aqui, porque libera
       (SDL_FreeSurface) y recrea g_game_surf, dejando gameMenu->gameScreen
       colgando -> crash en hw_refresh (write a 0xFEEEFEEE) al volver la ventana
       de segundo plano. (Igual que hace la ruta de reset de VSync.) */
    if (g_tex_w > 0 && g_tex_h > 0) {
        D3DFORMAT fmt = (g_tex_bpp == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
        if (g_game_tex) { g_game_tex->Release(); g_game_tex = NULL; }
        if (FAILED(g_dev->CreateTexture(g_tex_w, g_tex_h, 1, D3DUSAGE_DYNAMIC,
                                        fmt, D3DPOOL_DEFAULT, &g_game_tex, NULL)))
            return 0;
        UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
        g_dev->SetTexture(0, g_game_tex);
        g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
        g_dev->SetFVF(VTX_FVF);
        XBOX_SelectEffect(g_current_effect);
    }

    /* Recrear recursos GPU del overlay (su surface CPU sobrevive al reset,
       asi gameMenu->overlay sigue apuntando a memoria valida). */
    if (g_ovl_surf)
        InitOverlay();

    /* Re-iniciar el HLSL background tras el Reset */
    g_hlslBkg.init(g_dev);

    return 1;
}

void SDL_XBOX_SetVSync(int enable)
{
    if (!g_dev) return;
    if ((enable && g_vsync) || (!enable && !g_vsync)) return; /* no change */

    if (g_cs_init) EnterCriticalSection(&g_cs);

    g_vsync = enable ? 1 : 0;
    g_pp.PresentationInterval = g_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    /* Liberar recursos GPU del HLSL background ANTES del Reset.
     * m_vb y los pixel shaders son D3DPOOL_DEFAULT y se destruyen
     * con el Reset; sin esto quedan como dangling pointers. */
    g_hlslBkg.shutdown();

    /* Liberar solo recursos GPU (D3DPOOL_DEFAULT).  NO tocamos g_game_surf
     * ni g_ovl_surf (superficies CPU) — el core mantiene punteros a ellas
     * y se romperian si las recrearamos (use-after-free). */
    if (g_game_tex) { g_game_tex->Release(); g_game_tex = NULL; }
    if (g_ovl_tex)  { g_ovl_tex->Release();  g_ovl_tex  = NULL; }
    if (g_ovl_vb)   { g_ovl_vb->Release();   g_ovl_vb   = NULL; }
    if (g_vb)       { g_vb->Release();       g_vb       = NULL; }

    if (FAILED(g_dev->Reset(&g_pp))) {
        /* Intentar recuperar recursos minimos tras fallo de Reset */
        if (g_tex_w > 0 && g_tex_h > 0) {
            D3DFORMAT fmt = (g_tex_bpp == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
            g_dev->CreateTexture(g_tex_w, g_tex_h, 1, D3DUSAGE_DYNAMIC,
                                 fmt, D3DPOOL_DEFAULT, &g_game_tex, NULL);
        }
        g_dev->CreateVertexBuffer(sizeof(g_verts), D3DUSAGE_WRITEONLY, VTX_FVF,
                                  D3DPOOL_DEFAULT, &g_vb, NULL);
        if (g_ovl_surf) InitOverlay();
        g_hlslBkg.init(g_dev);
        if (g_cs_init) LeaveCriticalSection(&g_cs);
        return;
    }

    /* Recrear vertex buffer del quad principal */
    g_dev->CreateVertexBuffer(sizeof(g_verts), D3DUSAGE_WRITEONLY, VTX_FVF,
                              D3DPOOL_DEFAULT, &g_vb, NULL);

    /* Recrear textura GPU del juego.  g_game_surf (CPU) se conserva
     * intacto, incluidos sus pixels — el core sigue escribiendo ahi
     * sin saber que la textura ha cambiado. */
    if (g_tex_w > 0 && g_tex_h > 0) {
        D3DFORMAT fmt = (g_tex_bpp == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
        g_dev->CreateTexture(g_tex_w, g_tex_h, 1, D3DUSAGE_DYNAMIC,
                             fmt, D3DPOOL_DEFAULT, &g_game_tex, NULL);
    }

    /* Restaurar estado D3D (todo se pierde tras Reset) */
    if (g_game_tex) g_dev->SetTexture(0, g_game_tex);
    if (g_vb) {
        g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
        g_dev->SetFVF(VTX_FVF);
    }
    XBOX_SelectEffect(g_current_effect);
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);

    /* Recrear recursos GPU del overlay (g_ovl_surf preservado) */
    if (g_ovl_surf) InitOverlay();

    /* Re-iniciar el HLSL background tras el Reset */
    g_hlslBkg.init(g_dev);

    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

    if (g_cs_init) LeaveCriticalSection(&g_cs);
}

static int HandleDeviceLost(void)
{
    HRESULT hr = g_dev->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) return 0;          /* aun no recuperable */
    if (hr == D3DERR_DEVICENOTRESET) {
        ReleaseDefaultResources();
        if (FAILED(g_dev->Reset(&g_pp))) return 0;
        return RecreateDefaultResources();
    }
    return SUCCEEDED(hr);
}

/* =====================================================================
 * Present (sustituye a SDL_Flip en PC) - port de XBOX_RenderSurface.
 * =================================================================== */
void WinD3D9_Present(void)
{
    D3DLOCKED_RECT lr;
    HRESULT hr;

    if (!g_dev || !g_game_tex || !g_game_surf) return;

    if (g_cs_init) EnterCriticalSection(&g_cs);

    /* Subir el frame del core (g_game_surf) a la textura dinamica. Copiamos
       exactamente w*bpp por fila (los pitches de origen y destino pueden
       diferir por padding). */
    if (SUCCEEDED(g_game_tex->LockRect(0, &lr, NULL, D3DLOCK_DISCARD))) {
        int y;
        int rb = g_game_surf->w * g_game_surf->format->BytesPerPixel;
        if (rb > lr.Pitch) rb = lr.Pitch;
        for (y = 0; y < g_game_surf->h; y++)
            memcpy((unsigned char*)lr.pBits + y * lr.Pitch,
                   (unsigned char*)g_game_surf->pixels + y * g_game_surf->pitch, rb);
        g_game_tex->UnlockRect(0);
    }

    if (g_ovl_enabled) UploadOverlay();

    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

    if (SUCCEEDED(g_dev->BeginScene())) {
        g_dev->SetTexture(0, g_game_tex);
        g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
        g_dev->SetFVF(VTX_FVF);
        DrawMainQuad();
        if (g_hlslBkg_active) g_hlslBkg.draw();
        DrawOverlay();
        /* g_hlslBkg_active es estado RETENIDO (lo fija el frontend en las
         * transiciones de estado / arranque / callback del menu); la capa de
         * render solo lo LEE, ya no lo resetea por-frame. */
        g_dev->EndScene();
    }

    hr = g_dev->Present(NULL, NULL, NULL, NULL);
    if (hr == D3DERR_DEVICELOST)
        HandleDeviceLost();

    if (g_cs_init) LeaveCriticalSection(&g_cs);
}

/* =====================================================================
 * Init / Shutdown
 * =================================================================== */
int WinD3D9_Init(HWND hwnd, int bbw, int bbh)
{
    D3DDISPLAYMODE dm;
    D3DCAPS9 caps;

    if (g_dev) return 1; /* ya inicializado */
    if (!hwnd || bbw <= 0 || bbh <= 0) return 0;

    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) return 0;

    if (FAILED(g_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm)))
        dm.Format = D3DFMT_X8R8G8B8;

    if (SUCCEEDED(g_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps))) {
        if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0)) {
            OutputDebugStringA("WinD3D9: GPU sin soporte ps_3_0\n");
            g_d3d->Release(); g_d3d = NULL;
            return 0;
        }
    }

    ZeroMemory(&g_pp, sizeof(g_pp));
    g_pp.Windowed             = TRUE;
    g_pp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    g_pp.BackBufferWidth      = bbw;
    g_pp.BackBufferHeight     = bbh;
    g_pp.BackBufferFormat     = dm.Format;
    g_pp.hDeviceWindow        = hwnd;
    g_pp.EnableAutoDepthStencil = FALSE;
    g_pp.PresentationInterval = g_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    

    /* MULTITHREADED: el watcher thread (th_printLoading) puede llamar a
       WinD3D9_Present. FPU_PRESERVE: que D3D no cambie la FPU a precision
       simple y rompa el timing/matematicas del resto de Salvia. */
    if (FAILED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
            &g_pp, &g_dev))) {
        g_d3d->Release(); g_d3d = NULL;
        return 0;
    }

    g_bbw = bbw; g_bbh = bbh;

    if (!g_cs_init) { InitializeCriticalSection(&g_cs); g_cs_init = 1; }

    /* Vertex buffer del quad principal. */
    if (FAILED(g_dev->CreateVertexBuffer(sizeof(g_verts), D3DUSAGE_WRITEONLY, VTX_FVF,
                                         D3DPOOL_DEFAULT, &g_vb, NULL))) {
        WinD3D9_Shutdown();
        return 0;
    }

    g_dev->SetVertexShader(NULL);   /* pipeline de vertices fijo */
    g_dev->SetFVF(VTX_FVF);
    g_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_ZENABLE,  FALSE);

    InitShaders();
    g_hlslBkg.init(g_dev);

    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    g_dev->Present(NULL, NULL, NULL, NULL);
    return 1;
}

void WinD3D9_Shutdown(void)
{
    if (g_cs_init) EnterCriticalSection(&g_cs);

    DestroyOverlay();
    DestroyGameTexture();
    g_hlslBkg.shutdown();
    if (g_vb) { g_vb->Release(); g_vb = NULL; }
    DestroyShaders();
    if (g_dev) { g_dev->Release(); g_dev = NULL; }
    if (g_d3d) { g_d3d->Release(); g_d3d = NULL; }

    if (g_cs_init) {
        LeaveCriticalSection(&g_cs);
        DeleteCriticalSection(&g_cs);
        g_cs_init = 0;
    }
}

#endif /* WIN */
