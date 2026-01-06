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
        Dimension relacion(Dimension &src, Dimension &dst );
        Dimension centrado(Dimension &src, Dimension &dst);
    private:
        string filepath;
        SDL_Surface* img;

		void Image::stretch_blit_sdl(SDL_Surface* src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h);
};
