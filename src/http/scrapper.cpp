#include "scrapper.h"
#include <http/pugixml.hpp>

#define PICOJSON_USE_RVALUE_REFERENCE 0
#include <http/picojson.h>

volatile LONG Scrapper::scrapping = 0;
ScrapStatus Scrapper::g_status;
HANDLE Scrapper::hMainThread = NULL;

// Caracteres que queremos sustituir por un espacio (para no pegar palabras)
const char SYMBOLS_TO_SPACE[] = ":-._/\\|,;"; 

// Caracteres que queremos eliminar por completo (ruido)
const char SYMBOLS_TO_REMOVE[] = "\"\'!?*#¿¡";

Scrapper::Scrapper(){
	InterlockedExchange(&scrapping, 0);
	cargarEquivalencias(Constant::getAppDir() + ROUTE_SCRAP_TRANSLATIONS);
}

Scrapper::~Scrapper(){
}

bool Scrapper::isScrapping(){
	return InterlockedExchangeAdd(&scrapping, 0) == 1;
}

/*
* Llamada desde el menu
*/
void Scrapper::StartScrappingAsync(std::vector<ConfigEmu>& emu, ScrapperConfig cfg) {
	if (InterlockedCompareExchange((LONG*)&scrapping, 1, 0) == 0){
		InterlockedExchange(&CurlClient::g_abortScrapping, 0);
		ScrapParams* params = new ScrapParams();
		params->emu = emu;
		params->cfg = cfg;
		hMainThread  = CreateThread(NULL, 0, mainScrapThread, params, CREATE_SUSPENDED, NULL);
		if (hMainThread) {
			Constant::setup_and_run_thread(hMainThread, CPU_THREAD, false);
		}
	}
}

DWORD WINAPI Scrapper::mainScrapThread(LPVOID lpParam) {
	ScrapParams* params = (ScrapParams*)lpParam;
	Scrapper scrapper;
	SafeDownloadQueue dwQueue;
	InterlockedExchange(&scrapping, 1);
	g_status.procesados = 0;
	// Lanzamos el consumidor de imagenes
	HANDLE hImgThread = CreateThread(NULL, 0, imageDownloaderThread, &dwQueue, CREATE_SUSPENDED, NULL);
    if (!hImgThread) {
		return 1;
	}
	Constant::setup_and_run_thread(hImgThread, CPU_THREAD, false);

	// Ejecutamos el scrapper (Productor)
	for (std::size_t i=0; i < params->emu.size(); i++){
		//Si hay que abortar, salimos inmediatamente
		if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
			break;
		}
		LOG_DEBUG("Hilo Principal: Descargando para el sistema %s", params->emu[i].name.c_str());
		scrapper.scrapSystem(params->emu[i], params->cfg, dwQueue);
	}

	// Finalizacion
	dwQueue.setFinished();
	WaitForSingleObject(hImgThread, INFINITE);
	CloseHandle(hImgThread);
	delete params;
	InterlockedExchange(&scrapping, 0);
	g_status.abortType = ABORT_SCRAP_END;
	return 0;
}

