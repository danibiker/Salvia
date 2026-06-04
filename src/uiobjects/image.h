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

        uint8_t darkShift;
        bool tamAuto;
        int vAlign;
        bool fillGaps;

		static void convertirGrises16Bits(SDL_Surface*);
        static Dimension relacion(const Dimension &src, const Dimension &dst );
        static Dimension centrado(const Dimension &src, const Dimension &dst);

		Dimension relacionAuto(const Dimension &src, const Dimension &dst );

        bool loadImageFromGame(string baseDir, GameFile& game, string ext, SDL_PixelFormat* format = NULL);
        bool loadImage(string filepathToOpen, SDL_PixelFormat* format = NULL);
		void printImage(SDL_Surface *video_page);
		bool closeImage();
		bool hasImage();

		void Image::stretch_blit_sdl(SDL_Surface*& src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h);

    private:
        string filepath;
        SDL_Surface* img;

		SDL_Surface* cachedSurface; // Almacena la imagen ya escalada
		int lastW, lastH;           // Para detectar si el tamaño cambió
		
};
