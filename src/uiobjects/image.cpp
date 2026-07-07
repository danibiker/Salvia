#include <uiobjects/image.h>

#include <SDL_image.h>
#include <gfx/SDL_rotozoom.h>
#include <io/dirutil.h>

#ifndef SDL_RWFromConstMem
#define SDL_RWFromConstMem(p, s) SDL_RWFromMem(const_cast<void*>(static_cast<const void*>(p)), s)
#endif

const int SHADOW_OFFSET = 3;
const int SHADOW_THICKNESS = 5;
const int SHADOW_ALPHA = 128;

Image::Image(){
    init();
}

Image::Image(int x, int y, int w, int h){
    init();
	this->setX(x);
    this->setY(y);
    this->setW(w);
    this->setH(h);
}

void Image::init(){
    filepath = "";
    darkShift = 0xFF;
    tamAuto = true;
	fillGaps = false;
    vAlign = ALIGN_MIDDLE;
    setObjectType(GUIPICTURE);
    img = NULL;
	cachedSurface = NULL;
	this->setX(0);
    this->setY(0);
    this->setW(0);
    this->setH(0);
	keepAlpha = false;
	drawShadow = false;
}

Image::~Image(){
    if (img != NULL){
		SDL_FreeSurface(img);
        img = NULL;
    }
	
	if (cachedSurface != NULL){
		SDL_FreeSurface(cachedSurface);
        cachedSurface = NULL;
    }
}

bool Image::hasImage(){
	return cachedSurface != NULL || !filepath.empty();
}

bool Image::closeImage(){
   if (img != NULL){
		SDL_FreeSurface(img);
        img = NULL;
    }
	
	if (cachedSurface != NULL){
		SDL_FreeSurface(cachedSurface);
        cachedSurface = NULL;
    }
	filepath = "";
	clearShadows();
	return true;
}

void Image::clearShadows(){
	for (std::size_t i = 0; i < shadows.size(); i++)
        delete shadows[i];
    shadows.clear();
}

bool Image::loadImageFromGame(string baseDir, GameFile& game, string ext, SDL_PixelFormat* format){
    dirutil dir;
    return loadImage(baseDir + dir.getFileNameNoExt(game.longFileName) + ext, format);
}

/* ---------- Helpers estaticos para carga asincrona ----------
 * No tocan estado de instancia; pueden llamarse desde un worker thread. */
SDL_Surface* Image::loadConvertedSurface(const std::string& filepathToOpen, SDL_PixelFormat* format) {
    const char* cPath = filepathToOpen.c_str();
    if (!dirutil::fileExists(cPath)) return NULL;
    SDL_Surface* raw = IMG_Load(cPath);
    if (!raw) return NULL;
    if (format) {
        SDL_Surface* converted = SDL_ConvertSurface(raw, format, SDL_SWSURFACE);
        SDL_FreeSurface(raw);
        return converted; // puede ser NULL si fallo la conversion
    }
    return raw;
}

SDL_Surface* Image::loadConvertedSurfaceFromMem(const unsigned char* buffer, std::size_t bufferSize, SDL_PixelFormat* format) {
    if (buffer == NULL || bufferSize == 0) return NULL;
    SDL_RWops* rw = SDL_RWFromConstMem(buffer, bufferSize);
    if (rw == NULL) return NULL;
    SDL_Surface* raw = IMG_Load_RW(rw, 1);
    if (!raw) return NULL;
    if (format) {
        SDL_Surface* converted = SDL_ConvertSurface(raw, format, SDL_SWSURFACE);
        SDL_FreeSurface(raw);
        return converted;
    }
    return raw;
}

/* Atomico: reemplaza img/cachedSurface por newSurface, libera lo anterior.
 * Llamar bajo el lock externo que protege printImage(). */
void Image::adoptSurface(SDL_Surface* newSurface, const std::string& newPath) {
    if (img != NULL) {
        SDL_FreeSurface(img);
        img = NULL;
    }
    if (cachedSurface != NULL) {
        SDL_FreeSurface(cachedSurface);
        cachedSurface = NULL;
    }
	clearShadows();

    if (newSurface) {
        img = newSurface;
        filepath = newPath;
    } else {
        filepath = "";
    }
}

