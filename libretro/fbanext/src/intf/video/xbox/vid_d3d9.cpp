// Direct3D9 video output
// rewritten by regret  (Motion Blur source from VBA-M)
#include "burner.h"
#include "vid_directx_support.h"
#include "ScalingEffect.h"
#include <xgraphics.h>


#include <d3d9.h>
#include <d3dx9effect.h>

const float PI = 3.14159265358979323846f;

extern int ArcadeJoystick;
int dx9RenderStretch();
void dx9SetTargets();

static INT32 frameCount = 0;
static int nRotateGame;
 
struct d3dvertex {
	float x, y, z, rhw;
	float u, v;         //texture coords
};
 

LPDIRECT3DVERTEXBUFFER9 vertexBuffer = NULL;	 
D3DVertexShader* g_pGradientVertexShader = NULL;
D3DVertexDeclaration* g_pGradientVertexDecl = NULL;
 
extern IDirect3DDevice9 *pDevice;
extern IDirect3D9 *pD3D;
static IDirect3D9* pD3D = NULL;				// Direct3D interface
static D3DPRESENT_PARAMETERS d3dpp;
static IDirect3DDevice9* pD3DDevice = NULL;

static unsigned char mbCurrentTexture = 0;	// current texture for motion blur
static bool mbTextureEmpty = true;
static d3dvertex vertex[4];

static IDirect3DTexture9* pTexture = NULL;
static int nTextureWidth = 0;
static int nTextureHeight = 0;
D3DFORMAT textureFormat;
 
static D3DCOLOR osdColor = D3DCOLOR_ARGB(0xFF, 0xFF, 0xFF, 0xFF);

static int nGameImageWidth, nGameImageHeight;
static int nImageWidth, nImageHeight, nImageZoom;


extern int nXOffset;
extern int nYOffset;
extern int nXScale;
extern int nYScale;

static int nGameWidth = 0, nGameHeight = 0;				 

extern int bAdjustScreen;


// Projection matrices
D3DXMATRIX			m_matProj;
D3DXMATRIX			m_matWorld;
D3DXMATRIX			m_matView;

D3DXMATRIX			m_matPreProj;
D3DXMATRIX			m_matPreView;
D3DXMATRIX			m_matPreWorld;

// Pixel shader
char						pshader[30];
ScalingEffect*				psEffect;
LPDIRECT3DTEXTURE9			lpWorkTexture1;
LPDIRECT3DTEXTURE9			lpWorkTexture2;
LPDIRECT3DVOLUMETEXTURE9	lpHq2xLookupTexture;

bool 			psEnabled;
bool			preProcess;
 

static RECT Dest;

static int GetTextureSize(int nSize)
{
	int nTextureSize = 128;

	while (nTextureSize < nSize) {
		nTextureSize <<= 1;
	}

	return nTextureSize;
}
// ----------------------------------------------------------------------------

static inline TCHAR* TextureFormatString(D3DFORMAT nFormat)
{
	switch (nFormat) {
		case D3DFMT_X1R5G5B5:
			return _T("16-bit xRGB 1555");
		case D3DFMT_R5G6B5:
			return _T("16-bit RGB 565");
		case D3DFMT_X8R8G8B8:
			return _T("32-bit xRGB 8888");
		case D3DFMT_A8R8G8B8:
			return _T("32-bit ARGB 8888");
		case D3DFMT_A16B16G16R16F:
			return _T("64-bit ARGB 16161616fp");
		case D3DFMT_A32B32G32R32F:
			return _T("128-bit ARGB 32323232fp");
	}

	return _T("unknown format");
}

// Select optimal full-screen resolution
static inline int dx9SelectFullscreenMode(VidSDisplayScoreInfo* pScoreInfo)
{
	pScoreInfo->nBestWidth = nVidWidth;
	pScoreInfo->nBestHeight = nVidHeight; 

	if (!bDrvOkay && (pScoreInfo->nBestWidth < 640 || pScoreInfo->nBestHeight < 480)) {
		return 1;
	}

	return 0;
}

// ----------------------------------------------------------------------------

static inline void dx9ReleaseTexture()
{
 
	//pD3DDevice->SetTexture( 0, NULL);
	RELEASE(pTexture);

}

