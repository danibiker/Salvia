#pragma once

#include "allegro.h"

#include "utils/uiobjects/object.h"
#include "utils/beans/structures.h"
#include "utils/io/dirutil.h"

#include "alpng.h"

using namespace std;

class Image : public Object{
    public:
        Image(){
            init();
        }

        ~Image(){
            //if (img != NULL){
            //    destroy_bitmap(img);
            //    img = NULL;
            //}
        }

        Image(int x, int y, int w, int h){
            this->setX(x);
            this->setY(y);
            this->setW(w);
            this->setH(h);
            init();
        }

        void init(){
            filepath = "";
            drawfaded = false;
            tamAuto = true;
            vAlign = ALIGN_MIDDLE;
            setObjectType(GUIPICTURE);
            img = NULL;
        }

        bool drawfaded;
        bool tamAuto;
        int vAlign;
        PALETTE pal;

        bool loadImageFromGame(string baseDir, GameFile game, string ext){
            dirutil dir;
            if (!loadImage(baseDir + dir.getFileNameNoExt(game.shortFileName) + ext)){
                return loadImage(baseDir + dir.getFileNameNoExt(game.longFileName) + ext);
            } else {
                return true;
            }
        }

        bool loadImage(string filepathToOpen){
            bool ret = false;
            if (filepath.empty() || filepath.compare(filepathToOpen) != 0){
                if (img != NULL){
                    destroy_bitmap(img);
                    img = NULL;
                }
                
                if ((img = load_png(filepathToOpen.c_str(), pal)) != NULL){   
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
                }
            } else if (!filepath.empty() && filepath.compare(filepathToOpen) == 0){
                ret = true;
            }
            return ret;
        }

        void printImage(BITMAP *video_page){
            if (!this->filepath.empty() && img != NULL){
                if (tamAuto) {
                    Dimension src = {img->w, img->h};
                    Dimension dst = {this->getW(), this->getH()};
                    Dimension newDim = relacion(src, dst);
                    Dimension offset = centrado(newDim, dst);

                    if (vAlign == ALIGN_TOP){
                        offset.h = 0;
                    }

                    stretch_blit(img, video_page, 0, 0, img->w, img->h, this->getX() + offset.w, this->getY() + offset.h, newDim.w, newDim.h);
                } else {
                    stretch_blit(img, video_page, 0, 0, img->w, img->h, this->getX(), this->getY(), this->getW(), this->getH());
                }
            }
        }

        Dimension relacion(Dimension &src, Dimension &dst ) {
            Dimension dim = {0,0};
            int maxHeight = dst.h;
            int maxWidth = dst.w;
            
            if (tamAuto) {
                int priorHeight = src.h; 
                int priorWidth = src.w;

                // Calculate the correct new height and width
                if((float)priorHeight/(float)priorWidth > (float)maxHeight/(float)maxWidth){
                    dim.h = maxHeight;
                    dim.w = (int)(((float)priorWidth/(float)priorHeight)*dim.h);
                } else {
                    dim.w = maxWidth;
                    dim.h = (int)(((float)priorHeight/(float)priorWidth)*dim.w);
                }
            } else {
                dim.h = src.h;
                dim.w = src.w;
            }

            return dim;
        }

        Dimension centrado(Dimension &src, Dimension &dst) {
            Dimension offset;
            offset.h = (dst.h - src.h) / 2;
            offset.w = (dst.w - src.w) / 2;
            return offset;
        }
    private:
        string filepath;
        BITMAP* img;
};