/**
*
*/
int Scrapper::scrapSystem(ConfigEmu& emulatorCfg, ScrapperConfig& scrapperConfig, SafeDownloadQueue& dwQueue, bool onlyCount) {
	dirutil dir;
	std::string romsdir = dir.getPathPrefix(emulatorCfg.rom_directory);
	std::string assetsdir = dir.getPathPrefix(emulatorCfg.assets);

	if (assetsdir.at(assetsdir.length() - 1) != Constant::tempFileSep[0]) {
        assetsdir += Constant::tempFileSep[0];
    } 

	CurlClient downloader;
	int sistema = 0;
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

	// Configuracion de rutas
	ScraperResult resultado;
	resultado.snapdir = assetsdir + ASSETS_DIR[ASSETS_SS];
	resultado.titledir = assetsdir + ASSETS_DIR[ASSETS_TITLE];
	resultado.boxdir = assetsdir + ASSETS_DIR[ASSETS_BOX];
	resultado.sinopsisdir = assetsdir + ASSETS_DIR[ASSETS_SINOPSIS];

	// Creacion de directorios
	if (!onlyCount && !dir.dirExists(resultado.snapdir.c_str())) dir.createDirRecursive(resultado.snapdir.c_str());
	if (!onlyCount && !dir.dirExists(resultado.titledir.c_str())) dir.createDirRecursive(resultado.titledir.c_str());
	if (!onlyCount && !dir.dirExists(resultado.boxdir.c_str())) dir.createDirRecursive(resultado.boxdir.c_str());
	if (!onlyCount && !dir.dirExists(resultado.sinopsisdir.c_str())) dir.createDirRecursive(resultado.sinopsisdir.c_str());

	dir.listarFilesSuperFast(romsdir.c_str(), files, extFilter, true, false);
	std::string urlBase, fullUrl;

	for (std::size_t i = 0; i < files.size(); i++) {
		FileProps* file = files.at(i).get();
		LOG_DEBUG("Obteniendo file %s", dir.getFileName(file->filename).c_str());
		resultado.clear();
		resultado.filenameNoExt = dir.getFileNameNoExt(file->filename);
			
		//Si ya existen los elementos, no es necesario hacer peticiones a screenscrapper
		if (scrapperConfig.scrapArtType == SCRAP_NO_SCREENSHOT && dir.fileExists(std::string(resultado.snapdir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.scrapArtType == SCRAP_NO_BOX && dir.fileExists(std::string(resultado.boxdir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.scrapArtType == SCRAP_NO_TITLE && dir.fileExists(std::string(resultado.titledir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.scrapArtType == SCRAP_NO_METADATA && dir.fileExists(std::string(resultado.sinopsisdir + Constant::getFileSep() + resultado.filenameNoExt + ".txt").c_str())) 
			continue;

		if (onlyCount){
			counterFiles++;
			continue;
		}	

		//Si hay que abortar, salimos inmediatamente
		if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
			break;
		}

		actualizarProgreso(emulatorCfg.name.c_str(), resultado.filenameNoExt.c_str());

		ScraperAsk peticion;
		std::string response;
		float downloadProgress = 0.0f;
		peticion.regionPreferida = scrapperConfig.regionPreferida;
		peticion.lenguaPreferida = scrapperConfig.lenguaPreferida;
			
		// Escapamos el nombre
		peticion.romnameUnscaped = limpiarNombreJuego(resultado.filenameNoExt);
		peticion.romname = downloader.escape(peticion.romnameUnscaped);
		
		if (scrapperConfig.origin == SC_SCREENCSRAPER){
			// URL base fuera del bucle para ahorrar CPU/RAM
			urlBase = "https://api.screenscraper.fr/api2/jeuInfos.php";
			fullUrl = urlBase + "?devid=jelos&devpassword=jelos&softname=scrapdos1&output=xml&ssid=test&sspassword=test";
			fullUrl += "&systemeid=" + Constant::TipoToStr(sistema);
			fullUrl +=  "&romtype=rom&romnom=" + peticion.romname;
		} else if (scrapperConfig.origin == SC_THEGAMESDB){
			urlBase = "https://api.thegamesdb.net/v1/Games/ByGameName";
			urlBase += "?apikey=" + scrapperConfig.apiKeyTGDB;
			urlBase += "&" + downloader.escape("filter[platform]") + "=" + Constant::TipoToStr(gsTogdGameid[sistema]);
			urlBase += "&fields=overview";
			urlBase += "&name=";
			fullUrl = urlBase + peticion.romname;
		}
		LOG_DEBUG("Buscando datos para: %s en url: %s", file->filename.c_str(), fullUrl.c_str());

		if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
			if (scrapperConfig.origin == SC_SCREENCSRAPER){
				procesarRespuestaScreenscraper(response, peticion, resultado);
			} else if (scrapperConfig.origin == SC_THEGAMESDB){
				peticion.apiKeyTGDB = scrapperConfig.apiKeyTGDB;
				procesarGamesDbConReintentos(urlBase, downloadProgress, peticion, resultado);
			}
			guardarRecursos(dwQueue, resultado);
		}
	}
	return counterFiles;
}

DWORD WINAPI Scrapper::imageDownloaderThread(LPVOID lpParam) {
    SafeDownloadQueue* queue = (SafeDownloadQueue*)lpParam;
    CurlClient downloader;
    DownloadTask task;

    // pop() bloqueará aquí hasta que haya una tarea o se llame a setFinished()
    while (queue->pop(task)) {
		// Comprobación de abortar (Windows Atomic)
        if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
            break;
        }

        // Actualizar estado global
        EnterCriticalSection(&g_status.cs);
        g_status.remainingMedia = queue->size() + 1;
        LeaveCriticalSection(&g_status.cs);

        LOG_DEBUG("Descargando: %s", task.destPath.c_str());
        downloader.fetchFile(task.url, task.destPath, &task.downloadProgress);

        EnterCriticalSection(&g_status.cs);
        g_status.remainingMedia--;
        LeaveCriticalSection(&g_status.cs);
    }

    LOG_DEBUG("Hilo de descarga finalizado.");
    return 0;
}

/**
*
*/
void Scrapper::procesarRespuestaScreenscraper(std::string& xmlData, ScraperAsk& peticion, ScraperResult& resultado) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(xmlData.c_str());
	xmlData.clear();

	if (!result) return;

	pugi::xml_node juego = doc.child("Data").child("jeu");

	// --- SECCION NOMBRE ---
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
			// Es ingles, lo guardamos como backup pero seguimos buscando por si aparece el preferido
			resultado.nombre = it->child_value();
		}
		else if (resultado.nombre.empty()) {
			// Si no hay nada aun, cogemos cualquier idioma que venga (primer backup)
			resultado.nombre = it->child_value();
		}
	}

	resultado.nombre = cleanUTF8(resultado.nombre);

	// --- SECCION SYNOPSIS CON IDIOMA PREFERENTE ---
	pugi::xml_node synopsis_root = juego.child("synopsis");
	for (pugi::xml_node_iterator it = synopsis_root.begin(); it != synopsis_root.end(); ++it) {
		const char* lang = it->attribute("langue").value();
    
		if (strcmp(lang, peticion.lenguaPreferida.c_str()) == 0) {
			// Encontramos el idioma ideal, lo guardamos y salimos del bucle
			resultado.sinopsis = it->child_value();
			break; 
		} 
		else if (strcmp(lang, "en") == 0) {
			// Es ingles, lo guardamos como backup pero seguimos buscando por si aparece el preferido
			resultado.sinopsis = it->child_value();
		}
		else if (resultado.sinopsis.empty()) {
			// Si no hay nada aun, cogemos cualquier idioma que venga (primer backup)
			resultado.sinopsis = it->child_value();
		}
	}
	//resultado.sinopsis = cleanUTF8(resultado.sinopsis);
	// --- SECCION MEDIAS CON PRIORIDAD ---
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

				// Calculamos la "calidad" de lo que ya teniamos guardado
				int puntuacionActual = 0;
				if (!tMedia.url.empty()) {
					if (strcmp(tMedia.region.c_str(), peticion.regionPreferida.c_str()) == 0) puntuacionActual = 3;
					else if (tMedia.region == "eu") puntuacionActual = 2;
					else puntuacionActual = 1;
				}

				// Solo sobrescribimos si la nueva region es mejor que la actual
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

/**
*
*/
void Scrapper::procesarGamesDbConReintentos(std::string& urlBase, float &downloadProgress, ScraperAsk& peticion, ScraperResult& resultado) {
	int numWords = countWords(peticion.romnameUnscaped);
	std::string cutRomName = peticion.romnameUnscaped;
	std::string response;
	CurlClient downloader;
	std::string fullUrl;

	// --- INTENTO 1: Nombre completo limpio de simbolos ---
	if (!procesarRespuestaGamesDb(response, peticion, resultado)){
		LOG_DEBUG("No encontrado con nombre completo. Intentando quitar articulos...");
    
		// --- INTENTO 2: Quitar "Stop Words" (The, Of, And, etc.) ---
		// Solo lo hacemos si tiene mas de 2 palabras para no quedarnos con un string vacio
		if (numWords > 2) {
			cutRomName = quitarArticulos(peticion.romnameUnscaped);
		} else {
			cutRomName = removeLastWord(peticion.romnameUnscaped);
		}

		if (!cutRomName.empty() && cutRomName != peticion.romnameUnscaped) {
			fullUrl = urlBase + downloader.escape(cutRomName);
			response.clear();
			if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
				procesarRespuestaGamesDb(response, peticion, resultado);
			}
		}

		// --- INTENTO 3: Recorte drastico (Si sigue sin aparecer) ---
		// Si despues de quitar articulos aun no tenemos gameid, probamos con las 2 primeras palabras
		if (peticion.gameid == -1) {
			LOG_DEBUG("Sigue sin aparecer. Recortando a las 2 primeras palabras...");
			cutRomName = getFirstWords(peticion.romnameUnscaped, 2);
        
			if (!cutRomName.empty()) {
				fullUrl = urlBase + downloader.escape(cutRomName);
				response.clear();
				if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
					procesarRespuestaGamesDb(response, peticion, resultado);
				}
			}
		}
	}
}