static inline int dx9Exit()
{	 
 
    pD3DDevice->SetStreamSource(0,NULL,0,NULL);	
    pD3DDevice->SetVertexShader( NULL );
    pD3DDevice->SetVertexDeclaration( NULL );

	dx9ReleaseTexture();

    RELEASE(pTexture); 
    RELEASE(lpWorkTexture1);
    RELEASE(lpWorkTexture2);
    RELEASE(lpHq2xLookupTexture);

	VidSFreeVidImage();

	if (psEffect)
	{	
		delete psEffect;
		psEffect = NULL;
	}

	if( vertexBuffer )
	{
		vertexBuffer->Release();
		vertexBuffer = NULL;
	}
 	
	nRotateGame = 0;
 
 
	return 0;
}

static inline int dx9Resize(int width, int height)
{	
     
    if (FAILED(pD3DDevice->CreateTexture(width, height, 1, D3DUSAGE_CPU_CACHED_MEMORY, D3DFMT_LIN_R5G6B5, 0, &pTexture, NULL))) {
#ifdef PRINT_DEBUG_INFO
            dprintf(_T("  * Error: Couldn't create texture.\n"));
#endif
            return 1;
    }
#ifdef PRINT_DEBUG_INFO
    dprintf(_T("  * Allocated a %i x %i (%s) image texture.\n"), width, height, TextureFormatString(textureFormat));
#endif


	return 0;
}

