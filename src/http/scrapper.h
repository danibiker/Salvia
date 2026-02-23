#pragma once

#include <iostream>
#include <map>

#include <http/httputil.h>
#include <http/safedownloadqueue.h>
#include <const/constant.h>
#include <io/cfgloader.h>
#include <io/dirutil.h>
#include <const/menuconst.h>

enum MEDIA_TYPES { MEDIA_TITLE = 0, MEDIA_SS, MEDIA_BOX, MEDIA_MAX };
enum ASSETS_TYPES { ASSETS_TITLE = 0, ASSETS_SS, ASSETS_BOX, ASSETS_SINOPSIS, ASSETS_MAX};
// Solo anunciamos que el array existe en algún lugar
extern const char *MEDIAS_TO_FIND[];
extern const char *ASSETS_DIR[];

enum TGDB_Regions {
    REG_WORLDWIDE = 0,
    REG_USA = 1,
    REG_EUROPE = 2,
    REG_JAPAN = 3,
    REG_SPAIN = 10
};

struct t_media {
    MEDIA_TYPES type;
    std::string region;
    std::string url;
	
	t_media() : type(MEDIA_TITLE), region(""), url("") {}

	t_media(MEDIA_TYPES ptype, std::string pregion, std::string purl){
		type = ptype;
		region = pregion;
		url = purl;
	}
};

struct ScrapperConfig{
	bool downloadNoSS;
	bool downloadNoBox;
	bool downloadNoTitle;
	bool downloadNoMetadata;

	std::string regionPreferida;
	std::string lenguaPreferida;
	SCRAP_FROM origin;
	std::string apiKeyTGDB;

	ScrapperConfig(){
		clear();
	}
	void clear(){	
		downloadNoSS = false;
		downloadNoBox = false;
		downloadNoTitle = false;
		downloadNoMetadata = false;

		regionPreferida = "eu";
		lenguaPreferida = "en";
		origin = SC_THEGAMESDB;
	}
};

struct ScraperResult {
	std::string snapdir;
	std::string titledir;
	std::string boxdir;
	std::string sinopsisdir;
	std::string filenameNoExt;

    std::string nombre;
    std::string sinopsis;
    std::map<std::string, t_media> medias;
};

struct ScraperAsk {
    int gameid;
	int sistema;
	std::string regionPreferida;
	std::string lenguaPreferida;
	std::string romname;
	std::string apiKeyTGDB;

	ScraperAsk(){
		regionPreferida = "eu";
		lenguaPreferida = "en";
		sistema = 0;
		gameid = -1;
	}
};

// Estructura para pasar parámetros al hilo principal de scrap
struct ScrapParams {
    std::vector<ConfigEmu> emu;
    ScrapperConfig cfg;
};

struct ScrapStatus {
    CRITICAL_SECTION cs;
    int procesados;
    int total;
    char emuActual[64];
    char juegoActual[128];

    ScrapStatus() : procesados(0), total(0) {
        InitializeCriticalSection(&cs);
        emuActual[0] = '\0';
        juegoActual[0] = '\0';
    }
};

class Scrapper{
	public:
		Scrapper();
		~Scrapper();
		static ScrapStatus g_status;
		static bool isScrapping();
		int scrapSystem(ConfigEmu& emulatorCfg, ScrapperConfig& scrapperConfig, SafeDownloadQueue& dwQueue, bool onlyCount = false);
		static void StartScrappingAsync(std::vector<ConfigEmu> emu, ScrapperConfig cfg);
		static void ShutdownScrapper();
	private:
		static bool scrapping;
		static HANDLE hMainThread;
	
		static DWORD WINAPI imageDownloaderThread(LPVOID lpParam);
		static DWORD WINAPI mainScrapThread(LPVOID lpParam);

		void procesarRespuestaScreenscraper(std::string&, ScraperAsk&, ScraperResult&);
		void procesarRespuestaGamesDb(std::string&, ScraperAsk&, ScraperResult&);
		void obtenerImagenesTGDB(std::string&, ScraperAsk&, ScraperResult&);
		void actualizarProgreso(const char* emu, const char* juego);
		std::string leerArchivoTexto(const std::string& ruta);
		bool guardarArchivoTexto(const std::string& ruta, const std::string& contenido);
		// Una versión ultra-simple para limpiar caracteres UTF-8 comunes en nombres de juegos
		std::string cleanUTF8(const std::string& str);
		void guardarRecursos();
		int GSIdToGD(int);
		std::string limpiarNombreJuego(std::string nombre) ;
		map<int,int> gsTogdGameid;
		bool cargarEquivalencias(const std::string& nombreArchivo);
		int obtenerRegionPreferenciaTGDB(ScraperAsk& peticion);
		void obtenerImagenesTGDB(ScraperAsk&, ScraperResult&);
		void guardarRecursos(SafeDownloadQueue& dwQueue, ScraperResult &resultado);
};

	