#pragma once

#include <SDL.h>
#include <gfx/SDL_gfxPrimitives.h>

inline int fastline(SDL_Surface * dst, Sint16 x1, Sint16 y1,
		Sint16 x2, Sint16 y2, SDL_Color color){
	return lineRGBA(dst, x1, y1, x2, y2, color.r, color.g, color.b, 0xFF);
}

inline int fastlineA(SDL_Surface * dst, Sint16 x1, Sint16 y1,
		Sint16 x2, Sint16 y2, SDL_Color color, Sint16 alpha){
	return lineRGBA(dst, x1, y1, x2, y2, color.r, color.g, color.b, alpha);
}

inline int rect(SDL_Surface * dst, Sint16 x1, Sint16 y1,
		Sint16 x2, Sint16 y2, SDL_Color color){
	return rectangleRGBA(dst, x1, y1, x2, y2, color.r, color.g, color.b, 0xFF);
}

inline void DrawRectAlpha(SDL_Surface* dest, SDL_Rect rect, SDL_Color color, Uint8 alpha) {
    // 1. Crear una superficie temporal del tamaño del rectángulo
	SDL_Surface* temp = SDL_CreateRGBSurface(dest->flags, rect.w, rect.h, dest->format->BitsPerPixel, 
                                             dest->format->Rmask, dest->format->Gmask, dest->format->Bmask, dest->format->Amask);
    
    // 2. Pintar la superficie del color deseado
    Uint32 colorMapped = SDL_MapRGB(temp->format, color.r, color.g, color.b);
    SDL_FillRect(temp, NULL, colorMapped);
    
    // 3. Establecer el nivel de transparencia global de la superficie
    SDL_SetAlpha(temp, SDL_SRCALPHA, alpha);
    
    // 4. Hacer el Blit a la superficie de destino (aquí ocurre la mezcla)
    SDL_Rect dstPos = { rect.x, rect.y, 0, 0 };
    SDL_BlitSurface(temp, NULL, dest, &dstPos);
    
    // 5. Liberar la memoria
    SDL_FreeSurface(temp);
}

