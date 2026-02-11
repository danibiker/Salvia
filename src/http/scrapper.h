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
	Scrapper(){
		scrapping = false;
	}

	~Scrapper(){}

	// Instancia global o pasada por puntero
	static ScrapStatus g_status;
	static bool isScrapping(){
		return scrapping;
	}

	int scrapSystem(ConfigEmu& emulatorCfg, ScrapperConfig& scrapperConfig, SafeDownloadQueue& dwQueue, bool onlyCount = false) {
		std::string romsdir = Constant::getAppDir() + Constant::getFileSep() + emulatorCfg.rom_directory;
		std::string assetsdir = Constant::getAppDir() + Constant::getFileSep() + emulatorCfg.assets + Constant::getFileSep();
		CurlClient downloader;
		int sistema = 0;
		dirutil dir;
		std::vector<unique_ptr<FileProps> > files;
		
		int counterFiles = 0;

		// Filtro de extensiones
		std::string extFilter = " " + emulatorCfg.rom_extension;
        extFilter = Constant::replaceAll(extFilter, " ", ".");

		// Obtener ID de sistema
		std::vector<std::string> sistemaSplit = Constant::splitChar(emulatorCfg.system, '_');
		if (sistemaSplit.size() > 0) {
			sistema = Constant::strToTipo<int>(sistemaSplit[0]);
		} else {
			return counterFiles;
		}

		// Configuración de rutas
		std::string snapdir = assetsdir + ASSETS_DIR[ASSETS_SS];
		std::string titledir = assetsdir + ASSETS_DIR[ASSETS_TITLE];
		std::string boxdir = assetsdir + ASSETS_DIR[ASSETS_BOX];
		std::string sinopsisdir = assetsdir + ASSETS_DIR[ASSETS_SINOPSIS];

		// Creación de directorios
		if (!dir.dirExists(snapdir.c_str())) dir.createDirRecursive(snapdir.c_str());
		if (!dir.dirExists(titledir.c_str())) dir.createDirRecursive(titledir.c_str());
		if (!dir.dirExists(boxdir.c_str())) dir.createDirRecursive(boxdir.c_str());
		if (!dir.dirExists(sinopsisdir.c_str())) dir.createDirRecursive(sinopsisdir.c_str());

		dir.listarFilesSuperFast(romsdir.c_str(), files, extFilter, true, false);

		// URL base fuera del bucle para ahorrar CPU/RAM
		std::string urlBase = "https://api.screenscraper.fr/api2/jeuInfos.php";
		urlBase += "?devid=jelos&devpassword=jelos&softname=scrapdos1&output=xml&ssid=test&sspassword=test";
		urlBase += "&systemeid=" + Constant::TipoToStr(sistema);

		for (std::size_t i = 0; i < files.size(); i++) {
			FileProps* file = files.at(i).get();
			std::string filenameNoExt = dir.getFileNameNoExt(file->filename);
			
			//Si ya existen los elementos, no es necesario hacer peticiones a screenscrapper
			if (scrapperConfig.downloadNoSS && dir.fileExists(std::string(snapdir + Constant::getFileSep() + filenameNoExt + ".png").c_str())) 
				continue;
			if (scrapperConfig.downloadNoBox && dir.fileExists(std::string(boxdir + Constant::getFileSep() + filenameNoExt + ".png").c_str())) 
				continue;
			if (scrapperConfig.downloadNoTitle && dir.fileExists(std::string(titledir + Constant::getFileSep() + filenameNoExt + ".png").c_str())) 
				continue;
			if (scrapperConfig.downloadNoMetadata && dir.fileExists(std::string(sinopsisdir + Constant::getFileSep() + filenameNoExt + ".txt").c_str())) 
				continue;

			counterFiles++;
			if (onlyCount){
				continue;
			}			

			actualizarProgreso(emulatorCfg.name.c_str(), filenameNoExt.c_str());

			std::string response;
			float downloadProgress = 0.0f;
        
			ScraperAsk peticion;
			peticion.regionPreferida = scrapperConfig.regionPreferida;
			peticion.lenguaPreferida = scrapperConfig.lenguaPreferida;
			
			// Escapamos el nombre
			peticion.romname = downloader.escape(filenameNoExt);
			std::string fullUrl = urlBase + "&romtype=rom&romnom=" + peticion.romname;

			LOG_DEBUG("Buscando datos para: %s", file->filename.c_str());

			if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
				ScraperResult resultado;
				procesarRespuestaScreenscraper(response, peticion, resultado);

				// 1. Guardar Sinopsis
				std::string rutaTxt = sinopsisdir + Constant::getFileSep() + filenameNoExt + ".txt";
				if (!dir.fileExists(rutaTxt.c_str()) && !resultado.sinopsis.empty()) {
					guardarArchivoTexto(rutaTxt, resultado.sinopsis);
				}

				// 2. Guardar Imágenes
				std::map<std::string, t_media>::iterator it;
				for (it = resultado.medias.begin(); it != resultado.medias.end(); ++it) {
					std::string destDir = "";
					switch (it->second.type) {
						case MEDIA_TITLE: destDir = titledir; break;
						case MEDIA_SS:    destDir = snapdir; break;
						case MEDIA_BOX:   destDir = boxdir; break;
						default: continue;
					}

					std::string rutaImg = destDir + Constant::getFileSep() + filenameNoExt + ".png";
                
					if (!dir.fileExists(rutaImg.c_str()) && !it->second.url.empty()) {
						LOG_DEBUG("Descargando imagen: %s", rutaImg.c_str());
						//downloader.fetchFile(it->second.url, rutaImg, &downloadProgress);
						DownloadTask task;
						task.url = it->second.url;
						task.destPath = rutaImg;
						task.downloadProgress = 0.0f;
                
						dwQueue.push(task); // Encolamos para el otro hilo
					}
				}
			}
		}
		return counterFiles;
	}

	// Llamada desde tu menú/emulador:
	static void StartScrappingAsync(std::vector<ConfigEmu> emu, ScrapperConfig cfg) {
		if (!scrapping){
			ScrapParams* params = new ScrapParams();
			params->emu = emu;
			params->cfg = cfg;
    
			HANDLE hThread = CreateThread(NULL, 0, mainScrapThread, params, 0, NULL);
			if (hThread) CloseHandle(hThread); // No necesitamos el handle, que corra libre
		}
	}

