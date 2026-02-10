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
			std::string str = Constant::getAppDir() + "\\assets\\xmb\\flatui\\png\\" + std::string(ICONS_PATH[i]);
			if ((img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				// rotozoomSurfaceXY es más preciso para escalas no uniformes
				SDL_Surface *resizeImage = rotozoomSurfaceXY(img, 0, zoomX, zoomY, true);
				icons.push_back(resizeImage);
				SDL_FreeSurface(img);
			}
		}

		face_h = TTF_FontLineSkip(fontMenu) - icon_w_add / 2;

		for (int i=0; i < max_carts; i++){
			SDL_Surface *img;
			std::string str = Constant::getAppDir() + "\\assets\\xmb\\flatui\\png\\" + std::string(ICONS_CARTS_PATH[i]);
			if ((img = IMG_Load(str.c_str())) != NULL){   
				double zoomX = (double)face_h / img->w;
				double zoomY = (double)face_h / img->h;
				// rotozoomSurfaceXY es más preciso para escalas no uniformes
				SDL_Surface *resizeImage = rotozoomSurfaceXY(img, 0, zoomX, zoomY, true);
				icons_carts.push_back(resizeImage);
				SDL_FreeSurface(img);
			}
		}
	}

	

	static vector<SDL_Surface*> icons;
	static vector<SDL_Surface*> icons_carts;
	const static int icon_w_add = 10;

	private:
};