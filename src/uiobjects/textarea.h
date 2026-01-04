#pragma once

#include <uiobjects/object.h>
#include <beans/structures.h>

#include <fstream>
#include <string>

class TextArea : public Object{
    public:
        TextArea();
        ~TextArea();
        TextArea(int x, int y, int w, int h);

		void init();
        bool loadTextFileFromGame(std::string baseDir, GameFile game, std::string ext);
        bool loadTextFile(std::string filepathToOpen);
        void resetTicks(GameTicks gameTicks);
        void calcTicks(GameTicks gameTicks, int &scrollDesp, float &pixelDesp);
		void draw(SDL_Surface *video_page, GameTicks gameTicks);
        void draw(SDL_Surface *video_page);

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

    private:
        std::string filepath;     
        std::vector<std::string> lines; 
};