/**
*
*/
bool Scrapper::procesarRespuestaGamesDb(std::string& jsonStr, ScraperAsk& peticion, ScraperResult& resultado) {
    picojson::value v;
    string err = picojson::parse(v, jsonStr);
    jsonStr.clear();
    if (!err.empty()) return false;

    const int regionPreferencia = obtenerRegionPreferenciaTGDB(peticion);
    
    // Variables para el seguimiento del mejor candidato
    int mejorPuntuacion = -1;
    int idSeleccionado = -1;
    string mejorOverview = "Sin descripcion.";

    picojson::object& root = v.get<picojson::object>();

	if (root.count("remaining_monthly_allowance") && (int)root["remaining_monthly_allowance"].get<double>() == 0) {
		InterlockedExchange(&CurlClient::g_abortScrapping, 1); // Avisar a todos para cerrar recursos
		g_status.abortType = ABORT_LIMIT_CUOTA;
		LOG_DEBUG("Limit of monthly querys achieved");
	}

    if (root.count("data") && root["data"].is<picojson::object>()) {
        picojson::object& dataObj = root["data"].get<picojson::object>();
        if (dataObj.count("games") && dataObj["games"].is<picojson::array>()) {
            picojson::array& games = dataObj["games"].get<picojson::array>();

            for (size_t i = 0; i < games.size(); ++i) {
                if (!games[i].is<picojson::object>()) continue;
                picojson::object& g = games[i].get<picojson::object>();

                int currentId = g.count("id") ? (int)g["id"].get<double>() : -1;
                string currentTitle = g.count("game_title") ? g["game_title"].get<string>() : "";
                string currentOverview = g.count("overview") ? g["overview"].get<string>() : "";
                int currentRegion = g.count("region_id") ? (int)g["region_id"].get<double>() : -1;

                // --- CALCULAR PUNTUACION ---
                int puntosActuales = 0;

                // 1. Prioridad: Coincidencia de palabras (Peso alto: 10 pts por palabra)
                puntosActuales += (countWordsContained(peticion.romnameUnscaped, currentTitle) * 10);

                // 2. Prioridad: Region (Peso bajo: Desempata si los titulos son iguales)
                if (currentRegion == regionPreferencia) {
                    puntosActuales += 5;
                } else if (currentRegion == 2) { // Europa
                    puntosActuales += 2;
                }

                // --- EVALUAR SI ES EL MEJOR ---
                if (puntosActuales > mejorPuntuacion) {
                    mejorPuntuacion = puntosActuales;
                    idSeleccionado = currentId;
                    mejorOverview = currentOverview.empty() ? "Sin descripcion." : currentOverview;
                }
            }

            if (idSeleccionado > -1) {
                LOG_DEBUG("ID Usado: %d (Score: %d)", idSeleccionado, mejorPuntuacion);
                resultado.sinopsis = mejorOverview;
                peticion.gameid = idSeleccionado;
                
                // Llamada para bajar las imagenes del ID ganador
                obtenerImagenesTGDB(peticion, resultado);
            }
        }
    }
    return idSeleccionado > -1;
}

