#pragma once

// Logica de red y resolucion de ficheros .cht (localizar / descargar el .cht correcto),
// extraida de salvia.cpp para separar responsabilidades.
//
// IMPORTANTE: header "companion" de salvia.cpp -- se incluye UNICAMENTE dentro de
// salvia.cpp, tras salvia.h y los includes de red/cheats (httputil, rdbreader, picojson,
// filelist, constant, dirutil, listmenu). Usa sus globales (gameMenu, romPaths,
// g_currentRomCrc). NO incluir en ningun otro .cpp.

// --- Estado compartido para la descarga asincrona con barra de progreso ---
// La descarga corre en un hilo IO; el hilo principal lee g_cheatProgress (que rellena el
// CurlClient::ProgressCallback existente, SIN tocarlo) y g_cheatStage, y pinta la barra.
enum { CHEAT_STAGE_SEARCH = 1, CHEAT_STAGE_LIST = 2, CHEAT_STAGE_DB = 3, CHEAT_STAGE_CHEAT = 4 };
static volatile long g_cheatDone     = 0;      // 1 cuando el worker termina
static float         g_cheatProgress = 0.0f;   // 0..1 (por transferencia); lo escribe ProgressCallback
static volatile long g_cheatStage    = 0;      // paso actual (CHEAT_STAGE_*)
static char          g_cheatResult[520] = {0}; // ruta del .cht descargado (o vacio)

static const char* cheatStageKey(long s){
	switch (s){
		case CHEAT_STAGE_LIST:  return "msg.cheats.stage.list";
		case CHEAT_STAGE_DB:    return "msg.cheats.stage.db";
		case CHEAT_STAGE_CHEAT: return "msg.cheats.stage.cheat";
		default:                return "msg.cheats.stage.search";
	}
}

// Nombre de sistema de libretro-database (para el fichero .rdb / carpeta .cht) del
// emulador activo, via ConfigEmu::system (codigo numerico) -> cart_* -> RDB_SYSTEM_NAMES.
// Devuelve "" si el sistema no tiene base de cheats por CRC.
static std::string getRdbSystemName(){
	if (!gameMenu || !gameMenu->getCfgLoader() || !gameMenu->getCfgLoader()->getCfgEmu())
		return "";
	std::vector<std::string> v = Constant::splitChar(gameMenu->getCfgLoader()->getCfgEmu()->system, '_');
	if (v.empty()) return "";
	int cart = ListMenu::getCartForSystem(Constant::strToTipo<int>(v[0]));
	if (cart < 0 || cart >= max_carts) return "";
	return RDB_SYSTEM_NAMES[cart];
}

// Resuelve la ruta del .cht: primero por CRC via .rdb (nombre canonico No-Intro) si
// existe ese fichero; si no se puede, cae al .cht por nombre de ROM (fallbackByName).
static std::string resolveCheatPath(const std::string& fallbackByName){
	if (g_currentRomCrc == 0) return fallbackByName;
	std::string sys = getRdbSystemName();
	if (sys.empty()) return fallbackByName;

	std::string rdbPath = Constant::getAppDir() + Constant::getFileSep() + "data" + Constant::getFileSep() + "cheats" + Constant::getFileSep() +
		"database" + Constant::getFileSep() + sys + ".rdb";

	std::string canonical;
	if (RdbReader::findNameByCrc(rdbPath, g_currentRomCrc, canonical)){
		dirutil dir;
		std::string byCrc = dir.getFolder(fallbackByName) + Constant::getFileSep() + canonical + ".cht";
		if (dir.fileExists(byCrc.c_str()))
			return byCrc;
	}
	return fallbackByName;
}

// Percent-encode de un segmento de path para GitHub raw: codifica solo lo que rompe
// una URL (espacio -> %20, y algunos mas), dejando literales (),[],',&,. etc. como en
// las URLs de libretro-database. NO codifica '/': se aplica por segmento.
static std::string urlEncodeSegment(const std::string& s){
	static const char* hexd = "0123456789ABCDEF";
	std::string out;
	for (std::size_t i = 0; i < s.size(); ++i){
		unsigned char c = (unsigned char)s[i];
		if (c == ' ' || c == '%' || c == '#' || c == '?' || c == '+' || c < 0x20 || c >= 0x7f){
			out += '%'; out += hexd[c >> 4]; out += hexd[c & 0x0f];
		} else {
			out += (char)c;
		}
	}
	return out;
}

