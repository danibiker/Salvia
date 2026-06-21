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

		static void destroy();
		static void initFonts(int fontSize);
		static TTF_Font *getFont(int fontId);
		static std::size_t idxToCutTTF(std::string text, int maxW, int fontId);
		static int getSize(int, std::string);
		static std::string recortarAlTamanyo(std::string text, int maxWidth);
		static void getBadgeSize(int &w, int &h, int &badgePad, int &line_height);

		/* Crea una nueva instancia TTF_Font con la MISMA fuente y tamanyo
		 * que vFonts[fontId], pero con su propio FT_Face / caches de
		 * glifos.  Pensado para usar desde otros hilos: TTF no es
		 * thread-safe a nivel de la misma TTF_Font*, asi que cada hilo
		 * debe tener su instancia.  El llamante es el propietario y debe
		 * cerrarla con TTF_CloseFont. */
		static TTF_Font* createIndependentFont(int fontId);

		/* Mismas que getSize(fontId,...) pero con una TTF_Font* explicita.
		 * Pensadas para usar la fuente independiente de un worker. */
		static int getSize(TTF_Font* font, const char* text);
		static int getSize(TTF_Font* font, const std::string& text);

    private:

        static const int fsbig = 20;
        static const int fsmall = 10;
		static Fileio fileio;
		static TTF_Font* vFonts[2];
		static int s_fontSize;   // tamanyo usado en initFonts; para reabrir
};

#endif

