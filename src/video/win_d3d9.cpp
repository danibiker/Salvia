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

/* HLSL compartido con la rama Xbox (single source of truth). */
#include "../../libs/libSDLx360/SDL/src/video/xbox/SDL_shaders_src.h"
/* LUTs de HQ2x/HQ3x/HQ4x (datos embebidos, neutros de plataforma). */
#include "../../libs/libSDLx360/SDL/src/video/xbox/hq2x_lut.h"
#include "../../libs/libSDLx360/SDL/src/video/xbox/hq3x_lut.h"
#include "../../libs/libSDLx360/SDL/src/video/xbox/hq4x_lut.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* 0=Nearest,1=Sharp-Bilinear,2=LCD3x,3=Scanlines,4=CRT-Geom,5=CRT-Lottes,
   6=CRT-Easymode,7=HQ2x,8=HQ3x,9=HQ4x,10=xBR-lv2-fast,11=5xBR-Hyllian */
#define NUM_EFFECTS 12

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

static LPDIRECT3DPIXELSHADER9 g_shaders[NUM_EFFECTS] = { NULL };
static LPDIRECT3DTEXTURE9     g_hq2x_lut   = NULL;       /* MANAGED (sobrevive a reset) */
static LPDIRECT3DTEXTURE9     g_hq3x_lut   = NULL;
static LPDIRECT3DTEXTURE9     g_hq4x_lut   = NULL;

static int                    g_current_effect = 0;
static D3DTEXTUREFILTERTYPE   g_current_filter = D3DTEXF_LINEAR;
static float                  g_aspect     = 0.0f;       /* 0 = ratio nativo */
static int                    g_fullscreen = 1;          /* 1 = fill, 0 = pixel-perfect */
static int                    g_overflow   = 0;          /* 1 = integer scale puede salirse de pantalla */
static int                    g_rotation   = 0;          /* 0..3 (libretro) */
static RECT                   g_visible    = { 0, 0, 0, 0 };

/* Overlay ARGB (1 capa sobre el quad del juego). */
static LPDIRECT3DTEXTURE9      g_ovl_tex    = NULL;       /* DYNAMIC, DEFAULT */
static LPDIRECT3DVERTEXBUFFER9 g_ovl_vb     = NULL;
static SDL_Surface*            g_ovl_surf   = NULL;       /* fuente CPU (bb res) */
static int                     g_ovl_enabled = 0;

static CRITICAL_SECTION        g_cs;
static int                     g_cs_init    = 0;

/* UVs del "single fullscreen triangle" por rotacion (identico a Xbox). */
static const float g_uv_rot[4][6] = {
    /* 0:   0 deg  */ { 0.0f, 0.0f,  2.0f, 0.0f,  0.0f, 2.0f },
    /* 1:  90 CCW  */ { 1.0f, 0.0f,  1.0f, 2.0f, -1.0f, 0.0f },
    /* 2: 180      */ { 1.0f, 1.0f, -1.0f, 1.0f,  1.0f,-1.0f },
    /* 3: 270 CCW  */ { 0.0f, 1.0f,  0.0f,-1.0f,  2.0f, 1.0f },
};

/* =====================================================================
 * Shaders
 * =================================================================== */

static HRESULT CreateShader(const char* src, LPDIRECT3DPIXELSHADER9* target, DWORD flags)
{
    ID3DXBuffer* code  = NULL;
    ID3DXBuffer* errs  = NULL;
    HRESULT hr;

    if (!src || !target) return E_INVALIDARG;
    *target = NULL;

    hr = D3DXCompileShader(src, (UINT)strlen(src), NULL, NULL, "main", "ps_3_0",
                           flags, &code, &errs, NULL);
    if (FAILED(hr)) {
        if (errs) {
            OutputDebugStringA((const char*)errs->GetBufferPointer());
            errs->Release();
        }
        return hr;
    }
    hr = g_dev->CreatePixelShader((const DWORD*)code->GetBufferPointer(), target);
    code->Release();
    if (errs) errs->Release();
    return hr;
}

/* LUT BGRA (del extractor .NET) -> D3DFMT_A8R8G8B8.
   PC es little-endian: A8R8G8B8 se almacena en memoria como B,G,R,A, que
   coincide con el orden de los datos .NET -> memcpy directo, SIN swap
   (en Xenon, big-endian, si habia que reordenar). */
static LPDIRECT3DTEXTURE9 CreateLUT(const unsigned char* data, int w, int h)
{
    LPDIRECT3DTEXTURE9 tex = NULL;
    D3DLOCKED_RECT lr;
    int y;

    if (FAILED(g_dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8,
                                    D3DPOOL_MANAGED, &tex, NULL)) || !tex)
        return NULL;

    if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return NULL; }
    for (y = 0; y < h; y++)
        memcpy((unsigned char*)lr.pBits + y * lr.Pitch, data + y * w * 4, (size_t)w * 4);
    tex->UnlockRect(0);
    return tex;
}

