#include <xtl.h>
#include "gui.h"

#define USE_RETRO_ARCH_SHADER
#define PSX_WIDTH 1024
#define PSX_HEIGHT 512

#if defined(USE_RETRO_ARCH_SHADER)
// From https://github.com/Themaister/Emulator-Shader-Pack/blob/master/Cg/README
static char * vs_entry_point = "main_vertex";
static char * ps_entry_point = "main_fragment";
//static char * effect_file_name = "game:\\hlsl\\5xBR-v3.5.cg";
static char * effect_file_name = "game:\\hlsl\\5xBR-v3.7a.cg";

LPD3DXCONSTANTTABLE pPsShaderTable;
LPD3DXCONSTANTTABLE pVsShaderTable;

struct { 
	D3DXHANDLE video_size;
	D3DXHANDLE texture_size;
	D3DXHANDLE output_size;

} input_ps, input_vs;

D3DXHANDLE in_modelViewProj;

struct {
	float video_size[2];
	float texture_size[2];
	float output_size[2]; 
} retro_arch_shader_input;
#else

//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------

static const char* g_strVertexShaderProgram =
	"float4x4 matWVP : register(c0);							\n"
	"struct VS_IN												\n"
	"{															\n"
	"   float4   Position  : POSITION;							\n"
	"   float2   UV        : TEXCOORD0;							\n"
	"   float4   Color     : COLOR;								\n"  // Vertex color      
	"};															\n"
	"															\n"
	"struct VS_OUT												\n"
	"{															\n"
	"   float4 Position  : POSITION;							\n"
	"   float2 UV        : TEXCOORD0;							\n"
	"   float4 Color     : COLOR;								\n"  // Vertex color   
	"};															\n"
	"															\n"
	"VS_OUT main( VS_IN In )					\n"
	"{															\n"
	"   VS_OUT Out;												\n"
	//"	Out.Position = mul( matWVP, In.Position );				\n"
	"	Out.Position = In.Position;								\n"
	"	Out.Color = In.Color;									\n"
	//"   Out.UV  = 1.0f - (0.5f + (sign(In.Position.xy)*0.5f));										\n"
	"   Out.UV  = In.UV;										\n"
	"   return Out;												\n"
	"}															\n";


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const char*                                 g_strPixelShaderProgram =
	" struct PS_IN                                 "
	" {                                            "
	"     float2 TexCoord	: TEXCOORD;            "
	"     float4 Color		: COLOR;		       "
	" };                                           "  // the vertex shader
	"                                              "
	" sampler detail;                              "
	"                                              "
	" float4 main( PS_IN In ) : COLOR              "
	" {                                            "
	"     return tex2D( detail, In.TexCoord );     "  // Output color
	" }	                                       ";
#endif
//--------------------------------------------------------------------------------------
// Globals
//-------------------------------------------------------------------------------------
static IDirect3DDevice9*				g_pd3dDevice = NULL; // the rendering device
static LPDIRECT3DTEXTURE9				g_PsxTexture = NULL; // PSX Texture
static IDirect3DPixelShader9*			pPixelShader;
static IDirect3DVertexShader9*			pVertexShader;
static IDirect3DVertexDeclaration9*	pVertexDecl;

static XMMATRIX matWVP;

extern "C" unsigned char *pPsxScreen;
extern "C" unsigned int g_pPitch;

#if defined(USE_RETRO_ARCH_SHADER)
// Structure to hold vertex data.
struct COLORVERTEX
{
	float       Position[3];
	float       TextureUV[2];
};

COLORVERTEX Vertices[] =
{
	// {  X,	Y,	  Z,	U,	 V	}
	{ -1.f, -1.f, 0.0f,  0.0f,  1.0f}, //1
	{ -1.f,  1.f, 0.0f,  0.0f,  0.0f}, //2
	{  1.f, -1.f, 0.0f,  1.0f,  1.0f}, //3
};

