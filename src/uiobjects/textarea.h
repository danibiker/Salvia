#pragma once

#include "allegro.h"

#include "utils/uiobjects/object.h"
#include "utils/beans/structures.h"
#include "utils/io/dirutil.h"

#include <fstream>
#include <string>

using namespace std;

class TextArea : public Object{
    public:
        TextArea(){
           init();
        }

        ~TextArea(){
            Traza::print(Traza::T_DEBUG, "Deleting TextArea...");
            lines.clear();
        }

        TextArea(int x, int y, int w, int h){
            this->setX(x);
            this->setY(y);
            this->setW(w);
            this->setH(h);
            this->marginX = 0;
            init();
        }

        void init(){
            this->filepath = "";
            this->lineSpace = 3;
            this->marginTop = 10;
            this->lastScroll = 0;
            setObjectType(GUITEXTAREA);

            this->enableScroll = true;
            this->pixelDesp = 0;
            this->timesWaiting = 0;
            this->waiting = true;
        }

        int lineSpace;
        int marginTop;

        //To scroll the text
        bool enableScroll;
        int lastScroll;
        int marginX;
        uint16_t lastTick;
        uint16_t lastSubTick;
        uint16_t lastWaitTick;
        int timesWaiting;
        bool waiting;
        float pixelDesp;
        
        /**
         * 
         */
        bool loadTextFileFromGame(string baseDir, GameFile game, string ext){
            dirutil dir;
            string fileToOpen = baseDir + dir.getFileNameNoExt(game.shortFileName) + ext;

            if (dir.fileExists(fileToOpen.c_str())){
                return loadTextFile(fileToOpen);
            } else {
                return loadTextFile(baseDir + dir.getFileNameNoExt(game.longFileName) + ext);
            }
        }

        /**
         * 
         */
        bool loadTextFile(string filepathToOpen){
            bool ret = false;

            if (this->filepath.empty() || this->filepath.compare(filepathToOpen) != 0){
                fstream fileRomTxt;

                fileRomTxt.open(filepathToOpen, ios::in);
                this->lastScroll = 0;

                if (fileRomTxt.is_open()){
                    lines.clear();
                    string txt;
                    string fulltxt = "";
                    while(getline(fileRomTxt, txt)){
                        fulltxt.append(!fulltxt.empty() ? " " : "" + txt);
                    }
                    fileRomTxt.close();

                    vector<string> words = Constant::splitChar(fulltxt, ' ');
                    lines.push_back("");

                    ALFONT_FONT *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
                    const int spaceW = alfont_text_length(fontSmall, " ");
                    for (string word : words){
                        int wordW = alfont_text_length(fontSmall, word.c_str());
                        int lineW = alfont_text_length(fontSmall, lines.at(lines.size()-1).c_str());
                        if (lineW + wordW + spaceW >= this->getW() - this->marginX){
                            lines.push_back("");
                            lines.at(lines.size()-1).append(word);
                        } else {
                            if (!lines.at(lines.size()-1).empty()){
                                lines.at(lines.size()-1).append(" ");
                            }
                            lines.at(lines.size()-1).append(word);
                        }
                    }
                    this->filepath = filepathToOpen;
                    ret = true;
                } else {
                    lines.clear();
                }
            } else if (!this->filepath.empty() && this->filepath.compare(filepathToOpen) == 0){
                ret = true;
            }
            return ret;
        }

        /**
         * 
         */
        void resetTicks(GameTicks gameTicks){
            pixelDesp = 0;
            lastTick = gameTicks.ticks;
            lastSubTick = gameTicks.ticks;
            lastWaitTick = gameTicks.ticks;
            timesWaiting = 0;
            waiting = true;
        }

        /**
         * 
         */
        void calcTicks(GameTicks gameTicks, int &scrollDesp, float &pixelDesp){
            
            if (!enableScroll)
                return;

            ALFONT_FONT *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
            const size_t maxLines = (this->getH() - marginTop) / (fontSmall->face_h + lineSpace);
            const int TICKSTOLINE = 40;
            const int TICKSTOLINEPIXEL = 1;
            const int LOOPSTOSTART = 1;
            
            //To wait some moments before start scrolling
            if (scrollDesp == 0 && timesWaiting < LOOPSTOSTART){
                if (abs(gameTicks.ticks - lastWaitTick) >= TICKSTOLINE){
                    timesWaiting += 1;
                    lastWaitTick = gameTicks.ticks;
                    waiting = true;
                }
                return;
            } else if (waiting){
                lastTick = gameTicks.ticks;
                waiting = false;
            }

            //Move one element of the list if needed
            if (abs(gameTicks.ticks - lastTick) >= TICKSTOLINE ){
                lastTick = gameTicks.ticks;
                pixelDesp = 0;
                
                if (lines.size() > maxLines){
                    scrollDesp = (scrollDesp + 1) % (lines.size() - maxLines + 1) ;
                    //To reset the wait status when we reach the final position
                    if (scrollDesp == 0){
                        resetTicks(gameTicks);
                    }
                } else {
                    scrollDesp = 0;
                }
            }
            
            //Move the text line an amount of pixels relative to the font height
            if (abs(gameTicks.ticks - lastSubTick) >= TICKSTOLINEPIXEL){
                lastSubTick = gameTicks.ticks;
                if (lines.size() > maxLines){
                    if (TICKSTOLINEPIXEL != 0 && TICKSTOLINE / TICKSTOLINEPIXEL != 0)
                        pixelDesp += (fontSmall->face_h + lineSpace) / (float)(TICKSTOLINE / TICKSTOLINEPIXEL);
                    else 
                        pixelDesp = 0;
                }
            }
        }

        /**
         * 
         */
        void draw(BITMAP *video_page, GameTicks gameTicks){
            int nextLineY = this->getY() + marginTop;
            int i = 0;
            if (lines.empty() || lines.size() == 0){
                return;
            }
            calcTicks(gameTicks, this->lastScroll, pixelDesp);
            ALFONT_FONT *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
            do{
                string line = lines.at(i + this->lastScroll);
                //textout_justify_ex(video_page, font, line.c_str(), this->getX(), this->getX() + this->getW() -1,
                //    nextLineY - pixelDesp, this->getW() / 3, Constant::textColor, -1);
                alfont_textout_ex(video_page, fontSmall, line.c_str(), this->getX() + this->marginX, nextLineY - pixelDesp, Constant::textColor, -1);
                nextLineY = this->getY() + marginTop + (++i) * (fontSmall->face_h + lineSpace);
            } while ((size_t) (i + this->lastScroll) < lines.size() && nextLineY < this->getY() + this->getH() - fontSmall->face_h);
        }

        /**
         * 
         */
        void draw(BITMAP *video_page){
            this->enableScroll = false;
            draw(video_page, {0});
            //int nextLineY = this->getY() + marginTop;
            //int i = 0;
            //
            //if (lines.empty() || lines.size() == 0){
            //    return;
            //}
            //
            //do{
            //    string line = lines.at(i);
            //    textout_justify_ex(video_page, font, line.c_str(), this->getX(), this->getX() + this->getW() -1,
            //        nextLineY, this->getW() / 3, Constant::textColor, -1);
            //    nextLineY = this->getY() + marginTop + (++i) * (fontSmall->face_h + lineSpace);
            //} while (i < lines.size() && nextLineY < this->getY() + this->getH() - fontSmall->face_h);
        }


    private:
        string filepath;     
        vector<string> lines; 
};
