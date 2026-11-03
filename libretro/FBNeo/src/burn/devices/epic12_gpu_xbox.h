// Xenos offscreen compositor for EPIC12. All D3D access shares SDL's Present
// lock. A batch ends with one readback; textures are cached across batches.
#ifndef FBNEO_EPIC12_GPU_XBOX_H
#define FBNEO_EPIC12_GPU_XBOX_H
#include <xtl.h>
#include <d3dx9.h>
#include <xgraphics.h>
#include "epic12_gpu_untile.h"
#include "epic12_gpu_region.h"
#include "epic12_gpu_cache.h"
#include "epic12_gpu_alpha.h"
#include "epic12_gpu_tile.h"
#include "epic12_gpu_tile_alpha.h"

// [Salvia] liujiny declaraba aqui siete funciones SDL_XBOX_*CoreGpu* que el
// anadio a libSDLx360.  Salvia no pone codigo especifico de un core en el
// frontend: el device llega por RETRO_ENVIRONMENT_SALVIA_GET_D3D_DEVICE y el
// lock es del propio core (src/burner/libretro/retro_core_gpu.*).  Se
// conservan los nombres originales como envoltorios locales para no tocar el
// resto del fichero.  Sin presentacion asincrona en el frontend: el resultado
// siempre se lee de vuelta al bitmap del core y sale por video_cb.  [Salvia]
// Tambien se han quitado los contadores, tiempos y el log de diagnostico.
#include "retro_core_gpu.h"
static inline void *SDL_XBOX_AcquireCoreGpu(unsigned *generation) { return RetroCoreGpuAcquire(generation); }
static inline void SDL_XBOX_ReleaseCoreGpu(void) { RetroCoreGpuRelease(); }

static D3DDevice *epic12_xenos_device;
static D3DTexture *epic12_xenos_atlas, *epic12_xenos_input, *epic12_xenos_result;
static D3DSurface *epic12_xenos_target;
static D3DVertexShader *epic12_xenos_vs, *epic12_xenos_uniform_vs;
static D3DPixelShader *epic12_xenos_ps, *epic12_xenos_uniform_ps, *epic12_xenos_copy_ps;
static D3DPixelShader *epic12_xenos_fast_ps, *epic12_xenos_feedback_ps;
static D3DVertexDeclaration *epic12_xenos_decl, *epic12_xenos_uniform_decl;
static D3DStateBlock *epic12_xenos_state;
static unsigned epic12_xenos_generation;
static Epic12GpuVertex epic12_xenos_vertices[EPIC12_GPU_DRAW_RECTS * 3];
struct Epic12GpuUniformVertex { float x, y, u, v; };
static Epic12GpuUniformVertex epic12_xenos_uniform_vertices[EPIC12_GPU_DRAW_RECTS * 3];
static volatile LONG epic12_xenos_notice;
static int epic12_xenos_status, epic12_xenos_error;
static bool epic12_xenos_attributes;
static bool epic12_xenos_specialized, epic12_xenos_fast_transfer;
static bool epic12_xenos_alpha_trim, epic12_xenos_snapshot_reuse, epic12_xenos_tiled;
static bool epic12_xenos_reorder;
static Epic12GpuAlphaPage epic12_xenos_alpha[256];
static Epic12GpuCommand epic12_xenos_cropped[4096];

// The worker only publishes an integer. Libretro consumes it on the emulation
// thread after BurnDrvFrame; frontend UI callbacks never run on the GPU worker.
static void epic12_xenos_set_status(int status)
{
	if (epic12_xenos_status == status) return;
	epic12_xenos_status = status;
	InterlockedExchange(&epic12_xenos_notice, status);
}

// Name of the resource whose creation failed, for the fallback message above.
static char epic12_xenos_init_step[64];
static unsigned epic12_xenos_init_entry_free;
static unsigned epic12_xenos_init_hr;
// Last HRESULT from a shader compile or resource creation, so a failure can
// name both the step and the code that the banner has no room for.
static HRESULT epic12_xenos_last_hr;
static char epic12_xenos_last_error[128] = "";

static unsigned epic12_xenos_free_mb()
{
	MEMORYSTATUS memory; memory.dwLength=sizeof(memory);
	GlobalMemoryStatus(&memory);
	return (unsigned)(memory.dwAvailPhys>>20);
}

const char *epic12_gpu_take_message()
{
	switch (InterlockedExchange(&epic12_xenos_notice, 0)) {
		case 1: return "CV1000 GPU: active (sprite batching)";
		case 2: return "CV1000 GPU: off - CPU rendering";
		case 3:
		{
			// Lead with the failing resource: the frontend truncates the banner
			// to the overlay width, and this fallback only appears on hardware.
			static char message[160];
			sprintf(message,"CV1000 GPU: init@%s free %u->%uMB hr=%08X",
				epic12_xenos_init_step[0]?epic12_xenos_init_step:"unknown",
				epic12_xenos_init_entry_free,epic12_xenos_free_mb(),
				epic12_xenos_init_hr);
			return message;
		}
		case 4: return "CV1000 GPU: CPU fallback - pixel self-test failed";
		case 5: return "CV1000 GPU: CPU fallback - display unavailable";
		case 6:
		{
			static char message[160];
			sprintf(message, "CV1000 GPU: fallback - %s",
				epic12_xenos_last_error[0] ? epic12_xenos_last_error : "transfer failed");
			return message;
		}
		case 7: return "CV1000 GPU: active (compatible batching)";
	}
	return NULL;
}
static bool epic12_xenos_failed, epic12_xenos_validated;
static int epic12_xenos_consecutive_fails = 0;
static Epic12GpuPageCache epic12_xenos_cache;
static unsigned epic12_gpu_batch_page_capacity()
{
	// Keep the proven cross-batch 256-slot cache, but retire the unproven
	// 256-page batch experiment after the user's frame-rate regression.
	return epic12_xenos_cache.capacity < 128 ? epic12_xenos_cache.capacity : 128;
}
static int epic12_xenos_atlas_height = 1024;