void doStretch(void)
{	
	XINPUT_STATE state;

	dx9SetTargets();

	while ( 1 )
	{

		XInputGetState( 0, &state );

		dx9RenderStretch();
	 		 	
		if (ArcadeJoystick != 1)
		{
			if (state.Gamepad.sThumbLX <= -32000)
			{
				nXOffset -= 1;
			}
			else if (state.Gamepad.sThumbLX >= 32000)
			{
				nXOffset += 1;
			}

			if (state.Gamepad.sThumbLY <= -32000)
			{
				nYOffset += 1;
			}	
			else if (state.Gamepad.sThumbLY >= 32000)
			{
				nYOffset -= 1;
			}

			if (state.Gamepad.sThumbRX <= -32000 )
			{
				nXScale -= 1;
			}
			else if (state.Gamepad.sThumbRX >= 32000)
			{
				nXScale += 1;
			}

			if (state.Gamepad.sThumbRY <= -32000 )
			{
				nYScale += 1;
			}	
			else if (state.Gamepad.sThumbRY >= 32000)
			{
				nYScale -= 1;
			}
		}
		else
		{

			if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT && state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			{
				nXScale -= 1;
			}
			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT && state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			{
				nXScale += 1;
			}

			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN && state.Gamepad.wButtons & XINPUT_GAMEPAD_B )
			{
				nYScale += 1;
			}	
			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP && state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			{
				nYScale -= 1;
			}
			
			if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			{
				nXOffset -= 1;
			}
			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			{
				nXOffset += 1;
			}

			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
			{
				nYOffset += 1;
			}	
			else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
			{
				nYOffset -= 1;
			}

			

		}

		if(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
		{	
				nYOffset = 0;
				nXOffset = 0;
				nYScale = 0;
				nXScale = 0;
		}

		if (ArcadeJoystick != 1)
		{
			if(state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			{	
				//pDevice->SetTexture( 0, NULL); 
				pDevice->SetStreamSource(0, NULL, 0, 0);
				bAdjustScreen = 0;		
				break;
			}
		}
		else
		{
			if(state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
			{	
				//pDevice->SetTexture( 0, NULL); 
				pDevice->SetStreamSource(0, NULL, 0, 0);
				bAdjustScreen = 0;		
				break;
			}




		}
		
	}
	 
}



static inline int dx9TextureInit()
{
	if (nRotateGame & 1) {
		nVidImageWidth = nGameHeight;
		nVidImageHeight = nGameWidth;
	} else {
		nVidImageWidth = nGameWidth;
		nVidImageHeight = nGameHeight;
	}

	nGameImageWidth = nVidImageWidth;
	nGameImageHeight = nVidImageHeight;

	nVidImageDepth = nVidScrnDepth;
	 

	switch (nVidImageDepth) {
		case 32:
			textureFormat = D3DFMT_X8R8G8B8;
			break;
		case 16:
			textureFormat = D3DFMT_R5G6B5;
			break;
		case 15:
			textureFormat = D3DFMT_X1R5G5B5;
			break;
	}

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;					// Set Burn library Bytes per pixel

	// Use our callback to get colors:
	SetBurnHighCol(nVidImageDepth);

	// Make the normal memory buffer
	if (VidSAllocVidImage()) {
		dx9Exit();
		return 1;
	}

	nTextureWidth = GetTextureSize(nGameImageWidth);
	nTextureHeight = GetTextureSize(nGameImageHeight);

	if (dx9Resize(nTextureWidth, nTextureHeight)) {
		return 1;
	}

	return 0;
}

//  Vertex format:
  //
  //  0----------1
  //  |         /|
  //  |       /  |
  //  |     /    |
  //  |   /      |
  //  | /        |
  //  2----------3
  //
  //  (x,y) screen coords, in pixels
  //  (u,v) texture coords, betweeen 0.0 (top, left) to 1.0 (bottom, right)
static inline int dx9SetVertex(
	unsigned int px, unsigned int py, unsigned int pw, unsigned int ph,
    unsigned int tw, unsigned int th,
    unsigned int x, unsigned int y, unsigned int w, unsigned int h
    )
{
	// configure triangles
	// -0.5f is necessary in order to match texture alignment to display pixels
	float diff = -0.5f;

	void *pLockedVertexBuffer;
 
	if (nRotateGame & 1) {
		if (nRotateGame & 2) {
			vertex[2].x = vertex[3].x = (float)(y    ) + diff + nXOffset + nXScale;
			vertex[0].x = vertex[1].x = (float)(y + h) + diff + nXOffset;
			vertex[1].y = vertex[3].y = (float)(x + w) + diff + nYOffset + nYScale;
			vertex[0].y = vertex[2].y = (float)(x    ) + diff + nYOffset;
		} else {
			vertex[0].x = vertex[1].x = (float)(y    ) + diff + nXOffset + nXScale;
			vertex[2].x = vertex[3].x = (float)(y + h) + diff + nXOffset;
			vertex[1].y = vertex[3].y = (float)(x    ) + diff + nYOffset + nYScale;
			vertex[0].y = vertex[2].y = (float)(x + w) + diff + nYOffset;
		}
	} else {
		if (nRotateGame & 2) {
			vertex[1].x = vertex[3].x = (float)(y    ) + diff + nXOffset + nXScale;
			vertex[0].x = vertex[2].x = (float)(y + h) + diff + nXOffset;
			vertex[2].y = vertex[3].y = (float)(x    ) + diff + nYOffset + nYScale;
			vertex[0].y = vertex[1].y = (float)(x + w) + diff + nYOffset;
		} else {
			vertex[0].x = vertex[2].x = (float)(x    ) + diff + nXOffset + nXScale;
			vertex[1].x = vertex[3].x = (float)(x + w) + diff + nXOffset;
			vertex[0].y = vertex[1].y = (float)(y    ) + diff + nYOffset + nYScale;
			vertex[2].y = vertex[3].y = (float)(y + h) + diff + nYOffset;
		}
	}

	float rw = (float)w / (float)pw * (float)tw;
	float rh = (float)h / (float)ph * (float)th;

	vertex[0].u = vertex[2].u = (float)(px    ) / rw;
	vertex[1].u = vertex[3].u = (float)(px + w) / rw;
	vertex[0].v = vertex[1].v = (float)(py    ) / rh;
	vertex[2].v = vertex[3].v = (float)(py + h) / rh;
 

	// Z-buffer and RHW are unused for 2D blit, set to normal values
	vertex[0].z = vertex[1].z = vertex[2].z = vertex[3].z = 0.0f;	 
	vertex[0].rhw = vertex[1].rhw = vertex[2].rhw = vertex[3].rhw = 1.0f;
 
	pD3DDevice->SetStreamSource(0,NULL,0,NULL);	

	HRESULT hr = vertexBuffer->Lock(0,0,&pLockedVertexBuffer,NULL);
	memcpy(pLockedVertexBuffer,vertex,sizeof(vertex));
	vertexBuffer->Unlock();	

	pD3DDevice->SetStreamSource(	0,						  // Stream number
									vertexBuffer,	0,	      // Stream data
									sizeof(d3dvertex) );		  // Vertex stride


	pD3DDevice->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
 
	return 0;
}

// ==> osd for dx9 video output (ugly), added by regret
static inline int dx9CreateFont()
{
	return 0;
}

static inline void dx9DrawText()
{
 
}
// <== osd for dx9 video output (ugly)

static inline int dx9Init()
{

#ifdef ENABLE_PROFILING
	ProfileInit();
#endif

#ifdef PRINT_DEBUG_INFO
	dprintf(_T("*** Initialising Direct3D 9 blitter.\n"));
#endif

	HRESULT Result;
	XVIDEO_MODE VideoMode; 
	XMemSet( &VideoMode, 0, sizeof(XVIDEO_MODE) ); 
	XGetVideoMode( &VideoMode );
	 
	hVidWnd = (HWND)0x01;
	nVidFullscreen = 1;
	memset(&d3dpp, 0, sizeof(d3dpp));
	
	VidSDisplayScoreInfo ScoreInfo;
	memset(&ScoreInfo, 0, sizeof(VidSDisplayScoreInfo));

	if (dx9SelectFullscreenMode(&ScoreInfo)) {
		dx9Exit();
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("  * Error: Couldn't determine display mode.\n"));
#endif
		return 1;
	}

	if (!VideoMode.fIsWideScreen)
	{
		d3dpp.Flags |=  D3DPRESENTFLAG_NO_LETTERBOX;
	}
	
	// Determine high-definition back buffer values.
	d3dpp.BackBufferWidth = 1280;
	d3dpp.BackBufferHeight = 720;
 
	pD3DDevice = pDevice;
 
	nVidScrnWidth = d3dpp.BackBufferWidth; nVidScrnHeight = d3dpp.BackBufferHeight;
	nVidScrnDepth = 16;
 
	//VidInitInfo();
	
	mbCurrentTexture = 0;
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	// apply vertex alpha values to texture

	// for filter
	nRotateGame = 0;
	if (bDrvOkay) {
		// Get the game screen size
		BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				int n = nGameWidth;
				nGameWidth = nGameHeight;
				nGameHeight = n;
				nRotateGame |= (nVidRotationAdjust & 2);
			} else {
				nRotateGame |= 1;
			}
		}
		
		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}
	}

	// Initialize the buffer surfaces
	if (dx9TextureInit()) {
		dx9Exit();
		return 1;
	}


	nImageWidth = 0; nImageHeight = 0;

 
	unsigned int vertexbuffersize =  sizeof(d3dvertex) << 2;

	psEffect = new ScalingEffect(pD3DDevice);
	psEffect->LoadEffect(selectedShader);

	pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	
	Result = psEffect->Validate();

	if(psEffect->hasPreprocess())
	{
		vertexbuffersize = sizeof(d3dvertex) * 8;
		preProcess = true;
	}

	if (NULL == vertexBuffer)
	{
		Result = pD3DDevice->CreateVertexBuffer( vertexbuffersize ,D3DUSAGE_CPU_CACHED_MEMORY,0,0,&vertexBuffer,NULL);
		if(FAILED(Result)) {
			 
			return false;
		}
	}


	// Create vertex declaration
    if( NULL == g_pGradientVertexDecl )
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
			{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if( FAILED( pD3DDevice->CreateVertexDeclaration( decl, &g_pGradientVertexDecl ) ) )
            return E_FAIL;
    }
 
	
	pD3DDevice->SetVertexDeclaration( g_pGradientVertexDecl ); 		     
	pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pD3DDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

	D3DXMatrixIdentity(&m_matWorld);
	D3DXMatrixIdentity(&m_matView);
	D3DXMatrixIdentity(&m_matProj);

    

	if(preProcess) 
	{
    	    // Projection is (0,0,0) -> (1,1,1)
	    D3DXMatrixOrthoOffCenterLH(&m_matPreProj, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

	    // Align texels with pixels
	    D3DXMatrixTranslation(&m_matPreView, -0.5f/1280, 0.5f/720, 0.0f);

	    // Identity for world
	    D3DXMatrixIdentity(&m_matPreWorld);



	}  


	return 0;
}



