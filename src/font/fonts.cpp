#pragma once

#include "fonts.h"
#include <font/Arimo_Regular.ttf.h>
#include <utils/logger.h>

Fileio Fonts::fileio;
TTF_Font* Fonts::vFonts[2];
int Fonts::s_fontSize = 0;
SDL_mutex *Fonts::mutex = NULL;

Fonts::Fonts(){
	mutex = SDL_CreateMutex();
	if (TTF_Init() == -1) {
		LOG_ERROR("Error TTF_Init: %s\n", TTF_GetError());
	}
}

Fonts::~Fonts(){
	LOG_DEBUG("Deleting Fonts...");
	destroy();
}

/**
    * 
    */
void Fonts::destroy(){
    for (unsigned int i=0; i < 2; i++){
        if (vFonts[i] != NULL){
            TTF_CloseFont(vFonts[i]);
            vFonts[i] = NULL;
        }
    }
	TTF_Quit();
	fileio.clearFile();
	if (mutex) {
		SDL_DestroyMutex(mutex);
		mutex = NULL;
	}
}
        
/**
    * 
    */
void Fonts::initFonts(int fontSize){
	s_fontSize = fontSize;   // necesario para reabrir copias independientes
	vFonts[FONTBIG] = NULL;
	vFonts[FONTSMALL] = NULL;
	fileio.loadFromMem(Arimo_Regular_ttf, Arimo_Regular_ttf_size);
	SDL_RWops *RWOps = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps != NULL){
		vFonts[FONTBIG] = TTF_OpenFontRW(RWOps, 1, fontSize);
		if (vFonts[FONTBIG] == NULL) {
			LOG_ERROR("Error al cargar fuente grande: %s\n", TTF_GetError());
		} 
	}

	SDL_RWops *RWOps2 = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps2 != NULL){
		vFonts[FONTSMALL] = TTF_OpenFontRW(RWOps2, 1, fontSize - 9);
		if (vFonts[FONTSMALL] == NULL) {
			LOG_ERROR("Error al cargar fuente pequenya: %s\n", TTF_GetError());
		} 
	}
}

TTF_Font *Fonts::getFont(int fontId){
    if (fontId <= FONTSMALL && fontId >= 0)
        return vFonts[fontId];
    else 
        return NULL;
}

std::size_t Fonts::idxToCutTTF(std::string text, int maxW, int fontId){
    if (text.empty())
        return 0;

	int textW = getSize(fontId, text);

    if (textW < maxW){
        return text.length();
    }

    size_t i = 1;
    while(i < text.length()){
		textW = getSize(fontId, text.substr(0, i));
        if (textW >= maxW){
            i--;
            break;
        }
        i++;
    }
    return i;
}

int Fonts::getSize(int fontId, std::string text){
	ScopedFontLock lock(mutex);
	int textW = 0;
	TTF_Font *font = getFont(fontId);
	if (font == NULL || text.empty()) 
		return 0;
	TTF_SizeUTF8(font, text.c_str(), &textW, NULL);
	return textW;
}

int Fonts::getSize(TTF_Font* font, const std::string& text){
	ScopedFontLock lock(mutex);
	if (font == NULL || text.empty()) 
		return 0;
	int textW = 0;
	TTF_SizeUTF8(font, text.c_str(), &textW, NULL);
	return textW;
}

void Fonts::getSize(TTF_Font* font, const std::string& text, int &w, int &h){
	ScopedFontLock lock(mutex);
	w = 0; 
	h = 0;
	if (font == NULL || text.empty()) 
		return;
	TTF_SizeUTF8(font, text.c_str(), &w, &h);
}

int Fonts::getLineSkip(int fontId){
	ScopedFontLock lock(mutex);
	TTF_Font *font = getFont(fontId);
	if (font == NULL) 
		return 0;
	
	return TTF_FontLineSkip(font);
}

TTF_Font* Fonts::createIndependentFont(int fontId){
	ScopedFontLock lock(mutex);
	if (fontId < 0 || fontId > FONTSMALL) 
		return NULL;
	if (s_fontSize <= 0 || fileio.getFile() == NULL) 
		return NULL;

	const int sz = (fontId == FONTBIG) ? s_fontSize : (s_fontSize - 9);
	SDL_RWops* rw = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (!rw) return NULL;
	// 2do arg = 1 -> TTF cierra el RWops al cerrar la fuente
	TTF_Font* font = TTF_OpenFontRW(rw, 1, sz);
	if (!font) {
		LOG_ERROR("Fonts::createIndependentFont(%d): %s", fontId, TTF_GetError());
	}
	return font;
}

