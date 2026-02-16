#pragma once

#include <iostream>
#include <map>

#include <http/httputil.h>
#include <http/pugixml.hpp>
#include <http/safedownloadqueue.h>
#include <const/constant.h>
#include <io/cfgloader.h>
#include <io/dirutil.h>

enum MEDIA_TYPES { MEDIA_TITLE = 0, MEDIA_SS, MEDIA_BOX, MEDIA_MAX };
enum ASSETS_TYPES { ASSETS_TITLE = 0, ASSETS_SS, ASSETS_BOX, ASSETS_SINOPSIS, ASSETS_MAX};
// Solo anunciamos que el array existe en algún lugar
extern const char *MEDIAS_TO_FIND[];
extern const char *ASSETS_DIR[];

struct t_media {
    MEDIA_TYPES type;
    std::string region;
    std::string url;
};

struct ScrapperConfig{
	bool downloadNoSS;
	bool downloadNoBox;
	bool downloadNoTitle;
	bool downloadNoMetadata;

	std::string regionPreferida;
	std::string lenguaPreferida;

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
	}
};

struct ScraperResult {
    std::string nombre;
    std::string sinopsis;
    std::map<std::string, t_media> medias;
};

struct ScraperAsk {
    std::string regionPreferida;
	std::string lenguaPreferida;
	int sistema;
	std::string romname;
	
	ScraperAsk(){
		regionPreferida = "eu";
		lenguaPreferida = "en";
		sistema = 0;
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

		void procesarRespuestaScreenscraper(std::string& xmlData, ScraperAsk& peticion, ScraperResult& resultado);
		void actualizarProgreso(const char* emu, const char* juego);
		std::string leerArchivoTexto(const std::string& ruta);
		bool guardarArchivoTexto(const std::string& ruta, const std::string& contenido);
		// Una versión ultra-simple para limpiar caracteres UTF-8 comunes en nombres de juegos
		std::string cleanUTF8(const std::string& str);
};

