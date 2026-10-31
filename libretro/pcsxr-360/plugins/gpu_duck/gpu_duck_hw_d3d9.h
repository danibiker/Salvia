/*
 * gpu_duck_hw_d3d9.h  --  backend hardware Direct3D 9 de SwanStation
 *                         ("SwanStation HW") para pcsxr-360.
 *
 * [2026-10-30] Fase 1: Windows (PCSXR360_WIN32), x1, sin filtros.
 * Fase 2: Xbox 360 (_XBOX): EDRAM + Resolve; ver la cabecera del .cpp.
 * El diseno y las decisiones estan en HW_D3D9_FASE0.txt (misma carpeta).
 *
 * Hereda de GPUBackend igual que GPU_SW_Backend: gpu_duck_driver.cpp le
 * pasa los mismos comandos (DrawPolygon/Rectangle/Line, Fill/Copy VRAM).
 * Lo que NO pasa por el driver (LoadImage/StoreImage de xbox_soft, el
 * savestate y la salida a pantalla) entra por la API C de
 * gpu_duck_c_api.h (duck_hw_*).
 */
#pragma once

#include "gpu_duck_gpu_backend.h"

#if defined(PCSXR360_WIN32) || defined(_XBOX)
#define DUCK_HW_D3D9 1
#else
#define DUCK_HW_D3D9 0
#endif

#if DUCK_HW_D3D9

#if defined(_XBOX)
#include <xtl.h>
#else
#include <d3d9.h>
#endif

class GPU_HW_D3D9_Backend : public GPUBackend
{
  friend class GPU_Duck_Driver;

public:
  explicit GPU_HW_D3D9_Backend(u16* psx_vram);
  virtual ~GPU_HW_D3D9_Backend();

  virtual bool Initialize(bool force_thread);
  virtual void Reset(bool clear_vram);
  virtual void Shutdown();

  /* --- Entradas que no vienen del driver (API C duck_hw_*) --------- */

  /* psxVuw -> GPU.  `use_mask` = aplicar el set/check de mascara vigente
   * (LoadImage); 0 para cargas de estado. */
  void UploadFromCPU(u32 x, u32 y, u32 w, u32 h, bool use_mask);
  /* GPU -> psxVuw. */
  void ReadbackToCPU(u32 x, u32 y, u32 w, u32 h);
  /* Pasada de salida: contenido de la VRAM (vram_x, vram_y) de cw x ch,
   * colocado en (left, top) de una imagen de out_w x out_h. */
  void Present(s32 vram_x, s32 vram_y, s32 cw, s32 ch, s32 left, s32 top,
               s32 out_w, s32 out_h, bool rgb24);

  /* retro_run: entre begin y end todo el trabajo se acumula y el estado
   * del frontend se devuelve en end.  Fuera de ese par, cada entrada se
   * liga y se desliga sola. */
  void FrameBegin();
  void FrameEnd();

  /* Imagen para el frontend.  Devuelve true si este frame hubo Present. */
  bool TakeFrame(IDirect3DTexture9** tex, u32* w, u32* h, bool* changed);
  /* Antes de retro_unload_game: suelta la textura de salida. */
  void ReleaseOutput();
  /* L3+R3: volcar el frame siguiente (comandos al log, VRAM a BMP). */
  void ArmDump() { m_dump_armed = true; }

protected:
  virtual void FillVRAM(u32 x, u32 y, u32 width, u32 height, u32 color, GPUBackendCommandParameters params);
  virtual void UpdateVRAM(u32 x, u32 y, u32 width, u32 height, const void* data, GPUBackendCommandParameters params);
  virtual void CopyVRAM(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 width, u32 height,
                        GPUBackendCommandParameters params);
  virtual void DrawPolygon(const GPUBackendDrawPolygonCommand* cmd);
  virtual void DrawRectangle(const GPUBackendDrawRectangleCommand* cmd);
  virtual void DrawLine(const GPUBackendDrawLineCommand* cmd);
  virtual void FlushRender();

private:
  struct HWVertex
  {
    float x, y, z, w;
    D3DCOLOR color;
    float u, v;
    float tp_x, tp_y, clut_x, clut_y;
    float lim_u0, lim_v0, lim_u1, lim_v1;   /* [fase 3] rango de u/v del poligono (filtros) */
    float mode;                              /* [rendimiento] 0 sin textura, 1 = 4 bits, 2 = 8 bits, 3 = directa */
  };