void Image::cloneSurface(SDL_Surface* newSurface, const std::string& newPath, SDL_PixelFormat* format) {
    if (img != NULL) {
        SDL_FreeSurface(img);
        img = NULL;
    }
    if (cachedSurface != NULL) {
        SDL_FreeSurface(cachedSurface);
        cachedSurface = NULL;
    }
	clearShadows();

    if (newSurface) {
		img = SDL_ConvertSurface(newSurface, format, SDL_SWSURFACE);
        filepath = newPath;
    } else {
        filepath = "";
    }
}

bool Image::loadImage(string filepathToOpen, SDL_PixelFormat* format){
    bool ret = false;
    if (filepath.empty() || filepath.compare(filepathToOpen) != 0){
        if (img != NULL){
			SDL_FreeSurface(img);
            img = NULL;
        }
		if (cachedSurface != NULL){
			SDL_FreeSurface(cachedSurface);
			cachedSurface = NULL;
		}
		clearShadows();

		const char *cFilePathToOpen = filepathToOpen.c_str();
		SDL_Surface* raw;
		if (dirutil::fileExists(cFilePathToOpen) && (raw = IMG_Load(cFilePathToOpen)) != NULL){   
			filepath = filepathToOpen;
			//Convertimos al formato de la pantalla apropiadamente. Esto elimina transparencias
			//y deja las imagenes correctas para ser presentadas de la forma mas eficiente posible
			//if (format->BitsPerPixel == 8 && format->palette) {
			if (format != NULL){
				img = SDL_ConvertSurface(raw, format, SDL_SWSURFACE);
				SDL_FreeSurface(raw);
			} else {
				img = raw;
			}
			ret = true;
		} else {
            filepath = "";
        }
    } else if (!filepath.empty() && filepath.compare(filepathToOpen) == 0){
        ret = true;
    }
    return ret;
}

bool Image::loadImageFromMemory(const unsigned char* buffer, std::size_t bufferSize, SDL_PixelFormat* format){
    if (buffer == NULL || bufferSize == 0){
        return false;
    }

    if (img != NULL){
        SDL_FreeSurface(img);
        img = NULL;
    }
    if (cachedSurface != NULL){
        SDL_FreeSurface(cachedSurface);
        cachedSurface = NULL;
    }
	clearShadows();

    SDL_RWops* rw = SDL_RWFromConstMem(buffer, bufferSize);
    if (rw == NULL){
        filepath = "";
        return false;
    }

    SDL_Surface* raw = IMG_Load_RW(rw, 1);
    if (raw == NULL){
        filepath = "";
        return false;
    }

    if (format != NULL){
        img = SDL_ConvertSurface(raw, format, SDL_SWSURFACE);
        SDL_FreeSurface(raw);
    } else {
        img = raw;
    }

    filepath = ":memory:";
    return true;
}

void Image::printImage(SDL_Surface *video_page){
    if (!this->filepath.empty() && img != NULL){
        if (tamAuto) {
            Dimension src = {img->w, img->h};
            Dimension dst = {this->getW(), this->getH()};
            newDim = relacionAuto(src, dst);
            newOffset = centrado(newDim, dst);

            if (vAlign == ALIGN_TOP){
                newOffset.h = 0;
            }
            stretch_blit_sdl(img, video_page, 0, 0, img->w, img->h, this->getX() + newOffset.w, this->getY() + newOffset.h, newDim.w, newDim.h);
        } else {
            stretch_blit_sdl(img, video_page, 0, 0, img->w, img->h, this->getX(), this->getY(), this->getW(), this->getH());
        }
		//rect(video_page, getX(), getY(), getX() + getW(), getY() + getH(), Constant::colors[clWhite].sdlColor);
		if (drawShadow)
			printShadow(video_page);
    }
}

