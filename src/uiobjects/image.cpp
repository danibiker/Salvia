#include <uiobjects/image.h>

#include <SDL_image.h>
#include <gfx/SDL_rotozoom.h>
#include <io/dirutil.h>

Image::Image(){
    init();
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

Image::Image(int x, int y, int w, int h){
    this->setX(x);
    this->setY(y);
    this->setW(w);
    this->setH(h);
    init();
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
	return true;
}

void Image::init(){
    filepath = "";
    drawfaded = false;
    tamAuto = true;
    vAlign = ALIGN_MIDDLE;
    setObjectType(GUIPICTURE);
    img = NULL;
	cachedSurface = NULL;
}

bool Image::loadImageFromGame(string baseDir, GameFile game, string ext){
    dirutil dir;
    if (!loadImage(baseDir + dir.getFileNameNoExt(game.shortFileName) + ext)){
        return loadImage(baseDir + dir.getFileNameNoExt(game.longFileName) + ext);
    } else {
        return true;
    }
}

bool Image::loadImage(string filepathToOpen){
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

		const char *cFilePathToOpen = filepathToOpen.c_str();
		if (dirutil::fileExists(cFilePathToOpen) && (img = IMG_Load(cFilePathToOpen)) != NULL){   
			filepath = filepathToOpen;
			ret = true;
		} else {
            filepath = "";
        }
    } else if (!filepath.empty() && filepath.compare(filepathToOpen) == 0){
        ret = true;
    }
    return ret;
}

void Image::printImage(SDL_Surface *video_page){
    if (!this->filepath.empty() && img != NULL){
        if (tamAuto) {
            Dimension src = {img->w, img->h};
            Dimension dst = {this->getW(), this->getH()};
            Dimension newDim = relacion(src, dst);
            Dimension offset = centrado(newDim, dst);

            if (vAlign == ALIGN_TOP){
                offset.h = 0;
            }
            stretch_blit_sdl(img, video_page, 0, 0, img->w, img->h, this->getX() + offset.w, this->getY() + offset.h, newDim.w, newDim.h);
        } else {
            stretch_blit_sdl(img, video_page, 0, 0, img->w, img->h, this->getX(), this->getY(), this->getW(), this->getH());
        }
    }
}

Dimension Image::relacion(const Dimension &src, const Dimension &dst) {
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

Dimension Image::centrado(const Dimension &src, const Dimension &dst) {
    Dimension offset;
    offset.h = (dst.h - src.h) >> 1; // El desplazamiento de bits (>> 1) es igual a / 2
    offset.w = (dst.w - src.w) >> 1;
    return offset;
}

void Image::stretch_blit_sdl(SDL_Surface* src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h) {

    if (!cachedSurface || lastW != dst_w || lastH != dst_h) {
        if (cachedSurface) SDL_FreeSurface(cachedSurface);

        // 1. Crear el recorte (sub-sección)
        //SDL_Rect srcRect = {src_x, src_y, src_w, src_h};
        
        // 2. Normalizar: Convertir la fuente al formato de la pantalla/destino
        // Esto corrige automáticamente los errores de color (swapping de canales)
        /*SDL_Surface* normalizedSrc = SDL_ConvertSurface(src, dest->format, SDL_SWSURFACE | SDL_SRCALPHA);
        
        // 3. Crear superficie para el recorte
        SDL_Surface* subSrc = SDL_CreateRGBSurface(SDL_HWSURFACE, src_w, src_h, 
                                 dest->format->BitsPerPixel, 
                                 dest->format->Rmask, dest->format->Gmask, 
                                 dest->format->Bmask, dest->format->Amask);
								 */
        // Copiar sección sin mezclar (copia pura de píxeles)
        //SDL_SetAlpha(normalizedSrc, 0, 0);
        //SDL_BlitSurface(normalizedSrc, &srcRect, subSrc, NULL);

        // 4. Escalar
        double zoomX = (double)dst_w / src_w;
        double zoomY = (double)dst_h / src_h;
        
        // rotozoomSurfaceXY es más preciso para escalas no uniformes
		SDL_Surface* zoomedSurface = rotozoomSurfaceXY(src, 0, zoomX, zoomY, SMOOTHING_ON);

        // 5. Limpieza de temporales
        //SDL_FreeSurface(normalizedSrc);
        //SDL_FreeSurface(subSrc);

		cachedSurface = SDL_DisplayFormat(zoomedSurface);
		SDL_FreeSurface(zoomedSurface);
        
        lastW = dst_w; lastH = dst_h;
    }

    // 6. Dibujo final
    SDL_Rect dstRect = {dst_x, dst_y, dst_w, dst_h};
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