// Positions are pixel boundaries in a 512x512 target. D3D9 pixel centres
// lie on integers; the half-pixel adjustment is explicit in this shader.
static const char epic12_xenos_shader[] =
"struct V { float2 p:POSITION; float2 uv:TEXCOORD0; float4 tint:TEXCOORD1; float4 mode:TEXCOORD2; };\n"
"struct U { float2 p:POSITION; float2 uv:TEXCOORD0; };\n"
"struct P { float4 p:POSITION; float2 uv:TEXCOORD0; float2 dst:TEXCOORD1; float4 tint:TEXCOORD2; float4 mode:TEXCOORD3; };\n"
"float4 atlasScale:register(c0); // vertex shader constants are separate from pixel constants\n"
"P vs_main(V v) { P o; o.p=float4((v.p.x-0.5)/256.0-1.0,1.0-(v.p.y-0.5)/256.0,0,1); o.uv=v.uv*atlasScale.xy; o.dst=v.p/512.0; o.tint=v.tint; o.mode=v.mode; return o; }\n"
"P vs_uniform(U v) { P o; o.p=float4((v.p.x-0.5)/256.0-1.0,1.0-(v.p.y-0.5)/256.0,0,1); o.uv=v.uv; o.dst=v.p/512.0; o.tint=0; o.mode=0; return o; }\n"
"sampler2D source:register(s0); sampler2D destination:register(s1);\n"
"float4 uniformTint:register(c0); // tint RGB (0..63), source alpha (0..31)\n"
"float4 uniformMode:register(c1); // transparency, blend, destination mode, destination alpha\n"
"float4 copy_main(P p):COLOR0 { return tex2D(source,p.uv); }\n"
"float4 shade(P p, float4 tintAlpha, float4 mode) {\n"
" float4 raw=tex2D(source,p.uv);\n"
" if (mode.x>0.5) clip(raw.a-0.0625);\n"
" float3 s=floor(raw.rgb*(255.0/8.0)+0.001);\n"
" s=min(31.0,floor(s*tintAlpha.rgb/31.0+0.0001));\n"
" float3 outc=s;\n"
" if(mode.y>0.5) {\n"
"  if(mode.z>0.5 && tintAlpha.a<0.5) {\n"
"   outc=s;\n"
"  } else {\n"
"   outc=floor(s*tintAlpha.a/31.0+0.0001);\n"
"   if(mode.z>0.5 || (mode.w>0.5 && mode.w<30.5)) {\n"
"    float3 d=floor(tex2D(destination,p.dst).rgb*(255.0/8.0)+0.001);\n"
"    float3 factor=mode.w;\n"
"    if(mode.z>0.5) factor=s; // mode 4 was normalized on the CPU\n"
"    outc=min(31.0,outc+floor(d*factor/31.0+0.0001));\n"
"   }\n"
"  }\n"
" }\n"
" return float4(outc*(8.0/255.0),raw.a);\n"
"}\n"
"float4 ps_main(P p):COLOR0 { return shade(p,floor(p.tint+0.5),floor(p.mode+0.5)); }\n"
"float4 ps_uniform(P p):COLOR0 { return shade(p,uniformTint,uniformMode); }\n"
// Keep destination fetch/branch instructions out of the common plain and
// additive shaders. The CPU planner already separates feedback groups.
"float4 shade_fast(P p, float4 tintAlpha, float4 mode) {\n"
" float4 raw=tex2D(source,p.uv);\n"
" if(mode.x>0.5) clip(raw.a-0.0625);\n"
" float3 s=floor(raw.rgb*(255.0/8.0)+0.001);\n"
" s=min(31.0,floor(s*tintAlpha.rgb/31.0+0.0001));\n"
" float3 outc=s;\n"
" if(mode.y>0.5) {\n"
"  if(mode.z>0.5) outc=s;\n"
"  else outc=floor(s*tintAlpha.a/31.0+0.0001);\n"
" }\n"
" return float4(outc*(8.0/255.0),raw.a);\n"
"}\n"
"float4 shade_feedback(P p, float4 tintAlpha, float4 mode) {\n"
" float4 raw=tex2D(source,p.uv);\n"
" if(mode.x>0.5) clip(raw.a-0.0625);\n"
" float3 s=floor(raw.rgb*(255.0/8.0)+0.001);\n"
" s=min(31.0,floor(s*tintAlpha.rgb/31.0+0.0001));\n"
" float3 outc=floor(s*tintAlpha.a/31.0+0.0001);\n"
" float3 d=floor(tex2D(destination,p.dst).rgb*(255.0/8.0)+0.001);\n"
" float3 factor=mode.w;\n"
" if(mode.z>0.5) factor=s;\n"
" outc=min(31.0,outc+floor(d*factor/31.0+0.0001));\n"
" return float4(outc*(8.0/255.0),raw.a);\n"
"}\n"
"float4 ps_fast(P p):COLOR0 { return shade_fast(p,floor(p.tint+0.5),floor(p.mode+0.5)); }\n"
"float4 ps_feedback(P p):COLOR0 { return shade_feedback(p,floor(p.tint+0.5),floor(p.mode+0.5)); }\n";

static void epic12_xenos_log(const char *message)
{
	if (message) {
		strncpy(epic12_xenos_last_error, message, sizeof(epic12_xenos_last_error) - 1);
		epic12_xenos_last_error[sizeof(epic12_xenos_last_error) - 1] = 0;
	}
}