std::string Fonts::recortarAlTamanyo(std::string text, int maxWidth){
	std::string newText = text;
	TTF_Font* font = Fonts::getFont(Fonts::FONTBIG);
	if (!font) 
		return newText;

	int textPixelSize = getSize(font, text);

	if (textPixelSize > maxWidth) {
		int totalChars = text.length();
    
		// 1. Precalculamos el ancho promedio por caracter
		float avgCharWidth = (float)textPixelSize / (float)totalChars;
    
		// 2. Estimamos cuantos caracteres sobran para que quepa (incluyendo el "...")
		int dotsWidth = getSize(font, "...");
    
		int targetWidth = maxWidth - dotsWidth;
		int charsThatFit = (int)(targetWidth / avgCharWidth);
    
		// 3. Aplicamos el recorte inicial basado en la estimacion
		// Queremos la mitad de los que caben al principio y la otra mitad al final
		int leftPart = charsThatFit / 2;
		int rightPart = totalChars - (charsThatFit / 2);
    
		newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
		textPixelSize = getSize(font, newText);

		// 4. Ajuste fino (por si la estimacion fue optimista debido a caracteres anchos como 'W')
		// Este bucle se ejecutara como mucho 1 o 2 veces, ahorrando mucha CPU
		while (textPixelSize > maxWidth && leftPart > 0 && rightPart < totalChars) {
			if (leftPart > 0) leftPart--;
			if (rightPart < totalChars) rightPart++;
        
			newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
			textPixelSize = getSize(font, newText);
		}
	}

	return newText;
}

void Fonts::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = getLineSkip(Fonts::FONTSMALL);
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}

SDL_Rect Fonts::drawText(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg){
	SDL_Rect dest = { x, y, 0, 0 };
	if (font && s != NULL && s[0] != '\0') {
		ScopedFontLock lock(mutex);
		#ifdef _XBOX 
		SDL_Surface* textSurf = Fonts::renderUtf8Solid(font, s, color);
		#else
		SDL_Surface* textSurf = Fonts::renderUtf8Blended(font, s, color);
		#endif
				
		if (textSurf) {
			dest.w = textSurf->w;
			dest.h = textSurf->h;
			SDL_BlitSurface(textSurf, NULL, surface, &dest);
			SDL_FreeSurface(textSurf);
		} else {
			LOG_ERROR("Error al crear la surface para: %s\n", s);
		}
	} else {
		LOG_ERROR("Error al comprobar los parametros de entrada: %s\n", s);
	}
	return dest;
}

SDL_Rect Fonts::drawTextTransparent(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg, JFY_TYPE& justifyHelper){
	SDL_Rect dest = { x, y, 0, 0 };
	if (font && s != NULL && s[0] != '\0') {
		ScopedFontLock lock(mutex);
		#ifdef _XBOX 
		SDL_Surface* textSurf = Fonts::renderUtf8Solid(font, s, color);
		#else
		SDL_Surface* textSurf = Fonts::renderUtf8Blended(font, s, color);
		#endif
		if (textSurf) {
			dest.w = textSurf->w;
			dest.h = textSurf->h;
			dest.x += justifyHelper.getJustification(textSurf->w);
			SDL_BlitSurface(textSurf, NULL, surface, &dest);
			SDL_FreeSurface(textSurf);
		} else {
			LOG_ERROR("Error al crear la surface para: %s\n", s);
		}
	} else {
		LOG_ERROR("Error al comprobar los parametros de entrada: %s\n", s);
	}
	return dest;
}

void Fonts::drawTextCent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg){
	if (!font || !surface) return;

	int textW = 0, textH = 0;
	if (centx || centy) {
		Fonts::getSize(font, std::string(dato), textW, textH);
	}

	if (centx) x = (surface->w - textW) / 2 + x;
	if (centy) y = (surface->h - textH) / 2 + y;

	drawText(surface, font, dato, x, y, color, bg);
}

void Fonts::drawTextCentTransparent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg) {
	if (!font || !surface) return;

	int textW = 0, textH = 0;
	if (centx || centy) {
		Fonts::getSize(font, std::string(dato), textW, textH);
	}

	if (centx) x = (surface->w - textW) / 2 + x;
	if (centy) y = (surface->h - textH) / 2 + y;

	drawTextTransparent(surface, font, dato, x, y, color, bg);
}

SDL_Surface *Fonts::renderUtf8Blended(TTF_Font* font, const char* dato, const SDL_Color& color){
	if (font && dato != NULL && dato[0] != '\0') {
		ScopedFontLock lock(mutex);
		return TTF_RenderUTF8_Blended(font, dato, color);
	}
	return NULL;
}

SDL_Surface *Fonts::renderUtf8Solid(TTF_Font* font, const char* dato, const SDL_Color& color){
	if (font && dato != NULL && dato[0] != '\0') {
		ScopedFontLock lock(mutex);
		return TTF_RenderUTF8_Solid(font, dato, color);
	}
	return NULL;
}

SDL_Surface *Fonts::renderUtf8Shaded(TTF_Font* font, const char* dato, const SDL_Color& bg, const SDL_Color& fg){
	if (font && dato != NULL && dato[0] != '\0') {
		ScopedFontLock lock(mutex);
		return TTF_RenderUTF8_Shaded(font, dato, bg, fg);
	}
	return NULL;
}