// Puntua la coincidencia de region entre dos nombres (tokens de region en comun); mayor =
// mejor. Sirve para elegir la variante regional correcta cuando el juego no tiene CRC.
static int regionScore(const std::string& cand, const std::string& rom){
	static const char* regs[] = {
		"usa", "europe", "japan", "world", "asia", "korea", "france", "germany",
		"spain", "italy", "netherlands", "brazil", "china", "australia", "sweden", 0 };
	std::string c(cand), r(rom);
	for (std::size_t i = 0; i < c.size(); ++i) if (c[i] >= 'A' && c[i] <= 'Z') c[i] = (char)(c[i] + 32);
	for (std::size_t i = 0; i < r.size(); ++i) if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] + 32);
	int score = 0;
	for (int i = 0; regs[i]; ++i)
		if (r.find(regs[i]) != std::string::npos && c.find(regs[i]) != std::string::npos)
			score++;
	return score;
}

// Lista los nombres de fichero .cht del sistema en libretro-database, via la API de GitHub
// (2 llamadas: contents/cht -> sha de la carpeta del sistema -> git/trees). Cachea el
// resultado en data/cheats/index/<sys>.idx (texto, un nombre por linea) para no repetir la
// consulta. Devuelve true si hay lista (de cache o de red). fetchUrl ya pone User-Agent (la
// API de GitHub lo exige) y se auto-inicializa.
static bool getChtFolderList(const std::string& sys, std::vector<std::string>& filenames, float* progressOut){
	filenames.clear();

	dirutil dir;
	const std::string idxPath = Constant::getAppDir() + Constant::getFileSep() + "data" + Constant::getFileSep() +
		"cheats" + Constant::getFileSep() + "index" + Constant::getFileSep() + sys + ".idx";

	// Cache local
	if (dir.fileExists(idxPath.c_str())){
		FileList::cargarVector(idxPath, filenames);
		if (!filenames.empty()) return true;
	}

	// Paso: obteniendo la lista de cheats (solo si no habia cache -> vamos a la red).
	InterlockedExchange(&g_cheatStage, CHEAT_STAGE_LIST);

	CurlClient http;
	std::string body;
	const std::string API = "https://api.github.com/repos/libretro/libretro-database/";

	// 1) Carpetas de cht/ -> localizar el sha (tree) de la del sistema.
	if (!http.fetchUrl(API + "contents/cht", body, progressOut)) return false;
	std::string sha;
	{
		picojson::value v;
		if (!picojson::parse(v, body).empty() || !v.is<picojson::array>()) return false;
		picojson::array& arr = v.get<picojson::array>();
		for (std::size_t i = 0; i < arr.size(); ++i){
			if (!arr[i].is<picojson::object>()) continue;
			picojson::object& o = arr[i].get<picojson::object>();
			if (o.count("name") && o["name"].is<std::string>() &&
			    o["name"].get<std::string>() == sys && o.count("sha")){
				sha = o["sha"].get<std::string>();
				break;
			}
		}
	}
	if (sha.empty()) return false;

	// 2) Arbol de esa carpeta -> nombres de fichero (.cht).
	body.clear();
	if (!http.fetchUrl(API + "git/trees/" + sha, body, progressOut)) return false;
	{
		picojson::value v;
		if (!picojson::parse(v, body).empty() || !v.is<picojson::object>()) return false;
		picojson::object& root = v.get<picojson::object>();
		if (!root.count("tree") || !root["tree"].is<picojson::array>()) return false;
		picojson::array& tree = root["tree"].get<picojson::array>();
		for (std::size_t i = 0; i < tree.size(); ++i){
			if (!tree[i].is<picojson::object>()) continue;
			picojson::object& o = tree[i].get<picojson::object>();
			if (!o.count("path") || !o["path"].is<std::string>()) continue;
			std::string p = o["path"].get<std::string>();
			if (p.size() > 4 && p.compare(p.size() - 4, 4, ".cht") == 0)
				filenames.push_back(p);
		}
	}
	if (filenames.empty()) return false;

	// Cachear
	std::string idxDir = dir.getFolder(idxPath);
	if (!dir.dirExists(idxDir.c_str())) dir.createDirRecursive(idxDir.c_str());
	FileList::guardarVector(idxPath, filenames);
	return true;
}