private:
	static bool scrapping;

	void procesarRespuestaScreenscraper(std::string& xmlData, ScraperAsk& peticion, ScraperResult& resultado) {
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_string(xmlData.c_str());
		xmlData.clear();

		if (!result) return;

		pugi::xml_node juego = doc.child("Data").child("jeu");

		// --- SECCIÓN NOMBRE ---
		pugi::xml_node noms = juego.child("noms");
		for (pugi::xml_node_iterator it = noms.begin(); it != noms.end(); ++it) {
			const char* reg = it->attribute("region").value();
			// Priorizamos 'eu', si no, cualquier cosa es mejor que nada
			if (strcmp(reg, peticion.regionPreferida.c_str()) == 0) {
				// Encontramos el idioma ideal, lo guardamos y salimos del bucle
				resultado.nombre = it->child_value();
				break; 
			} 
			else if (strcmp(reg, "eu") == 0) {
				// Es inglés, lo guardamos como backup pero seguimos buscando por si aparece el preferido
				resultado.nombre = it->child_value();
			}
			else if (resultado.nombre.empty()) {
				// Si no hay nada aún, cogemos cualquier idioma que venga (primer backup)
				resultado.nombre = it->child_value();
			}
		}

		resultado.nombre = cleanUTF8(resultado.nombre);

		// --- SECCIÓN SYNOPSIS CON IDIOMA PREFERENTE ---
		pugi::xml_node synopsis_root = juego.child("synopsis");
		for (pugi::xml_node_iterator it = synopsis_root.begin(); it != synopsis_root.end(); ++it) {
			const char* lang = it->attribute("langue").value();
    
			if (strcmp(lang, peticion.lenguaPreferida.c_str()) == 0) {
				// Encontramos el idioma ideal, lo guardamos y salimos del bucle
				resultado.sinopsis = it->child_value();
				break; 
			} 
			else if (strcmp(lang, "en") == 0) {
				// Es inglés, lo guardamos como backup pero seguimos buscando por si aparece el preferido
				resultado.sinopsis = it->child_value();
			}
			else if (resultado.sinopsis.empty()) {
				// Si no hay nada aún, cogemos cualquier idioma que venga (primer backup)
				resultado.sinopsis = it->child_value();
			}
		}
		//resultado.sinopsis = cleanUTF8(resultado.sinopsis);
		// --- SECCIÓN MEDIAS CON PRIORIDAD ---
		pugi::xml_node medias_root = juego.child("medias");

		for (pugi::xml_node_iterator it = medias_root.begin(); it != medias_root.end(); ++it) {
			const char* typeAttr = it->attribute("type").value();

			for (int i = 0; i < MEDIA_MAX; i++) {
				if (strcmp(typeAttr, MEDIAS_TO_FIND[i]) == 0) {
					t_media& tMedia = resultado.medias[typeAttr];
					const char* reg = it->attribute("region").value();

					// Calculamos la "calidad" de lo que acabamos de encontrar
					// 3 = Preferida (es), 2 = Europa (eu), 1 = Cualquier otra, 0 = Nada
					int puntuacionNueva = 1;
					if (strcmp(reg, peticion.regionPreferida.c_str()) == 0) puntuacionNueva = 3;
					else if (strcmp(reg, "eu") == 0) puntuacionNueva = 2;

					// Calculamos la "calidad" de lo que ya teníamos guardado
					int puntuacionActual = 0;
					if (!tMedia.url.empty()) {
						if (strcmp(tMedia.region.c_str(), peticion.regionPreferida.c_str()) == 0) puntuacionActual = 3;
						else if (tMedia.region == "eu") puntuacionActual = 2;
						else puntuacionActual = 1;
					}

					// Solo sobrescribimos si la nueva región es mejor que la actual
					if (puntuacionNueva > puntuacionActual) {
						tMedia.region = reg;
						tMedia.type = static_cast<MEDIA_TYPES>(i);
						tMedia.url = it->child_value();
						LOG_DEBUG("Mejor imagen encontrada: %s (%s)", MEDIAS_TO_FIND[i], reg);
					}
					break; 
				}
			}
		}

		// --- LOGS ---
		LOG_DEBUG("Juego: %s", resultado.nombre.c_str());
		if (resultado.medias.count("box-2D")) {
			LOG_DEBUG("URL Box (%d): %s",resultado.medias.count("box-2D"), resultado.medias["box-2D"].url.c_str());
		}
	}

	static DWORD WINAPI imageDownloaderThread(LPVOID lpParam) {
		SafeDownloadQueue* queue = (SafeDownloadQueue*)lpParam;
		CurlClient downloader;
		DownloadTask task;

		while (queue->pop(task)) {
			LOG_DEBUG("Hilo Secundario: Descargando %s", task.destPath.c_str());
			downloader.fetchFile(task.url, task.destPath, &task.downloadProgress);
		}
		return 0;
	}

	// Dentro de mainScrapThread o scrapSystem
	void actualizarProgreso(const char* emu, const char* juego) {
		EnterCriticalSection(&g_status.cs);
		g_status.procesados++;
		strncpy(g_status.emuActual, emu, 63);
		strncpy(g_status.juegoActual, juego, 127);
		LeaveCriticalSection(&g_status.cs);
	}

	static DWORD WINAPI mainScrapThread(LPVOID lpParam) {
		ScrapParams* params = (ScrapParams*)lpParam;
		Scrapper scrapper;
		SafeDownloadQueue dwQueue;
		scrapping = true;
		g_status.procesados = 0;
		// Lanzamos el consumidor de imágenes
		HANDLE hImgThread = CreateThread(NULL, 0, imageDownloaderThread, &dwQueue, 0, NULL);
    
		// Ejecutamos el scrapper (Productor)
		for (int i=0; i < params->emu.size(); i++){
			LOG_DEBUG("Hilo Principal: Descargando para el sistema %s", params->emu[i].name.c_str());
			scrapper.scrapSystem(params->emu[i], params->cfg, dwQueue);
		}

		// Finalización
		dwQueue.setFinished();
		WaitForSingleObject(hImgThread, INFINITE);
		CloseHandle(hImgThread);
		delete params;
		scrapping = false;
		return 0;
	}

	std::string leerArchivoTexto(const std::string& ruta) {
		// 1. Abrimos el flujo de entrada
		std::ifstream archivo(ruta.c_str(), std::ios::in | std::ios::binary);
    
		if (!archivo.is_open()) {
			return ""; // O manejar el error según necesites
		}

		// 2. Buscamos el final del archivo para saber su tamaño
		archivo.seekg(0, std::ios::end);
		std::size_t tamano = (std::size_t)archivo.tellg();
    
		// 3. Preparamos el string con el tamaño exacto (evita reasignaciones)
		std::string contenido;
		contenido.reserve(tamano);
    
		// 4. Volvemos al principio y leemos
		archivo.seekg(0, std::ios::beg);
		contenido.assign((std::istreambuf_iterator<char>(archivo)),
						  std::istreambuf_iterator<char>());

		archivo.close();
		return contenido;
	}	

	bool guardarArchivoTexto(const std::string& ruta, const std::string& contenido) {
		// Abrimos el flujo de salida en modo binario
		std::ofstream archivo(ruta.c_str(), std::ios::out | std::ios::binary);

		if (!archivo.is_open()) {
			// En Xbox 360, esto suele fallar si la ruta no existe o el dispositivo no está montado
			return false; 
		}

		// Escribimos todo el bloque de memoria del string de golpe
		archivo.write(contenido.data(), contenido.size());

		archivo.close();

		// Verificamos si hubo algún error durante la escritura (ej: disco lleno)
		return !archivo.fail();
	}

	// Una versión ultra-simple para limpiar caracteres UTF-8 comunes en nombres de juegos
	std::string cleanUTF8(const std::string& str) {
		std::string out;
		for (std::size_t i = 0; i < str.length(); ++i) {
			unsigned char c = (unsigned char)str[i];
			if (c < 0x80) out += str[i]; // ASCII estándar
			else if (c == 0xC3) { // Caracteres extendidos comunes (acentos)
				i++;
				if (i < str.length()) {
					unsigned char c2 = (unsigned char)str[i];
					if (c2 == 0xA1) out += 'a'; // á -> a
					else if (c2 == 0xA9) out += 'e'; // é -> e
					else if (c2 == 0xAD) out += 'i'; // í -> i
					else if (c2 == 0xB3) out += 'o'; // ó -> o
					else if (c2 == 0xBA) out += 'u'; // ú -> u
					else if (c2 == 0xB1) out += 'n'; // ñ -> n
				}
			}
		}
		return out;
	}

	/*void testCurl(){
		CurlClient downloader;
		std::string response;
		float downloadProgress = 0.0f;

		ScraperAsk peticion;
		peticion.regionPreferida = "eu";
		peticion.lenguaPreferida = "es";
		peticion.sistema = 2;
		// El nombre de la ROM suele tener espacios o caracteres especiales
		peticion.romname = downloader.escape("alex kidd in miracle world");
		
		//std::string urlInfo = "https://api.screenscraper.fr/api2/jeuInfos.php";
		std::string urlInfo = "https://141.94.139.59/api2/jeuInfos.php";
		urlInfo += "?devid=jelos&devpassword=jelos&softname=scrapdos1&output=xml&ssid=test&sspassword=test";
		urlInfo += "&systemeid=" + Constant::TipoToStr(peticion.sistema);
		urlInfo	+= "&romtype=rom&romnom=" + peticion.romname;

		printf("Iniciando descarga...\n");

		//if (downloader.fetchUrl(urlInfo, response, &downloadProgress)) {
		response = leerArchivoTexto("D:\\develop\\Github\\xbox360\\project\\Salvia\\test\\screenscrapper.xml");
			LOG_DEBUG("\rProgreso: 100%% | Descarga completada. Tamaño: %d bytes\n", response.size());
			ScraperResult resultado;
			procesarRespuestaScreenscraper(response, peticion, resultado);
			LOG_DEBUG("Informacion descargada");
		//} else {
		//    printf("\nError en la descarga.\n");
		//}
	}*/

};

