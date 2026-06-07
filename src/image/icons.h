#pragma once

#include <SDL_image.h>
#include <vector>

class Icons{
public:
	Icons();
	~Icons();

	static void freeIcons();
	static void loadIcons(SDL_Surface*);
	static std::vector<SDL_Surface*> icons;
	static std::vector<SDL_Surface*> icons_carts;
	const static int icon_w_add = 10;
};