// Elige el mejor .cht de 'listing' cuyo nombre normalizado coincide con 'wanted' (ya
// limpio con cleanName). Prefiere GameShark, descarta Game Buster (pcsxr-360 no la
// parsea) y desempata por region (regionScore vs romNoExt). "" si ninguno coincide.
static std::string pickBestCht(const std::vector<std::string>& listing,
                               const std::string& wanted, const std::string& romNoExt){
	if (wanted.empty()) return "";
	std::string best;
	int bestScore = -1;
	for (std::size_t i = 0; i < listing.size(); ++i){
		const std::string& fn = listing[i];
		if (fn.size() <= 4 || fn.compare(fn.size() - 4, 4, ".cht") != 0) continue;
		const std::string base = fn.substr(0, fn.size() - 4);
		if (RdbReader::cleanName(base) != wanted) continue;

		std::string low = base;
		for (std::size_t k = 0; k < low.size(); ++k)
			if (low[k] >= 'A' && low[k] <= 'Z') low[k] = (char)(low[k] + 32);
		if (low.find("game buster") != std::string::npos) continue;    // pcsxr-360 no la parsea

		int score = 1;                                                  // el nombre coincide
		if (low.find("gameshark") != std::string::npos) score += 1000; // preferir GameShark
		score += regionScore(base, romNoExt);                          // desempate por region
		if (score > bestScore){ bestScore = score; best = fn; }
	}
	return best;
}

// Descarga bajo demanda el .cht del juego actual desde libretro-database (raw). SINCRONA.
// 1) intento directo por nombre EXACTO de ROM; 2) si falla, listo la carpeta REAL de .cht del
// sistema (cacheada) y elijo por match de nombre normalizado -- los nombres de los .cht NO
// derivan del .rdb (p.ej. PSX usa "(World)" + sufijo de dispositivo, no las regiones Redump).
// Prefiero la variante GameShark y descarto Game Buster (pcsxr-360 no la parsea). Guardo en
// romPaths.cht (nombre de ROM). Devuelve la ruta, o "" si no hay.
static std::string downloadCheatForCurrentGame(float* progressOut){
	const std::string RAW = "https://raw.githubusercontent.com/libretro/libretro-database/master/";

	std::string sys = getRdbSystemName();
	if (sys.empty() || romPaths.cht.empty()) return "";

	dirutil dir;
	const std::string romNoExt = dir.getFileNameNoExt(romPaths.rompath);
	const std::string sysEnc   = urlEncodeSegment(sys);

	CurlClient dl;

	// 1) Intento directo por nombre de ROM (funciona si ya tiene nombre canonico).
	//    fetchFile devuelve true solo en 2xx; en 404 borra el fichero parcial y da false.
	InterlockedExchange(&g_cheatStage, CHEAT_STAGE_SEARCH);
	if (dl.fetchFile(RAW + "cht/" + sysEnc + "/" + urlEncodeSegment(romNoExt) + ".cht", romPaths.cht, progressOut))
		return romPaths.cht;

	// 2) Listar la carpeta REAL de .cht del sistema y elegir por match de nombre normalizado.
	std::vector<std::string> listing;
	if (!getChtFolderList(sys, listing, progressOut)) return "";

	std::string best = pickBestCht(listing, RdbReader::cleanName(romNoExt), romNoExt);

	// 2b) Fallback por HASH: si el nombre de la ROM no casa con ningun .cht (ROM renombrada),
	//     identificamos el juego por CRC en el .rdb del sistema y reintentamos el match con el
	//     nombre canonico (que normaliza bien contra los .cht). Solo cartuchos: las imagenes de
	//     CD (PSX) tienen CRC capado a 0 y no entran aqui.
	if (best.empty() && g_currentRomCrc != 0){
		std::string rdbPath = Constant::getAppDir() + Constant::getFileSep() + "data" + Constant::getFileSep() +
			"cheats" + Constant::getFileSep() + "database" + Constant::getFileSep() + sys + ".rdb";
		if (!dir.fileExists(rdbPath.c_str())){
			std::string rdbDir = dir.getFolder(rdbPath);
			if (!dir.dirExists(rdbDir.c_str())) dir.createDirRecursive(rdbDir.c_str());
			InterlockedExchange(&g_cheatStage, CHEAT_STAGE_DB);
			dl.fetchFile(RAW + "rdb/" + sysEnc + ".rdb", rdbPath, progressOut);  // si falla -> findNameByCrc dara false
		}
		std::string canonical;
		if (RdbReader::findNameByCrc(rdbPath, g_currentRomCrc, canonical) && !canonical.empty())
			best = pickBestCht(listing, RdbReader::cleanName(canonical), romNoExt);
	}

	if (best.empty()) return "";

	// 3) Descargar el elegido -> romPaths.cht (nombre de ROM, para que lo reencuentre
	//    resolveCheatPath en la siguiente carga).
	InterlockedExchange(&g_cheatStage, CHEAT_STAGE_CHEAT);
	if (dl.fetchFile(RAW + "cht/" + sysEnc + "/" + urlEncodeSegment(best), romPaths.cht, progressOut))
		return romPaths.cht;

	return "";
}