void Image::printShadow(SDL_Surface *video_page){
	if (tamAuto) {
		//if (shadows.empty()){
		//	const int posX = this->getX() + newOffset.w + SHADOW_OFFSET;
		//	const int posY = this->getY() + newOffset.h + newDim.h;
		//	const int posYRight = this->getY() + newOffset.h + SHADOW_OFFSET;
		//	int posXRight = posX + newDim.w - SHADOW_OFFSET;
		//
		//	//Creamos la sombra inferior
		//	t_shadow* shadow = new t_shadow();
		//	shadow->rect.x = posX;
		//	shadow->rect.y = posY;
		//	shadow->rect.w = newDim.w + SHADOW_THICKNESS - SHADOW_OFFSET;
		//	shadow->rect.h = SHADOW_THICKNESS;
		//	Constant::createRectAlphaFilled(shadow->srf, shadow->rect, video_page->format, clBG);
		//	shadows.push_back(shadow);
		//	
		//	//Creamos la sombra derecha
		//	if (posXRight < video_page->w){
		//		const int posXRightThick = posXRight + SHADOW_THICKNESS < video_page->w ? posXRight + SHADOW_THICKNESS : video_page->w - posXRight;
		//		t_shadow* shadowR = new t_shadow();
		//		shadowR->rect.x = posXRight;
		//		shadowR->rect.y = posYRight;
		//		shadowR->rect.w = posXRightThick - posXRight;
		//		shadowR->rect.h = posY - posYRight;
		//		Constant::createRectAlphaFilled(shadowR->srf, shadowR->rect, video_page->format, clBG);
		//		shadows.push_back(shadowR);
		//	}
		//} 
		//
		//for (unsigned int i=0; i < shadows.size(); i++){
		//	SDL_BlitSurface(shadows[i]->srf, NULL, video_page, &shadows[i]->rect);
		//}

		//const int posX = this->getX() + newOffset.w + SHADOW_OFFSET;
		//const int posY = this->getY() + newOffset.h + newDim.h;
		////Dibujamos la sombra inferior
		//boxRGBA(video_page, posX, posY, posX + newDim.w + SHADOW_THICKNESS - SHADOW_OFFSET, posY + SHADOW_THICKNESS, 0, 0, 0, SHADOW_ALPHA);
		//
		//const int posYRight = this->getY() + newOffset.h + SHADOW_OFFSET;
		//int posXRight = posX + newDim.w - SHADOW_OFFSET;
		//if (posXRight < video_page->w){
		//	const int posXRightThick = posXRight + SHADOW_THICKNESS < video_page->w ? posXRight + SHADOW_THICKNESS : video_page->w - posXRight;
		//	//Dibujamos la sombra lateral derecha
		//	boxRGBA(video_page, posXRight, posYRight, 
		//		posXRightThick, posY - 1, 
		//		0, 0, 0, SHADOW_ALPHA);	
		//}

		const int posX = this->getX() + newOffset.w + SHADOW_OFFSET;
		const int posY = this->getY() + newOffset.h + newDim.h;

		const int posXBottomRight = posX + newDim.w - SHADOW_OFFSET + SHADOW_THICKNESS;
		//Dibujamos la sombra inferior
		boxColor(video_page, posX, posY, 
			posXBottomRight > video_page->w ? video_page->w : posXBottomRight, posY + SHADOW_THICKNESS, 
			Constant::colors[clBG].colorRaw);
		
		const int posYRight = this->getY() + newOffset.h + SHADOW_OFFSET;
		const int posXRight = posX + newDim.w - SHADOW_OFFSET;
		if (posXRight < video_page->w){
			const int posXRightThick = posXRight + SHADOW_THICKNESS;
			//Dibujamos la sombra lateral derecha
			boxColor(video_page, posXRight, posYRight, 
				posXRightThick > video_page->w ? video_page->w : posXRightThick, posY - 1, 
				Constant::colors[clBG].colorRaw);	
		}
	}
}

Dimension Image::relacionAuto(const Dimension &src, const Dimension &dst) {
    if (!tamAuto) return src;

    Dimension dim;
    // Comparamos proporciones usando multiplicaciones: 
    // (src.h / src.w > dst.h / dst.w) es igual a (src.h * dst.w > dst.h * src.w)
    if ((long)src.h * dst.w > (long)dst.h * src.w) {
        dim.h = dst.h;
        dim.w = (src.w * dst.h) / src.h;
    } else {
        dim.w = dst.w;
        dim.h = (src.h * dst.w) / src.w;
    }
    return dim;
}

