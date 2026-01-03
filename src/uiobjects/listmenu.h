#pragma once 

#include "allegro.h"

#include "utils/uiobjects/object.h"
#include "utils/beans/structures.h"
#include "utils/io/dirutil.h"
#include "utils/so/dosnames.h"
#include "utils/font/fonts.h"
#include "utils/font/alfont.h"

#include <fstream>
#include <string>
#include <cmath>

using namespace std;

class ListMenu : public Object{
    private:
        const int waitTitleMove = 2000;
        const float textFps = 20.0;
        const int frameTimeText = 1000 / textFps;

        void clearSelectedText(){
            if (imgText != NULL){
                destroy_bitmap(imgText);
                imgText = NULL;
            }
        }

    public:
        ListMenu(int screenw, int screenh){
            iniPos = 0;
            endPos = 0;
            curPos = 0;
            listSize = 0;
            maxLines = 0;
            marginX = floor(screenw / 100);
            marginY = screenh / SCREENHDIV * 1.5;
            lastSel = -1;
            pixelShift = 0;
            keyUp = false;
            animateBkg = true;
            setObjectType(GUILISTBOX);
            setLayout(LAYSIMPLE, screenw, screenh);
            set_trans_blender(255, 255, 255, 190);
        }

        ~ListMenu(){
            Traza::print(Traza::T_DEBUG, "Deleting ListMenu...");
            clear();
        }
        
        int marginX;
        int marginY;
        int iniPos;
        int endPos;
        int curPos;
        int listSize;
        int maxLines;
        int layout;
        bool animateBkg;
        bool centerText;
        bool keyUp;
        int lastSel;
        float pixelShift;
        vector<unique_ptr<GameFile>> listGames;
        static BITMAP* imgText;
        
        void clear(){
            if (listGames.size() > 0)
                listGames.clear();
        }

        size_t getNumGames(){
            return listGames.size();
        }

        int getScreenNumLines(){
            ALFONT_FONT *fontMenu = Fonts::getFont(Fonts::FONTBIG);
            return fontMenu->face_h != 0 ? std::ceil(getH() / fontMenu->face_h) : 0;
        }

        /**
         * 
         */
        void setLayout(int layout, int screenw, int screenh){
            this->marginY = screenh / SCREENHDIV * 1.5;
            clearSelectedText();

            if (layout == LAYBOXES){
                this->setX(0);
                this->setY(marginY);
                this->setW(screenw / 2);
                this->setH(screenh - marginY);
                this->centerText = false;
                this->layout = layout;

                for (size_t i=0; i < listGames.size(); i++){
                    auto file = listGames.at(i).get();
                    file->cutTitleIdx = Fonts::idxToCutTTF(file->gameTitle, this->getW() - 2*this->marginX, Fonts::FONTBIG);
                }
            } else {
                this->setX(marginX);
                this->setY(marginY);
                this->setW(screenw - marginX);
                this->setH(screenh - marginY);
                this->centerText = true;
                this->layout = layout;
            }
        }

