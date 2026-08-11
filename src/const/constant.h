#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <gfx/gfx_utils.h>
#include <string>
#include <sstream>
#include <cctype> // Para isdigit
#include <algorithm>
#include <stdint.h>
#include <cmath>
#include <vector>
#include <unordered_set>
#include <stdarg.h>

#include <utils/logger.h>
#include <font/fonts.h>

static const int video_bpp = 16;

/* Final layout:
 *   core 0  Xbox 360 Dashboard                      ┐
 *   core 1  main / Salvia / retro_run / dynarec PSX ┘ same physical core
 *   core 2  PSX GPU helper thread        ┐
 *   core 3  idle                         ┘ same physical core
 *   core 4  IO & HTTP					  ┐
 *   core 5  Xaudio and SDL Sound thread  ┘ same physical core
 */
#define CPU_THREAD 1
#define IO_THREAD 4

static int video_width = 1280;
static int video_height = 720;

#ifdef _XBOX
	static Uint32 video_flags = SDL_SWSURFACE;
	static const char *LOG_PATH = "game:\\salvia.log";
	#include <xtl.h>
#elif defined(WIN)
	//SDL_SWSURFACE | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN
	static Uint32 video_flags = SDL_SWSURFACE; 
	static const char *LOG_PATH = "salvia.log";
	#include <windows.h>
#endif

/* Video por GPU con shaders: en Xbox lo hace el driver SDL custom
(SDL_xboxvideo.c); en Windows la capa D3D9 propia (src/video/win_d3d9.*).
Ambas plataformas comparten los call-sites del frontend bajo este flag. */
#if defined(_XBOX) || defined(WIN)
	#define SALVIA_GPU_VIDEO 1
#endif

static const int bkgSpeedPixPerS = 15;
static const double bkgFrameTimeTick = 1000.0 / bkgSpeedPixPerS;

static const unsigned long KEYRETRASO = 500;
static const int JOYHATOFFSET = 100;
static const int JOYAXISOFFSET = 200;
static const int DEADZONE = 23000;
static const int DEADZONE_ANAL = 300;
static const unsigned long DBLCLICKSPEED = 300; //tiempo en ms para poder hacer un doble click
static const unsigned long KEYDOWNSPEED = 50;
static const unsigned long MOUSEVISIBLE = 8000;
static const int CURSORVISIBLE = 1;
static const int LONGKEYTIMEOUT = 2000;
static const int MAX_SAVESTATES = 10;
static const char *STATE_IMG_EXT = ".png";
static const char *STATE_EXT = ".state";
static const char *CD_FILTER = ".bin .cue .img .mdf .pbp .cbn .iso .chd .m3u";
static const char *TMP_DIR = "tmp";
static const std::string BIOS_ONLY = "@bios-only";
static const std::string ASSETS_ICONS_DIR = "\\assets\\xmb\\retrosystem\\png\\";

const bool SMOOTH_RESIZE = true;

typedef enum {
    cursor_hidden,
    cursor_arrow,
    cursor_resize,
    cursor_hand,
    cursor_wait,
    totalCursors
} enumCursors;

typedef enum {
    clBackground = 0,
	clBlack,
	clWhite,
	clYellow,
	clRed,
	clBlue,
	clBkgMenu,
	clBkgMenuLighter,
	clBG,
	clBorder,
	clMenuBars,
	clTxtNavBar,
	clDarkGray,
	clSwitchEnabled,
	clSwitchDisabled,
	clPaleBlue,
	clAskTitle,
	clAskBg, 	
	clAskLine, 
	clAskText, 
    clTotalColors
} enumColors;


struct svColor{
	SDL_Color sdlColor;
	Uint32 color;		//Color converted through SDL_MapRGBA
	Uint32 colorRaw;	//Color converted through byte shifts
};


struct Message {
    std::string content;
    Uint32 ticks;
    Uint32 timeout;
    SDL_Surface* cache; // Nueva superficie para el mensaje renderizado
    SDL_Rect rect;      // Para guardar el tamanyo y posicion calculados