void dx9ClearTargets()
{
	//pD3DDevice->SetVertexShader(NULL);
	//pD3DDevice->SetPixelShader(NULL);
	//pD3DDevice->SetStreamSource(0,NULL,0,NULL);		
	//pD3DDevice->SetTexture( 0, NULL); 	 
}
 
static inline int dx9Reset()
{
#ifdef PRINT_DEBUG_INFO
	dprintf(_T("*** Resestting Direct3D device.\n"));
#endif

	dx9ReleaseTexture();

	if (FAILED(pD3DDevice->Reset(&d3dpp))) {
		return 1;
	}

	dx9TextureInit();

	nImageWidth = 0; nImageHeight = 0;

	return 0;
}

void doResetDX9()
{
	pD3DDevice->Reset(&d3dpp);
}

void dx9SetTargets()
{
    //pD3DDevice->SetVertexShader( NULL );
    //pD3DDevice->SetVertexDeclaration( NULL );
    //pD3DDevice->SetPixelShader( NULL );
	//pD3DDevice->SetTexture(0, NULL );

	//pD3DDevice->SetVertexShader( g_pGradientVertexShader );		
	//pD3DDevice->SetPixelShader( g_pPixelShader );
    //pD3DDevice->SetVertexDeclaration( g_pGradientVertexDecl );
	//pD3DDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
}

