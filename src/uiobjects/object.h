#pragma once

#include <SDL.h>
#include <beans/structures.h>

enum {
    COBJECT = 0,
    GUILISTBOX,
    GUIPICTURE,
    GUITEXTAREA,
	GUIOPTIONS
};

class Object{
    public:
        Object();
        virtual ~Object();
        virtual void draw(SDL_Surface *video_page);
        virtual void draw(SDL_Surface *video_page, GameTicks gameTicks);
        virtual int getX(){return x;}
        virtual int getY(){return y;}
        virtual int getW(){return w;}
        virtual int getH(){return h;}
        virtual void setX(int var){x = var;}
        virtual void setY(int var){y = var;}
        virtual void setW(int var){if (var > 0) w = var; else w = 0;}
        virtual void setH(int var){if (var > 0) h = var; else h = 0;}

        int getObjectType() { return classType; }
        void setObjectType(int val) { classType = val; }

     protected:
        int x;          //Posicion horizontal del objeto
        int y;          //Posicion vertical del objeto
        int w;          //Ancho en pixeles del objeto
        int h;          //Alto en pixeles del objeto
        int classType;  //Tipo de elemento al que hace referencia el objeto
};