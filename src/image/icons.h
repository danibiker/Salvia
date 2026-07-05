#pragma once

#include <SDL_image.h>
#include <vector>
#include <const/constant.h>
#include <array>

class Icons{
public:
	Icons();
	~Icons();

	static std::array<SDL_Surface*, max_icons> icons;
	static std::array<SDL_Surface*, max_carts> icons_carts;


	static void freeIcons();
	static void loadIcons(SDL_Surface*);
	const static int icon_w_add = 10;
};