static const char *stock_hlsl_program =
      "void main_vertex\n"
      "(\n"
      "  float4 position : POSITION,\n"
      "  float4 color    : COLOR,\n"
      "\n"
      "  uniform float4x4 modelViewProj,\n"
      "\n"
      "  float4 texCoord : TEXCOORD0,\n"
      "  out float4 oPosition : POSITION,\n"
      "  out float4 oColor : COLOR,\n"
      "  out float2 otexCoord : TEXCOORD\n"
      ")\n"
      "{\n"
      "  oPosition = mul(modelViewProj, position);\n"
	   "  oColor = color;\n"
      "  otexCoord = texCoord;\n"
      "}\n"
      "\n"
      "struct output\n"
      "{\n"
      "  float4 color: COLOR;\n"
      "};\n"
      "\n"
      "struct input\n"
      "{\n"
      "  float2 video_size;\n"
      "  float2 texture_size;\n"
      "  float2 output_size;\n"
	   "  float frame_count;\n"
	   "  float frame_direction;\n"
	   "  float frame_rotation;\n"
      "};\n"
      "\n"
      "output main_fragment(float2 texCoord : TEXCOORD0,\n" 
      "uniform sampler2D decal : TEXUNIT0, uniform input IN)\n"
      "{\n"
      "  output OUT;\n"
      "  OUT.color = tex2D(decal, texCoord);\n"
      "  return OUT;\n"
      "}\n";
#else
// Structure to hold vertex data.
struct COLORVERTEX
{
	float       Position[3];
	float       TextureUV[2];
	DWORD       Color;
};

COLORVERTEX Vertices[] =
{
	// {  X,	Y,	  Z,	U,	 V,		Color	}
	{ -1.f, -1.f, 0.0f,  0.0f,  1.0f, 0xFFFF0000 }, //1
	{ -1.f,  1.f, 0.0f,  0.0f,  0.0f, 0xFF0FF000 }, //2
	{  1.f, -1.f, 0.0f,  1.0f,  1.0f, 0xFF00FF00 }, //3
};
#endif

// Define the vertex elements.
static const D3DVERTEXELEMENT9 VertexElements[4] =
{
	{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
#if !defined(USE_RETRO_ARCH_SHADER)
	{ 0, 20, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
#endif
	D3DDECL_END()
};


void PcsxSetD3D(IDirect3DDevice9* device) {
	g_pd3dDevice = device;
}


#if defined(USE_RETRO_ARCH_SHADER)

static XMMATRIX WorldView;

#endif


void DrawPcsxSurface() {

#if defined(USE_RETRO_ARCH_SHADER)
	const float texture_size[2] = {retro_arch_shader_input.texture_size[0], retro_arch_shader_input.texture_size[1]};
	const float output_size[2] = {retro_arch_shader_input.output_size[0], retro_arch_shader_input.output_size[1]};
	const float video_size[2] = {retro_arch_shader_input.video_size[0], retro_arch_shader_input.video_size[1]};

	if (in_modelViewProj != NULL)
		pVsShaderTable->SetMatrix(g_pd3dDevice, in_modelViewProj, (D3DXMATRIX*)&WorldView);

	if(input_vs.texture_size != NULL)
		pVsShaderTable->SetFloatArray(g_pd3dDevice, input_vs.texture_size, texture_size, 2);
#ifndef _DEBUG
	if(input_vs.output_size != NULL)
		pVsShaderTable->SetFloatArray(g_pd3dDevice, input_vs.output_size, output_size, 2);
	if(input_vs.video_size != NULL)
		pVsShaderTable->SetFloatArray(g_pd3dDevice, input_vs.video_size, video_size, 2);
#endif
	
	if (input_ps.texture_size != NULL)
		pPsShaderTable->SetFloatArray(g_pd3dDevice, input_ps.texture_size, texture_size, 2);
#ifndef _DEBUG
	if(input_ps.output_size != NULL)
		pPsShaderTable->SetFloatArray(g_pd3dDevice, input_ps.output_size, output_size, 2);
	if(input_ps.video_size != NULL)
		pPsShaderTable->SetFloatArray(g_pd3dDevice, input_ps.video_size, video_size, 2);	
#endif
#endif

	// Set shaders.
	g_pd3dDevice->SetVertexShader( pVertexShader );
	g_pd3dDevice->SetPixelShader( pPixelShader );

	// Set texture	
	g_pd3dDevice->SetTexture(0, g_PsxTexture);	

	// set some stuff overwritten by xui ...
if(linearfilter)
{
///////////////////////////////////////////////////////////////////////////////////
    g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
///////////////////////////////////////////////////////////////////////////////////
}
else
{
///////////////////////////////////////////////////////////////////////////////////
    g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
///////////////////////////////////////////////////////////////////////////////////
}
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	g_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);

	// Set the vertex declaration.
	g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

	// Draw Pcsx surface
	g_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, Vertices, sizeof( COLORVERTEX ) );

}


