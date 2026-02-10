#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <sstream>
#include <cctype> // Para isdigit
#include <algorithm>
#include <stdint.h>

#include <utils/logger.h>

static const int video_bpp = 16;


#ifdef _XBOX
	static Uint32 video_flags = SDL_SWSURFACE;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "game:\\salvia.log";
	#include <xtl.h>
#elif defined(WIN)
	static Uint32 video_flags = SDL_SWSURFACE; //SDL_SWSURFACE; //SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "salvia.log";
	#include <windows.h>
#endif

static const SDL_Color white = { 255,255,255 };
static const SDL_Color black = { 0,0,0 };
static const SDL_Color lightgray = {222, 224, 219, 255};

static const SDL_Color backgroundColor = black;
static const SDL_Color textColor = white;
static const SDL_Color menuBars = { 128, 128, 128, 255};
static const SDL_Color bkgMenu = {247, 221, 114};

static const SDL_Color askClTitle = {59,59,59};
static const SDL_Color askClBg = {69,69,69};
static const SDL_Color askClLine = {91,91,91};
static const SDL_Color askClText = {190,190,190};

static const int bkgSpeedPixPerS = 15;
static const double bkgFrameTimeTick = 1000.0 / bkgSpeedPixPerS;

static const int SCREENHDIV = 15;

static const unsigned long KEYRETRASO = 500;
static const int JOYHATOFFSET = 100;
static const int JOYAXISOFFSET = 200;
static const int DEADZONE = 23000;
static const unsigned long DBLCLICKSPEED = 300; //tiempo en ms para poder hacer un doble click
static const unsigned long KEYDOWNSPEED = 50;
static const unsigned long MOUSEVISIBLE = 8000;
static const int CURSORVISIBLE = 1;
static const int LONGKEYTIMEOUT = 2000;
static const int MAX_SAVESTATES = 10;
static const char *STATE_IMG_EXT = ".png";
static const char *STATE_EXT = ".state";

typedef enum {
    cursor_hidden,
    cursor_arrow,
    cursor_resize,
    cursor_hand,
    cursor_wait,
    totalCursors
} enumCursors;

struct Message {
    std::string content;
    Uint32 ticks;
    Uint32 timeout;
    SDL_Surface* cache; // Nueva superficie para el mensaje renderizado
    SDL_Rect rect;      // Para guardar el tamaño y posición calculados

	Message(){
		cache = NULL;
		ticks = 0;
		timeout = 0;
		content = "";
	}
};

#define MOUSE_BUTTON_LEFT		1
#define MOUSE_BUTTON_MIDDLE	2
#define MOUSE_BUTTON_RIGHT	3
#define MOUSE_BUTTON_WHEELUP	4
#define MOUSE_BUTTON_WHEELDOWN	5
#define MOUSE_BUTTON_X1         6
#define MOUSE_BUTTON_X2         7

typedef enum{ TIPODIRECTORIO, TIPOFICHERO} enumFileAttr;
typedef enum{ COMPAREWHOLEWORD, COMPAREBEGINNING} enumFileCompare;
typedef enum{ LAYTEXT, LAYSIMPLE, LAYBOXES} enumLayout;
typedef enum{ ALIGN_TOP, ALIGN_MIDDLE} enumAlign;
typedef enum{ SBTNCLICK, SBTNLOAD } enumSounds;

