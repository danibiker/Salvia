#ifndef ICONS_H
#define ICONS_H

#include <array>
#include <string>
#include <SDL.h>

#include <const/constant.h>

struct t_icon{
	SDL_Surface* srf;
	bool failed;

	t_icon(){
		srf = NULL;
		failed = false;
	}

	t_icon(SDL_Surface* srf, bool failed){
		this->srf = srf;
		this->failed = failed;
	}

	~t_icon(){
		if (srf != NULL){
			SDL_FreeSurface(srf);
			srf = NULL;
		}
	}

	void clear(){
		if (srf != NULL){
			SDL_FreeSurface(srf);
			srf = NULL;
		}
	}
};

class Icons {
private:
	std::array<t_icon, max_icons> icons;
	std::array<t_icon, max_carts> icons_carts;

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