extern "C" void UpdateScrenRes(int x,int y) {
#if defined(USE_RETRO_ARCH_SHADER)
	retro_arch_shader_input.video_size[0] = x;
	retro_arch_shader_input.video_size[1] = y;
#endif
	// Update Vertices
	Vertices[0].TextureUV[1] = (float) y / (float) PSX_HEIGHT;

	Vertices[2].TextureUV[0] = (float) x / (float) PSX_WIDTH;
	Vertices[2].TextureUV[1] = (float) y / (float) PSX_HEIGHT;
}

extern "C" void DisplayUpdate()
{
	// Clear the backbuffer.
	g_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);

	DrawPcsxSurface();
    // Present the backbuffer contents to the display.
	// g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
	VideoPresent();
}

static void ReleaseShader() {
	// free shader
	g_pd3dDevice->SetVertexShader( NULL );
	g_pd3dDevice->SetPixelShader( NULL );

	if(pPixelShader ) {
		pPixelShader->Release();
		pPixelShader = NULL;
	}

	if(pVertexShader ) {
		pVertexShader->Release();
		pVertexShader = NULL;
	}

	if(pPsShaderTable) {
		pPsShaderTable->Release();
		pPsShaderTable = NULL;
	}

	if (pVsShaderTable){
		pVsShaderTable->Release();
		pVsShaderTable = NULL;
	}
}

static bool _CompileShader(const char * effect_file_name) {
	// Buffers to hold compiled shaders and possible error messages
	ID3DXBuffer* pShaderCode = NULL;
	ID3DXBuffer* pErrorMsg = NULL;

	// Compile vertex shader.
	HRESULT hr = D3DXCompileShaderFromFileA( effect_file_name,
		NULL, NULL, vs_entry_point, "vs_3_0", 0,
		&pShaderCode, &pErrorMsg, &pVsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return false;
	}

	// Create vertex shader.
	g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pVertexShader );

	// Shader code is no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Compile pixel shader.
	hr = D3DXCompileShaderFromFileA( effect_file_name,
		NULL, NULL, ps_entry_point, "ps_3_0", 0,
		&pShaderCode, &pErrorMsg, &pPsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return false;
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pPixelShader );

	// Shader code no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Handle
	input_vs.video_size = pVsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_vs.texture_size = pVsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_vs.output_size = pVsShaderTable->GetConstantByName(NULL, "$IN.output_size");

	
	input_ps.video_size = pPsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_ps.texture_size = pPsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_ps.output_size = pPsShaderTable->GetConstantByName(NULL, "$IN.output_size");


	in_modelViewProj = pVsShaderTable->GetConstantByName(NULL, "$modelViewProj");

	return true;
}

bool LoadDefaultShader() {
	ReleaseShader();

	// Buffers to hold compiled shaders and possible error messages
	ID3DXBuffer* pShaderCode = NULL;
	ID3DXBuffer* pErrorMsg = NULL;

	// Compile vertex shader.
	HRESULT hr = D3DXCompileShader( stock_hlsl_program , ( UINT )strlen( stock_hlsl_program  ),
		NULL, NULL, vs_entry_point, "vs_3_0", 0,
		&pShaderCode, &pErrorMsg, &pVsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return false;
	}

	// Create vertex shader.
	g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pVertexShader );

	// Shader code is no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Compile pixel shader.
	hr = D3DXCompileShader( stock_hlsl_program , ( UINT )strlen( stock_hlsl_program  ),
		NULL, NULL, ps_entry_point, "ps_3_0", 0,
		&pShaderCode, &pErrorMsg, &pPsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return false;
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pPixelShader );

	// Shader code no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Handle
	input_vs.video_size = pVsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_vs.texture_size = pVsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_vs.output_size = pVsShaderTable->GetConstantByName(NULL, "$IN.output_size");

	
	input_ps.video_size = pPsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_ps.texture_size = pPsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_ps.output_size = pPsShaderTable->GetConstantByName(NULL, "$IN.output_size");


	in_modelViewProj = pVsShaderTable->GetConstantByName(NULL, "$modelViewProj");

	return true;
}

void LoadShaderFromFile(const char * filename) {
	ReleaseShader();

	if (!_CompileShader(filename)) {
		LoadDefaultShader();
	}
}

