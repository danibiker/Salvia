#pragma once

#include <uiobjects/object.h>
#include <beans/structures.h>

using namespace std;

class Image : public Object{
    public:
        Image();
        ~Image();
        Image(int x, int y, int w, int h);
        void init();

        bool drawfaded;
        bool tamAuto;
        int vAlign;
        //PALETTE pal;
        bool loadImageFromGame(string baseDir, GameFile game, string ext);
        bool loadImage(string filepathToOpen);
		void printImage(SDL_Surface *video_page);
        Dimension relacion(const Dimension &src, const Dimension &dst );
        Dimension centrado(const Dimension &src, const Dimension &dst);
		bool closeImage();

		void Image::stretch_blit_sdl(SDL_Surface* src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h);

    private:
        string filepath;
        SDL_Surface* img;

		SDL_Surface* cachedSurface; // Almacena la imagen ya escalada
		int lastW, lastH;           // Para detectar si el tamaño cambió

		
};