// --- Descarga asincrona con barra de progreso ---------------------------------------
// Worker: corre en IO_THREAD y hace la descarga (el float lo actualiza el ProgressCallback
// existente, sin tocarlo). Deja la ruta en g_cheatResult y marca g_cheatDone.
static DWORD WINAPI cheatDownloadWorker(LPVOID){
	std::string r = downloadCheatForCurrentGame(&g_cheatProgress);
	strncpy(g_cheatResult, r.c_str(), sizeof(g_cheatResult) - 1);
	g_cheatResult[sizeof(g_cheatResult) - 1] = '\0';
	InterlockedExchange(&g_cheatDone, 1);
	return 0;
}

// Punto de entrada (extern, llamado desde GestorMenus::descargarCheats): lanza el worker y, en el
// hilo principal, hace un bucle que pinta el titulo + el paso actual + la barra de progreso cada
// ~30ms hasta que el worker termina. runGameLoop queda bloqueado aqui, asi que este bucle ES el
// render durante la descarga. Devuelve la ruta del .cht descargado, o "".
std::string downloadCheatWithProgress(){
	g_cheatProgress  = 0.0f;
	g_cheatResult[0] = '\0';
	InterlockedExchange(&g_cheatDone, 0);
	InterlockedExchange(&g_cheatStage, CHEAT_STAGE_SEARCH);
	InterlockedExchange(&CurlClient::g_abortScrapping, 0);   // flag global (compartido con el scraper): limpiar

	HANDLE h = CreateThread(NULL, 1024 * 1024, cheatDownloadWorker, NULL, CREATE_SUSPENDED, NULL);
	if (!h)
		return downloadCheatForCurrentGame(&g_cheatProgress);   // sin hilo: cae a sincrono, sin barra

	Constant::setup_and_run_thread(h, IO_THREAD, false);        // pin core IO; no cierra el handle

	const int fBig   = Fonts::getLineSkip(Fonts::FONTBIG);
	const int fSmall = Fonts::getLineSkip(Fonts::FONTSMALL);

	while (InterlockedExchangeAdd(&g_cheatDone, 0) == 0){
		if (gameMenu && gameMenu->overlay){
			gameMenu->fillOverlay(clBackground);
			Fonts::drawTextCentTransparent(gameMenu->overlay, Fonts::getFont(Fonts::FONTBIG),
				LanguageManager::instance()->get("msg.cheats.downloading").c_str(),
				0, -fBig - fSmall, true, true, Constant::colors[clWhite].sdlColor, 0);
			Fonts::drawTextCentTransparent(gameMenu->overlay, Fonts::getFont(Fonts::FONTSMALL),
				LanguageManager::instance()->get(cheatStageKey(InterlockedExchangeAdd(&g_cheatStage, 0))).c_str(),
				0, -fSmall, true, true, Constant::colors[clWhite].sdlColor, 0);
			drawLoadingProgressBar(gameMenu->overlay, g_cheatProgress);  // barra + % y hace salviaFlip
		}
		SDL_Delay(30);
	}

	WaitForSingleObject(h, INFINITE);
	CloseHandle(h);
	return std::string(g_cheatResult);
}
