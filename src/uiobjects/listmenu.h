#pragma once 

#include <fstream>
#include <string>
#include <cmath>
#include <memory>
#include <unordered_map>

#include <uiobjects/object.h>
#include <io/fileprops.h>
#include <image/icons.h>
#include <menus/mameparser.h>

using namespace std;


// Estructura para pasar múltiples datos al hilo secundario
struct DatosDestruccion {
    std::unordered_map<std::string, GameData> *vectorVacio;
};

class ListMenu : public Object{
    private:
        void clearSelectedText();
		static const int waitTitleMove = 2000;
		static const int textFps = 20;
		static const int frameTimeText = (int)(1000 / textFps);
		Icons *icons;
		// El diccionario principal para mame: <nombre_zip, datos>
		std::unordered_map<std::string, GameData> mameDatabase;
		void loadMameDatabase(ConfigEmu& emu);
		SDL_Surface *selecAlphaRec;
		void drawIconListElem(SDL_Surface *video_page, GameFile *game, SDL_Rect& dstRectIcon);
		void drawNavBar(SDL_Surface *video_page, const SDL_Color& txtColor, TTF_Font *fontMenu, const int& face_h);
		std::string lastTxtNav;
		SDL_Surface *navPath;
		int face_h_big;
		int face_h_small;

		static void appendSegment(std::string& path, const std::string& seg) {
			if (seg.empty()) return;
			if (!path.empty()) path += " > ";
			path += seg;
		}

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
		bool showBottomInfo;
        bool animateBkg;
        bool centerText;
        bool keyUp;
        int lastSel;
        float pixelShift;
        vector<unique_ptr<GameFile>> listGames;
		// Vista filtrada (punteros no propietarios)
		vector<GameFile*> filteredGames;
		t_zipped_file_paths listZipped;
		t_dir_file_paths listDir;
		void zippedToList(int system);
		_inline std::string extractSystem(const std::string &sourceFile);

		GameDataFields gameDataFields;
        static SDL_Surface* imgText;
        
        void clear();
        std::size_t getNumGames();
        int getScreenNumLines();
        void setLayout(int layout, int screenw, int screenh);
        void draw(SDL_Surface *video_page, bool haveFocus = true);
        void mapFileToList(string filepath);
        static bool compareUniquePtrs(const std::unique_ptr<GameFile>& a,
                                const std::unique_ptr<GameFile>& b);
		static bool compareUniquePtrsFast(const std::unique_ptr<GameFile>&,
                                 const std::unique_ptr<GameFile>&);

		int getCartForSystem(int systemid);
        void filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu);
		void checkFilter();
		void applyFilter();
		void resetFilter();
		void sortFilters();


        void resetIndexPos();
        void nextPos();
        void prevPos();
        void nextPage();
        void prevPage();
		void resizeMarginTop(int addedMargin, int screenH);
};