        /**
         * 
         */
        void draw(BITMAP *video_page){
            static const int bkg = makecol(247, 221, 114);
            static const int black = makecol(0, 0, 0);
            static const int white = makecol(255, 255, 255);
            static const int lightgray = makecol(222,224,219);
            static const int transpink = makecol(255, 0, 255);
            static int colorTrans = makecol(247, 221, 114);

            ALFONT_FONT *fontMenu = Fonts::getFont(Fonts::FONTBIG);
            const int centerPos = this->getX() + this->getW() / 2;
            //To scroll one letter in one second. We use the face_h because the width of 
            //a letter is not fixed.
            const float pixelsScrollFps = max(ceil(fontMenu->face_h / textFps), 1.0f);

            for (int i=this->iniPos; i < this->endPos; i++){
                auto game = this->listGames.at(i).get();
                const int screenPos = i - this->iniPos;
                const int fontHeightRect = screenPos * fontMenu->face_h;
                const int lineBackground = -1;
                int lineTextColor = i == this->curPos ? black : white;
                string line = game->gameTitle.empty() ? game->shortFileName : game->gameTitle;

                //Drawing a faded background selection rectangle
                if (i == this->curPos){
                    int y = this->getY() + fontHeightRect;
                    //Gaining some extra fps when the screen resolution is low
                    if (SCREEN_H >= 768){
                        //Weird things happen if this line is not used here
                        //when using antialiased text
                        set_trans_blender(255, 255, 255, 190);
                        drawing_mode(DRAW_MODE_TRANS, video_page, this->getX(), this->getY());
                        rectfill(video_page, this->getX() + marginX, y, this->getW() - marginX, y + fontMenu->face_h, colorTrans);
                        drawing_mode(DRAW_MODE_SOLID, video_page, this->getX(), this->getY());
                    } else {
                        lineTextColor = lightgray;
                    }
                    rect(video_page, this->getX() - 1 + marginX, y - 1, this->getW() + 1 - marginX, y + fontMenu->face_h, bkg);
                }
                
                //Drawing the selected option in a separate bitmap to allow scrolling
                if (layout == LAYBOXES){
                    static int txtDifWidth = 0;
                    static uint32_t lastTick = Constant::getTicks();

                    if (lastSel != this->curPos && i == this->curPos){
                        clearSelectedText();
                        const int txtMaxWidth = alfont_text_length(Fonts::getFont(Fonts::FONTBIG), line.substr(0, game->cutTitleIdx).c_str());
                        const int txtTotalWidth = alfont_text_length(Fonts::getFont(Fonts::FONTBIG), line.c_str());
                        txtDifWidth = txtTotalWidth - txtMaxWidth;
                        imgText = create_bitmap(txtTotalWidth, fontMenu->face_h);
                        clear_to_color(imgText, transpink);
                        alfont_textout_ex(imgText, fontMenu, line.c_str(), 0, 0, lineTextColor, colorTrans);
                        lastSel = this->curPos;
                    }
                    
                    //Scrolling the text if it's big enough to not fill on the screen 
                    if (game->cutTitleIdx < game->gameTitle.length() && txtDifWidth > 0 && Constant::getTicks() > lastTick + frameTimeText){
                        //Waiting at the beginning and the end of the scrolling
                        if (pixelShift == 0 || pixelShift + pixelsScrollFps >= txtDifWidth){
                            //Adding a decimal to not enter again while we should be waiting
                            pixelShift += 0.1;
                            lastTick += waitTitleMove;
                        } else {
                            pixelShift = (int)floor(pixelShift + pixelsScrollFps) % txtDifWidth;
                            lastTick = Constant::getTicks();
                        }
                    }
                    line = game->gameTitle.substr(0, game->cutTitleIdx);
                }

                //Finally drawing the text
                if (this->centerText){
                    alfont_textout_centre_ex(video_page, fontMenu, line.c_str(), centerPos, 
                        this->getY() + fontHeightRect, lineTextColor, lineBackground);
                } else {
                    if (layout == LAYBOXES && i == this->curPos && imgText != NULL){
                        //masked_blit for transparent surfaces with pink background
                        masked_blit(imgText, video_page, pixelShift, 0, 
                            this->getX() + marginX, this->getY() + fontHeightRect, 
                            this->getW() - 2*this->marginX, fontMenu->face_h);
                    } else {
                        Constant::drawText(video_page, fontMenu, line.c_str(), this->getX() + marginX, 
                            this->getY() + fontHeightRect, lineTextColor, lineBackground);
                    }
                }
            }
        }

        /**
         * 
         */
        void mapFileToList(string filepath) {
            dirutil dir;
            //if (dir.fileExists(filepath.c_str()) && !dir.isDir(filepath.c_str())){
                fstream fileRomList;
                fileRomList.open(filepath, ios::in);

                if (fileRomList.is_open()){
                    this->clear();
                    string uri;
                    string filelong;
                    while(getline(fileRomList, uri)){
                        if (uri.length() > 1){
                            GameFile gameFile;
                            std::size_t found = uri.find_first_of(" ");
                            if (found != string::npos){
                                gameFile.shortFileName = uri.substr(0, found);
                                gameFile.longFileName = gameFile.shortFileName;

                                filelong = Constant::Trim(Constant::replaceAll(uri.substr(found + 1), "\"", ""));
                                //gameFile.gameTitle = Constant::cutToLength(filelong, this->getW());
                                gameFile.gameTitle = filelong;
                                gameFile.cutTitleIdx = Fonts::idxToCutTTF(filelong, this->getW() - 2*this->marginX, Fonts::FONTBIG);
                            }
                            listGames.push_back(make_unique<GameFile>(gameFile));
                        }
                    }
                    std::sort(listGames.begin(), listGames.end(), ListMenu::compareUniquePtrs);
                    resetIndexPos();
                }
                fileRomList.close();
            //}
        }

