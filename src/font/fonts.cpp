#pragma once

#include "fonts.h"
#include "../Arimo_Regular.ttf.h"

Fileio Fonts::fileio;
TTF_Font* Fonts::vFonts[2];

Fonts::Fonts(){
}

Fonts::~Fonts(){
	LOG_DEBUG("Deleting Fonts...");
}
 /**
    * 
    */
void Fonts::init(){
    if (TTF_Init() == -1) {
		LOG_ERROR("Error TTF_Init: %s\n", TTF_GetError());
	}
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

size_t Fonts::idxToCutTTF(std::string text, int maxW, int fontId){
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