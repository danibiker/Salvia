#pragma once

#include <SDL.h>
#include <SDL_image.h>

#include <random>
#include <string>
#include <utils/logger.h>

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
            SDL_FreeSurface(img);
        }

        int tileX;
        int tileY;
        int tileW;
        int tileH;
        float speed;

        void load(std::string imgpath){
            /*PALETTE pal;
            if ((img = load_png(imgpath.c_str(), pal)) != NULL){   
                if (bitmap_color_depth(img) == 8)
                    set_palette(pal);
                
                findTile(tileX, tileY);
            }*/
			if ((img = IMG_Load(imgpath.c_str())) != NULL){ 
				findTile(tileX, tileY);
			} else {
				LOG_ERROR("Couldn't load %s\n", imgpath.c_str());
			}
        }

        void findTile(int x, int y){

			if (img == NULL) {
				LOG_ERROR("Image surface is null...\n");
				return;
			}

            if (tile != NULL)
				SDL_FreeSurface(tile);

            this->tileX = x;
            this->tileY = y;

            //tile = create_bitmap(tileW, tileW);

			tile = SDL_CreateRGBSurface( SDL_SWSURFACE, tileW, tileW, 16,
                                        0,0,0,0);

            //blit(img, tile, tileX * tileW, tileY * tileH, 0, 0, tileW, tileH);
			// 1. Definir el recorte de origen (el tile específico dentro del tileset)
			SDL_Rect srcRect;
			srcRect.x = tileX * tileW;
			srcRect.y = tileY * tileH;
			srcRect.w = tileW;
			srcRect.h = tileH;

			// 2. Definir la posición de destino (esquina superior izquierda del tile destino)
			SDL_Rect dstRect;
			dstRect.x = 0;
			dstRect.y = 0;
			// w y h se toman automáticamente de srcRect

			// 3. Ejecutar el Blit
			// img es el tileset, tile es la superficie individual de destino
			SDL_BlitSurface(img, &srcRect, tile, &dstRect);
        }

		void draw(SDL_Surface *video_page){
			if (tile == NULL || video_page == NULL){
				LOG_ERROR("Some surface is null...\n");
				return;
			}

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
			SDL_Rect srcRect;
			SDL_Rect dstRect;
            for (int y = -tileH; y < video_page->h + tileH; y+= tileH){
                for (int x= -tileW; x < video_page->w + tileW; x += tileW){
                    //blit(tile, video_page, 0, 0, x - ((int)(x + speed) % tileW), y - ((int)(y + speed) % tileH), tileW, tileH);
					// 1. Definir el área de origen (el tile completo)
					
					srcRect.x = 0;
					srcRect.y = 0;
					srcRect.w = tileW;
					srcRect.h = tileH;

					// 2. Definir la posición de destino con la misma lógica de scroll de tu código original
					dstRect.x = x - ((int)(x + speed) % tileW);
					dstRect.y = y - ((int)(y + speed) % tileH);
					// Nota: SDL_BlitSurface ignora w y h en el rectángulo de destino, 
					// usa siempre el tamaño del srcRect para el recorte.

					// 3. Ejecutar el Blit
					// tile es la superficie origen, video_page es la superficie destino (screen o buffer)
					SDL_BlitSurface(tile, &srcRect, video_page, &dstRect);
                }
            }
        }

    private:
        int imageW;
        int imageH;
        SDL_Surface* img;
        SDL_Surface* tile;
};