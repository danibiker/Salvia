#ifndef ICONS_H
#define ICONS_H

#include <array>
#include <string>
#include <SDL.h>

#include <const/constant.h>

class Icons {
private:
	std::array<SDL_Surface*, max_icons> icons;
	std::array<SDL_Surface*, max_carts> icons_carts;

    // Constructor privado: nadie fuera de esta clase puede hacer "Icons miObjeto;"
    Icons(); 
    ~Icons();

    // Deshabilitar copias (importante para evitar errores de memoria)
    Icons(const Icons&);
    Icons& operator=(const Icons&);

    SDL_Surface* loadAndResizeIcon(SDL_Surface* dest, const std::string& path, int target_size);
    SDL_Surface* getIcon(std::size_t posicion) const;
    SDL_Surface* getIconCart(std::size_t posicion) const;

public:
    // El unico punto de acceso global a la clase
    static Icons& getInstance() {
        static Icons instance; // Se crea una unica vez en toda la vida del programa
        return instance;
    }

	const static int icon_w_add = 10;

    bool drawIcon(SDL_Surface* dest, SDL_Rect* dstRect, std::size_t icoPos);
    bool drawIconCart(SDL_Surface* dest, SDL_Rect* dstRect, std::size_t icoPos);
};

#endif