extern "C" unsigned int  VideoInit()
{
	// Buffers to hold compiled shaders and possible error messages
	ID3DXBuffer* pShaderCode = NULL;
	ID3DXBuffer* pErrorMsg = NULL;

#if !defined(USE_RETRO_ARCH_SHADER)
	// Compile vertex shader.
	HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
		NULL, NULL, "main", "vs_2_0", 0,
		&pShaderCode, &pErrorMsg, NULL );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create vertex shader.
	g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pVertexShader );

	// Shader code is no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Compile pixel shader.
	hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
		NULL, NULL, "main", "ps_2_0", 0,
		&pShaderCode, &pErrorMsg, NULL );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pPixelShader );

	// Shader code no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

#else
	WorldView = XMMatrixIdentity();

	retro_arch_shader_input.texture_size[0] = PSX_WIDTH;
	retro_arch_shader_input.texture_size[1] = PSX_HEIGHT;

	retro_arch_shader_input.output_size[0] = g_d3dpp.BackBufferWidth;
	retro_arch_shader_input.output_size[1] = g_d3dpp.BackBufferHeight;
#if 0
	// Compile vertex shader.
	HRESULT hr = D3DXCompileShaderFromFileA( effect_file_name,
		NULL, NULL, vs_entry_point, "vs_3_0", 0,
		&pShaderCode, &pErrorMsg, &pVsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create vertex shader.
	g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pVertexShader );

	// Shader code is no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Compile pixel shader.
	hr = D3DXCompileShaderFromFileA( effect_file_name,
		NULL, NULL, ps_entry_point, "ps_3_0", 0,
		&pShaderCode, &pErrorMsg, &pPsShaderTable );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pPixelShader );

	// Shader code no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Handle
	input_vs.video_size = pVsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_vs.texture_size = pVsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_vs.output_size = pVsShaderTable->GetConstantByName(NULL, "$IN.output_size");

	
	input_ps.video_size = pPsShaderTable->GetConstantByName(NULL, "$IN.video_size");
	input_ps.texture_size = pPsShaderTable->GetConstantByName(NULL, "$IN.texture_size");
	input_ps.output_size = pPsShaderTable->GetConstantByName(NULL, "$IN.output_size");


	in_modelViewProj = pVsShaderTable->GetConstantByName(NULL, "$modelViewProj");
#else
	LoadDefaultShader();
#endif

#endif

	// Create a vertex declaration from the element descriptions.
	g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );
		
	// Set shaders.
	g_pd3dDevice->SetVertexShader( pVertexShader );
	g_pd3dDevice->SetPixelShader( pPixelShader );

	// Set the vertex declaration.
	g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

/* 
	// Create texture for 16 bit color
	D3DXCreateTexture(
			g_pd3dDevice, PSX_WIDTH,
			PSX_HEIGHT, D3DX_DEFAULT, 0, D3DFMT_LIN_R5G6B5, D3DPOOL_MANAGED,
			&g_PsxTexture
			);
	D3DLOCKED_RECT texture_info;
	g_PsxTexture->LockRect( 0,  &texture_info, NULL, NULL );
	pPsxScreen = (unsigned char *)texture_info.pBits;

	
	// Memset fail :s => clear to black
	unsigned short * s = (unsigned short*)pPsxScreen;
	for(int i =0; i < PSX_WIDTH * PSX_HEIGHT; i++) {
		s[i] = 0;
	}
*/

// Create texture 32 bit color
	D3DXCreateTexture(
			g_pd3dDevice, PSX_WIDTH,
			PSX_HEIGHT, D3DX_DEFAULT, 0, D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED,
			&g_PsxTexture
			);
	D3DLOCKED_RECT texture_info;
	g_PsxTexture->LockRect( 0,  &texture_info, NULL, NULL );
	pPsxScreen = (unsigned char *)texture_info.pBits;

	
	// Memset fail :s => clear to black
	unsigned int * s = (unsigned int*)pPsxScreen;
	for(int i =0; i < PSX_WIDTH * PSX_HEIGHT; i++) {
		s[i] = 0;
	}

	g_PsxTexture->UnlockRect(0);

	g_pPitch = texture_info.Pitch;

	UpdateScrenRes(PSX_WIDTH, PSX_HEIGHT);

	return S_OK;
}