static void epic12_gpu_invalidate(const rectangle &r)
{
	epic12_xenos_cache.invalidate(r.min_x,r.min_y,r.max_x,r.max_y);
}

static void epic12_gpu_reset()
{
	epic12_xenos_cache.clear();
	for (int b = 0; b < EPIC12_GPU_BINS; ++b) {
		epic12_gpu_count[b] = 0;
		epic12_gpu_pixels[b] = 0;
		epic12_gpu_source_count[b] = 0;
		memset(epic12_gpu_source_pages[b], 0, sizeof(epic12_gpu_source_pages[b]));
		memset(epic12_gpu_dest_pages[b], 0, sizeof(epic12_gpu_dest_pages[b]));
	}
	epic12_gpu_snapshot_reset();
	epic12_gpu_staging_reset();
}

static void epic12_xenos_release()
{
#define EPIC12_RELEASE(p) if(p) { (p)->Release(); (p)=NULL; }
	EPIC12_RELEASE(epic12_xenos_state);
	EPIC12_RELEASE(epic12_xenos_decl);
	EPIC12_RELEASE(epic12_xenos_uniform_decl);
	EPIC12_RELEASE(epic12_xenos_vs);
	EPIC12_RELEASE(epic12_xenos_uniform_vs);
	EPIC12_RELEASE(epic12_xenos_ps);
	EPIC12_RELEASE(epic12_xenos_fast_ps);
	EPIC12_RELEASE(epic12_xenos_feedback_ps);
	EPIC12_RELEASE(epic12_xenos_uniform_ps);
	EPIC12_RELEASE(epic12_xenos_copy_ps);
	EPIC12_RELEASE(epic12_xenos_atlas);
	EPIC12_RELEASE(epic12_xenos_input);
	EPIC12_RELEASE(epic12_xenos_result);
	EPIC12_RELEASE(epic12_xenos_target);
#undef EPIC12_RELEASE
	epic12_xenos_validated=false;
	epic12_gpu_reset();
}

static void epic12_gpu_exit()
{
	unsigned generation;
	D3DDevice *d=(D3DDevice*)SDL_XBOX_AcquireCoreGpu(&generation);
	if (d) {
		d->BlockUntilIdle();
		epic12_xenos_release();
		SDL_XBOX_ReleaseCoreGpu();
	}
	epic12_xenos_device=NULL;
	epic12_xenos_failed=false;
	epic12_xenos_status=epic12_xenos_error=0;
	InterlockedExchange(&epic12_xenos_notice,0);
}

static bool epic12_xenos_compile(const char *entry, const char *profile, ID3DXBuffer **code)
{
	ID3DXBuffer *error=NULL;
	HRESULT hr=D3DXCompileShader(epic12_xenos_shader,(UINT)strlen(epic12_xenos_shader),NULL,NULL,
		entry,profile,0,code,&error,NULL);
	epic12_xenos_last_hr=hr;
	if (error) { if (FAILED(hr)) epic12_xenos_log((const char*)error->GetBufferPointer()); error->Release(); }
	return SUCCEEDED(hr) && *code;
}

static bool epic12_xenos_create_pixel_shader(const char *entry, D3DPixelShader **shader)
{
	ID3DXBuffer *code=NULL;
	if(!epic12_xenos_compile(entry,"ps_3_0",&code)) return false;
	HRESULT hr=epic12_xenos_device->CreatePixelShader((DWORD*)code->GetBufferPointer(),shader);
	epic12_xenos_last_hr=hr;
	code->Release();
	return SUCCEEDED(hr);
}

// Only source-cache capacity changes: each batch still uses at most 128 pages.
// Allocate after mandatory GPU resources; low-memory machines retain 128 slots.
static bool epic12_xenos_create_atlas(int capacity)
{
	if(epic12_xenos_atlas) { epic12_xenos_atlas->Release(); epic12_xenos_atlas=NULL; }
	epic12_xenos_cache.reset(capacity);
	epic12_xenos_atlas_height=capacity*8;
	HRESULT hr=epic12_xenos_device->CreateTexture(2048,epic12_xenos_atlas_height,1,0,
		(epic12_xenos_tiled?D3DFMT_A8R8G8B8:D3DFMT_LIN_A8R8G8B8),D3DPOOL_DEFAULT,&epic12_xenos_atlas,NULL);
	epic12_xenos_last_hr=hr;
	return SUCCEEDED(hr);
}

// Record the failing step and keep the false return readable at each call site.
// Guarda el recurso que fallo y su HRESULT para el aviso en pantalla.
static bool epic12_xenos_init_failed(const char *step, HRESULT hr)
{
	unsigned i=0;
	if (step) while (step[i] && i+1<sizeof(epic12_xenos_init_step)) { epic12_xenos_init_step[i]=step[i]; ++i; }
	epic12_xenos_init_step[i]=0;
	epic12_xenos_init_hr=(unsigned)hr;
	return false;
}