/**
*
*/
void Scrapper::obtenerImagenesTGDB(ScraperAsk& peticion, ScraperResult& resultado) {
	CurlClient downloader;
	float downloadProgress = 0.0f;
	std::string response;

	std::string urlBase = "https://api.thegamesdb.net/v1/Games/Images";
	std::string fullUrl = urlBase + "?apikey=" + peticion.apiKeyTGDB;
	fullUrl += "&games_id=" + Constant::TipoToStr(peticion.gameid);
	LOG_DEBUG("Buscando imagenes en url: %s", fullUrl.c_str());

	if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
		// Variables para almacenar las URLs finales
		string urlBoxart = "";
		string urlScreenshot = "";
		string urlTitlescreen = "";
		string urlFanart = "";

		picojson::value v;
		string err = picojson::parse(v, response); // jsonStr es el string que me has pasado
		response.clear();

		if (err.empty() && v.is<picojson::object>()) {
			picojson::object& root = v.get<picojson::object>();
    
			// 1. Validar que data y base_url existen
			if (root.count("data") && root["data"].is<picojson::object>()) {
				picojson::object& dataObj = root["data"].get<picojson::object>();
        
				// Obtener la URL base (usamos "medium" para no saturar la RAM de la 360)
				string baseUrl = "";
				if (dataObj.count("base_url") && dataObj["base_url"].is<picojson::object>()) {
					baseUrl = dataObj["base_url"].get<picojson::object>()["thumb"].get<string>();
				}

				// 2. Acceder al diccionario de imagenes
				if (dataObj.count("images") && dataObj["images"].is<picojson::object>()) {
					picojson::object& imagesDict = dataObj["images"].get<picojson::object>();
            
					// Convertimos el ID a buscar
					char idStr[16];
					sprintf(idStr, "%d", peticion.gameid); 

					if (imagesDict.count(idStr) && imagesDict[idStr].is<picojson::array>()) {
						picojson::array& imgList = imagesDict[idStr].get<picojson::array>();

						// 3. Iterar por la lista de imagenes para encontrar los tipos
						for (size_t i = 0; i < imgList.size(); ++i) {
							picojson::object& img = imgList[i].get<picojson::object>();
							string type = img["type"].get<string>();
                    
							// Boxart (buscamos solo el front)
							if (type == "boxart" && urlBoxart.empty()) {
								if (img["side"].get<string>() == "front") {
									urlBoxart = baseUrl + img["filename"].get<string>();
								}
							}
							// Screenshot (nos quedamos con el primero que aparezca)
							else if (type == "screenshot" && urlScreenshot.empty()) {
								urlScreenshot = baseUrl + img["filename"].get<string>();
							}
							// Titlescreen (titleshot en el codigo anterior, aqui es titlescreen)
							else if (type == "titlescreen" && urlTitlescreen.empty()) {
								urlTitlescreen = baseUrl + img["filename"].get<string>();
							}
							else if (type == "fanart" && urlFanart.empty()) {
								urlFanart = baseUrl + img["filename"].get<string>();
							}
						}
					}
				}
			}
		}

		// Imprimir resultados para verificar
		LOG_DEBUG("Boxart: %s", urlBoxart.c_str());
		LOG_DEBUG("Screenshot: %s", urlScreenshot.c_str());
		LOG_DEBUG("Fanart: %s", urlFanart.c_str());
		LOG_DEBUG("Titlescreen: %s", urlTitlescreen.c_str());

		if (!urlTitlescreen.empty())
			resultado.medias.insert(
				std::pair<std::string, t_media>(
					MEDIAS_TO_FIND[MEDIA_TITLE], 
					t_media(MEDIA_TITLE, peticion.regionPreferida, urlTitlescreen)
				)
			);

		if (!urlScreenshot.empty())
			resultado.medias.insert(
				std::pair<std::string, t_media>(
					MEDIAS_TO_FIND[MEDIA_SS], 
					t_media(MEDIA_SS, peticion.regionPreferida, urlScreenshot)
				)
			);
		else if (!urlFanart.empty())
			resultado.medias.insert(
				std::pair<std::string, t_media>(
					MEDIAS_TO_FIND[MEDIA_SS], 
					t_media(MEDIA_SS, peticion.regionPreferida, urlFanart)
				)
			);

		if (!urlBoxart.empty())
			resultado.medias.insert(
				std::pair<std::string, t_media>(
					MEDIAS_TO_FIND[MEDIA_BOX], 
					t_media(MEDIA_BOX, peticion.regionPreferida, urlBoxart)
				)
			);
	}
}

