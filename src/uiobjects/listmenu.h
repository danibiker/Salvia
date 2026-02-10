#pragma once 

#include <fstream>
#include <string>
#include <cmath>
#include <memory>

#include <uiobjects/object.h>
#include <io/fileprops.h>

using namespace std;


class ListMenu : public Object{
    private:
        void clearSelectedText();
		static const int waitTitleMove = 2000;
		static const int textFps = 20;
		static const int frameTimeText = (int)(1000 / textFps);

    public:
        ListMenu(int screenw, int screenh);
        ~ListMenu();
        
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

        static SDL_Surface* imgText;
        
        void clear();
        std::size_t getNumGames();
        int getScreenNumLines();
        void setLayout(int layout, int screenw, int screenh);
        void draw(SDL_Surface *video_page);
        void mapFileToList(string filepath);
        static bool compareUniquePtrs(const std::unique_ptr<GameFile>& a,
                                const std::unique_ptr<GameFile>& b);
		int getCartForSystem(int systemid);

        void filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu);
        void resetIndexPos();
        void nextPos();
        void prevPos();
        void nextPage();
        void prevPage();
};