	Message(){
		cache = NULL;
		ticks = 0;
		timeout = 0;
		content = "";
	}
};

enum ACH_TYPE{ACH_LOAD_GAME, ACH_UNLOCKED, ACH_WARNING};

struct AchievementState{
	volatile bool isDownloading;		// Especifica si se esta descargando el logro
	bool locked;			// Especifica si el logro esta bloqueado
	bool isSection;			// Seccion del logro
	uint8_t sectionType;	// Tipo de seccion
	uint32_t points;		// Puntos del logro
	ACH_TYPE type;			// Tipo de mensaje

	std::string title;		  // Titulo del mensaje
	std::string description;  // Descripcion del mensaje
	std::string badgeUrl;     // Url de la imagen
	std::string badgeName;    // Nombre de la imagen
	SDL_Surface *badge;       // Original a color
	SDL_Surface *badgeLocked; // Version en gris (cache)
	std::string progress;	  // Progreso del logro
	uint32_t id;			  // Id del logro
	uint32_t gameId;		  // Id del juego
	
    uint32_t ticks;			  // Para el temporizador de pantalla
    uint32_t timeout;         // Cuanto tiempo debe mostrarse (ms)
    
    // Campos extra para el mensaje de "Juego Cargado"
    uint32_t achvTotal;		  // Numero de logros totales
    uint32_t scoreTotal;	  // Puntuacion conseguida
	uint32_t scoreUnlocked;
    uint32_t achvUnlocked;    // Numer de logros conseguidos

	int reqWidth;
	int reqHeight;

	AchievementState(){
		inicializar();
	}

	~AchievementState() {
	}

	void inicializar(){
		badge = NULL;
		badgeLocked = NULL;
		locked = false;
		isDownloading = false;
		isSection = false;
		points = 0;
		sectionType = 0;
		type = ACH_UNLOCKED;
		id = 0;
		gameId = 0;
		ticks = 0;
        timeout = 3000; // 3 segundos por defecto
        achvTotal = 0;
        scoreTotal = 0;
        achvUnlocked = 0;
		scoreUnlocked = 0;
		reqWidth = 0;
		reqHeight = 0;
	}

	AchievementState(std::string pTitle, uint8_t st){
		inicializar();		
		title = pTitle;
		isSection = true;
		sectionType = st;
	}

	// 1. Constructor de copia (necesario para pasar por valor)
    AchievementState(const AchievementState& other) {
        inicializar();
        copiarDesde(other);
    }

    // 2. Operador de asignacion (operador =)
    AchievementState& operator=(const AchievementState& other) {
        if (this != &other) { // Evitar auto-asignacion
            copiarDesde(other);
        }
        return *this;
    }

	void clearSurfaces(){
		if (badge != NULL) {
			SDL_FreeSurface(badge);
			badge = NULL;
		}
		if (badgeLocked != NULL) {
			SDL_FreeSurface(badgeLocked);
			badgeLocked = NULL;
		}
	}

