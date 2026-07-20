#pragma once

#include <uiobjects/object.h>
#include <beans/structures.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

using namespace std;

struct t_shadow{
    SDL_Rect rect;
    SDL_Surface *srf;

    t_shadow() : srf(NULL) {
        rect.x = rect.y = rect.w = rect.h = 0;
    }

    ~t_shadow(){
        if (srf != NULL) {
            SDL_FreeSurface(srf);
            srf = NULL;
        }
    }

private:
    t_shadow(const t_shadow&);
    t_shadow& operator=(const t_shadow&);
};

class Image : public Object{
    public:
        Image();
        ~Image();
        Image(int x, int y, int w, int h);
        void init();

        uint8_t darkShift;
        bool tamAuto;
        int vAlign;
        bool fillGaps;
		bool keepAlpha;
		bool drawShadow;
		Dimension newDim;
		Dimension newOffset;
		SDL_Rect fitRect;
		

		static void convertirGrises16Bits(SDL_Surface*);
        static Dimension relacion(const Dimension &src, const Dimension &dst );
        static Dimension centrado(const Dimension &src, const Dimension &dst);

		Dimension relacionAuto(const Dimension &src, const Dimension &dst );

        bool loadImageFromGame(string baseDir, GameFile& game, string ext, SDL_PixelFormat* format = NULL);
        bool loadImage(string filepathToOpen, SDL_PixelFormat* format = NULL);
        bool loadImageFromMemory(const unsigned char* buffer, std::size_t bufferSize, SDL_PixelFormat* format = NULL);
		void printImage(SDL_Surface *video_page);
		bool closeImage();
		bool hasImage();

		/* Helpers para carga asincrona: la parte lenta (file IO + IMG_Load
		 * + SDL_ConvertSurface) se hace estaticamente sin tocar ninguna
		 * instancia; el adoptSurface() reemplaza el surface interno y libera
		 * el anterior — esa parte si debe ir bajo un lock externo si se
		 * concurre con printImage(). */
		static SDL_Surface* loadConvertedSurface(const std::string& filepathToOpen,
		                                          SDL_PixelFormat* format);
		static SDL_Surface* loadConvertedSurfaceFromMem(const unsigned char* buffer,
		                                                std::size_t bufferSize,
		                                                SDL_PixelFormat* format);
		void adoptSurface(SDL_Surface* newSurface, const std::string& newPath);
		const std::string& getFilepath() const { return filepath; }
		void cloneSurface(SDL_Surface* newSurface, const std::string& newPath, SDL_PixelFormat* format);

		void stretch_blit_sdl(SDL_Surface*& src, SDL_Surface* dest, 
                      int src_x, int src_y, int src_w, int src_h, 
                      int dst_x, int dst_y, int dst_w, int dst_h);

		void normal_blit_sdl(SDL_Surface*& src, SDL_Surface* dest,
                            int dst_x, int dst_y, int dst_w, int dst_h);
		
		void softMove(const float &dt, const int &button);

		mutable CRITICAL_SECTION* m_objCS; // apunta al CS externo que protege este objeto

		SDL_Surface* getImg(){
			return img;
		}

		void setTamAuto(bool b, const SDL_Rect &fr);
		bool isTamAuto(){ return tamAuto; }

    private:
        string filepath;
        SDL_Surface* img;
		SDL_Surface* cachedSurface; // Almacena la imagen ya escalada
		int lastW, lastH;           // Para detectar si el tamanyo cambio
		std::vector<t_shadow*> shadows;
		void printShadow(SDL_Surface *video_page);
		void clearShadows();
		float targetX, targetY;

};
