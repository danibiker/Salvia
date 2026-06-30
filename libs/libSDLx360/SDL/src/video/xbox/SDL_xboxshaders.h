#ifndef _SDL_xboxshaders_h
#define _SDL_xboxshaders_h

#include "SDL.h"
#include <xtl.h>
#include <d3dx9.h>
#include <XGraphics.h>

void initShaders();
void destroyShaders();
void XBOX_SelectEffect(int effectID);

/* Shader source strings extraidos a un header neutro compartido con la
   ruta D3D9 de Windows (src/video/win_d3d9.cpp). HLSL puro, sin includes
   de plataforma. */
#include "SDL_shaders_src.h"

#endif