typedef enum {JOY_BUTTON_A = 0,
            JOY_BUTTON_B,
            JOY_BUTTON_X,
            JOY_BUTTON_Y,
            JOY_BUTTON_L,
            JOY_BUTTON_R,
            JOY_BUTTON_SELECT,
            JOY_BUTTON_START,
            JOY_BUTTON_L3,
            JOY_BUTTON_R3,
            JOY_BUTTON_UP,
            JOY_BUTTON_UPLEFT,
            JOY_BUTTON_LEFT,
            JOY_BUTTON_DOWNLEFT,
            JOY_BUTTON_DOWN,
            JOY_BUTTON_DOWNRIGHT,
            JOY_BUTTON_RIGHT,
            JOY_BUTTON_UPRIGHT,
            JOY_BUTTON_VOLUP,
            JOY_BUTTON_VOLDOWN,
            JOY_BUTTON_CLICK,
            JOY_AXIS1_RIGHT,
            JOY_AXIS1_LEFT,
            JOY_AXIS1_UP,
            JOY_AXIS1_DOWN,
            JOY_AXIS2_RIGHT,
            JOY_AXIS2_LEFT,
            JOY_AXIS2_UP,
            JOY_AXIS2_DOWN,
            JOY_AXIS_L2,
            JOY_AXIS_R2,
            MAXJOYBUTTONS} joystickButtons;

static int FRONTEND_BTN_VAL[] = {JOY_BUTTON_UP, JOY_BUTTON_DOWN, JOY_BUTTON_LEFT, JOY_BUTTON_RIGHT, JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_X, JOY_BUTTON_Y
		, JOY_BUTTON_L, JOY_BUTTON_R, JOY_BUTTON_SELECT, JOY_BUTTON_START, JOY_BUTTON_L3, JOY_BUTTON_R3};

#define MAX_PLAYERS 4

typedef enum {
        page_white_text,
        folder,
        page_white,
        page_white_gear,
        page_white_compressed,
        page_white_picture,
        page_white_zip,
		cfg_video,
		cfg_settings,
		cfg_settings_core,
		cfg_subsettings,
		cfg_remap,
		cfg_savestates,
		cfg_saving,
		cfg_return,
		max_icons
}enumIco;

typedef enum {cart_gba,
			  cart_gb,
			  cart_sms,
			  cart_genesis,
			  cart_snes,
			  cart_32x,
			  cart_gg,
			  cart_mcd,
			  cart_nes,
			  cart_pce,
			  cart_psx,
			  max_carts};

extern const char *JOY_DESCRIPTIONS[];
extern const char *FRONTEND_BTN_TXT[];
extern const char *ICONS_PATH[];
extern const char *ICONS_CARTS_PATH[];

typedef enum {
    launch_system,          //0
    launch_spawn,           //1
    launch_create_process,  //2
    launch_batch
} launchMethods;

enum {
	SYNC_TO_AUDIO = 0,
	SYNC_TO_VIDEO,
	SYNC_NONE,
	SYNC_FAST_FORWARD
};


/* SDL 1.2: Definir máscaras según el orden de bytes del sistema */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    // Formato ARGB (Típico en PowerPC / Xbox 360)
    static Uint32 rmask = 0x00FF0000;
    static Uint32 gmask = 0x0000FF00;
    static Uint32 bmask = 0x000000FF;
    static Uint32 amask = 0xFF000000;
#else
    // Formato ARGB en Little Endian (x86) se almacena como BGRA en memoria, 
    // pero SDL maneja estas máscaras para que el uint32_t sea 0xAARRGGBB
    static Uint32 rmask = 0x00FF0000;
    static Uint32 gmask = 0x0000FF00;
    static Uint32 bmask = 0x000000FF;
    static Uint32 amask = 0xFF000000;
#endif

static const char FILE_SEPARATOR_UNIX = '/';

class Constant{
	public:
		Constant();
		~Constant();

		static const std::string MAME_SYS_ID;
        static const std::string WHITESPACE;
        static char tempFileSep[2];
        static volatile uint32_t totalTicks;

        static std::string getAppDir(){ 
            return appDir; 
        }
        static void setAppDir(std::string var){
            appDir = var;
        }

