#pragma 

#include <vector>
#include <SDL_image.h>
#include <gfx/SDL_rotozoom.h>
#include <const/constant.h>
#include <uiobjects/image.h>
#include <font/fonts.h>

class Icons{

public:
	Icons(){
		
	}

	~Icons(){
		
	}

	void freeIcons(){
		for (int i=0; i < max_icons; i++){
			SDL_FreeSurface(icons[i]);
		}
	}

	void loadIcons(){
		TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
		int face_h = TTF_FontLineSkip(fontMenu) + icon_w_add;

		for (int i=0; i < max_icons; i++){
			SDL_Surface *img;
			//std::string str = Constant::getAppDir() + "\\assets\\xmb\\flatui\\png\\" + std::string(ICONS_PATH[i]);
			std::string str = Constant::getAppDir() + "\\assets\\xmb\\retrosystem\\png\\" + std::string(ICONS_PATH[i]);
			if ((img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				// rotozoomSurfaceXY es más preciso para escalas no uniformes
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
			std::string str = Constant::getAppDir() + "\\assets\\xmb\\retrosystem\\png\\" + std::string(ICONS_CARTS_PATH[i]);
			if ((img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				// rotozoomSurfaceXY es más preciso para escalas no uniformes
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