static bool epic12_xenos_create()
{
	D3DDevice *d=epic12_xenos_device;
	ID3DXBuffer *code=NULL;
	HRESULT hr;
	epic12_xenos_init_entry_free=epic12_xenos_free_mb();
	if (!epic12_xenos_compile("vs_main","vs_3_0",&code)) return epic12_xenos_init_failed("vs_main shader",epic12_xenos_last_hr);
	hr=d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&epic12_xenos_vs); code->Release(); code=NULL;
	if (FAILED(hr)) return epic12_xenos_init_failed("vs_main create",hr);
	if (!epic12_xenos_compile("vs_uniform","vs_3_0",&code)) return epic12_xenos_init_failed("vs_uniform shader",epic12_xenos_last_hr);
	hr=d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&epic12_xenos_uniform_vs); code->Release(); code=NULL;
	if (FAILED(hr)) return epic12_xenos_init_failed("vs_uniform create",hr);
	if (!epic12_xenos_compile("ps_uniform","ps_3_0",&code)) return epic12_xenos_init_failed("ps_uniform shader",epic12_xenos_last_hr);
	hr=d->CreatePixelShader((DWORD*)code->GetBufferPointer(),&epic12_xenos_uniform_ps); code->Release(); code=NULL;
	if (FAILED(hr)) return epic12_xenos_init_failed("ps_uniform create",hr);
	// Keep the already tested uniform path if the new shader cannot be used.
	epic12_xenos_attributes=false;
	if (epic12_xenos_compile("ps_main","ps_3_0",&code)) {
		hr=d->CreatePixelShader((DWORD*)code->GetBufferPointer(),&epic12_xenos_ps); code->Release(); code=NULL;
		epic12_xenos_attributes=SUCCEEDED(hr);
	}
	epic12_xenos_specialized=epic12_xenos_attributes &&
		epic12_xenos_create_pixel_shader("ps_fast",&epic12_xenos_fast_ps) &&
		epic12_xenos_create_pixel_shader("ps_feedback",&epic12_xenos_feedback_ps);
	if (!epic12_xenos_compile("copy_main","ps_3_0",&code)) return epic12_xenos_init_failed("copy_main shader",epic12_xenos_last_hr);
	hr=d->CreatePixelShader((DWORD*)code->GetBufferPointer(),&epic12_xenos_copy_ps); code->Release();
	if (FAILED(hr)) return epic12_xenos_init_failed("copy_main create",hr);
	D3DVERTEXELEMENT9 elements[]={
		{0,0,D3DDECLTYPE_USHORT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,4,D3DDECLTYPE_USHORT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,8,D3DDECLTYPE_UBYTE4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,12,D3DDECLTYPE_UBYTE4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},D3DDECL_END()};
	hr=d->CreateVertexDeclaration(elements,&epic12_xenos_decl);
	if (FAILED(hr)) return epic12_xenos_init_failed("vertex declaration",hr);
	D3DVERTEXELEMENT9 uniformElements[]={
		{0,0,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,8,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
	hr=d->CreateVertexDeclaration(uniformElements,&epic12_xenos_uniform_decl);
	if (FAILED(hr)) return epic12_xenos_init_failed("uniform declaration",hr);
	hr=d->CreateStateBlock(D3DSBT_ALL,&epic12_xenos_state);
	if (FAILED(hr)) return epic12_xenos_init_failed("state block",hr);
	hr=d->CreateTexture(512,512,1,0,D3DFMT_LIN_A8R8G8B8,D3DPOOL_DEFAULT,&epic12_xenos_input,NULL);
	if (FAILED(hr)) return epic12_xenos_init_failed("input texture",hr);
	// Xenos Resolve requires a tiled destination. Readonly LockRect provides
	// synchronization/cache coherency; untile only the batch's output region.
	hr=d->CreateTexture(512,512,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&epic12_xenos_result,NULL);
	if (FAILED(hr)) return epic12_xenos_init_failed("result texture",hr);
	// Allocate offscreen render target above the 720p frontbuffer (tiles 0..719)
	// to avoid EDRAM collision with live presentation scanout.
	D3DSURFACE_PARAMETERS surface={0};
	surface.Base = 1024;
	hr=d->CreateRenderTarget(512,512,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&epic12_xenos_target,&surface);
	if (FAILED(hr)) return epic12_xenos_init_failed("render target",hr);
	// Validate real VMX loads/stores and all 512x512 tile addresses once,
	// including unaligned destination rows, before using the fused transfer.
	epic12_xenos_fast_transfer=epic12_gpu_untile_selftest();
	epic12_xenos_log(epic12_xenos_fast_transfer?
		"fused VMX transfer self-test passed":"fused transfer unavailable; using SDK transfer");
	epic12_xenos_alpha_trim=true; epic12_xenos_snapshot_reuse=true; epic12_xenos_reorder=false;
	bool alphaVector=epic12_gpu_alpha_vector_selftest();
	epic12_xenos_log(alphaVector?"VMX alpha mask self-test passed":"VMX alpha mask unavailable; using scalar masks");
	epic12_xenos_tiled=epic12_gpu_tile_selftest();
	epic12_xenos_log(epic12_xenos_tiled?"tiled atlas layout self-test passed":"tiled atlas unavailable; using linear layout");
	bool fusedAlpha=epic12_xenos_tiled && alphaVector && epic12_gpu_tile_alpha_selftest();
	epic12_xenos_log(fusedAlpha?"fused atlas/alpha self-test passed":"fused atlas/alpha unavailable; using separate passes");
	if(!epic12_xenos_create_atlas(256) && !epic12_xenos_create_atlas(128)) {
		if(!epic12_xenos_tiled) return epic12_xenos_init_failed("atlas texture",epic12_xenos_last_hr);
		epic12_xenos_tiled=false;
		if(!epic12_xenos_create_atlas(256) && !epic12_xenos_create_atlas(128)) return epic12_xenos_init_failed("atlas texture",epic12_xenos_last_hr);
	}
	return true;
}

static void epic12_xenos_quad(float x,float y,float w,float h,float u0,float v0,float u1,float v1)
{
	Epic12GpuUniformVertex v[3]={{x,y,u0,v0},{x+w,y,u1,v0},{x,y+h,u0,v1}};
	epic12_xenos_device->DrawPrimitiveUP(D3DPT_RECTLIST,1,v,sizeof(v[0]));
}

static void epic12_xenos_draw(int vertices)
{
	if (!vertices) return;
	if(epic12_xenos_attributes) {
		epic12_xenos_device->DrawPrimitiveUP(D3DPT_RECTLIST,vertices/3,
			epic12_xenos_vertices,sizeof(Epic12GpuVertex));
	} else {
		for(int i=0;i<vertices;++i) {
			const Epic12GpuVertex &v=epic12_xenos_vertices[i];
			Epic12GpuUniformVertex &u=epic12_xenos_uniform_vertices[i];
			u.x=(float)v.x; u.y=(float)v.y; u.u=v.u/2048.0f; u.v=v.v/(float)epic12_xenos_atlas_height;
		}
		epic12_xenos_device->DrawPrimitiveUP(D3DPT_RECTLIST,vertices/3,
			epic12_xenos_uniform_vertices,sizeof(Epic12GpuUniformVertex));
	}
}

static void epic12_xenos_resolve(int x,int y,int w,int h)
{
	// Resolve requires 8-pixel alignment and must never exceed the 512x512 render target.
	int x1 = x & ~7; if (x1 < 0) x1 = 0; if (x1 > 512) x1 = 512;
	int y1 = y & ~7; if (y1 < 0) y1 = 0; if (y1 > 512) y1 = 512;
	int x2 = (x + w + 7) & ~7; if (x2 < 0) x2 = 0; if (x2 > 512) x2 = 512;
	int y2 = (y + h + 7) & ~7; if (y2 < 0) y2 = 0; if (y2 > 512) y2 = 512;
	if (x2 <= x1 || y2 <= y1) return;
	D3DRECT rect = { x1, y1, x2, y2 };
	D3DPOINT point = { x1, y1 };
	epic12_xenos_device->Resolve(D3DRESOLVE_RENDERTARGET0, &rect, epic12_xenos_result, &point, 0, 0, NULL, 0, 0, NULL);
}

static bool epic12_xenos_run(UINT32 *vram,const Epic12GpuCommand *cmd,int count,const rectangle &originalBounds)
{
	epic12_xenos_last_error[0] = '\0';
	D3DDevice *d=epic12_xenos_device;
	bool needed[EPIC12_VRAM_PAGES]={false};
	int slots[EPIC12_VRAM_PAGES];
	int pages=0;
	for (int i=0;i<count;++i) {
		const Epic12GpuCommand &c=cmd[i];
		for (int y=c.sy/128;y<=(c.sy+c.h-1)/128;++y)
			for (int x=c.sx/128;x<=(c.sx+c.w-1)/128;++x) {
				int p=y*64+x;
				if (p < 0 || p >= EPIC12_VRAM_PAGES) {
					char msg[128]; sprintf(msg, "fail: p oob %d cmd %d (%d,%d,%d,%d)", p, i, c.sx, c.sy, c.w, c.h);
					epic12_xenos_log(msg); return false;
				}
				if (!needed[p]) {
					needed[p]=true;
					if (++pages>(int)epic12_gpu_batch_page_capacity()) {
						char msg[128]; sprintf(msg, "fail: pages > %d", (int)epic12_gpu_batch_page_capacity());
						epic12_xenos_log(msg); return false;
					}
				}
			}
	}
	epic12_xenos_cache.begin_batch(needed,slots);
	D3DLOCKED_RECT lr;
	bool locked=false;
	for (int p=0;p<EPIC12_VRAM_PAGES;++p) if (needed[p] && slots[p]<0) {
		int next=epic12_xenos_cache.allocate(p);
		if(next<0) {
			if(locked) epic12_xenos_atlas->UnlockRect(0);
			char msg[128]; sprintf(msg, "fail: alloc slot for p=%d", p);
			epic12_xenos_log(msg); return false;
		}
		if (!locked) {
			if (FAILED(epic12_xenos_atlas->LockRect(0,&lr,NULL,0))) {
				epic12_xenos_log("fail: atlas lock"); return false;
			}
			locked=true;
		}
		int sx=(p&63)*128, sy=(p>>6)*128;
		int ax=(next&15)*128, ay=(next>>4)*128;
		const UINT32 *source=vram+sy*8192+sx;
		if(epic12_xenos_tiled) {
			if(!epic12_xenos_alpha_trim || !epic12_gpu_tile_alpha_page(lr.pBits,source,next,epic12_xenos_alpha[next])) {
				epic12_gpu_tile_page(lr.pBits,source,next);
				if(epic12_xenos_alpha_trim) epic12_xenos_alpha[next].build(source,8192);
			}
		} else {
			epic12_gpu_copy_rows((BYTE*)lr.pBits + (size_t)ay * lr.Pitch + (size_t)ax * 4,
				(size_t)lr.Pitch, source, 8192 * sizeof(UINT32), 128 * sizeof(UINT32), 128);
			if(epic12_xenos_alpha_trim) epic12_xenos_alpha[next].build(source,8192);
		}
		slots[p]=next;
	}
	if (locked) epic12_xenos_atlas->UnlockRect(0);
	rectangle bounds=originalBounds;
	if(epic12_xenos_alpha_trim) {
		int cropped=epic12_gpu_alpha_crop_batch(cmd,count,epic12_xenos_cropped,
			epic12_xenos_alpha,slots,epic12_xenos_cache.capacity,bounds);
		if(cropped<0) {
			char msg[128]; sprintf(msg, "fail: alpha crop cropped=%d", cropped);
			epic12_xenos_log(msg); return false;
		}
		cmd=epic12_xenos_cropped; count=cropped;
	}
	if(epic12_xenos_attributes && epic12_xenos_reorder) {
		epic12_gpu_reorder(cmd,count,epic12_xenos_cropped);
		cmd=epic12_xenos_cropped;
	}
	if(!count) return true;
	int width=bounds.max_x-bounds.min_x+1, height=bounds.max_y-bounds.min_y+1;
	if (FAILED(epic12_xenos_input->LockRect(0,&lr,NULL,0))) {
		epic12_xenos_log("fail: input lock"); return false;
	}
	// The bounding box of one batch, row by row. The source rows are 32 KB
	// apart and the locked rows are lr.Pitch apart, so a memcpy() given one row
	// cannot know where the next one is; epic12_gpu_region.h does, and fetches
	// it ahead while it copies this one.
	epic12_gpu_copy_rows(lr.pBits,(size_t)lr.Pitch,
		vram+(size_t)bounds.min_y*8192+(size_t)bounds.min_x,
		8192*sizeof(UINT32),(size_t)width*sizeof(UINT32),(size_t)height);
	epic12_xenos_input->UnlockRect(0);

	D3DSurface *oldTarget=NULL,*oldDepth=NULL;
	d->GetRenderTarget(0,&oldTarget); d->GetDepthStencilSurface(&oldDepth);
	epic12_xenos_state->Capture();
	float atlasScale[4]={1.0f/2048.0f,1.0f/(float)epic12_xenos_atlas_height,0,0};
	d->SetVertexShaderConstantF(0,atlasScale,1);
	d->SetDepthStencilSurface(NULL);
	d->SetRenderTarget(0,epic12_xenos_target);
	D3DVIEWPORT9 vp={0,0,512,512,0.0f,1.0f}; d->SetViewport(&vp);
	d->SetRenderState(D3DRS_VIEWPORTENABLE,TRUE);
	d->SetRenderState(D3DRS_HALFPIXELOFFSET,FALSE);
	d->SetRenderState(D3DRS_ZENABLE,FALSE); d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
	d->SetRenderState(D3DRS_STENCILENABLE,FALSE); d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
	d->SetRenderState(D3DRS_ALPHATOMASKENABLE,FALSE); d->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
	d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE); d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
	d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
	d->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff);
	d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
	d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,TRUE);
	d->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ONE);
	d->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ZERO);
	d->SetRenderState(D3DRS_BLENDOPALPHA,D3DBLENDOP_ADD);
	d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE); d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
	d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
	d->SetVertexDeclaration(epic12_xenos_uniform_decl); d->SetVertexShader(epic12_xenos_uniform_vs);
	for (int s=0;s<2;++s) {
		d->SetSamplerState(s,D3DSAMP_MINFILTER,D3DTEXF_POINT); d->SetSamplerState(s,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
		d->SetSamplerState(s,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
		d->SetSamplerState(s,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP); d->SetSamplerState(s,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
	}
	d->SetTexture(0,epic12_xenos_input); d->SetTexture(1,NULL);
	d->SetPixelShader(epic12_xenos_copy_ps);
	epic12_xenos_quad(0,0,(float)width,(float)height,0,0,width/512.0f,height/512.0f);
	if(epic12_xenos_attributes) {
		d->SetVertexDeclaration(epic12_xenos_decl); d->SetVertexShader(epic12_xenos_vs);
	}
	d->SetTexture(0,epic12_xenos_atlas);
	D3DPixelShader *activePs=epic12_xenos_attributes?epic12_xenos_ps:epic12_xenos_uniform_ps;
	d->SetPixelShader(activePs);
	int snapshotEnd=0;
	for (int first=0;first<count;) {
		const Epic12GpuCommand &c=cmd[first];
		rectangle snapshot;
		int end=epic12_gpu_group_end(cmd,first,count,bounds,snapshot,epic12_xenos_attributes);
		bool feedback=epic12_gpu_feedback(c);
		if (epic12_xenos_attributes && epic12_xenos_specialized) {
			D3DPixelShader *wanted=feedback?epic12_xenos_feedback_ps:epic12_xenos_fast_ps;
			if(wanted!=activePs) { d->SetPixelShader(wanted); activePs=wanted; }
		}
		if (feedback && first>=snapshotEnd) {
			snapshotEnd=epic12_xenos_snapshot_reuse?
				epic12_gpu_snapshot_end(cmd,first,end,count,bounds,snapshot,epic12_xenos_attributes):end;
			d->SetTexture(1,NULL);
			epic12_xenos_resolve(snapshot.min_x,snapshot.min_y,
				snapshot.max_x-snapshot.min_x+1,snapshot.max_y-snapshot.min_y+1);
			d->SetTexture(1,epic12_xenos_result);
		}
		int da_norm = epic12_gpu_dest_alpha(c);
		int dm_norm = epic12_gpu_dest_mode(c);
		if (!c.blend || (dm_norm == 0 && da_norm == 0) || feedback) {
			d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		} else if (dm_norm == 1 && c.sa == 0) {
			d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
			d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		} else if (da_norm == 31) {
			d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		} else {
			d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR);
			DWORD factor = (da_norm * 255 + 15) / 31;
			d->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_RGBA(factor, factor, factor, factor));
		}
		if (!epic12_xenos_attributes) {
			float constants[8]={(float)c.tint.r,(float)c.tint.g,(float)c.tint.b,(float)c.sa,
				(float)c.transparent,(float)c.blend,(float)epic12_gpu_dest_mode(c),(float)epic12_gpu_dest_alpha(c)};
			d->SetPixelShaderConstantF(0,constants,2);
		}
		int vertices=0;
		for (int i=first;i<end;++i) {
			if (vertices+epic12_gpu_vertex_count(cmd[i])>EPIC12_GPU_DRAW_RECTS*3) {
				epic12_xenos_draw(vertices); vertices=0;
			}
			vertices+=epic12_gpu_vertices(cmd[i],bounds,slots,epic12_xenos_vertices+vertices);
		}
		epic12_xenos_draw(vertices);
		first=end;
	}
	d->SetTexture(1,NULL);
	epic12_xenos_resolve(0,0,width,height);
	d->SetRenderTarget(0,oldTarget); d->SetDepthStencilSurface(oldDepth);
	epic12_xenos_state->Apply();
	if(oldTarget) oldTarget->Release(); if(oldDepth) oldDepth->Release();
	RECT sourceRect={0,0,width,height}; POINT destPoint={bounds.min_x,bounds.min_y};
	if (FAILED(epic12_xenos_result->LockRect(0,&lr,&sourceRect,D3DLOCK_READONLY))) {
		epic12_xenos_log("fail: result lock"); return false;
	}
	if(epic12_xenos_fast_transfer) {
		epic12_gpu_untile_mask(lr.pBits,vram,bounds.min_x,bounds.min_y,width,height);
	} else {
		XGUntileTextureLevel(512,512,0,XGGetGpuFormat(D3DFMT_A8R8G8B8),0,
			vram,8192*4,&destPoint,lr.pBits,&sourceRect);
		for (int y=0;y<height;++y) {
			UINT32 *out=vram+(bounds.min_y+y)*8192+bounds.min_x;
			// ONE+ONE blending clamps at 255. Masking recovers EPIC12's clamp
			// at 31*8 exactly, also after repeated additive saturation.
			for (int x=0;x<width;++x) out[x]&=0x20f8f8f8;
		}
	}
	epic12_xenos_result->UnlockRect(0);
	return true;
}

