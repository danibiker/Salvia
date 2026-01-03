#pragma once

#include "allegro.h"
#include "alpng.h"
#include <random>

class TileMap {
    public:
        TileMap(int tileX, int tileY, int tileW, int tileH){
            this->tileX = tileX;
            this->tileY = tileY;
            this->tileW = tileW;
            this->tileH = tileH;
            img = NULL;
            tile = NULL;
            speed = 0;
        }

        ~TileMap(){
            Traza::print(Traza::T_DEBUG, "Deleting TileMap...");
            destroy_bitmap(img);
        }

        int tileX;
        int tileY;
        int tileW;
        int tileH;
        float speed;

        void load(string imgpath){
            PALETTE pal;
            if ((img = load_png(imgpath.c_str(), pal)) != NULL){   
                if (bitmap_color_depth(img) == 8)
                    set_palette(pal);
                
                findTile(tileX, tileY);
            }
        }

        void findTile(int x, int y){
            if (tile != NULL)
                destroy_bitmap(tile);

            this->tileX = x;
            this->tileY = y;

            tile = create_bitmap(tileW, tileW);
            blit(img, tile, tileX * tileW, tileY * tileH, 0, 0, tileW, tileH);
        }

        void draw(BITMAP *video_page){
            //int spOffsetX = tileX * tileW;
            //int spOffsetY = tileY * tileH;
            //
            //if (tileW > 0 && tileH > 0 && video_page != NULL && img != NULL && img->w > 0 && img->h > 0)
            //for (int y=0; y < video_page->h; y++){
            //    for (int x=0; x < video_page->w; x++){
            //        _putpixel16(video_page, x, y, _getpixel16(img, spOffsetX + ((int)(x + speed) % tileW) , spOffsetY + ((int)(y + speed) % tileH)));
            //    }
            //}

            //Much faster method than the above pixel based but just a little bit more memory required depending
            //on the tiles size
            for (int y = -tileH; y < video_page->h + tileH; y+= tileH){
                for (int x= -tileW; x < video_page->w + tileW; x += tileW){
                    blit(tile, video_page, 0, 0, x - ((int)(x + speed) % tileW), y - ((int)(y + speed) % tileH), tileW, tileH);
                }
            }
        }

    private:
        int imageW;
        int imageH;
        BITMAP* img;
        BITMAP* tile;
};