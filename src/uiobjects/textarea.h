#pragma once

#include <uiobjects/object.h>
#include <beans/structures.h>
#include <font/fonts.h>

#include <fstream>
#include <string>

class TextArea : public Object{
    public:
        TextArea();
        ~TextArea();
        TextArea(int x, int y, int w, int h);

		void init();
        bool loadTextFileFromGame(std::string baseDir, GameFile& game, std::string ext);
        bool loadTextFile(std::string filepathToOpen);
		bool loadString(std::string fulltxt);
        void resetTicks(GameTicks gameTicks);
        void calcTicks(GameTicks gameTicks, int &scrollDesp, float &pixelDesp);
		void draw(SDL_Surface *video_page, GameTicks gameTicks);
        void draw(SDL_Surface *video_page);
		void setFontType(Fonts::enumFonts);
		void clear();
		bool isEmpty();

        int lineSpace;
        int marginTop;
        //To scroll the text
        bool enableScroll;
        int lastScroll;
        int marginX;
        uint32_t lastTick;
        uint32_t lastSubTick;
        uint32_t lastWaitTick;
        int timesWaiting;
		int timesWaitingEnd;
        bool waiting;
        float pixelDesp;

    private:
        std::string filepath;     
        std::vector<std::string> lines; 
		TTF_Font *fontText;
		int face_h;
		Fonts::enumFonts fontType;
};