  struct BatchKey
  {
    int texmode;        /* 1 = con textura en un lote con transparencia; si no 0 (modo y raw en el vertice) */
    int raw;
    int transparency;   /* 0..3, 4 = sin transparencia */
    int dithering;
    int set_mask;
    int check_mask;
    int interlaced;
    int field;
    int win_and_x, win_and_y, win_or_x, win_or_y;
  };

  struct DirtyRect
  {
    u32 left, top, right, bottom;   /* right/bottom exclusivos */
    bool valid;
  };

  enum { MAX_BATCH_VERTICES = 16383 * 3 };
  enum { HW_UPLOAD_RING = 8, HW_VB_COUNT = 3 };   /* Xbox */
  enum { MAX_DEPTH_COUNTER = 65535 - 2 };

  /* --- recursos ---------------------------------------------------- */
  bool CreateResources();
  void DestroyResources();
  bool CompileShaders();
  void DestroyShaders();
  bool CompileBatchShaders(int true_color, int filter);
  void DestroyBatchShaders();
  void ApplyRenderOptions();
  IDirect3DPixelShader9*  CompilePS(const char* src, const char* defines_texmode, const char* defines_raw,
                                    int true_color = 0, int filter = 0);
  IDirect3DVertexShader9* CompileVS(const char* src);

  /* --- ligado al device -------------------------------------------- */
  void Bind();
  void Unbind();
  void EndOutsideFrame();
  void ApplyFixedStates();
  void SetVRAMTarget();

  /* --- lotes ------------------------------------------------------- */
  float BeginPrimitive(const GPUBackendDrawCommand* cmd, bool textured, int texmode, bool raw, int transparency,
                       bool dithering);
  void  CheckTexturePage(int texmode, u32 tpx, u32 tpy, u32 clutx, u32 cluty);
  void  SetUVLimits(u32 min_u, u32 max_u, u32 min_v, u32 max_v);
  /* Dithering del juego Y de la opcion; nunca con true color (gpu_hw.cpp:1380). */
  bool  DitherOn(const GPUBackendDrawCommand* cmd) const;
  bool  TouchesTexturePage(u32 left, u32 top, u32 right, u32 bottom) const;
  void  EnsureSpace(u32 nverts);
  void  AddVertex(float x, float y, float z, u32 color, float u, float v, const float* tp, float w = 1.0f);
  void  DrawLineQuad(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float depth);
  void  DrawBatch(int render_mode);
  void  UpdateScissor();
  void  IncludeDirty(u32 left, u32 top, u32 right, u32 bottom);
  void  RefreshReadTexture();
  void  ResetDepthCounter();
  float CurrentDepth() const { return 1.0f - (static_cast<float>(m_depth_counter) / 65535.0f); }
  float NextMaskedDepth(bool check_mask);

  /* --- utilidades -------------------------------------------------- */
  void DrawUtilRect(float x0, float y0, float x1, float y1, float rt_w, float rt_h);
  void UploadRegion(u32 x, u32 y, u32 w, u32 h, bool set_mask, bool check_mask, float depth);
  static u16 ArgbTo1555(D3DCOLOR p);

#if defined(_XBOX)
  /* --- Xbox 360 (fase 2) ------------------------------------------- */
  bool CreateResourcesXbox();
  void RotateVB();
  void RestoreEDRAM();
  void RestoreFrontendStates();
#endif

