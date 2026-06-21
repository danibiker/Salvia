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
		static std::vector<std::string> wrapString(const std::string& fulltxt,
		                                            Fonts::enumFonts fontType,
		                                            int maxW);
		static std::vector<std::string> wrapTextFile(const std::string& filepathToOpen,
		                                              Fonts::enumFonts fontType,
		                                              int maxW);
		static std::vector<std::string> wrapStringWithFont(const std::string& fulltxt,
		                                                    TTF_Font* font,
		                                                    int maxW);
		static std::vector<std::string> wrapTextFileWithFont(const std::string& filepathToOpen,
		                                                      TTF_Font* font,
		                                                      int maxW);
		void adoptLines(const std::vector<std::string>& newLines, const std::string& newPath);
		const std::string& getFilepath() const { return filepath; }
		Fonts::enumFonts   getFontType() const { return fontType; }

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
