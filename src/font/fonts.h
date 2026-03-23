#ifndef FONTS
#define FONTS

#include <SDL_ttf.h>
#include <vector>
#include <string>

#include <const/Constant.h>
#include <io/fileio.h>


class Fonts{
    public:
        typedef enum{ FONTBIG = 0, FONTSMALL } enumFonts;
        Fonts();
        ~Fonts();

		static void exit();
		static void initFonts(int fontSize);
		static TTF_Font *getFont(int fontId);	
		static std::size_t idxToCutTTF(std::string text, int maxW, int fontId);
		static int getSize(int, std::string);
		static std::string recortarAlTamanyo(std::string text, int maxWidth);
		static void getBadgeSize(int &w, int &h, int &badgePad, int &line_height);

    private:
        
        static const int fsbig = 20;
        static const int fsmall = 10;
		static Fileio fileio;
		static TTF_Font* vFonts[2];
};

#endif