static bool epic12_xenos_selftest()
{
	UINT32 *scratch=(UINT32*)calloc(128*8192,sizeof(UINT32));
	UINT32 result[64*64];
	if (!scratch) return false;
	// Exercise the final atlas row, including slots 254/255 on the large
	// texture. Dummy tags are never sampled and are cleared after this test.
	bool needed[EPIC12_VRAM_PAGES]={false}; int slots[EPIC12_VRAM_PAGES];
	epic12_xenos_cache.clear(); epic12_xenos_cache.begin_batch(needed,slots);
	for(int i=0;i<epic12_xenos_cache.capacity-2;++i) epic12_xenos_cache.allocate(128+i);
	for(int y=0;y<128;++y) for(int x=0;x<256;++x)
		scratch[y*8192+x]=((x+y)&1?0x20000000:0)|((x&31)<<19)|((y&31)<<11)|(((x+y)&31)<<3);
	Epic12GpuCommand commands[224];
	for(int i=0;i<32;++i) {
		Epic12GpuCommand &c=commands[i];
		c.sx=128+i; c.sy=i; c.x=(i*7)%33; c.y=(i*11)%33; c.w=32; c.h=32;
		c.flipx=i&1; c.flipy=(i>>1)&1; c.transparent=(i>>2)&1;
		c.blend=i%5!=0; c.dmode=i%5==3?1:(i%5==4?4:0);
		c.sa=i; c.da=i==2?0:(i%5==1?31:31-i);
		c.tint.r=(i*13)&63; c.tint.g=i&1?32:63; c.tint.b=(i*7)&63;
	}
	for(int i=32;i<128;++i) {
		Epic12GpuCommand &c=commands[i]; int group=(i-32)/8;
		c.sx=120+(i&1)*64; c.sy=80; c.x=(i&3)*16; c.y=((i>>2)&1)*16+16;
		c.w=c.h=16; c.flipx=i&1; c.flipy=(i>>1)&1; c.transparent=group&1;
		c.blend=group%4!=0; c.dmode=group%4==1?1:(group%4==3?4:0);
		c.sa=17; c.da=group<4?31:16; c.tint.r=32; c.tint.g=27; c.tint.b=45;
	}
	// Adjacent rectangles with different tint/alpha/transparency in the same
	// submission; include non-feedback, additive and destination-dependent mixes.
	for(int i=128;i<192;++i) {
		Epic12GpuCommand &c=commands[i]; int group=(i-128)/8;
		c=commands[32+(i&7)]; c.transparent=i&1; c.blend=1;
		c.tint.r=(i*13)&63; c.tint.g=(i*7)&63; c.tint.b=(i*23)&63; c.sa=i&31;
		c.dmode=group%3==2 && (i&1)?1:0;
		c.da=group%3==0?0:(group%3==1?31:1+(i%30));
		if(group%3==0 && (i&1)) c.blend=0;
	}
	// Blank and sparse source borders exercise crop/skip, both flips, and
	// opaque draws that must retain transparent-looking pixels. Page 2 is
	// outside the original two sources and also exercises cache replacement.
	for(int y=85;y<100;++y) for(int x=327;x<342;++x) if((x+y)%3)
		scratch[y*8192+x]=0x20000000|((x&31)<<19)|((y&31)<<11)|(((x+y)&31)<<3);
	for(int i=192;i<208;++i) {
		Epic12GpuCommand &c=commands[i]; c=commands[i-192];
		c.sx=i<200?256:320; c.sy=80; c.w=c.h=32;
		c.x=(i&1)*24; c.y=((i>>1)&1)*24;
		c.flipx=i&1; c.flipy=(i>>1)&1; c.transparent=i<196 || i>=200;
	}
	// Feedback, unrelated plain write, then adjacent feedback: the two
	// destination-dependent groups can reuse a snapshot without reordering.
	for(int i=208;i<224;++i) {
		Epic12GpuCommand &c=commands[i]; c=commands[200]; int part=i&3;
		c.sx=328; c.sy=88; c.w=c.h=8; c.transparent=1;
		c.x=part==0?0:(part==1?40:(part==2?8:24)); c.y=part==1?40:(part==3?24:0);
		c.blend=part!=1; c.dmode=0; c.da=part==3?31:16; c.sa=17;
	}
	rectangle bounds(0,63,0,63);
	bool ok=epic12_xenos_run(scratch,commands,224,bounds);
	if(ok) {
		for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
			result[y*64+x]=scratch[y*8192+x];
			scratch[y*8192+x]=((x+y)&1?0x20000000:0)|((x&31)<<19)|((y&31)<<11)|(((x+y)&31)<<3);
		}
		UINT32 *saved=m_bitmaps; UINT64 delay=epic12_device_blit_delay; m_bitmaps=scratch;
		for(int i=0;i<224;++i) epic12_gpu_replay(commands[i]);
		m_bitmaps=saved; epic12_device_blit_delay=delay;
		for(int y=0;y<64 && ok;++y) for(int x=0;x<64;++x) {
			UINT32 g = result[y*64+x], c = scratch[y*8192+x];
			if (g != c) {
				int da = abs((int)((g >> 24) & 0xff) - (int)((c >> 24) & 0xff));
				int dr = abs((int)((g >> 16) & 0xff) - (int)((c >> 16) & 0xff));
				int dg = abs((int)((g >> 8) & 0xff) - (int)((c >> 8) & 0xff));
				int db = abs((int)(g & 0xff) - (int)(c & 0xff));
				if (da > 0 || dr > 8 || dg > 8 || db > 8) {
					char message[160];
					sprintf(message,"self-test mismatch x=%d y=%d GPU=%08x CPU=%08x",x,y,g,c);
					epic12_xenos_log(message); ok=false; break;
				}
			}
		}
	}
	free(scratch); epic12_gpu_reset();
	return ok;
}

