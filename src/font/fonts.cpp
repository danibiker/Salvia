#pragma once

#include "fonts.h"
#include <font/Arimo_Regular.ttf.h>

Fileio Fonts::fileio;
TTF_Font* Fonts::vFonts[2];

Fonts::Fonts(){
}

Fonts::~Fonts(){
	LOG_DEBUG("Deleting Fonts...");
	exit();
}

/**
    * 
    */
void Fonts::exit(){
    for (unsigned int i=0; i < 2; i++){
        if (vFonts[i] != NULL){
            TTF_CloseFont(vFonts[i]);
            vFonts[i] = NULL;
        }
    }

	fileio.clearFile();
}
        
/**
    * 
    */
void Fonts::initFonts(int fontSize){
	vFonts[FONTBIG] = NULL;
	vFonts[FONTSMALL] = NULL;
	fileio.loadFromMem(Arimo_Regular_ttf, Arimo_Regular_ttf_size);
	SDL_RWops *RWOps = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps != NULL){
		vFonts[FONTBIG] = TTF_OpenFontRW(RWOps,1, fontSize);
		if (vFonts[FONTBIG] == NULL) {
			LOG_ERROR("Error al cargar fuente grande: %s\n", TTF_GetError());
		} 
	}

	SDL_RWops *RWOps2 = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps2 != NULL){
		vFonts[FONTSMALL] = TTF_OpenFontRW(RWOps2,1, fontSize - 9);
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

	int textW = 0;
	TTF_SizeText(vFonts[fontId], text.c_str(), &textW, NULL);

    if (textW < maxW){
        return text.length();
    }

    size_t i = 1;
    while(i < text.length()){
		TTF_SizeText(vFonts[fontId], text.substr(0, i).c_str(), &textW, NULL);
        if (textW >= maxW){
            i--;
            break;
        }
        i++;
    }
    return i;
}

int Fonts::getSize(int fontId, std::string text){
	int textW = 0;
	TTF_SizeText(vFonts[fontId], text.c_str(), &textW, NULL);
	return textW;
}

std::string Fonts::recortarAlTamanyo(std::string text, int maxWidth){
	std::string newText = text;
	TTF_Font* font = Fonts::getFont(Fonts::FONTBIG);
	if (!font) return newText;

	int textPixelSize = 0;
	TTF_SizeText(font, text.c_str(), &textPixelSize, NULL);

	if (textPixelSize > maxWidth) {
		int totalChars = text.length();
    
		// 1. Precalculamos el ancho promedio por carácter
		float avgCharWidth = (float)textPixelSize / (float)totalChars;
    
		// 2. Estimamos cuántos caracteres sobran para que quepa (incluyendo el "...")
		int dotsWidth = 0;
		TTF_SizeText(font, "...", &dotsWidth, NULL);
    
		int targetWidth = maxWidth - dotsWidth;
		int charsThatFit = (int)(targetWidth / avgCharWidth);
    
		// 3. Aplicamos el recorte inicial basado en la estimación
		// Queremos la mitad de los que caben al principio y la otra mitad al final
		int leftPart = charsThatFit / 2;
		int rightPart = totalChars - (charsThatFit / 2);
    
		newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
		TTF_SizeText(font, newText.c_str(), &textPixelSize, NULL);

		// 4. Ajuste fino (por si la estimación fue optimista debido a caracteres anchos como 'W')
		// Este bucle se ejecutará como mucho 1 o 2 veces, ahorrando mucha CPU
		while (textPixelSize > maxWidth && leftPart > 0 && rightPart < totalChars) {
			if (leftPart > 0) leftPart--;
			if (rightPart < totalChars) rightPart++;
        
			newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
			TTF_SizeText(font, newText.c_str(), &textPixelSize, NULL);
		}
	}

	return newText;
}

void Fonts::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}