		static std::string getAppExecutable(){ 
            return appExecutable; 
        }
        static void setAppExecutable(std::string var){
            appExecutable = var;
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

		static void drawTextTransparent(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg){
			if (font) {
				SDL_Surface* textSurf = TTF_RenderText_Blended(font, s, color);
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
			} 
		}

		static void drawTextCentTransparent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg){
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
				drawTextTransparent(surface, font, dato,posDatox,posDatoy,color, bg);
			} 
		}

		static double round(double number){
			return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
		}

        /**
        * Obtiene el separador de directorios de windows o unix
        */
        static std::string getFileSep(){
            return std::string(tempFileSep);
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
		static std::vector<std::string> &Constant::split(std::string s, std::string delim, std::vector<std::string> &elems) {
			std::stringstream ss(s);
			std::string item;
			while(std::getline(ss, item, delim.at(0))) {
				elems.push_back(item);
			}
			return elems;
		}

		/**
		*
		*/
		static std::vector<std::string> Constant::split(std::string s, std::string delim) {
			std::vector<std::string> elems;
			return split(s, delim, elems);
		}

        /**
        *
        */
        static std::vector<std::string> splitChar(const std::string &s, char delim) {
            std::vector<std::string> elems;
            return splitChar(s, delim, elems);
        }

		static std::vector<int> Constant::splitInt(const std::string& s, char delimiter) {
			std::vector<int> tokens;
			std::string token;
			std::istringstream tokenStream(s);

			while (std::getline(tokenStream, token, delimiter)) {
				// Trim opcional por si hay espacios entre la coma y el número
				std::string trimmed = Constant::Trim(token); 
				if (!trimmed.empty()) {
					tokens.push_back(std::atoi(trimmed.c_str()));
				}
			}
			return tokens;
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
           std::stringstream ss;//create a stringstream
           ss << number;//add number to the stream
           return ss.str();//return a std::string with the contents of the stream
        }

        static void lowerCase(std::string *var){
			if (var != NULL)
				std::transform(var->begin(), var->end(), var->begin(), ::tolower);
        }

        static void upperCase(std::string *var){
			if (var != NULL)
				std::transform(var->begin(), var->end(), var->begin(), ::toupper);
        }

        template<class TIPO> 
		static TIPO strToTipo(std::string str) {
			std::stringstream s_str(str);
    
			// Si el tipo es de 1 byte (int8_t, uint8_t, char), 
			// stringstream lo leería como carácter.
			if (sizeof(TIPO) == 1) {
				int temp;
				s_str >> temp;
				return static_cast<TIPO>(temp);
			} else {
				TIPO i;
				s_str >> i;
				return i;
			}
		}

		static bool esNumerico(const std::string& s) {
			if (s.empty()) return false;
    
			// Si permites números negativos, saltamos el signo '-'
			std::size_t inicio = (s[0] == '-' && s.size() > 1) ? 1 : 0;

			for (size_t i = inicio; i < s.size(); i++) {
				if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
					return false;
				}
			}
			return true;
		}

		// Función auxiliar
		static bool compareNoCase(const std::string& a, const std::string& b) {
			for (size_t i = 0; i < a.length() && i < b.length(); ++i) {
				if (tolower(a[i]) != tolower(b[i]))
					return tolower(a[i]) < tolower(b[i]);
			}
			return a.length() < b.length();
		}

		static double getTicks() {
			#if defined(_WIN32) || defined(_WIN64) || defined(_XBOX)
				// Cacheamos la frecuencia por rendimiento (Win32 API)
				static LARGE_INTEGER freq;
				static bool freqInitialized = false;
				if (!freqInitialized) {
					QueryPerformanceFrequency(&freq);
					freqInitialized = true;
				}
				LARGE_INTEGER counter;
				QueryPerformanceCounter(&counter);
				// Retorna milisegundos con alta precisión decimal
				return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
			#else
				// Para otras plataformas, usamos la versión de 64 bits si es posible
				// o el performance counter de SDL si usas SDL 2.0+
				return (double)SDL_GetTicks(); 
			#endif
			}

        static void setExecMethod(int var){EXEC_METHOD = var;}
        static int getExecMethod(){return EXEC_METHOD;}
	private:
		static std::string appDir;
		static std::string appExecutable;
        static int EXEC_METHOD;
};