static bool epic12_gpu_render(const Epic12GpuCommand *cmd,int count,const rectangle &bounds)
{
	unsigned generation;
	D3DDevice *d=(D3DDevice*)SDL_XBOX_AcquireCoreGpu(&generation);
	if(!d) {
		if(epic12_xenos_status!=5) epic12_xenos_log("display unavailable; using CPU");
		epic12_xenos_set_status(5); return false;
	}
	if(d!=epic12_xenos_device || generation!=epic12_xenos_generation) {
		epic12_xenos_release(); epic12_xenos_device=d; epic12_xenos_generation=generation; epic12_xenos_failed=false; epic12_xenos_error=0; epic12_xenos_consecutive_fails=0;
	}
	bool ok=false;
	if(!epic12_xenos_failed) {
		if(!epic12_xenos_target && !epic12_xenos_create()) {
			epic12_xenos_log("resource/shader initialization failed; using CPU"); epic12_xenos_failed=true; epic12_xenos_error=3;
		}
		if(!epic12_xenos_failed && !epic12_xenos_validated) {
			epic12_xenos_validated=epic12_xenos_selftest();
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_reorder) {
				epic12_xenos_log("draw reordering validation failed; checking original order");
				epic12_xenos_reorder=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_validated && epic12_xenos_tiled) {
				epic12_xenos_log("tiled atlas pixel validation failed; checking linear atlas");
				epic12_xenos_tiled=false;
				int capacity=epic12_xenos_cache.capacity;
				if(epic12_xenos_create_atlas(capacity) || (capacity>128 && epic12_xenos_create_atlas(128)))
					epic12_xenos_validated=epic12_xenos_selftest();
				else epic12_xenos_failed=true;
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_snapshot_reuse) {
				epic12_xenos_log("snapshot reuse validation failed; checking adjacent groups");
				epic12_xenos_snapshot_reuse=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_alpha_trim) {
				epic12_xenos_log("alpha crop validation failed; checking full sprites");
				epic12_xenos_alpha_trim=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_cache.capacity==256) {
				epic12_xenos_log("large atlas validation unavailable; checking 128-slot cache");
				if(epic12_xenos_create_atlas(128)) epic12_xenos_validated=epic12_xenos_selftest();
				else epic12_xenos_failed=true;
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_specialized) {
				epic12_xenos_log("split shader self-test failed; checking unified shader");
				epic12_xenos_specialized=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_fast_transfer) {
				epic12_xenos_log("GPU transfer self-test failed; checking SDK transfer");
				epic12_xenos_fast_transfer=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_failed && !epic12_xenos_validated && epic12_xenos_attributes) {
				epic12_xenos_log("sprite-attribute self-test failed; checking compatible GPU path");
				epic12_xenos_attributes=false;
				epic12_xenos_validated=epic12_xenos_selftest();
			}
			if(!epic12_xenos_validated) {
				epic12_xenos_log("pixel self-test failed; using CPU"); epic12_xenos_failed=true; epic12_xenos_error=4;
			} else epic12_xenos_log(epic12_xenos_attributes?
				"pixel self-test passed; GPU compositor active (sprite attributes)":
				"pixel self-test passed; GPU compositor active (uniform compatible)");
		}
		if(!epic12_xenos_failed) {
			ok=epic12_xenos_run(m_bitmaps,cmd,count,bounds);
		}
	}
	if (ok) {
		epic12_xenos_consecutive_fails = 0;
		epic12_xenos_set_status(epic12_xenos_attributes ? 1 : 7);
	} else {
		epic12_xenos_cache.clear();
		epic12_xenos_log(epic12_xenos_last_error[0] ? epic12_xenos_last_error : "transfer failed");
		if (++epic12_xenos_consecutive_fails >= 5) {
			epic12_xenos_failed = true;
			epic12_xenos_error = 6;
			epic12_xenos_set_status(6);
		}
	}
	SDL_XBOX_ReleaseCoreGpu();
	return ok;
}
#endif
