#pragma once

#include <uiobjects/object.h>
#include <beans/structures.h>
#include <font/fonts.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <fstream>
#include <string>

struct t_line{
	SDL_Surface *lineSrf;
	std::string text;
	SDL_Rect dest;

 	t_line() {
		lineSrf = NULL;
		dest.x = 0;
		dest.y = 0;
		dest.w = 0;
		dest.h = 0;
	}
};

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

		/* Helpers para carga asincrona: separan la fase lenta (lectura
		 * de fichero + ajuste de palabras con TTF) de la "adopcion" de
		 * los resultados, que es atomica y rapida.  Permite que un worker
		 * prepare el resultado sin lock y solo entre brevemente al lock
		 * para reemplazar el estado de la instancia.
		 *
		 * Las variantes "WithFont" reciben un TTF_Font* explicito — para
		 * uso desde otro hilo se debe pasar una fuente INDEPENDIENTE (ver
		 * Fonts::createIndependentFont), porque TTF no es thread-safe a
		 * nivel de la misma TTF_Font*. */
		static std::vector<t_line> wrapString(const std::string& fulltxt,
		                                            Fonts::enumFonts fontType,
		                                            int maxW);
		static std::vector<t_line> wrapTextFile(const std::string& filepathToOpen,
		                                              Fonts::enumFonts fontType,
		                                              int maxW);
		static std::vector<t_line> wrapStringWithFont(const std::string& fulltxt,
		                                                    TTF_Font* font,
		                                                    int maxW);
		static std::vector<t_line> wrapTextFileWithFont(const std::string& filepathToOpen,
		                                                      TTF_Font* font,
		                                                      int maxW);
		void adoptLines(const std::vector<t_line>& newLines, const std::string& newPath);
		const std::string& getFilepath() const { return filepath; }
		Fonts::enumFonts   getFontType() const { return fontType; }
		void setFilepath(const string& p){ filepath = p;}

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
        std::vector<t_line> lines; 
		TTF_Font *fontText;
		int face_h;
		Fonts::enumFonts fontType;
	public:
		mutable CRITICAL_SECTION* m_objCS; // apunta al CS externo que protege este objeto
	private:

		SDL_Rect drawTextAreaTransparent(SDL_Surface*, TTF_Font*, t_line&, int, int, SDL_Color, int bg = -1,   JFY_TYPE& justifyHelper = JFY_TYPE(JFY_LEFT, 0));
};
