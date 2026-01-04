#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdint.h>

#include "..\utils\logger.h"

static const int video_bpp = 16;
static const int audio_samples = 1024;

#ifdef _XBOX
	static Uint32 video_flags = SDL_SWSURFACE;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "game:\\salvia.log";
#elif  defined(WIN)
	static Uint32 video_flags = SDL_SWSURFACE; //SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "salvia.log";
#endif

static const SDL_Color white = { 255,255,255 };
static const SDL_Color black = { 0,0,0 };
static const SDL_Color lightgray = {222, 224, 219, 255};

static const SDL_Color backgroundColor = black;
static const SDL_Color textColor = white;
static const SDL_Color menuBars = { 128, 128, 128, 255};
static const SDL_Color bkgMenu = {247, 221, 114};

static const int bkgSpeedPixPerS = 15;
static const float bkgFrameTimeTick = 1000.0 / bkgSpeedPixPerS;

static const int SCREENHDIV = 15;

typedef enum{ TIPODIRECTORIO, TIPOFICHERO} enumFileAttr;
typedef enum{ COMPAREWHOLEWORD, COMPAREBEGINNING} enumFileCompare;
typedef enum{ LAYTEXT, LAYSIMPLE, LAYBOXES} enumLayout;
typedef enum{ ALIGN_TOP, ALIGN_MIDDLE} enumAlign;
typedef enum{ SBTNCLICK, SBTNLOAD } enumSounds;

typedef enum {
        page_white_text,
        folder,
        page_white,
        page_white_gear,
        page_white_compressed,
        page_white_picture,
        page_white_zip
}enumIco;

typedef enum {
    launch_system,          //0
    launch_spawn,           //1
    launch_create_process,  //2
    launch_batch
} launchMethods;

class Constant{
	public:
		Constant();
		~Constant();

		static const std::string MAME_SYS_ID;
        static const std::string WHITESPACE;
        static const char FILE_SEPARATOR_UNIX;
        static char FILE_SEPARATOR;
        static char tempFileSep[2];
        static volatile uint32_t totalTicks;

        static std::string getAppDir(){ 
            return appDir; 
        }
        static void setAppDir(std::string var){
            appDir = var;
        }

		// Función auxiliar para convertir int a std::string (VS2008 no tiene std::to_string)
		static std::string intToString(int value) {
			std::stringstream ss;
			ss << value;
			return ss.str();
		}

		static void drawText(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg){
			if (font) {
				SDL_Surface* textSurf = TTF_RenderText_Solid(font, s, color);
				if (textSurf) {
					SDL_Rect dest = { x, y, 0, 0 };
					SDL_BlitSurface(textSurf, NULL, surface, &dest);
					SDL_FreeSurface(textSurf); // ¡Vital!
				}
			}
		}

		static void drawTextCent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg){
			if (font != NULL){
				int pixelDato = 0;
				TTF_SizeText(font, dato, &pixelDato, NULL);
				int posDatox = x;
				int posDatoy = y;

				if (centx){
					posDatox = (surface->w - pixelDato)/2;
					posDatox += x;
				}
				if (centy){
					posDatoy = (surface->h)/2;
					posDatoy += y;
				}
				drawText(surface, font, dato,posDatox,posDatoy,color, bg);
			} else {
				//LOG_ERROR("Fallo en drawTextCent: La fuente es NULL");
			}
		}

		static double round(double number){
			return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
		}

        /**
        * Obtiene el separador de directorios de windows o unix
        */
        static std::string getFileSep(){
            char tmpFileSep[2] = {FILE_SEPARATOR,'\0'};
            return std::string(tmpFileSep);
        }

        static std::string replaceAll(std::string str, std::string tofind, std::string toreplace){
            size_t position = 0;
            size_t lastPosition = 0;
            std::string replaced = "";

            if (!str.empty()){
                for ( position = str.find(tofind); position != std::string::npos; position = str.find(tofind,lastPosition) ){
                        replaced.append(str.substr(lastPosition, position - lastPosition));
                        replaced.append(toreplace);
                        lastPosition = position + tofind.length();
                }
                if (str.length() > 0){
                    replaced.append(str.substr(lastPosition, str.length()));
                }
            }
            return(replaced);
        }

        static std::string TrimLeft(const std::string& s)
        {
            size_t startpos = s.find_first_not_of(WHITESPACE);
            return (startpos == std::string::npos) ? "" : s.substr(startpos);
        }

        static std::string TrimRight(const std::string& s)
        {
            size_t endpos = s.find_last_not_of(WHITESPACE);
            return (endpos == std::string::npos) ? "" : s.substr(0, endpos+1);
        }

        static std::string Trim(const std::string& s)
        {
            return TrimRight(TrimLeft(s));
        }

        static std::string toString(char c){
            std::string str(1, c); // creates a std::string with a single character 'A'
            return str;
        }

        /**
        *
        */
        static std::vector<std::string> splitChar(const std::string &s, char delim) {
            std::vector<std::string> elems;
            return splitChar(s, delim, elems);
        }

        /**
        *
        */
        static std::vector<std::string> &splitChar(const std::string &s, char delim, std::vector<std::string> &elems) {
            std::stringstream ss(s);
            std::string item;
            while(std::getline(ss, item, delim)) {
                elems.push_back(item);
            }
            return elems;
        }

        template<class TIPO> static std::string TipoToStr(TIPO number){
           stringstream ss;//create a stringstream
           ss << number;//add number to the stream
           return ss.str();//return a std::string with the contents of the stream
        }

        static void lowerCase(std::string *var){
            std::transform(var->begin(), var->end(), var->begin(), ::tolower);
        }

        static void upperCase(std::string *var){
            std::transform(var->begin(), var->end(), var->begin(), ::toupper);
        }

        template<class TIPO> static TIPO strToTipo(std::string str){
                TIPO i;
                stringstream s_str( str );
                s_str >> i;
                return i;
        }

        static void setExecMethod(int var){EXEC_METHOD = var;}
        static int getExecMethod(){return EXEC_METHOD;}
		
	private:
		static std::string appDir;
        static int EXEC_METHOD;
};