static inline int dx9Scale(RECT* pRect, int nWidth, int nHeight)
{
	return VidSScaleImage(pRect, nWidth, nHeight, bVidScanRotate);
 
}

// Copy BlitFXsMem to pddsBlitFX
static inline int dx9Render()
{

	HRESULT hr;
    UINT uPasses, psActive = 1;
	D3DLOCKED_RECT d3dlr;

	frameCount ++;

	// draw the current frame to the screen
	pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET,  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);


	if (pTexture==NULL)
	{
		return 0;
	}

 
	GetClientScreenRect(hVidWnd, &Dest);

	dx9Scale(&Dest, nGameWidth, nGameHeight);

	{
		int nNewImageWidth  = nRotateGame ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
		int nNewImageHeight = nRotateGame ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);

		if (nImageWidth != nNewImageWidth || nImageHeight != nNewImageHeight) {
			nImageWidth  = nNewImageWidth;
			nImageHeight = nNewImageHeight;

			int nWidth = nGameImageWidth;
			int nHeight = nGameImageHeight;
			
			
			dx9SetVertex(0, 0, nWidth, nHeight, nTextureWidth, nTextureHeight, nRotateGame ? Dest.top : Dest.left, nRotateGame ? Dest.left : Dest.top, nImageWidth, nImageHeight);

		 
			
		}
	}


	// Copy the game image onto a texture for rendering	

	pTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOOVERWRITE);

	int pitch = d3dlr.Pitch;
	unsigned char* __restrict pd = (unsigned char*)d3dlr.pBits;
	unsigned char* __restrict ps = pVidImage + nVidImageLeft * nVidImageBPP;	 
	int p = d3dlr.Pitch;
	int s = nVidImageWidth * nVidImageBPP;

	for (int y = 0; y < nVidImageHeight; y++, pd += p, ps += nVidImagePitch) {
		XMemCpyStreaming(pd, ps, s);
	}

	pTexture->UnlockRect(0); 	
	psEffect->SetTextures(pTexture, lpWorkTexture1, lpWorkTexture2, lpHq2xLookupTexture);

	uPasses = 0;	 

	psEffect->Begin(ScalingEffect::Combine, &uPasses);	 
	hr=psEffect->BeginPass(0);	    

	// Render the vertex buffer contents

	psEffect->SetVideoInputs((float)nTextureWidth,(float)nTextureHeight,1280.0,720.0, frameCount); 
	pD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
	pD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);
	pD3DDevice->SetVertexDeclaration( g_pGradientVertexDecl );
	pD3DDevice->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
	pD3DDevice->SetStreamSource(	0,						  // Stream number
									vertexBuffer,	0,	      // Stream data
									sizeof(d3dvertex) );		  // Vertex stride
	pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2 );
	
	psEffect->EndPass();	
	psEffect->End();
 
	return 0;
}
 
