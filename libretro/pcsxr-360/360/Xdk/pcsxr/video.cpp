#include <xtl.h>
#include <stdio.h>

//--------------------------------------------------------------------------------------
// Globals
//-------------------------------------------------------------------------------------
IDirect3D9*						g_pD3D = NULL; // Used to create the D3DDevice
IDirect3DDevice9*				g_pd3dDevice = NULL; // the rendering device
D3DPRESENT_PARAMETERS			g_d3dpp;


#define MAX_FRONT_BUFFERS 3



// Multiple front buffers
static IDirect3DTexture9* pFrontBuffer[MAX_FRONT_BUFFERS];
static int iFrontBufferSelected;

// in gpu plugin
void PcsxSetD3D(IDirect3DDevice9* device);

bool linearfilter;

void InitD3D() {

	// Create the D3D object.
	g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	// Set up the structure used to create the D3DDevice.
	XVIDEO_MODE VideoMode;
	ZeroMemory( &VideoMode, sizeof( VideoMode ) );
	XGetVideoMode( &VideoMode );


	//todo set backbuffer width and height remotly
	// Set up the structure used to create the D3DDevice.
	ZeroMemory( &g_d3dpp, sizeof( g_d3dpp ) );
	g_d3dpp.BackBufferWidth = 1280;
	if (VideoMode.fIsWideScreen) {
		g_d3dpp.BackBufferHeight = 720;
	} else {
		g_d3dpp.BackBufferHeight = 1024;
	}

	g_d3dpp.BackBufferFormat =  D3DFMT_X8R8G8B8;
	g_d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
	g_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	g_d3dpp.MultiSampleQuality = 0;
	g_d3dpp.BackBufferCount = 0;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	
    // Required to support async swaps
    g_d3dpp.DisableAutoFrontBuffer = TRUE;

	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	// Create the Direct3D device.
	g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_BUFFER_2_FRAMES,
		&g_d3dpp, &g_pd3dDevice );

	g_pd3dDevice->SetRenderState(D3DRS_PRESENTIMMEDIATETHRESHOLD, 8);

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

	
    // Allocate front buffers
    for( int i = 0; i < MAX_FRONT_BUFFERS; ++i )
    {
        g_pd3dDevice->CreateTexture( g_d3dpp.BackBufferWidth, 
            g_d3dpp.BackBufferHeight, 
            1, 
            0, 
            g_d3dpp.FrontBufferFormat, 
            0, 
            &pFrontBuffer[i], 
            NULL );
    }

	iFrontBufferSelected = 0;

	PcsxSetD3D(g_pd3dDevice);

}


void VideoPresent() {	
    IDirect3DTexture9* pCurFrontBuffer = pFrontBuffer[iFrontBufferSelected];

	g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0|D3DRESOLVE_CLEARRENDERTARGET|D3DRESOLVE_CLEARDEPTHSTENCIL, NULL, pCurFrontBuffer, NULL, 0, 0, NULL, 0.0f, 0, NULL );

	g_pd3dDevice->SynchronizeToPresentationInterval();

	g_pd3dDevice->Swap( pCurFrontBuffer, NULL );

	iFrontBufferSelected++;
	iFrontBufferSelected = iFrontBufferSelected % MAX_FRONT_BUFFERS;
}