  IDirect3DDevice9* m_dev;

  IDirect3DTexture9* m_vram_tex;     /* render target: la VRAM de la PSX */
  IDirect3DSurface9* m_vram_surf;
  IDirect3DTexture9* m_read_tex;     /* copia muestreable (las lotes leen de aqui) */
  IDirect3DSurface9* m_read_surf;
  IDirect3DSurface9* m_depth;        /* D24S8: el bit de mascara */
  IDirect3DTexture9* m_upload_tex;   /* A1R5G5B5 dinamica: psxVuw tal cual */
  IDirect3DTexture9* m_dither_tex;   /* 4x4, desplazamiento de la matriz + 4 */
  IDirect3DSurface9* m_sys_surf;     /* SYSTEMMEM para leer la VRAM */

  IDirect3DTexture9* m_out_tex;      /* la que ve Salvia (env 30003) */
  u32 m_out_w, m_out_h;
  bool m_out_changed;
  bool m_frame_ready;
  IDirect3DTexture9* m_out_graveyard[4];

  IDirect3DVertexDeclaration9* m_decl_batch;
  IDirect3DVertexDeclaration9* m_decl_util;
  IDirect3DVertexShader9* m_vs_batch;
  IDirect3DVertexShader9* m_vs_util;
  IDirect3DPixelShader9*  m_ps_batch[2][2];   /* [con textura][raw], de la variante m_tc / m_filter */
  IDirect3DPixelShader9*  m_ps_upload;
  IDirect3DPixelShader9*  m_ps_copy;
  IDirect3DPixelShader9*  m_ps_depth;
  IDirect3DPixelShader9*  m_ps_fill;    /* [Xbox] relleno de VRAM con un quad (no Clear) */
  IDirect3DPixelShader9*  m_ps_disp15;
  IDirect3DPixelShader9*  m_ps_disp24;
  int m_tc;              /* [fase 3] true color con el que estan compilados los lotes */
  int m_req_tc, m_req_filter;   /* lo ultimo pedido por las opciones (no reintentar cada frame) */
  int m_filter;          /* [fase 3] filtro de texturas: 0 nearest, 1 bilineal, 2 JINC2, 3 xBR */
  float m_uvlim[4];      /* rango de u/v del primitivo en curso (AddVertex lo copia) */
  float m_vmode;         /* modo de textura + 4 * raw del primitivo en curso (AddVertex lo copia) */
  bool  m_batch_tex;     /* el lote en curso tiene algun primitivo con textura */

  /* estado del frontend mientras estamos ligados */
  bool m_bound;
  bool m_in_frame;
  bool m_in_scene;
  IDirect3DStateBlock9* m_sb;
  IDirect3DSurface9* m_saved_rt;
  IDirect3DSurface9* m_saved_ds;

  /* lote en curso */
  HWVertex* m_verts;
  u32 m_nverts;
  BatchKey m_batch;
  bool m_batch_valid;
  RECT m_scissor;
  bool m_scissor_valid;

  u32  m_depth_counter;
  DirtyRect m_dirty;
  u32  m_last_tp_key;
  bool m_tp_key_valid;
  GPUBackendCommandParameters m_last_params;

  /* --- trazas [DUCK-HW] -------------------------------------------- */
  DWORD m_owner_thread;      /* hilo que creo el backend (el de retro_run) */
  bool  m_warned_thread;
  bool  m_warned_draw;
  u32   m_st_frames, m_st_prims, m_st_verts, m_st_draws, m_st_presents;
  u32   m_st_uploads, m_st_readbacks, m_st_refresh, m_st_fills, m_st_copies;
  u32   m_presents_logged;
  u32   m_presents_total;
  bool  m_warned_util;
  u32   m_log_polys, m_log_uploads, m_log_fills;
  void  LogStats();
  void  DiagDump(IDirect3DSurface9* out_surf, s32 vram_x, s32 vram_y, s32 cw, s32 ch);