/**
*
*/
void Scrapper::guardarRecursos(SafeDownloadQueue& dwQueue, ScraperResult &resultado){
	// 1. Guardar Sinopsis
	dirutil dir;
	std::string rutaTxt = resultado.sinopsisdir + Constant::getFileSep() + resultado.filenameNoExt + ".txt";
	if (!dir.fileExists(rutaTxt.c_str()) && !resultado.sinopsis.empty()) {
		guardarArchivoTexto(rutaTxt, resultado.sinopsis);
	}

	// 2. Guardar Imagenes
	std::map<std::string, t_media>::iterator it;
	for (it = resultado.medias.begin(); it != resultado.medias.end(); ++it) {
		std::string destDir = "";
		switch (it->second.type) {
			case MEDIA_TITLE: destDir = resultado.titledir; break;
			case MEDIA_SS:    destDir = resultado.snapdir; break;
			case MEDIA_BOX:   destDir = resultado.boxdir; break;
			default: continue;
		}
		std::string rutaImg = destDir + Constant::getFileSep() + resultado.filenameNoExt + ".png";
		if (!dir.fileExists(rutaImg.c_str()) && !it->second.url.empty()) {
			//downloader.fetchFile(it->second.url, rutaImg, &downloadProgress);
			DownloadTask task;
			task.url = it->second.url;
			task.destPath = rutaImg;
			task.downloadProgress = 0.0f;
			LOG_DEBUG("Descargando imagen: %s con url: %s", dir.getFileName(rutaImg).c_str(), task.url.c_str());
			dwQueue.push(task); // Encolamos para el otro hilo
			LOG_DEBUG("Imagen encolada");
		}
	}
}



