#include <SDL_image.h>
#include <uiobjects/image.h>
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
        
		if ((img = IMG_Load(filepathToOpen.c_str())) != NULL){   
			filepath = filepathToOpen;
		} else {
            filepath = "";
        }

        /*if ((img = load_png(filepathToOpen.c_str(), pal)) != NULL){   
            if (bitmap_color_depth(img) == 8)
                set_palette(pal);
                    
            if (this->drawfaded){
                //Setting translucency effect
                if (get_color_depth() == 8)
                    color_map = &Constant::global_trans_table;
                else
                    set_trans_blender(128, 128, 128, 160);

                BITMAP *tmp_bm = create_bitmap(img->w, img->h);
                rectfill(tmp_bm, 0, 0, img->w, img->h, makecol(0,0,0));
                draw_trans_sprite(img, tmp_bm, 0, 0);
                destroy_bitmap(tmp_bm);
            }
            filepath = filepathToOpen;
            ret = true;
        } else {
            filepath = "";
        }*/
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

/**
 * Equivalente a stretch_blit(src, dest, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h) de Allegro
 */
/*void Image::stretch_blit_sdl(SDL_Surface* src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h) {
    
    // Configurar el rectángulo de origen (Recorte de la imagen original)
	SDL_Rect srcRect = {src_x, src_y, src_w, src_h};

    // Configurar el rectángulo de destino (Posición y nuevo tamaño en pantalla)
    SDL_Rect dstRect = {dst_x, dst_y, dst_w, dst_h};

    // Realizar el blit. SDL 1.2 escala automáticamente si srcRect y dstRect 
    // tienen dimensiones diferentes.
    SDL_BlitSurface(src, &srcRect, dest, &dstRect);
}*/

#include <gfx/SDL_rotozoom.h>

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
        cachedSurface = rotozoomSurfaceXY(src, 0, zoomX, zoomY, SMOOTHING_ON);

        // 5. Limpieza de temporales
        //SDL_FreeSurface(normalizedSrc);
        //SDL_FreeSurface(subSrc);

		//cachedSurface = SDL_DisplayFormat(ampliada);
		//SDL_FreeSurface(ampliada);
        
        lastW = dst_w; lastH = dst_h;
    }

    // 6. Dibujo final
    SDL_Rect dstRect = {dst_x, dst_y, dst_w, dst_h};
    SDL_BlitSurface(cachedSurface, NULL, dest, &dstRect);
}