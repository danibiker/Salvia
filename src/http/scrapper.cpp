#include "scrapper.h"
#include <http/pugixml.hpp>

#define PICOJSON_USE_RVALUE_REFERENCE 0
#include <http/picojson.h>

bool Scrapper::scrapping = false;
ScrapStatus Scrapper::g_status;
HANDLE Scrapper::hMainThread = NULL;

Scrapper::Scrapper(){
	scrapping = false;
	cargarEquivalencias(Constant::getAppDir() + "\\config\\scrap_translations.cfg");
}

Scrapper::~Scrapper(){
}

bool Scrapper::isScrapping(){
	return scrapping;
}

int Scrapper::scrapSystem(ConfigEmu& emulatorCfg, ScrapperConfig& scrapperConfig, SafeDownloadQueue& dwQueue, bool onlyCount) {
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
	ScraperResult resultado;
	resultado.snapdir = assetsdir + ASSETS_DIR[ASSETS_SS];
	resultado.titledir = assetsdir + ASSETS_DIR[ASSETS_TITLE];
	resultado.boxdir = assetsdir + ASSETS_DIR[ASSETS_BOX];
	resultado.sinopsisdir = assetsdir + ASSETS_DIR[ASSETS_SINOPSIS];

	// Creación de directorios
	if (!dir.dirExists(resultado.snapdir.c_str())) dir.createDirRecursive(resultado.snapdir.c_str());
	if (!dir.dirExists(resultado.titledir.c_str())) dir.createDirRecursive(resultado.titledir.c_str());
	if (!dir.dirExists(resultado.boxdir.c_str())) dir.createDirRecursive(resultado.boxdir.c_str());
	if (!dir.dirExists(resultado.sinopsisdir.c_str())) dir.createDirRecursive(resultado.sinopsisdir.c_str());

	dir.listarFilesSuperFast(romsdir.c_str(), files, extFilter, true, false);
	std::string urlBase, fullUrl;

	for (std::size_t i = 0; i < files.size(); i++) {
		//Si hay que abortar, salimos inmediatamente
		if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
			break;
		}

		FileProps* file = files.at(i).get();
		resultado.filenameNoExt = dir.getFileNameNoExt(file->filename);
			
		//Si ya existen los elementos, no es necesario hacer peticiones a screenscrapper
		if (scrapperConfig.downloadNoSS && dir.fileExists(std::string(resultado.snapdir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.downloadNoBox && dir.fileExists(std::string(resultado.boxdir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.downloadNoTitle && dir.fileExists(std::string(resultado.titledir + Constant::getFileSep() + resultado.filenameNoExt + ".png").c_str())) 
			continue;
		if (scrapperConfig.downloadNoMetadata && dir.fileExists(std::string(resultado.sinopsisdir + Constant::getFileSep() + resultado.filenameNoExt + ".txt").c_str())) 
			continue;

		counterFiles++;
		if (onlyCount){
			continue;
		}			

		actualizarProgreso(emulatorCfg.name.c_str(), resultado.filenameNoExt.c_str());

		std::string response;
		float downloadProgress = 0.0f;
        
		ScraperAsk peticion;
		peticion.regionPreferida = scrapperConfig.regionPreferida;
		peticion.lenguaPreferida = scrapperConfig.lenguaPreferida;
			
		// Escapamos el nombre
		peticion.romname = downloader.escape(limpiarNombreJuego(resultado.filenameNoExt));
		
		if (scrapperConfig.origin == SC_SCREENCSRAPER){
			// URL base fuera del bucle para ahorrar CPU/RAM
			urlBase = "https://api.screenscraper.fr/api2/jeuInfos.php";
			fullUrl = urlBase + "?devid=jelos&devpassword=jelos&softname=scrapdos1&output=xml&ssid=test&sspassword=test";
			fullUrl += "&systemeid=" + Constant::TipoToStr(sistema);
			fullUrl +=  "&romtype=rom&romnom=" + peticion.romname;
		} else if (scrapperConfig.origin == SC_THEGAMESDB){
			urlBase = "https://api.thegamesdb.net/v1/Games/ByGameName";
			fullUrl = urlBase + "?apikey=" + scrapperConfig.apiKeyTGDB;
			fullUrl += "&" + downloader.escape("filter[platform]") + "=" + Constant::TipoToStr(gsTogdGameid[sistema]);
			fullUrl += "&fields=overview";
			fullUrl += "&name=" + peticion.romname;
		}
		LOG_DEBUG("Buscando datos para: %s en url: %s", file->filename.c_str(), fullUrl.c_str());

		if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
			if (scrapperConfig.origin == SC_SCREENCSRAPER){
				procesarRespuestaScreenscraper(response, peticion, resultado);
			} else if (scrapperConfig.origin == SC_THEGAMESDB){
				peticion.apiKeyTGDB = scrapperConfig.apiKeyTGDB;
				procesarRespuestaGamesDb(response, peticion, resultado);
			}
			guardarRecursos(dwQueue, resultado);
		}
	}
	return counterFiles;
}

void Scrapper::guardarRecursos(SafeDownloadQueue& dwQueue, ScraperResult &resultado){
	// 1. Guardar Sinopsis
	dirutil dir;
	std::string rutaTxt = resultado.sinopsisdir + Constant::getFileSep() + resultado.filenameNoExt + ".txt";
	if (!dir.fileExists(rutaTxt.c_str()) && !resultado.sinopsis.empty()) {
		guardarArchivoTexto(rutaTxt, resultado.sinopsis);
	}

	// 2. Guardar Imágenes
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

// Llamada desde tu menú/emulador:
void Scrapper::StartScrappingAsync(std::vector<ConfigEmu> emu, ScrapperConfig cfg) {
	if (!scrapping){
		InterlockedExchange(&CurlClient::g_abortScrapping, 0);
		ScrapParams* params = new ScrapParams();
		params->emu = emu;
		params->cfg = cfg;
		hMainThread  = CreateThread(NULL, 0, mainScrapThread, params, 0, NULL);
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

void Scrapper::procesarRespuestaGamesDb(std::string& jsonStr, ScraperAsk& peticion, ScraperResult& resultado) {
	picojson::value v;
    string err = picojson::parse(v, jsonStr);
	jsonStr.clear();

    if (!err.empty()) return;

	const int regionPreferencia = obtenerRegionPreferenciaTGDB(peticion);
	string overviewFinal = "";
	int idSeleccionado = -1;

    picojson::object& root = v.get<picojson::object>();
    // 1. Validar que "data" existe y es un objeto
	if (root.count("data") && root["data"].is<picojson::object>()) {
		picojson::object& dataObj = root["data"].get<picojson::object>();

		// 2. Validar que "games" existe y es un array
		if (dataObj.count("games") && dataObj["games"].is<picojson::array>()) {
			picojson::array& games = dataObj["games"].get<picojson::array>();

			// 3. Comprobar que el array no esté vacío
			if (!games.empty()) {
				string overviewEuropa = "";
				

				for (size_t i = 0; i < games.size(); ++i) {
					// Validación de seguridad para cada elemento del array
					if (!games[i].is<picojson::object>()) continue;
                
					picojson::object& g = games[i].get<picojson::object>();

					// Comprobar que los campos necesarios existen antes de usarlos
					if (g.count("id") && g["id"].is<double>()) {
						int currentId = (int)g["id"].get<double>();
						if (idSeleccionado == -1) idSeleccionado = currentId;

						if (g.count("overview") && g["overview"].is<string>()) {
							string currentOverview = g["overview"].get<string>();
							int currentRegion = g.count("region_id") ? (int)g["region_id"].get<double>() : -1;

							if (currentRegion == regionPreferencia) {
								overviewFinal = currentOverview;
								idSeleccionado = currentId;
								break; 
							}
							if (currentRegion == 2) { // Europa
								overviewEuropa = currentOverview;
							}
						}
					}
				}

				// Fallback: Si no hay la de preferencia, usar Europa. Si no, la primera que haya.
				if (overviewFinal.empty()) {
					overviewFinal = !overviewEuropa.empty() ? overviewEuropa : 
								   (games[0].get<picojson::object>().count("overview") ? 
									games[0].get<picojson::object>()["overview"].get<string>() : "Sin descripción.");
				}
            
				// Aquí ya puedes proceder con el ID seleccionado y el Overview...
			} else {
				std::cout << "La busqueda no devolvió ningun juego." << std::endl;
			}
		}
	}
    // Resultados
    LOG_DEBUG("ID Usado: %d", idSeleccionado);
	LOG_DEBUG("Overview: %s", overviewFinal.substr(0, 100).c_str());
	resultado.sinopsis = overviewFinal;
	peticion.gameid = idSeleccionado;
	
	if (idSeleccionado > -1){
		obtenerImagenesTGDB(peticion, resultado);
	}
}

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

				// 2. Acceder al diccionario de imágenes
				if (dataObj.count("images") && dataObj["images"].is<picojson::object>()) {
					picojson::object& imagesDict = dataObj["images"].get<picojson::object>();
            
					// Convertimos el ID a buscar
					char idStr[16];
					sprintf(idStr, "%d", peticion.gameid); 

					if (imagesDict.count(idStr) && imagesDict[idStr].is<picojson::array>()) {
						picojson::array& imgList = imagesDict[idStr].get<picojson::array>();

						// 3. Iterar por la lista de imágenes para encontrar los tipos
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
							// Titlescreen (titleshot en el código anterior, aquí es titlescreen)
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

void Scrapper::procesarRespuestaScreenscraper(std::string& xmlData, ScraperAsk& peticion, ScraperResult& resultado) {
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

DWORD WINAPI Scrapper::imageDownloaderThread(LPVOID lpParam) {
	SafeDownloadQueue* queue = (SafeDownloadQueue*)lpParam;
	CurlClient downloader;
	DownloadTask task;

	while (queue->pop(task)) {
		//Si hay que abortar, salimos inmediatamente
		if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
			break;
		}
		LOG_DEBUG("Hilo Secundario: Descargando %s", task.destPath.c_str());
		downloader.fetchFile(task.url, task.destPath, &task.downloadProgress);
	}
	return 0;
}

void Scrapper::actualizarProgreso(const char* emu, const char* juego) {
	EnterCriticalSection(&g_status.cs);
	g_status.procesados++;
	strncpy(g_status.emuActual, emu, 63);
	strncpy(g_status.juegoActual, juego, 127);
	LeaveCriticalSection(&g_status.cs);
}

DWORD WINAPI Scrapper::mainScrapThread(LPVOID lpParam) {
	ScrapParams* params = (ScrapParams*)lpParam;
	Scrapper scrapper;
	SafeDownloadQueue dwQueue;
	scrapping = true;
	g_status.procesados = 0;
	// Lanzamos el consumidor de imágenes
	HANDLE hImgThread = CreateThread(NULL, 0, imageDownloaderThread, &dwQueue, 0, NULL);
    
	// Ejecutamos el scrapper (Productor)
	for (int i=0; i < params->emu.size(); i++){
		//Si hay que abortar, salimos inmediatamente
		if (InterlockedExchangeAdd(&CurlClient::g_abortScrapping, 0) == 1) {
			break;
		}
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

std::string Scrapper::leerArchivoTexto(const std::string& ruta) {
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

bool Scrapper::guardarArchivoTexto(const std::string& ruta, const std::string& contenido) {
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
std::string Scrapper::cleanUTF8(const std::string& str) {
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
            
        // 1. Buscamos el inicio de la sección
        if (line == "[SCREENSCRAPPER_TO_GAMESDB]") {
            seccionEncontrada = true;
            continue; // Pasamos a la siguiente línea
        }

        // 2. Si ya estamos en la sección correcta, procesamos los datos
        if (seccionEncontrada) {
            // Si encontramos otra sección (empieza por [), dejamos de leer
            if (!line.empty() && line[0] == '[') {
                break; 
            }

            // Ignorar comentarios o líneas vacías
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
    std::string resultado = "";
    int nivelParentesis = 0;

    for (size_t i = 0; i < nombre.length(); ++i) {
        char c = nombre[i];

        // Detectar apertura de paréntesis o corchetes
        if (c == '(' || c == '[') {
            nivelParentesis++;
            continue;
        }
        
        // Detectar cierre
        if (c == ')' || c == ']') {
            if (nivelParentesis > 0) nivelParentesis--;
            continue;
        }

        // Si no estamos dentro de un paréntesis, añadimos el carácter
        if (nivelParentesis == 0) {
            resultado += c;
        }
    }

    // Limpiar espacios dobles o al final que hayan quedado
    // (Opcional: puedes usar una lógica simple para trim)
    size_t last = resultado.find_last_not_of(" \t\n\r");
    if (last != std::string::npos) {
        resultado = resultado.substr(0, last + 1);
    }
    
    return resultado;
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