void Scrapper::ShutdownScrapper() {
	InterlockedExchange(&CurlClient::g_abortScrapping, 1); // Avisar a todos para cerrar recursos
	if (hMainThread != NULL) {
		// Esperar hasta 5 segundos a que los hilos cierren sockets y archivos
		WaitForSingleObject(hMainThread, 5000);
		CloseHandle(hMainThread);
		hMainThread = NULL;
	}
}

int Scrapper::obtenerRegionPreferenciaTGDB(ScraperAsk& peticion){
	if (peticion.regionPreferida == "es"){
		return REG_SPAIN;
	} else if (peticion.regionPreferida == "jp"){
		return REG_JAPAN;
	} else if (peticion.regionPreferida == "eu"){
		return REG_EUROPE;
	} else if (peticion.regionPreferida == "us"){
		return REG_USA;
	} else {
		return REG_WORLDWIDE;
	}
}

void Scrapper::actualizarProgreso(const char* emu, const char* juego) {
	EnterCriticalSection(&g_status.cs);
	g_status.procesados++;
	strncpy(g_status.emuActual, emu, 63);
	g_status.emuActual[63] = '\0';
	strncpy(g_status.juegoActual, juego, 127);
	g_status.juegoActual[127] = '\0';
	LeaveCriticalSection(&g_status.cs);
}