  /* --- Xbox 360 (en PC se quedan a cero) ---------------------------- */
  IDirect3DSurface9*      m_out_rt;      /* salida en la EDRAM, HW_OUT_MAX */
  IDirect3DTexture9*      m_sys_tex;     /* LIN_ para leer la VRAM por CPU */
  IDirect3DTexture9*      m_upload_ring[HW_UPLOAD_RING];
  u32                     m_upload_idx;
  IDirect3DVertexBuffer9* m_vb[HW_VB_COUNT];
  HWVertex*               m_vb_data;     /* el del anillo, bloqueado */
  u32                     m_vb_idx, m_vb_used, m_vb_start;
  bool                    m_edram_valid; /* la EDRAM tiene la VRAM (desde Bind) */

  /* --- medidor de tiempos [DUCK-HW/PERF] (DUCK_HW_PERF) --------------- */
  enum { PF_RUN, PF_GAP, PF_FLUSH, PF_VBLOCK, PF_UPLOAD, PF_UPLOCK, PF_RESOLVE, PF_PRESENT, PF_RESTORE,
         PF_READBACK, PF_IDLE, PF_PRIM, PF_COUNT };
  u64  m_pf[PF_COUNT];
  u64  m_pf_frame_begin, m_pf_frame_end;
  void LogPerf(u32 frames);

  /* --- cache del pipeline de los lotes (rendimiento) ------------------ */
  bool                    m_pipe_valid;   /* decl/VS/texturas/blend fijos puestos */
  IDirect3DPixelShader9*  m_cur_ps;
  IDirect3DVertexBuffer9* m_cur_vb;
  float                   m_cur_psc[20];
  bool                    m_cur_psc_valid;
  u32                     m_cur_blend, m_cur_bf, m_cur_bop, m_cur_zfunc;

  /* --- [traza] por que se corta cada lote ----------------------------- */
  enum { BRK_TEX, BRK_TRANSP, BRK_BF, BRK_DITH, BRK_MASK, BRK_ILACE, BRK_WIN, BRK_OP, BRK_SCISSOR, BRK_PAGE,
         BRK_FULL, BRK_END, BRK_OTHER, BRK_COUNT };
  u32 m_brk[BRK_COUNT];
  int m_flush_reason;
  s32 m_lp[9];   /* ultima geometria de salida registrada */

  /* --- Xbox: fences de los anillos (el Lock no espera a la GPU) -------- */
  DWORD m_upload_fence[HW_UPLOAD_RING];
  bool  m_upload_fence_ok[HW_UPLOAD_RING];
  DWORD m_vb_fence[HW_VB_COUNT];
  bool  m_vb_fence_ok[HW_VB_COUNT];
  u32   m_st_upload_waits;

  /* --- [traza] desglose de comandos ------------------------------------ */
  enum { CT_TEX0, CT_TEX1, CT_TEX2, CT_TEX3, CT_SEMI0, CT_SEMI1, CT_SEMI2, CT_SEMI3, CT_RAW, CT_DITH, CT_SETMASK,
         CT_CHKMASK, CT_ILACE, CT_WIN, CT_RECT, CT_RECT_THIN, CT_LINE, CT_COPY_THIN, CT_COUNT };
  u32 m_ct[CT_COUNT];
  u32 m_log_copies, m_log_readbacks;

  /* pagina de textura y CLUT vigentes (CheckTexturePage / IncludeDirty) */
  int m_tp_mode;
  u32 m_tp_x, m_tp_y, m_clut_x, m_clut_y;

  /* --- [traza] volcado de un frame (L3+R3) ----------------------------- */
  bool m_dump_armed, m_dump_active;
  u32  m_dump_lines, m_dump_seq, m_dump_left;
  bool Dumping();
  void DumpVRAM(const char* when);
};

#endif /* DUCK_HW_D3D9 */