        // Define the comparison function
        static bool compareUniquePtrs(const std::unique_ptr<GameFile>& a,
                                const std::unique_ptr<GameFile>& b) {
            string sA = !a->gameTitle.empty() ? a->gameTitle : a->shortFileName;
            string sB = !b->gameTitle.empty() ? b->gameTitle : b->shortFileName;
            Constant::lowerCase(&sA);
            Constant::lowerCase(&sB);
            return sA.compare(sB) <= 0;
        }

        /**
         * 
         */
        void filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu) {
            this->clear();
            dirutil dir;
            //string filelong;
            for (size_t i=0; i < files.size(); i++){
                GameFile gameFile;
                auto file = files.at(i).get();
                //filelong = emu.use_extension ? file->filename : dir.getFileNameNoExt(file->filename);
                gameFile.shortFileName = file->filename;
                gameFile.longFileName = file->filename;
                //gameFile.gameTitle = Constant::cutToLength(dir.getFileNameNoExt(file->filename), this->getW());
                gameFile.gameTitle = dir.getFileNameNoExt(file->filename);
                gameFile.cutTitleIdx = Fonts::idxToCutTTF(gameFile.gameTitle, this->getW() - 2*this->marginX, Fonts::FONTBIG);
                this->listGames.push_back(make_unique<GameFile>(gameFile));
            }
            
            #ifndef DOS
                //This is to be able of showing the images or the synopsis
                //if the download was in dos and we want to use another Operating System
                //but not having to download all again
                vector<string> v = Constant::splitChar(emu.system, '_');
                if (v.size() > 0 && Constant::MAME_SYS_ID.compare(v.at(0)) != 0){
                    //For mame there is no need, because names are already short
                    DosNames dosnames;
                    dosnames.convertirNombresCortos(this->listGames);
                }
            #endif

            // Sort the vector
            std::sort(this->listGames.begin(), this->listGames.end(), ListMenu::compareUniquePtrs);
            resetIndexPos();
        }

        /**
         * 
         */
        void resetIndexPos(){
            this->listSize = this->listGames.size();
            this->maxLines = this->getScreenNumLines();
            /*To go to the bottom of the list*/
            //this->endPos = getListGames()->size();
            //this->iniPos = (int)getListGames()->size() >= this->maxLines ? getListGames()->size() - this->maxLines : 0;
            //this->curPos = this->endPos - 1;
            /*To go to the init of the list*/
            this->iniPos = 0;
            this->curPos = 0;
            this->endPos = (int)this->listGames.size() > this->maxLines ? this->maxLines : this->listGames.size();
            this->pixelShift = 0;
            this->lastSel = -1;
        }

        void nextPos(){
            if (this->curPos < this->listSize - 1){
                this->curPos++;
                int posCursorInScreen = this->curPos - this->iniPos;

                if (posCursorInScreen > this->maxLines - 1){
                    this->iniPos++;
                    this->endPos++;
                }
                this->pixelShift = 0;
                this->lastSel = -1;
            }
        }

        void prevPos(){
            if (this->curPos > 0){
                this->curPos--;
                if (this->curPos < this->iniPos && this->curPos >= 0){
                    this->iniPos--;
                    this->endPos--;
                }
                this->pixelShift = 0;
                this->lastSel = -1;
            }
        }

        void nextPage(){
            for (int i=0; i < this->maxLines -1; i++){
                nextPos();
            }
        }

        void prevPage(){
            for (int i=0; i < this->maxLines -1; i++){
                prevPos();
            }
        }
};