std::string Scrapper::leerArchivoTexto(const std::string& ruta) {
	// 1. Abrimos el flujo de entrada
	std::ifstream archivo(ruta.c_str(), std::ios::in | std::ios::binary);
    
	if (!archivo.is_open()) {
		return ""; // O manejar el error segun necesites
	}

	// 2. Buscamos el final del archivo para saber su tamanyo
	archivo.seekg(0, std::ios::end);
	std::size_t tamano = (std::size_t)archivo.tellg();
    
	// 3. Preparamos el string con el tamanyo exacto (evita reasignaciones)
	std::string contenido;
	contenido.reserve(tamano);
    
	// 4. Volvemos al principio y leemos
	archivo.seekg(0, std::ios::beg);
	contenido.assign((std::istreambuf_iterator<char>(archivo)),
						std::istreambuf_iterator<char>());

	archivo.close();
	return contenido;
}	

bool Scrapper::guardarArchivoTexto(const std::string& ruta, const std::string& contenido) {
	// Abrimos el flujo de salida en modo binario
	std::ofstream archivo(ruta.c_str(), std::ios::out | std::ios::binary);

	if (!archivo.is_open()) {
		// En Xbox 360, esto suele fallar si la ruta no existe o el dispositivo no esta montado
		return false; 
	}

	// Escribimos todo el bloque de memoria del string de golpe
	archivo.write(contenido.data(), contenido.size());

	archivo.close();

	// Verificamos si hubo algun error durante la escritura (ej: disco lleno)
	return !archivo.fail();
}

// Una version ultra-simple para limpiar caracteres UTF-8 comunes en nombres de juegos
std::string Scrapper::cleanUTF8(const std::string& str) {
	std::string out;
	for (std::size_t i = 0; i < str.length(); ++i) {
		unsigned char c = (unsigned char)str[i];
		if (c < 0x80) out += str[i]; // ASCII estandar
		else if (c == 0xC3) { // Caracteres extendidos comunes (acentos)
			i++;
			if (i < str.length()) {
				unsigned char c2 = (unsigned char)str[i];
				if (c2 == 0xA1) out += 'a'; 
				else if (c2 == 0xA9) out += 'e'; 
				else if (c2 == 0xAD) out += 'i'; 
				else if (c2 == 0xB3) out += 'o'; 
				else if (c2 == 0xBA) out += 'u'; 
				else if (c2 == 0xB1) out += 'n'; 
			}
		}
	}
	return out;
}

/**
*
*/
bool Scrapper::cargarEquivalencias(const std::string& nombreArchivo) {
    std::ifstream file(nombreArchivo.c_str());
    std::string line;
    bool seccionEncontrada = false;

    if (!file.is_open()) return false;

    while (std::getline(file, line)) {
        // Eliminar espacios en blanco al inicio/final si fuera necesario (opcional)
            
        // 1. Buscamos el inicio de la seccion
        if (line == "[SCREENSCRAPPER_TO_GAMESDB]") {
            seccionEncontrada = true;
            continue; // Pasamos a la siguiente linea
        }

        // 2. Si ya estamos en la seccion correcta, procesamos los datos
        if (seccionEncontrada) {
            // Si encontramos otra seccion (empieza por [), dejamos de leer
            if (!line.empty() && line[0] == '[') {
                break; 
            }

            // Ignorar comentarios o lineas vacias
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 3. Extraer clave=valor
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string keyStr = line.substr(0, pos);
                std::string valueStr = line.substr(pos + 1);

                int key = atoi(keyStr.c_str());
                int value = atoi(valueStr.c_str());

                gsTogdGameid[key] = value;
            }
        }
    }
    file.close();
    return seccionEncontrada;
}