static void InitLUTs(void)
{
    if (!g_hq2x_lut) g_hq2x_lut = CreateLUT(hq2x_lut_data, HQ2X_LUT_WIDTH, HQ2X_LUT_HEIGHT);
    if (!g_hq3x_lut) g_hq3x_lut = CreateLUT(hq3x_lut_data, HQ3X_LUT_WIDTH, HQ3X_LUT_HEIGHT);
    if (!g_hq4x_lut) g_hq4x_lut = CreateLUT(hq4x_lut_data, HQ4X_LUT_WIDTH, HQ4X_LUT_HEIGHT);
}

static void DestroyLUTs(void)
{
    if (g_hq2x_lut) { g_hq2x_lut->Release(); g_hq2x_lut = NULL; }
    if (g_hq3x_lut) { g_hq3x_lut->Release(); g_hq3x_lut = NULL; }
    if (g_hq4x_lut) { g_hq4x_lut->Release(); g_hq4x_lut = NULL; }
}

static void InitShaders(void)
{
    if (g_shaders[0] != NULL) return; /* ya compilados */

    InitLUTs();

    CreateShader(g_strShaderNormalSource,            &g_shaders[0],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderSharpBilinearSource,     &g_shaders[1],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderLCDGridSource,           &g_shaders[2],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderScanlinesSource,         &g_shaders[3],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderCRTSource,               &g_shaders[4],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderCRTLottesSource,         &g_shaders[5],  PS_FLAGS_FULL_PRECISION);
    CreateShader(g_strShaderCRTEasymodeSource,       &g_shaders[6],  PS_FLAGS_FULL_PRECISION);
    CreateShader(g_strShaderHQ2xSource,              &g_shaders[7],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderHQ3xSource,              &g_shaders[8],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderHQ4xSource,              &g_shaders[9],  PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderXBRlv2FastSource,        &g_shaders[10], PS_FLAGS_DEFAULT);
    CreateShader(g_strShaderXBRHyllianRoundedSource, &g_shaders[11], PS_FLAGS_FULL_PRECISION);
}

