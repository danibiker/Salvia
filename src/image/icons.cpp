#include "icons.h"

#include <gfx/SDL_rotozoom.h>
#include <const/constant.h>
#include <uiobjects/image.h>
#include <font/fonts.h>
#include <io/dirutil.h>

vector<SDL_Surface *> Icons::icons;
vector<SDL_Surface *> Icons::icons_carts;

Icons::Icons(){
	//Icons are loaded once from engine.cpp
}

Icons::~Icons(){
	//Icons are destroyed from engine.cpp
}

void Icons::freeIcons(){
	for (std::size_t i=0; i < icons.size(); i++){
		if (icons[i] != NULL){
			SDL_FreeSurface(icons[i]);
			icons[i] = NULL;
		}
	}
	icons.clear();
		
	for (std::size_t i=0; i < icons_carts.size(); i++){
		if (icons_carts[i] != NULL){
			SDL_FreeSurface(icons_carts[i]);
			icons_carts[i] = NULL;
		}
	}
	icons_carts.clear();
}

void Icons::loadIcons(SDL_Surface* dest){
	int face_h = Fonts::getLineSkip(Fonts::FONTBIG) + icon_w_add;

	for (int i=0; i < max_icons; i++){
		SDL_Surface *img;
		std::string str = Constant::getAppDir() + ASSETS_ICONS_DIR + std::string(ICONS_PATH[i]);
		if (dirutil::fileExists(str.c_str()) && (img = IMG_Load(str.c_str())) != NULL){   
			SDL_Surface *formattedImg;
			if (img->w > face_h || img->h > face_h){
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				SDL_Surface *resizeImage = zoomSurface(img, zoomX, zoomY, true);
				SDL_FreeSurface(img);
#ifdef _XBOX
				formattedImg = SDL_ConvertSurface(resizeImage, dest->format, dest->flags);
#else
				formattedImg = SDL_DisplayFormatAlpha(resizeImage);
#endif
				SDL_FreeSurface(resizeImage);
			} else {
#ifdef _XBOX
				formattedImg = SDL_ConvertSurface(img, dest->format, dest->flags);
#else
				formattedImg = SDL_DisplayFormatAlpha(img);
#endif
				SDL_FreeSurface(img);
			}
			icons.push_back(formattedImg);
		} else {
			icons.push_back(NULL);
		}
	}

	face_h = Fonts::getLineSkip(Fonts::FONTBIG) - icon_w_add / 2;

	for (int i=0; i < max_carts; i++){
		SDL_Surface *img;
		std::string str = Constant::getAppDir() + ASSETS_ICONS_DIR + std::string(ICONS_CARTS_PATH[i]);
		if (dirutil::fileExists(str.c_str()) && (img = IMG_Load(str.c_str())) != NULL){   
			SDL_Surface *formattedImg;
			if (img->w > face_h || img->h > face_h){
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				SDL_Surface *resizeImage = zoomSurface(img, zoomX, zoomY, true);
				SDL_FreeSurface(img);
#ifdef _XBOX
				formattedImg = SDL_ConvertSurface(resizeImage, dest->format, dest->flags);
#else
				formattedImg = SDL_DisplayFormatAlpha(resizeImage);
#endif
				SDL_FreeSurface(resizeImage);
			} else {
#ifdef _XBOX
				formattedImg = SDL_ConvertSurface(img, dest->format, dest->flags);
#else
				formattedImg = SDL_DisplayFormatAlpha(img);
#endif
				SDL_FreeSurface(img);
			}
			icons_carts.push_back(formattedImg);
				
		} else {
			icons_carts.push_back(NULL);
		}
	}
}