std::string Scrapper::limpiarNombreJuego(std::string nombre) {
    std::string temporal = "";
    int nivelParentesis = 0;

    for (size_t i = 0; i < nombre.length(); ++i) {
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

    for (size_t i = 0; i < temporal.length(); ++i) {
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

std::string Scrapper::quitarArticulos(std::string texto) {
    // Lista de "Stop Words" en ingles y espanyol
    const char* stopWords[] = { "the", "a", "an", "of", "and", "for", "with", "in", "on", "at", "to", "el", "la", "los", "las", "de", "y" };
    const int numStopWords = sizeof(stopWords) / sizeof(stopWords[0]);

    std::stringstream ss(texto);
    std::string palabra, resultado = "";
    bool primero = true;

    while (ss >> palabra) {
        std::string palabraLower = toLower(palabra);
        bool esStopWord = false;

        for (int i = 0; i < numStopWords; ++i) {
            if (palabraLower == stopWords[i]) {
                esStopWord = true;
                break;
            }
        }

        if (!esStopWord) {
            if (!primero) resultado += " ";
            resultado += palabra;
            primero = false;
        }
    }
    return resultado;
}

std::string Scrapper::getFirstWords(std::string text, int n) {
    if (n <= 0) return "";
    
    std::stringstream ss(text);
    std::string palabra;
    std::string resultado = "";
    int contador = 0;

    // Extraemos palabras una a una hasta llegar al limite 'n'
    while (ss >> palabra && contador < n) {
        if (contador > 0) {
            resultado += " "; // Anyadimos espacio entre palabras
        }
        resultado += palabra;
        contador++;
    }

    return resultado;
}

std::string Scrapper::removeLastWord(std::string text) {
    // 1. Eliminamos espacios en blanco al final (trim right) 
    // para asegurar que el ultimo caracter no sea un espacio.
    size_t last = text.find_last_not_of(" \t\n\r");
    if (last == std::string::npos) return ""; // El string solo tiene espacios
    
    std::string str = text.substr(0, last + 1);

    // 2. Buscamos el ultimo espacio que separa la ultima palabra
    size_t pos = str.find_last_of(" \t\n\r");

    // 3. Si no hay espacios, significa que solo hay una palabra
    if (pos == std::string::npos) {
        return ""; 
    }

    // 4. Devolvemos el string hasta la posicion del espacio
    return str.substr(0, pos);
}

int Scrapper::countWords(std::string text) {
    int count = 0;
    bool inWord = false;

    for (size_t i = 0; i < text.length(); ++i) {
        // Consideramos espacio, tabulador o saltos de linea como separadores
        if (isspace(text[i])) {
            inWord = false;
        } 
        else if (!inWord) {
            // Si no estabamos en una palabra y encontramos un caracter, empieza una nueva
            inWord = true;
            count++;
        }
    }
    return count;
}

// Funcion auxiliar para convertir a minusculas (VS2010 compatible)
std::string Scrapper::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

int Scrapper::countWordsContained(std::string text1, std::string text2) {
    if (text1.empty() || text2.empty()) return 0;

    // Convertimos ambos a minusculas para una comparacion justa
    std::string s1 = toLower(text1);
    std::string s2 = toLower(text2);

    std::stringstream ss(s1);
    std::string palabra;
    int coincidencias = 0;

    while (ss >> palabra) {
        // Buscamos la palabra de text1 dentro de text2
        if (s2.find(palabra) != std::string::npos) {
            coincidencias++;
        }
    }

    return coincidencias;
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
		LOG_DEBUG("\rProgreso: 100%% | Descarga completada. Tama�o: %d bytes\n", response.size());
		ScraperResult resultado;
		procesarRespuestaScreenscraper(response, peticion, resultado);
		LOG_DEBUG("Informacion descargada");
	//} else {
	//    printf("\nError en la descarga.\n");
	//}
}*/