static void DestroyShaders(void)
{
    int i;
    for (i = 0; i < NUM_EFFECTS; i++)
        if (g_shaders[i]) { g_shaders[i]->Release(); g_shaders[i] = NULL; }
    DestroyLUTs();
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
    switch (g_current_effect) {
//		case 7:  return 2; /* HQ2x */
//		case 8:  return 3; /* HQ3x */
//		case 9:  return 4; /* HQ4x */
//		case 10: return 3; /* xBR-lv2-fast */
//		case 11: return 3; /* 5xBR-Hyllian (rendered at 3x via HQ3x infra) */
		default: return 1; /* 0=Nearest, 1=Sharp-Bilinear, 2=LCD-Grid-v2,
		                      3=Scanlines, 4=CRT-Geom, 5=CRT-Lottes, 6=CRT-Easymode */
    }
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
        if (aspect_ratio > 0.0f && tex_w > 0 && tex_h > 0) {
            if (scale == 1 || g_overflow) {
                int mh = (int)floor(bbh / (float)tex_h);
                int mw = (int)floor(bbw / ((float)tex_h * aspect_ratio));
                int ms = g_overflow ? max(mh, mw) : min(mh, mw);
                if (ms > 1) scale = ms;
            }
            display_h = (float)(tex_h * scale);
            display_w = (float)floor(display_h * aspect_ratio);
        } else {
            if ((scale == 1 || g_overflow) && tex_w > 0 && tex_h > 0) {
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
    if (!g_dev || !g_shaders[0]) return;
    if (effectID < 0 || effectID >= NUM_EFFECTS) effectID = 0;
    g_current_effect = effectID;

    g_dev->SetTexture(1, NULL); /* desvincula LUT por defecto */

    switch (effectID) {
    case 0:
        g_dev->SetPixelShader(g_shaders[0]);
        SetSampler0Filter(D3DTEXF_POINT);
        break;
    case 1: {
        float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
        g_dev->SetPixelShader(g_shaders[1]);
        g_dev->SetPixelShaderConstantF(1, dims, 1);
        SetSampler0Filter(D3DTEXF_LINEAR);
        break;
    }
    case 2: {
        if (!g_shaders[2]) { g_dev->SetPixelShader(g_shaders[0]); SetSampler0Filter(D3DTEXF_LINEAR); }
        else {
            float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
            g_dev->SetPixelShader(g_shaders[2]);
            g_dev->SetPixelShaderConstantF(1, dims, 1);
            SetSampler0Filter(D3DTEXF_POINT);
        }
        break;
    }
    case 3: {
        float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
        g_dev->SetPixelShader(g_shaders[3]);
        g_dev->SetPixelShaderConstantF(1, dims, 1);
        SetSampler0Filter(D3DTEXF_POINT);
        break;
    }
    case 4: {
        float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
        g_dev->SetPixelShader(g_shaders[4]);
        g_dev->SetPixelShaderConstantF(1, dims, 1);
        SetSampler0Filter(D3DTEXF_LINEAR);
        break;
    }
    case 5:
    case 6: {
        if (!g_shaders[effectID]) { g_dev->SetPixelShader(g_shaders[0]); SetSampler0Filter(D3DTEXF_LINEAR); }
        else {
            float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
            g_dev->SetPixelShader(g_shaders[effectID]);
            g_dev->SetPixelShaderConstantF(1, dims, 1);
            SetSampler0Filter(D3DTEXF_POINT);
            g_dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            g_dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        }
        break;
    }
    case 7:
    case 8:
    case 9: {
        if (!g_shaders[effectID]) { g_dev->SetPixelShader(g_shaders[0]); SetSampler0Filter(D3DTEXF_LINEAR); }
        else {
            float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
            LPDIRECT3DTEXTURE9 lut = (effectID == 7) ? g_hq2x_lut :
                                     (effectID == 8) ? g_hq3x_lut : g_hq4x_lut;
            g_dev->SetPixelShader(g_shaders[effectID]);
            g_dev->SetPixelShaderConstantF(1, dims, 1);
            SetSampler0Filter(D3DTEXF_POINT);
            if (lut) {
                g_dev->SetTexture(1, lut);
                g_dev->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                g_dev->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                g_dev->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                g_dev->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            }
        }
        break;
    }
    case 10:
    case 11: {
        if (!g_shaders[effectID]) { g_dev->SetPixelShader(g_shaders[0]); SetSampler0Filter(D3DTEXF_LINEAR); }
        else {
            float dims[4] = { (float)g_tex_w, (float)g_tex_h, 0, 0 };
            g_dev->SetPixelShader(g_shaders[effectID]);
            g_dev->SetPixelShaderConstantF(1, dims, 1);
            SetSampler0Filter(D3DTEXF_POINT);
        }
        break;
    }
    default:
        g_dev->SetPixelShader(g_shaders[0]);
        SetSampler0Filter(D3DTEXF_POINT);
        break;
    }

    if (!g_fullscreen && g_tex_w > 0)
        UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
}

/* =====================================================================
 * Overlay (capa ARGB sobre el quad del juego) - port de XBOX_*Overlay.
 * =================================================================== */
/* Idempotente por recurso: crea solo lo que falte. El surface CPU del
   overlay persiste a traves de device-lost; la textura/VB (DEFAULT) se
   recrean en el reset llamando otra vez aqui. */
static void InitOverlay(void)
{
    void* locked;
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
        VTX ov[4];
        if (FAILED(g_dev->CreateVertexBuffer(sizeof(ov), D3DUSAGE_WRITEONLY, VTX_FVF,
                                             D3DPOOL_DEFAULT, &g_ovl_vb, NULL)))
            return;
        ov[0].x = -0.5f;      ov[0].y = bbh - 0.5f; ov[0].z = 0; ov[0].rhw = 1; ov[0].u = 0; ov[0].v = 1;
        ov[1].x = -0.5f;      ov[1].y = -0.5f;      ov[1].z = 0; ov[1].rhw = 1; ov[1].u = 0; ov[1].v = 0;
        ov[2].x = bbw - 0.5f; ov[2].y = bbh - 0.5f; ov[2].z = 0; ov[2].rhw = 1; ov[2].u = 1; ov[2].v = 1;
        ov[3].x = bbw - 0.5f; ov[3].y = -0.5f;      ov[3].z = 0; ov[3].rhw = 1; ov[3].u = 1; ov[3].v = 0;
        if (SUCCEEDED(g_ovl_vb->Lock(0, 0, (void**)&locked, 0))) {
            memcpy(locked, ov, sizeof(ov));
            g_ovl_vb->Unlock();
        }
    }
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
    g_dev->SetPixelShader(g_shaders[0]);
    g_dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    g_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    /* Restaurar lo que toco el overlay (textura+vb+shader+filtro del s0). */
    g_dev->SetTexture(0, g_game_tex);
    g_dev->SetStreamSource(0, g_vb, 0, sizeof(VTX));
    g_dev->SetPixelShader(g_shaders[g_current_effect]);
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

void SDL_XBOX_SetDisplayOverflow(int overflow)
{
    g_overflow = overflow;
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
}

void SDL_XBOX_SetRotation(int rotation)
{
    g_rotation = (rotation >= 0 && rotation <= 3) ? rotation : 0;
    UpdateVertexBuffer(g_tex_w, g_tex_h, g_aspect);
    if (g_dev) g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
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

/* =====================================================================
 * Device-lost (reset de recursos DEFAULT). Los shaders y las LUT
 * (MANAGED) sobreviven al reset.
 * =================================================================== */
static void ReleaseDefaultResources(void)
{
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

    /* Recrear textura del juego al tamano vigente. */
    if (g_tex_w > 0 && g_tex_h > 0) {
        if (!CreateGameTexture(g_tex_w, g_tex_h, g_tex_bpp)) return 0;
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
    return 1;
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
        DrawOverlay();
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
    g_pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; /* vsync (como Xbox sin NOVSYNC) */

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

    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    g_dev->Present(NULL, NULL, NULL, NULL);
    return 1;
}

void WinD3D9_Shutdown(void)
{
    if (g_cs_init) EnterCriticalSection(&g_cs);

    DestroyOverlay();
    DestroyGameTexture();
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