int RenderMap()
{

	HRESULT hr;
    UINT uPasses, psActive = 1;
	D3DLOCKED_RECT d3dlr;
	
	frameCount ++;
 
	// draw the current frame to the screen
	pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET,  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);


	if (pTexture==NULL)
	{
		return 0;
	}
	
 
	GetClientScreenRect(hVidWnd, &Dest);
	dx9Scale(&Dest, nGameWidth, nGameHeight);

	int nNewImageWidth  = nRotateGame ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
	int nNewImageHeight = nRotateGame ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);
		 
	nImageWidth  = nNewImageWidth;
	nImageHeight = nNewImageHeight;

	int nWidth = nGameImageWidth;
	int nHeight = nGameImageHeight;
	

	pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, bVidDX9Bilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);

	dx9SetVertex(0, 0, nWidth, nHeight, nTextureWidth, nTextureHeight, nRotateGame ? Dest.top : Dest.left, nRotateGame ? Dest.left : Dest.top, nImageWidth, nImageHeight);
 
	// Copy the game image onto a texture for rendering	

	pTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOOVERWRITE);

	int pitch = d3dlr.Pitch;
	unsigned char* pd = (unsigned char*)d3dlr.pBits;
	unsigned char* ps = pVidImage + nVidImageLeft * nVidImageBPP;	 
	int p = d3dlr.Pitch;
	int s = nVidImageWidth * nVidImageBPP;

	for (int y = 0; y < nVidImageHeight; y++, pd += p, ps += nVidImagePitch) {
		XMemCpyStreaming(pd, ps, s);
	}

	pTexture->UnlockRect(0); 	
	 
	if (psEnabled)
	{
		psEffect->SetTextures(pTexture, lpWorkTexture1, lpWorkTexture2, lpHq2xLookupTexture);
		uPasses = 0;	 
		psEffect->SetVideoInputs((float)nTextureWidth,(float)nTextureHeight,1280.0,720.0, frameCount); 
		psEffect->Begin(ScalingEffect::Combine, &uPasses);	 
		hr=psEffect->BeginPass(0);	    
	}

	// Render the vertex buffer contents

	pD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
	pD3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);
	pD3DDevice->SetVertexDeclaration( g_pGradientVertexDecl );
	pD3DDevice->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);
	pD3DDevice->SetStreamSource(	0,						  // Stream number
									vertexBuffer,	0,	      // Stream data
									sizeof(d3dvertex) );		  // Vertex stride
	pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2 );
	
	if (psEnabled)
	{
		psEffect->EndPass();	
		psEffect->End();
	}
 
	return 0;
}

int dx9RenderStretch()
{
	
	if (pTexture==NULL)
	{
		return 0;
	}
 
	GetClientScreenRect(hVidWnd, &Dest);

	dx9Scale(&Dest, nGameWidth, nGameHeight);

	{
		int nNewImageWidth  = nRotateGame ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
		int nNewImageHeight = nRotateGame ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);

 
		nImageWidth  = nNewImageWidth;
		nImageHeight = nNewImageHeight;

		int nWidth = nGameImageWidth;
		int nHeight = nGameImageHeight;

 
		dx9SetVertex(0, 0, nWidth, nHeight, nTextureWidth, nTextureHeight, nRotateGame ? Dest.top : Dest.left, nRotateGame ? Dest.left : Dest.top, nImageWidth, nImageHeight);
 
	}

 
	{
		// Copy the game image onto a texture for rendering
		D3DLOCKED_RECT d3dlr;

		pTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOOVERWRITE);

		int pitch = d3dlr.Pitch;
		unsigned char* pd = (unsigned char*)d3dlr.pBits;
		unsigned char* ps = pVidImage + nVidImageLeft * nVidImageBPP;	 
		int p = d3dlr.Pitch;
		int s = nVidImageWidth * nVidImageBPP;

		for (int y = 0; y < nVidImageHeight; y++, pd += p, ps += nVidImagePitch) {
			memcpy(pd, ps, s);
		}

		pTexture->UnlockRect(0);
	}
 
 
	// draw the current frame to the screen
	pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET,  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	//pD3DDevice->SetTexture( 0, pTexture );	 	 
	pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2); 
	 

	pD3DDevice->EndScene();
	pD3DDevice->Present(NULL, NULL, NULL, NULL);

	return 0;
}

// Run one frame and render the screen
inline int dx9Frame(bool bRedraw)		// bRedraw = 0
{
	if (pVidImage == NULL) {
		return 1;
	}

 
	if (bDrvOkay) {
		if (bRedraw) {					// Redraw current frame
			if (BurnDrvRedraw()) {
				BurnDrvFrame();			// No redraw function provided, advance one frame
			}
		} else {
			BurnDrvFrame();				// Run one frame and draw the screen
		}
	}
 
	dx9Render();

	if (bAdjustScreen == 1)
	{
		doStretch();
	}

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static inline int dx9Paint(int bValidate)
{
 
	RECT rect = { 0, 0, 0, 0 };

	if (!nVidFullscreen) {
		GetClientScreenRect(hVidWnd, &rect);

		dx9Scale(&rect, nGameWidth, nGameHeight);

		if ((rect.right - rect.left) != (Dest.right - Dest.left)
			|| (rect.bottom - rect.top ) != (Dest.bottom - Dest.top)) {
			bValidate |= 2;
		}
	}

	if (bValidate & 2) {
		dx9Render();
	}

	pD3DDevice->Present(NULL, NULL, NULL, NULL);

	return 0;
}

// ----------------------------------------------------------------------------

static inline int dx9GetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutD3D = { dx9Init, dx9Exit, dx9Frame, dx9Paint, dx9Scale, dx9GetSettings };
