#include "icons.h"
#include <SDL_image.h>
#include <gfx/SDL_rotozoom.h>
#include <const/constant.h>
#include <font/fonts.h>
#include <io/dirutil.h>

// El constructor ya no necesita inicializar los arrays manualmente a NULL 
// porque la inicializacion agregada '{}' en la cabecera ya se encarga de ello.
Icons::Icons() {
}

// El destructor se ejecuta automaticamente al cerrar el programa.
// Libera de forma segura toda la memoria dinamica de SDL.
Icons::~Icons() {
    for (std::size_t i = 0; i < icons.size(); ++i) {
        icons[i].clear();
    }
    for (std::size_t i = 0; i < icons_carts.size(); ++i) {
        icons_carts[i].clear();
    }
}

// Acceso seguro al array de iconos estandar
SDL_Surface* Icons::getIcon(std::size_t posicion) const {
    if (posicion < icons.size()) {
		return icons[posicion].srf;
    }
    return NULL;
}

// Acceso seguro al array de iconos de cartuchos
SDL_Surface* Icons::getIconCart(std::size_t posicion) const {
    if (posicion < icons_carts.size()) {
        return icons_carts[posicion].srf;
    }
    return NULL;
}

// Procesador generico privado: elimina la duplicacion de codigo de carga y reescalado
SDL_Surface* Icons::loadAndResizeIcon(SDL_Surface* dest, const std::string& fileName, int target_size) {
    std::string fullPath = Constant::getAppDir() + ASSETS_ICONS_DIR + fileName;
    
    // Verificacion de seguridad del archivo antes de intentar cargarlo
    if (!dirutil::fileExists(fullPath.c_str())) {
        return NULL;
    }

    SDL_Surface* img = IMG_Load(fullPath.c_str());
    if (img == NULL) {
        return NULL;
    }

    SDL_Surface* formattedImg = NULL;

    // Si la imagen es mas grande que el espacio disponible, aplicamos el zoom tecnico
    if (img->w > target_size || img->h > target_size) {
        double zoomX = (double)target_size / img->w;
        double zoomY = (double)target_size / img->h;
        
        SDL_Surface* resizeImage = zoomSurface(img, zoomX, zoomY, true);
        SDL_FreeSurface(img); // Liberamos la imagen original de disco
        
        if (resizeImage != NULL) {
            // Convertimos la superficie al formato nativo de la pantalla destino (optimiza el Blit)
            formattedImg = SDL_ConvertSurface(resizeImage, dest->format, dest->flags);
            SDL_FreeSurface(resizeImage);
        }
    } else {
        // Si no requiere redimension, solo optimizamos su formato de pixeles
        formattedImg = SDL_ConvertSurface(img, dest->format, dest->flags);
        SDL_FreeSurface(img);
    }

    return formattedImg;
}

// Dibuja el icono del sistema cargandolo bajo demanda (Lazy Loading)
bool Icons::drawIcon(SDL_Surface* dest, SDL_Rect* dstRect, std::size_t icoPos) {
    // Proteccion estricta contra desbordamientos de array y evitar reintento de carga
	if (icoPos >= max_icons || (icoPos < max_icons && icoPos >= 0 && icons[icoPos].failed)) {
        return false; 
    }

    SDL_Surface* ico = getIcon(icoPos);
    
    // Si no esta cargado en memoria, lo procesamos ahora
    if (ico == NULL) {
        int face_h = Fonts::getLineSkip(Fonts::FONTBIG) + icon_w_add;
		icons[icoPos].srf = loadAndResizeIcon(dest, ICONS_PATH[icoPos], face_h);
        ico = getIcon(icoPos);
        
        if (ico == NULL) {
			icons[icoPos].failed = true;
            return false; // Error al cargar o procesar el archivo
        }
    }
    
    SDL_BlitSurface(ico, NULL, dest, dstRect);
    return true;
}

// Dibuja el icono del cartucho cargandolo bajo demanda (Lazy Loading)
bool Icons::drawIconCart(SDL_Surface* dest, SDL_Rect* dstRect, std::size_t icoPos) {
    // Proteccion estricta contra desbordamientos de array
    if (icoPos >= max_carts || (icoPos < max_icons && icoPos >= 0 && icons_carts[icoPos].failed)) {
        return false; 
    }

    SDL_Surface* ico = getIconCart(icoPos);
    
    // Si no esta cargado en memoria, lo procesamos ahora
    if (ico == NULL) {
        int face_h = Fonts::getLineSkip(Fonts::FONTBIG) - icon_w_add / 2;
        icons_carts[icoPos].srf = loadAndResizeIcon(dest, ICONS_CARTS_PATH[icoPos], face_h);
        ico = getIconCart(icoPos);
        
        if (ico == NULL) {
			icons_carts[icoPos].failed = true;
            return false; // Error al cargar o procesar el archivo
        }
    }
    
    SDL_BlitSurface(ico, NULL, dest, dstRect);
    return true;
}
