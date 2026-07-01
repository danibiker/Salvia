#ifndef FONTS
#define FONTS

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>

#include <io/fileio.h>

#define MAX_FONTS 2

enum TXT_JUSTIFY{JFY_LEFT, JFY_RIGHT, JFY_CENTER};

struct JFY_TYPE{
	//Tipo de justificacion
	TXT_JUSTIFY jfy;
	//Ancho para la justificacion a la derecha
	int w;

	JFY_TYPE(TXT_JUSTIFY pJfy, int pW) : jfy(JFY_LEFT), w(0){
		jfy = pJfy;
		w = pW;
	}

	int getJustification(int txtSize){
		if (jfy == JFY_RIGHT && w > 0){
			return w - txtSize;
		} else if (jfy == JFY_CENTER && w > 0){
			return (w - txtSize) / 2;
		}
		else 
			return 0;
	}
};

class ScopedFontLock {
    SDL_mutex* m;
public:
    // Al crear el objeto en el stack, bloqueamos
    explicit ScopedFontLock(SDL_mutex* mutex) : m(mutex) {
        if(m) SDL_mutexP(m);
    }
    // Al salir del scope (llave de cierre), desbloqueamos
    ~ScopedFontLock() {
        if(m) SDL_mutexV(m);
    }
private:
    // Prohibimos copiar el lock para evitar errores lógicos
    ScopedFontLock(const ScopedFontLock&);
    ScopedFontLock& operator=(const ScopedFontLock&);
};


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
		static void getSize(TTF_Font* font, const std::string& text, int &w, int &h);
		static std::string recortarAlTamanyo(std::string text, int maxWidth);
		static void getBadgeSize(int &w, int &h, int &badgePad, int &line_height);
		static int getLineSkip(int fontId);

		/* Crea una nueva instancia TTF_Font con la MISMA fuente y tamanyo
		 * que vFonts[fontId], pero con su propio FT_Face / caches de
		 * glifos.  Pensado para usar desde otros hilos: TTF no es
		 * thread-safe a nivel de la misma TTF_Font*, asi que cada hilo
		 * debe tener su instancia.  El llamante es el propietario y debe
		 * cerrarla con TTF_CloseFont. */
		static TTF_Font* createIndependentFont(int fontId);

		/* Mismas que getSize(fontId,...) pero con una TTF_Font* explicita.
		 * Pensadas para usar la fuente independiente de un worker. */
		static int getSize(TTF_Font* font, const std::string& text);
		
		static SDL_Rect drawText(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg);
		static SDL_Rect drawTextTransparent(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg = -1, JFY_TYPE& justifyHelper = JFY_TYPE(JFY_LEFT, 0));
		static void drawTextCent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg);
		static void drawTextCentTransparent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg);

		static SDL_Surface *renderUtf8Blended(TTF_Font* font, const char* dato, const SDL_Color& color);
		static SDL_Surface *renderUtf8Solid(TTF_Font* font, const char* dato, const SDL_Color& color);
		static SDL_Surface *renderUtf8Shaded(TTF_Font* font, const char* dato, const SDL_Color& bg, const SDL_Color& fg);

    private:
        static const int fsbig = 20;
        static const int fsmall = 10;
		static Fileio fileio;
		static TTF_Font* vFonts[MAX_FONTS];
		static int s_fontSize;   // tamanyo usado en initFonts; para reabrir
		static SDL_mutex *mutex;
};

#endif

