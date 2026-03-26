#pragma once

#include <vector>
#include <SDL_image.h>
#include <gfx/SDL_rotozoom.h>
#include <const/constant.h>
#include <uiobjects/image.h>
#include <font/fonts.h>
#include <io/dirutil.h>

class Icons{

public:
	Icons(){
	}

	~Icons(){
	}

	static void freeIcons(){
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

	static void loadIcons(){
		TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
		int face_h = TTF_FontLineSkip(fontMenu) + icon_w_add;
		const string assetsDir = "\\assets\\xmb\\retrosystem\\png\\";


		for (int i=0; i < max_icons; i++){
			SDL_Surface *img;
			std::string str = Constant::getAppDir() + assetsDir + std::string(ICONS_PATH[i]);
			if (dirutil::fileExists(str.c_str()) && (img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				SDL_Surface *resizeImage = rotozoomSurfaceXY(img, 0, zoomX, zoomY, true);
				SDL_Surface *formattedImg = SDL_DisplayFormatAlpha(resizeImage);
				SDL_FreeSurface(resizeImage);
				SDL_FreeSurface(img);
				icons.push_back(formattedImg);
			} else {
				icons.push_back(NULL);
			}
		}

		face_h = TTF_FontLineSkip(fontMenu) - icon_w_add / 2;

		for (int i=0; i < max_carts; i++){
			SDL_Surface *img;
			std::string str = Constant::getAppDir() + assetsDir + std::string(ICONS_CARTS_PATH[i]);
			if (dirutil::fileExists(str.c_str()) && (img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				SDL_Surface *resizeImage = rotozoomSurfaceXY(img, 0, zoomX, zoomY, true);
				SDL_FreeSurface(img);
				SDL_Surface *formattedImg = SDL_DisplayFormatAlpha(resizeImage);
				SDL_FreeSurface(resizeImage);
				icons_carts.push_back(formattedImg);
				
			} else {
				icons_carts.push_back(NULL);
			}
		}
	}

	static vector<SDL_Surface*> icons;
	static vector<SDL_Surface*> icons_carts;
	const static int icon_w_add = 10;

	private:
};