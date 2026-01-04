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
};