	private:
    // Funcion auxiliar para centralizar la logica de copia
    void copiarDesde(const AchievementState& other) {
        locked = other.locked;
        isDownloading = other.isDownloading;
        isSection = other.isSection;
        sectionType = other.sectionType;
        points = other.points;
        type = other.type;
        id = other.id;
        badgeUrl = other.badgeUrl;
        badgeName = other.badgeName;
        title = other.title;
        description = other.description;
        progress = other.progress;
		gameId = other.gameId;
		ticks = other.ticks;
		timeout = other.timeout; 
		achvTotal = other.achvTotal;
		scoreTotal = other.scoreTotal;
		achvUnlocked = other.achvUnlocked;
		scoreUnlocked = other.scoreUnlocked;
		reqWidth = other.reqWidth;
		reqHeight = other.reqHeight;
        // COPIA SUPERFICIAL: Solo copiamos la direccion de memoria.
		// No usamos SDL_DisplayFormat aqui para evitar fugas de memoria.
		this->badge = other.badge;
		this->badgeLocked = other.badgeLocked;
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

#define MAX_PLAYERS 4

typedef enum {
        page_white_text,
        folder,
        page_white,
        page_white_gear,
        page_white_compressed,
        page_white_picture,
        page_white_zip,
		ico_video,
		ico_settings,
		ico_settings_core,
		ico_subsettings,
		ico_remap,
		ico_savestates,
		ico_saving,
		ico_return,
		ico_scrapper,
		ico_achievements,
		ico_shutdown,
		ico_help,
		ico_cheats,
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
			  cart_pce_cd,
			  cart_mame,
			  cart_neogeo_pocket,
			  cart_zx,
			  cart_msx,
			  cart_dos,
			  cart_doom,
			  cart_supergrafx,
			  cart_quake,
			  cart_default,
			  cart_3do,
			  cart_wonderswan,
			  cart_virtualboy,
			  cart_atarilynx,
			  cart_atari800,
			  cart_atari5200,
			  cart_c64,
			  max_carts};

extern float aspectRatioValues[]; 
extern const char *JOY_DESCRIPTIONS[];
extern const char *ICONS_PATH[];
extern const char *ICONS_CARTS_PATH[];
// Nombres de sistema de libretro-database (para los ficheros .rdb / carpetas .cht),
// indexado por el enum cart_*. "" = sistema sin base de cheats por CRC. Independiente
// de ICONS_CARTS_PATH (que es solo para los iconos del listado).
extern const char *RDB_SYSTEM_NAMES[];
extern const std::string CFG_EXT;
extern const std::string RETROPAD_INI;
extern const std::string ROUTE_ACHIEVEMENT_TRANSLATIONS;
extern const std::string ROUTE_SCRAP_TRANSLATIONS;
extern const std::string PREFIX_DEFAULTS;
extern const std::string BG_FILENAME;
extern const std::string TITLE_EMU_FILENAME;
extern const std::string QUAKE_LIST_URL;
// Definimos el tamaño exacto a mano
const int QUAKE_MAPS_COUNT = 1; 
extern const std::string QUAKE_MAPS_URL[QUAKE_MAPS_COUNT];
extern const std::string START_FROM_EXCEPTION;
extern const char *SDL_BTN_TO_XBOX[12];
extern std::string SDL_JOY_TO_XBOX[6];
extern std::string SDL_HAT_TO_XBOX[9];
extern std::string FRONTEND_BTN_TXT[MAXJOYBUTTONS];
extern const std::string SCRAPPING_DAT;
extern const std::string PASS_MASK;
extern const char SYMBOLS_TO_SPACE[];
extern const char SYMBOLS_TO_REMOVE[];

typedef enum {
    launch_system,          //0
    launch_spawn,           //1
    launch_create_process,  //2
    launch_batch
} launchMethods;

enum SYNC_TYPES{
	SYNC_TO_AUDIO = 0,
	SYNC_TO_VIDEO,
	SYNC_NONE,
	SYNC_FAST_FORWARD
};


/* SDL 1.2: Definir mascaras segun el orden de bytes del sistema */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    // Formato ARGB (Tipico en PowerPC / Xbox 360)
    static Uint32 rmask = 0x00FF0000;
    static Uint32 gmask = 0x0000FF00;
    static Uint32 bmask = 0x000000FF;
    static Uint32 amask = 0xFF000000;
#else
    // Formato ARGB en Little Endian (x86) se almacena como BGRA en memoria, 
    // pero SDL maneja estas mascaras para que el uint32_t sea 0xAARRGGBB
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

		static svColor colors[clTotalColors];

		static std::string getAppDir(){ 
            return appDir; 
        }
        static void setAppDir(std::string var){
            appDir = var;
        }

		static std::string getTmpDir(){
			return Constant::getAppDir() + Constant::getFileSep() + TMP_DIR;
		}

		static std::string getAppExecutable(){ 
            return appExecutable; 
        }
        static void setAppExecutable(std::string var){
            appExecutable = var;
        }

		// Funcion auxiliar para convertir int a std::string (VS2008 no tiene std::to_string)
		static std::string intToString(int value) {
			std::stringstream ss;
			ss << value;
			return ss.str();
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
            std::size_t position = 0;
            std::size_t lastPosition = 0;
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
            std::size_t startpos = s.find_first_not_of(WHITESPACE);
            return (startpos == std::string::npos) ? "" : s.substr(startpos);
        }

        static std::string TrimRight(const std::string& s)
        {
            std::size_t endpos = s.find_last_not_of(WHITESPACE);
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

		static std::vector<std::string> split(const std::string &s, const std::string &delims) {
			std::vector<std::string> elems;
			size_t start = 0;
			size_t end = s.find_first_of(delims);

			while (end != std::string::npos) {
				// Evita añadir elementos vacíos si hay delimitadores consecutivos
				if (end > start) {
					elems.push_back(s.substr(start, end - start));
				}
				start = end + 1;
				end = s.find_first_of(delims, start);
			}

			if (start < s.length()) {
				elems.push_back(s.substr(start));
			}

			return elems;
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
				// Trim opcional por si hay espacios entre la coma y el numero
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

		static std::unordered_set<std::string>& splitCharSet(const std::string &s, char delim, std::unordered_set<std::string> &elems) {
			std::stringstream ss(s);
			std::string item;
			while (std::getline(ss, item, delim)) {
				if (!item.empty()) { // Opcional: evita insertar cadenas vacias si hay dobles espacios
					elems.insert(item);
				}
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
			// stringstream lo leeria como caracter.
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
    
			// Si permites numeros negativos, saltamos el signo '-'
			std::size_t inicio = (s[0] == '-' && s.size() > 1) ? 1 : 0;

			for (std::size_t i = inicio; i < s.size(); i++) {
				if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
					return false;
				}
			}
			return true;
		}

		// Funcion auxiliar
		static bool compareNoCase(const std::string& a, const std::string& b) {
			for (std::size_t i = 0; i < a.length() && i < b.length(); ++i) {
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
				// Retorna milisegundos con alta precision decimal
				return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
			#else
				// Para otras plataformas, usamos la version de 64 bits si es posible
				// o el performance counter de SDL si usas SDL 2.0+
				return (double)SDL_GetTicks(); 
			#endif
			}

		static std::string sanitizePathForXbox(const std::string& fullPath) {
			// 1. Separar ruta y nombre
			std::size_t lastSlash = fullPath.find_last_of("\\/");
			std::string directory = (lastSlash == std::string::npos) ? "" : fullPath.substr(0, lastSlash + 1);
			std::string filename = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);

			// 2. Caracteres prohibidos (FATX es estricto con estos)
			const char* forbiddenChars = "<>:\"/\\|?*&[]!,;=+";
			for (std::size_t i = 0; i < filename.length(); ++i) {
				if (strchr(forbiddenChars, filename[i])) {
					filename[i] = '_';
				}
			}

			// 3. Gestion de longitud maxima (42 caracteres)
			if (filename.length() > 42) {
				// Buscamos el PRIMER punto para identificar donde empiezan las extensiones
				// Ejemplo: "Sonic.Knuckles.bin" -> queremos conservar ".Knuckles.bin"
				std::size_t firstDot = filename.find_first_of('.');
        
				if (firstDot != std::string::npos) {
					std::string allExtensions = filename.substr(firstDot); // ".bin.state"
            
					// Si las extensiones son absurdamente largas (mas de 30 chars), recortamos a lo bruto
					if (allExtensions.length() > 30) {
						filename = filename.substr(0, 42);
					} else {
						// Recortamos el nombre base para que quepan las extensiones
						std::size_t maxBaseLen = 42 - allExtensions.length();
						filename = filename.substr(0, maxBaseLen) + allExtensions;
					}
				} else {
					// Sin puntos, recorte directo
					filename = filename.substr(0, 42);
				}
			}

			return directory + filename;
		}

		static std::string checkPath(std::string path){
			std::string prefix = path.substr(0, 6);
			std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);

			if (prefix == "game:\\") {
				return sanitizePathForXbox(path);
			} else {
				return path;
			}
		}

		 static std::string string_format(const std::string fmt, ...) {
			int size = 256; // Tamanyo inicial sugerido
			std::string str;
			va_list ap;
			while (1) {
				str.resize(size);
				va_start(ap, fmt);
				// _vsnprintf es la version segura para Visual Studio 2010
				int n = _vsnprintf_s((char *)str.c_str(), size, _TRUNCATE, fmt.c_str(), ap);
				va_end(ap);
				if (n > -1 && n < size) {
					str.resize(n);
					return str;
				}
				if (n > -1) size = n + 1; // Tamanyo exacto necesario
				else size *= 2; // Doblar y reintentar (especifico de implementaciones antiguas)
			}
		}

		static std::string formatPlayTime(uint32_t totalSeconds) {
			uint32_t hours = totalSeconds / 3600;
			uint32_t minutes = (totalSeconds % 3600) / 60;
    
			char buffer[32];
			sprintf(buffer, "%dh %dm", hours, minutes);
			return std::string(buffer);
		}

		static void setup_and_run_thread(HANDLE hThread, int core, bool autoClose = true) {
			#ifdef _XBOX
			// Forzar la ejecucion en el nucleo especificado
			XSetThreadProcessor(hThread, core); 
			#endif

			// Bajar prioridad para no afectar el rendimiento del juego/emulador
			SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

			// Arrancar el hilo que estaba suspendido
			ResumeThread(hThread);

			if (autoClose){
				// Liberar el handle (el hilo continua su ejecucion de forma independiente)
				CloseHandle(hThread);
			}
		}

		static void createRectAlphaFilled(SDL_Surface*& selecAlphaRec, SDL_Rect& rectElem, SDL_PixelFormat* format, enumColors color, bool border = false){
			bool drawWithAlfa = true;
			#if defined(_XBOX)
				drawWithAlfa = false;
			#endif

			if (selecAlphaRec == NULL){
				SDL_Surface* rawSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, rectElem.w, rectElem.h, 
															   format->BitsPerPixel,
															   format->Rmask, 
															   format->Gmask, 
															   format->Bmask, 
															   format->Amask);

				const Uint8 KEY_ALPHA = 180;
				Uint8 fillColorAlpha = KEY_ALPHA;
				#if !defined(_XBOX) && !defined(SALVIA_GPU_VIDEO)
					Uint32 colorkey = SDL_MapRGB(rawSurface->format, 255, 0, 255);
					SDL_FillRect(rawSurface, nullptr, colorkey);
					SDL_SetColorKey(rawSurface, SDL_SRCCOLORKEY, colorkey);
					fillColorAlpha = 0xFF;
				#endif
				
				Uint32 keyBg = SDL_MapRGBA(rawSurface->format,
                                            Constant::colors[color].sdlColor.r,
                                            Constant::colors[color].sdlColor.g,
                                            Constant::colors[color].sdlColor.b,
                                            drawWithAlfa ? fillColorAlpha : 0xFF);

				SDL_FillRect(rawSurface, NULL, keyBg);
				
				if (border && drawWithAlfa)
					rect(rawSurface, 0, 0, rectElem.w - 1, rectElem.h - 1, Constant::colors[color].sdlColor);

				#if defined(_XBOX)
					//En xbox es demasiado intensivo dibujar transparencias
					SDL_SetAlpha(rawSurface, SDL_RLEACCEL, 0xFF);
					selecAlphaRec = rawSurface;
				#elif defined(SALVIA_GPU_VIDEO)
					selecAlphaRec = rawSurface;
				#else 
						SDL_SetAlpha(rawSurface, SDL_SRCALPHA, KEY_ALPHA);
						selecAlphaRec = SDL_DisplayFormatAlpha(rawSurface);
						SDL_FreeSurface(rawSurface);
				#endif
			}
		}

		// Convierte de CP_ACP (ANSI) a UTF-8 si la cadena no es UTF-8 válido
		static std::string ansiToUtf8(const std::string& s) {
			if (s.empty()) return s;
			// Verificar si ya es UTF-8 válido
			bool validUtf8 = true;
			for (std::size_t i = 0; i < s.size(); ++i) {
				unsigned char c = (unsigned char)s[i];
				if (c < 0x80) continue;
				int seqLen = 0;
				if      ((c & 0xE0) == 0xC0) seqLen = 2;
				else if ((c & 0xF0) == 0xE0) seqLen = 3;
				else if ((c & 0xF8) == 0xF0) seqLen = 4;
				else { validUtf8 = false; break; }
				for (int j = 1; j < seqLen; ++j) {
					if (++i >= s.size() || ((unsigned char)s[i] & 0xC0) != 0x80)
						{ validUtf8 = false; break; }
				}
				if (!validUtf8) break;
			}
			if (validUtf8) return s;
			// No es UTF-8 válido -> convertir de CP_ACP a UTF-8
			int wideLen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
			if (wideLen <= 0) return s;
			std::wstring wide(wideLen, L'\0');
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &wide[0], wideLen);
			int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
			if (utf8Len <= 0) return s;
			std::string utf8(utf8Len, '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Len, nullptr, nullptr);
			utf8.pop_back(); // quitar null terminator
			return utf8;
		}

		static std::string unicodeToUtf8String(unsigned int cp){
			std::string value;
			if (cp < 0x80) {
				value = (char)cp;
			} else if (cp < 0x800) {
				value = (char)(0xC0 | (cp >> 6));
				value += (char)(0x80 | (cp & 0x3F));
			} else if (cp < 0x10000) {
				value = (char)(0xE0 | (cp >> 12));
				value += (char)(0x80 | ((cp >> 6) & 0x3F));
				value += (char)(0x80 | (cp & 0x3F));
			} else {
				value = (char)(0xF0 | (cp >> 18));
				value += (char)(0x80 | ((cp >> 12) & 0x3F));
				value += (char)(0x80 | ((cp >> 6) & 0x3F));
				value += (char)(0x80 | (cp & 0x3F));
			}

			return value;
		}

		static std::string limpiarNombreJuego(std::string nombre) {
			std::string temporal = "";
			int nivelParentesis = 0;

			for (std::size_t i = 0; i < nombre.length(); ++i) {
				char c = nombre[i];

				// 1. Gestion de parentesis/corchetes
				if (c == '(' || c == '[') { nivelParentesis++; continue; }
				if (c == ')' || c == ']') { if (nivelParentesis > 0) nivelParentesis--; continue; }

				if (nivelParentesis == 0) {
					// 2. Es un caracter para sustituir por espacio?
					if (strchr(SYMBOLS_TO_SPACE, c)) {
						temporal += ' ';
					}
					// 3. Es un caracter para eliminar?
					else if (strchr(SYMBOLS_TO_REMOVE, c)) {
						continue;
					}
					// 4. Caracter normal
					else {
						temporal += c;
					}
				}
			}

			// 5. Colapsar espacios multiples y Trim (Limpieza final)
			std::string resultado = "";
			bool ultimoFueEspacio = true; // Empezamos en true para evitar espacio al inicio

			for (std::size_t i = 0; i < temporal.length(); ++i) {
				if (isspace(temporal[i])) {
					if (!ultimoFueEspacio) {
						resultado += ' ';
						ultimoFueEspacio = true;
					}
				} else {
					resultado += temporal[i];
					ultimoFueEspacio = false;
				}
			}

			// Eliminar el posible espacio final
			if (!resultado.empty() && resultado[resultado.length()-1] == ' ') {
				resultado.erase(resultado.length()-1);
			}

			return resultado;
		}

        static void setExecMethod(int var){EXEC_METHOD = var;}
        static int getExecMethod(){return EXEC_METHOD;}
	private:
		static std::string appDir;
		static std::string appExecutable;
        static int EXEC_METHOD;
};