Dimension Image::relacion(const Dimension &src, const Dimension &dst) {
    Dimension dim;
    // Comparamos proporciones usando multiplicaciones: 
    // (src.h / src.w > dst.h / dst.w) es igual a (src.h * dst.w > dst.h * src.w)
    if ((long)src.h * dst.w > (long)dst.h * src.w) {
        dim.h = dst.h;
        dim.w = (src.w * dst.h) / src.h;
    } else {
        dim.w = dst.w;
        dim.h = (src.h * dst.w) / src.w;
    }
    return dim;
}

Dimension Image::centrado(const Dimension &src, const Dimension &dst) {
    Dimension offset;
    offset.h = (dst.h - src.h) >> 1; // El desplazamiento de bits (>> 1) es igual a / 2
    offset.w = (dst.w - src.w) >> 1;
    return offset;
}

void Image::stretch_blit_sdl(SDL_Surface*& src, SDL_Surface* dest,
                            int src_x, int src_y, int src_w, int src_h,
                            int dst_x, int dst_y, int dst_w, int dst_h) {
	
	if (!src || !dest || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

	SDL_Rect dstRect = {
        fillGaps ? this->getX() : dst_x,
        fillGaps ? this->getY() : dst_y,
        dst_w, dst_h
    };

	// --- 1. Cache hit: blit directo y salir ---
    if (cachedSurface && lastW == dst_w && lastH == dst_h) {
        SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
        return;
    }

	// --- 2. Escalado de la imagen ---
	SDL_Surface* zoomedSurface = NULL;
	if (dst_w == src_w && dst_h == src_h){
		// Copia exacta en el formato original
		zoomedSurface = SDL_ConvertSurface(src, src->format, src->flags);
	} else {
		double zoomX = (double)dst_w / src_w;
		double zoomY = (double)dst_h / src_h;
		// zoomSurface (SDL_gfx) siempre devuelve una superficie RGBA de 32 bits genérica
		zoomedSurface = zoomSurface(src, zoomX, zoomY, true); //SMOOTH_RESIZE = TRUE
	}

	if (!zoomedSurface) return;

	// --- 3. Procesar Canal Alfa según 'keepAlpha' ---
	// Si tiene alfa por píxel, optimizamos el formato RGBA para que coincida con la pantalla
	// usando SDL_DisplayFormatAlpha. Esto acelera el Blit enormemente.
	SDL_Surface* finalSurface = SDL_ConvertSurface(zoomedSurface, dest->format, dest->flags);
	SDL_FreeSurface(zoomedSurface);

	if (!finalSurface) return;

	if (!keepAlpha) {
		// Eliminamos el canal alfa adaptando la superficie al formato exacto del destino (RGB)
		SDL_SetAlpha(finalSurface, SDL_RLEACCEL, 0xFF);
	} else {
		if (src->format->Amask != 0) {
			// Alfa por píxel: Desactivamos el alfa global para que no rompa el mapa de bits
			SDL_SetAlpha(finalSurface, 0, 0); 
			finalSurface->flags |= SDL_SRCALPHA;
		} 
		else {
			if (src->flags & SDL_SRCCOLORKEY) {
				// No tiene mapa alfa, pero tiene transparencia por color clave
				SDL_SetColorKey(finalSurface, SDL_SRCCOLORKEY | SDL_RLEACCEL, src->format->colorkey);
			} else {
				// Imagen plana sin transparencias
				SDL_SetAlpha(finalSurface, SDL_RLEACCEL, 0xFF);
			}
		}
	}

	// --- 4. Aplicar Oscurecimiento Seguro para SDL 1.2 ---
    if (this->darkShift < 0xFF && finalSurface) {
        /*Uint8 factor = 255 - this->darkShift; // 255 = sin oscurecer, 0 = negro total
		if (keepAlpha && src->format->Amask != 0) {
			// ALFA POR PÍXEL: Modulamos los canales RGB píxel a píxel respetando el canal Alfa original
			if (SDL_MUSTLOCK(finalSurface)) SDL_LockSurface(finalSurface);

			Uint32* pixels = (Uint32*)finalSurface->pixels;
			int pixelCount = finalSurface->w * finalSurface->h;
			SDL_PixelFormat* fmt = finalSurface->format;

			for (int i = 0; i < pixelCount; ++i) {
				Uint32 pixel = pixels[i];
				
				// Extraemos los componentes usando el formato de la arquitectura actual
				Uint8 r = (pixel & fmt->Rmask) >> fmt->Rshift;
				Uint8 g = (pixel & fmt->Gmask) >> fmt->Gshift;
				Uint8 b = (pixel & fmt->Bmask) >> fmt->Bshift;
				Uint8 a = (pixel & fmt->Amask) >> fmt->Ashift;

				// Multiplicamos RGB por el factor de oscuridad
				r = (r * factor) / 255;
				g = (g * factor) / 255;
				b = (b * factor) / 255;

				// Reensamblamos el píxel manteniendo intacto el Alfa ('a') original
				pixels[i] = (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift) | (a << fmt->Ashift);
			}

			if (SDL_MUSTLOCK(finalSurface)) SDL_UnlockSurface(finalSurface);
		} else {*/
			// SIN ALFA POR PÍXEL: Podemos usar boxRGBA de forma segura ya que no hay bordes transparentes
			boxRGBA(finalSurface, 0, 0, dst_w - 1, dst_h - 1, 0, 0, 0, this->darkShift);
		//}
    }

	// --- 5. Guardar en Caché y Renderizar ---
    if (cachedSurface) SDL_FreeSurface(cachedSurface);
	
	cachedSurface = finalSurface;
    lastW = dst_w;
    lastH = dst_h;

	SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
}

void Image::convertirGrises16Bits(SDL_Surface* surface) {
    if (!surface) return;
    
    // Bloquear si es necesario
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    Uint16* pixels = (Uint16*)surface->pixels;
    int pixelCount = surface->w * surface->h;

    for (int i = 0; i < pixelCount; i++) {
        Uint8 r, g, b, a;
        
        // SDL_GetRGBA funciona correctamente con 16 bits detectando el formato
        SDL_GetRGBA(pixels[i], surface->format, &r, &g, &b, &a);

        // Cálculo de luminosidad (Gris)
        Uint8 v = (Uint8)(0.299f * r + 0.587f * g + 0.114f * b);

        // Volvemos a empaquetar en el formato original de 16 bits
        pixels[i] = (Uint16)SDL_MapRGBA(surface->format, v, v, v, a);
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}	

/*void Image::stretch_blit_sdl(SDL_Surface*& src, SDL_Surface* dest,
                             int src_x, int src_y, int src_w, int src_h,
                             int dst_x, int dst_y, int dst_w, int dst_h) {

    if (!src || !dest) return;

    if (!cachedSurface || lastW != dst_w || lastH != dst_h) {
        if (cachedSurface) SDL_FreeSurface(cachedSurface);

        // =====================================================================
        // PASO 1: NORMALIZACIÓN (El secreto para que no salgan imágenes negras)
        // Creamos una superficie temporal con un formato estándar RGBA de 32 bits
        // =====================================================================
        SDL_Surface* normalizedSrc = SDL_CreateRGBSurface(
            SDL_SWSURFACE, src_w, src_h, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#else
            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#endif
        );

        if (!normalizedSrc) return;

        // Guardamos el alfa del origen y lo desactivamos momentáneamente para hacer
        // una copia exacta del bloque de píxeles (incluyendo el color de fondo cielo)
        Uint32 srcFlags = src->flags & SDL_SRCALPHA;
        Uint8 srcAlpha = src->format->alpha;
        SDL_SetAlpha(src, 0, 0);

        // Copiamos la porción exacta (src_x, src_y) al inicio de nuestra superficie normalizada
        SDL_Rect srcRect = {src_x, src_y, src_w, src_h};
        SDL_BlitSurface(src, &srcRect, normalizedSrc, NULL);

        // Restauramos el comportamiento del origen de inmediato
        SDL_SetAlpha(src, srcFlags, srcAlpha);

        // =====================================================================
        // PASO 2: ESCALADO SEGURO
        // Ahora zoomSurface recibe un formato limpio y garantizado de 32 bits
        // =====================================================================
        double zoomX = (double)dst_w / src_w;
        double zoomY = (double)dst_h / src_h;
        
        // Desactivamos el alpha en la superficie normalizada antes del zoom
        // para obligar a zoomSurface a procesar los colores puros sin corromperlos
        SDL_SetAlpha(normalizedSrc, 0, 0);
        SDL_Surface* zoomedSurface = zoomSurface(normalizedSrc, zoomX, zoomY, false);
        
        // Ya no necesitamos la superficie normalizada de origen, la liberamos
        SDL_FreeSurface(normalizedSrc);

        if (!zoomedSurface) return;

        // =====================================================================
        // PASO 3: PREPARACIÓN DE LA CACHÉ Y RENDERIZADO FINAL
        // =====================================================================
        // Clonamos la estructura exacta que nos devolvió zoomSurface
        cachedSurface = SDL_CreateRGBSurface(
            SDL_SWSURFACE, dst_w, dst_h,
            zoomedSurface->format->BitsPerPixel,
            zoomedSurface->format->Rmask, zoomedSurface->format->Gmask, 
            zoomedSurface->format->Bmask, zoomedSurface->format->Amask
        );

        if (!cachedSurface) {
            SDL_FreeSurface(zoomedSurface);
            return;
        }

        // Copia directa del contenido escalado a la caché sin mezclas intermedias
        SDL_SetAlpha(zoomedSurface, 0, 0);
        SDL_BlitSurface(zoomedSurface, NULL, cachedSurface, NULL);
        SDL_FreeSurface(zoomedSurface);

        // CONDICIÓN INTELIGENTE: Si el PNG original manejaba transparencias reales
        // le indicamos a la caché que use mezcla nativa al dibujar en la pantalla
        if (src->format->Amask != 0 || (src->flags & SDL_SRCCOLORKEY)) {
            SDL_SetAlpha(cachedSurface, SDL_SRCALPHA, srcAlpha);
        } else {
            SDL_SetAlpha(cachedSurface, 0, 255);
        }

        lastW = dst_w;
        lastH = dst_h;
    }

    // Dibujar de forma nativa sobre la pantalla usando el blitter de SDL
    SDL_Rect dstRect = {dst_x, dst_y, dst_w, dst_h};
    SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
}*/

/*void Image::stretch_blit_sdl(SDL_Surface* src, SDL_Surface* dest,
                            int src_x, int src_y, int src_w, int src_h,
                            int dst_x, int dst_y, int dst_w, int dst_h) {

   if (!src || !dest || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

   // --- Cache hit: blit directo y salir ---
   if (cachedSurface && lastW == dst_w && lastH == dst_h) {
       SDL_Rect dstRect = {
           fillGaps ? this->getX() : dst_x,
           fillGaps ? this->getY() : dst_y,
           dst_w, dst_h
       };
       SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
       return;
   }

   // --- (Re)crear caché ---
   if (cachedSurface) SDL_FreeSurface(cachedSurface);
   cachedSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, dst_w, dst_h, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
       0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#else
       0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#endif
   );
   if (!cachedSurface) return;
   lastW = dst_w;
   lastH = dst_h;

   // --- Escalado nearest-neighbor (punto fijo 16.16) ---
   if (SDL_MUSTLOCK(src))           SDL_LockSurface(src);
   if (SDL_MUSTLOCK(cachedSurface)) SDL_LockSurface(cachedSurface);

   Uint8*  srcBytes  = (Uint8*)src->pixels;
   Uint32* dstPixels = (Uint32*)cachedSurface->pixels;
   const int srcPitch   = src->pitch;
   const int dstPitch32 = cachedSurface->pitch >> 2;
   const int bpp        = src->format->BytesPerPixel;
   const Uint32 aMask   = src->format->Amask;
   const int x_ratio = (int)((src_w << 16) / dst_w) + 1;
   const int y_ratio = (int)((src_h << 16) / dst_h) + 1;

   for (int y = 0; y < dst_h; ++y) {
       Uint8*  srcRow = srcBytes  + (src_y + ((y * y_ratio) >> 16)) * srcPitch;
       Uint32* dstRow = dstPixels + y * dstPitch32;
       for (int x = 0; x < dst_w; ++x) {
           Uint8* p = srcRow + (src_x + ((x * x_ratio) >> 16)) * bpp;
           Uint32 rawColor;
           switch (bpp) {
               case 4: rawColor = *(Uint32*)p; break;
               case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                   rawColor = (p[0] << 16) | (p[1] << 8) | p[2];
#else
                   rawColor = p[0] | (p[1] << 8) | (p[2] << 16);
#endif
                   break;
               case 2: rawColor = *(Uint16*)p; break;
               default: rawColor = *p; break;
           }
           Uint8 r, g, b, a;
           SDL_GetRGBA(rawColor, src->format, &r, &g, &b, &a);
           if (aMask == 0) a = 255;

		   if (darkShift < 0xFF){
			   r = (r*darkShift >> 8);
			   g = (g*darkShift >> 8);
			   b = (b*darkShift >> 8);
		   }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
           dstRow[x] = (r << 24) | (g << 16) | (b << 8) | a;
#else
           dstRow[x] = r | (g << 8) | (b << 16) | (a << 24);
#endif
       }
   }

   if (SDL_MUSTLOCK(cachedSurface)) SDL_UnlockSurface(cachedSurface);
   if (SDL_MUSTLOCK(src))           SDL_UnlockSurface(src);

   SDL_Surface* converted = SDL_ConvertSurface(cachedSurface, dest->format, SDL_SWSURFACE);
   SDL_FreeSurface(cachedSurface);
   cachedSurface = converted;

   // --- Configuración alfa ---
   if (aMask != 0 || (src->flags & SDL_SRCCOLORKEY)) {
       cachedSurface->flags |= SDL_SRCALPHA;
       cachedSurface->format->alpha = src->format->alpha;
   } else {
       cachedSurface->flags &= ~SDL_SRCALPHA;
   }

   // --- FillGaps: envolver caché en superficie mayor con fondo rojo ---
   const int offsetX = dst_x - this->getX();
   const int offsetY = dst_y - this->getY();
   SDL_Rect dstRect = {this->getX(), this->getY(), 0, 0};

   if (fillGaps && offsetX > 0) {
       SDL_Surface* tmp = SDL_CreateRGBSurface(SDL_SWSURFACE,
           this->getW(), this->getH(), dest->format->BitsPerPixel,
           dest->format->Rmask, dest->format->Gmask,
           dest->format->Bmask, dest->format->Amask);
       //Drawing to fill the gaps
	   SDL_FillRect(tmp, NULL, Constant::colors[clBackground].color);
       //Drawing the final image centered
	   SDL_Rect r = {offsetX, offsetY, 0, 0};
       SDL_BlitSurface(cachedSurface, NULL, tmp, &r);
       SDL_FreeSurface(cachedSurface);
       cachedSurface = tmp;
   } else if (!fillGaps){
	   dstRect.x += offsetX;
	   dstRect.y += offsetY;
   }

// Justo antes del SDL_BlitSurface final:
LOG_DEBUG("Properties of %s", filepath.c_str());

LOG_DEBUG("src fmt: bpp=%d mask R=%08x G=%08x B=%08x A=%08x",
    src->format->BitsPerPixel, src->format->Rmask,
    src->format->Gmask, src->format->Bmask, src->format->Amask);
LOG_DEBUG("dst fmt: bpp=%d mask R=%08x G=%08x B=%08x A=%08x",
    dest->format->BitsPerPixel, dest->format->Rmask,
    dest->format->Gmask, dest->format->Bmask, dest->format->Amask);
LOG_DEBUG("cache fmt: bpp=%d mask R=%08x G=%08x B=%08x A=%08x",
    cachedSurface->format->BitsPerPixel, cachedSurface->format->Rmask,
    cachedSurface->format->Gmask, cachedSurface->format->Bmask,
    cachedSurface->format->Amask);

   SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
}*/