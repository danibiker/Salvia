#include "cfgloader.h"
#include <utils/langmanager.h>
#include <const/constant.h>
#include <const/cfgconst.h>
#include <http/pugixml.hpp>
#include <io/filelist.h>
#include <io/dirutil.h>
#include <io/fileio.h>
#include <video/shaderpreset.h>

#include <libretro/libretro.h>

#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm> // Requerido para std::rotate

extern "C"{
	void retro_get_system_info(struct retro_system_info *info);
}

cfg::t_cfg_props CfgLoader::configMain [cfg::MAIN_CFG_MAX];
std::string CfgLoader::appliedFileParmsCore;

CfgLoader::CfgLoader(){
	emuCfgPos = 0;
	idxRegion = 0;
	idxIdioma = 0;
	initMainConfig();
	loadMainConfig();
	loadCoreParams();

	// Se cargan los textos una vez que hemos cargado la configuracion y ya tenemos idioma asignado
	const std::string mainLang = this->configMain[cfg::mainLang].valueStr;
	LanguageManager::instance()->loadLanguage(Constant::getAppDir() + "\\assets\\i18n\\" + mainLang + ".ini");
	findAllBgMusic();
}

CfgLoader::~CfgLoader(){
}

void CfgLoader::initMainConfig(){
	dirutil dir;
	//Cargamos valores por defecto
	configMain[cfg::emulators] = cfg::t_cfg_props("emulators", "");
	configMain[cfg::emulators].desc = "#emulators: Sets a list of emulators in the order they will be presented in the frontend."
									  "\n#each name is the filename of a .cfg file inside the config directory";

	configMain[cfg::debug] = cfg::t_cfg_props("debug", false);
	configMain[cfg::debug].desc = "#Enables debug mode";

	configMain[cfg::resolution_width] = cfg::t_cfg_props("resolution_width", 1280);
	configMain[cfg::resolution_width].desc = "#The screen resolution width";

	configMain[cfg::resolution_height] = cfg::t_cfg_props("resolution_height", 720);
	configMain[cfg::resolution_height].desc = "#The screen resolution height";

	configMain[cfg::resolutionIndex] = cfg::t_cfg_props("resolutionIndex", (int)0);
	configMain[cfg::resolutionIndex].desc = "#Screen mode";

	configMain[cfg::fullscreen] = cfg::t_cfg_props("fullscreen", false);
	configMain[cfg::fullscreen].desc = "#The fullscreen mode or window mode";

	configMain[cfg::path_prefix] = cfg::t_cfg_props("path_prefix", dir.getDirActual() + Constant::getFileSep());
	configMain[cfg::path_prefix].desc = "#Path where the .xex or exe files for each core are stored."
										"\n#if empty, it will be set to the executable's path";

	configMain[cfg::aspectRatio] = cfg::t_cfg_props("aspectRatio", (int)RATIO_CORE);
	configMain[cfg::aspectRatio].desc = "#The screen aspect ratio"
										"\n#RATIO_CORE     0"
										"\n#RATIO_4_3      1"
										"\n#RATIO_3_2      2"
										"\n#RATIO_8_7      3"
										"\n#RATIO_10_9     4"
										"\n#RATIO_1_1      5"
										"\n#RATIO_5_4      6"
										"\n#RATIO_16_9     7"
										"\n#RATIO_16_10    8";

	configMain[cfg::scaleMode] = cfg::t_cfg_props("scaleMode", (int)FULLSCREEN);
	configMain[cfg::scaleMode].desc = "#Scaler used in SW mode. Not used";

	/* El shader se guarda por NOMBRE de preset (el fichero .hlslp de
	 * assets\shaders, sin extension) en vez de por indice: la lista es
	 * dinamica y un indice dejaria de apuntar a lo mismo en cuanto se anadiese
	 * o quitase un preset. El indice vivo se mantiene en valueInt, que es a lo
	 * que se ata el menu; resolveShaderModes() sincroniza los dos.
	 * Los valores numericos 0..12 de versiones anteriores se migran solos. */
	configMain[cfg::shaderMode] = cfg::t_cfg_props("shaderMode",
		std::string(ShaderRegistry::defaultId()));
	configMain[cfg::shaderMode].desc =
		"#Video shader: name of a preset in assets\\shaders (without the .hlslp extension)."
		"\n#If the preset is missing, it falls back to the default one.";

	configMain[cfg::syncMode] = cfg::t_cfg_props("syncMode", (int)OPT_SYNC_VIDEO);
	configMain[cfg::syncMode].desc = "#Video Synchronization mode"
									"\n#SYNC_TO_AUDIO	0"
									"\n#SYNC_TO_VIDEO	1"
									"\n#SYNC_NONE		2";

	configMain[cfg::libretrosystem] = cfg::t_cfg_props("libretrosystem", dir.getDirActual() + Constant::getFileSep() + "system");
	configMain[cfg::libretrosystem].desc = "#Directory where the libretro bios are stored";

	configMain[cfg::libretro_save] = cfg::t_cfg_props("libretro_save", dir.getDirActual() + Constant::getFileSep() + "data" + Constant::getFileSep() + "saves");
	configMain[cfg::libretro_save].desc = "#Directory to store sram data of games";

	configMain[cfg::libretro_state] = cfg::t_cfg_props("libretro_state", dir.getDirActual() + Constant::getFileSep() + "data" + Constant::getFileSep() + "states");
	configMain[cfg::libretro_state].desc = "#Directory to store savestates";

	configMain[cfg::roms_path] = cfg::t_cfg_props("roms_path", "");	
	configMain[cfg::roms_path].desc = "#The path usb:\\roms automatically scans Usb0, Usb1, and Usb2 for the target directory"
									"\n#Or Hdd:\\roms for the main console hard disk"
									"\n#Or game:\\roms for this directory";

	configMain[cfg::showFps] = cfg::t_cfg_props("showFps", false);
	configMain[cfg::showFps].desc = "#Show fps, memory and cpu utilization";

	configMain[cfg::packedImages] = cfg::t_cfg_props("packedImages", true);
	configMain[cfg::packedImages].desc = "#Specify if the files scraped should be packed in a .bin file (recommended)";

	configMain[cfg::integerScale] = cfg::t_cfg_props("integerScale", false);
	configMain[cfg::integerScale].desc = "#Enable or disable the screen integer scale";

	configMain[cfg::scaleIntMode] = cfg::t_cfg_props("scaleIntMode", (int)SCALE_INT_REDUCE);
	configMain[cfg::scaleIntMode].desc = "#Integer screen scale mode used when integerScale is selected"
										"\n#SCALE INT REDUCE     0"
										"\n#SCALE INT INCREASE   1"
										"\n#SCALE FIXED 1X       2"
										"\n#SCALE FIXED 2X       3"
										"\n#SCALE FIXED 3X       4"
										"\n#SCALE FIXED 4X       5"
										"\n#SCALE FIXED 5X       6";

	/* Volumen de la musica de menu.  Se guarda el INDICE en pasos de 10%, igual
	 * que scaleIntMode o animBG guardan indices: es lo que espera el widget
	 * OpcionLista del menu.  Por defecto 7 = 70%, para que la musica quede por
	 * debajo de los efectos del frontend y no moleste. */
	configMain[cfg::musicEnabled] = cfg::t_cfg_props("musicEnabled", true);
	configMain[cfg::musicEnabled].desc = "#Enable or disable the menu music completely";

	configMain[cfg::musicVolume] = cfg::t_cfg_props("musicVolume", (int)7);
	configMain[cfg::musicVolume].desc = "#Menu music volume, in 10% steps"
										"\n#0 = mute ... 10 = 100%";

	/* Musica GENERAL: la que suena cuando el core activo no define la suya en
	 * su .cfg (clave 'music_file').  Ruta relativa al directorio de la app. */
	configMain[cfg::musicFile] = cfg::t_cfg_props("musicFile", std::string("assets\\music\\menu.mp3"));
	configMain[cfg::musicFile].desc = "#Menu music file, relative to the app directory."
										"\n#Used when the active core does not define its own 'music_file'."
										"\n#Leave empty for no music.";

	configMain[cfg::animBG] = cfg::t_cfg_props("animBG", (int)BG_TILES);
	configMain[cfg::animBG].desc = "#Set the frontend background" 
								"\n#Moving tiles        0"
								"\n#Images              1"
								"\n#Plasma hlsl         2"	
								"\n#Purple drop hlsl    3"
								"\n#Wormholes hlsl      4"
								"\n#none                5";

	configMain[cfg::apikeytgdb] = cfg::t_cfg_props("apikey.tgdb", "");
	configMain[cfg::apikeytgdb].desc = "#Api key for thegamesdb";

	configMain[cfg::mainLang] = cfg::t_cfg_props("mainLang", "");
	configMain[cfg::mainLang].desc = "#mainLang: Main system lang. If not specified on xbox 360, it will try to find the configured"
									 "\n#from the console with some of this values: es, en, fr, de, it (although only 'es' and 'en' are actually supported)";

	configMain[cfg::scrapRegion] = cfg::t_cfg_props("scrapRegion", "");
	configMain[cfg::scrapRegion].desc = "#Region for the artwork scraper";

	configMain[cfg::scrapLang] = cfg::t_cfg_props("scrapLang", "");
	configMain[cfg::scrapLang].desc = "#Language for the artwork scraper";

	configMain[cfg::scrapOrigin] = cfg::t_cfg_props("scrapOrigin", (int)SC_SCREENCSRAPER);
	configMain[cfg::scrapOrigin].desc = "#scrapOrigin: Sets the scraper source"
										"\n#screenscraper.fr 	0"
										"\n#thegamesdb			1";

	configMain[cfg::enableAchievements] = cfg::t_cfg_props("enableAchievements", true);
	configMain[cfg::enableAchievements].desc = "#Enable retroachievements";

	configMain[cfg::hardcoreRA] = cfg::t_cfg_props("hardcoreRA", true);
	configMain[cfg::hardcoreRA].desc = "#hardcoreRA: Sets the hardcore mode for retroachievements -> Not validated yet to use from the retroachievements team :(";

	configMain[cfg::raUser] = cfg::t_cfg_props("raUser", "");
	configMain[cfg::raUser].desc = "#retroachievements username";

	configMain[cfg::raPass] = cfg::t_cfg_props("raPass", "");
	configMain[cfg::raPass].desc = "#retroachievements password";

	configMain[cfg::scrapUser] = cfg::t_cfg_props("scrapUser", "");
	configMain[cfg::scrapUser].desc = "#Username for screenscraper.fr";

	configMain[cfg::scrapPass] = cfg::t_cfg_props("scrapPass", "");
	configMain[cfg::scrapPass].desc = "#Password for screenscraper.fr";

	configMain[cfg::showEmptyEmulators] = cfg::t_cfg_props("showEmptyEmulators", true);
	configMain[cfg::showEmptyEmulators].desc = "#Set to 'no' to hide emulators without games";

	configMain[cfg::fastForwardMult] = cfg::t_cfg_props("fastForwardMult", (int)30);
	configMain[cfg::fastForwardMult].desc = "#Specify the fast forward nultiplier. Divide it by 10 to have the actual value. 30 = 3x speed";

	configMain[cfg::overscan_x] = cfg::t_cfg_props("overscan_x", (int)0);
	configMain[cfg::overscan_x].desc = "#Sets the frontend overscan to enlarge or shrink the x axis of the frontend screen"
		"\n#It doesn't affect the game screen";

	configMain[cfg::overscan_y] = cfg::t_cfg_props("overscan_y", (int)0);
	configMain[cfg::overscan_y].desc = "#Sets the frontend overscan to enlarge or shrink the y axis of the frontend screen"
		"\n#It doesn't affect the game screen";

	struct retro_system_info info;
	memset(&info, 0, sizeof(info));
	retro_get_system_info(&info);
	configMain[cfg::libretro_core].setPropValue(std::string(info.library_name));
	configMain[cfg::libretro_core_version].setPropValue(std::string(info.library_version));
	configMain[cfg::libretro_core_extensions].setPropValue(std::string(info.valid_extensions));
	configMain[cfg::lastOptSel].setPropValue((int)-1);
}

/**
* 
*/
void CfgLoader::loadMainConfig(){
	dirutil dir;
	std::string filepath = Constant::getAppDir() + std::string(Constant::tempFileSep) + CONFIGFILE;

	if (!dir.fileExists(filepath.c_str())){
		LOG_ERROR("Main config file not found: %s", filepath.c_str());
		std::string upperCfgFile = CONFIGFILE;
		Constant::upperCase(&upperCfgFile);
		filepath = Constant::getAppDir() + std::string(Constant::tempFileSep) + upperCfgFile;
		if (!dir.fileExists(filepath.c_str())){
			LOG_ERROR("Main config file not found: %s", filepath.c_str());
		}
	}

	fstream filecfg;
	filecfg.open(filepath, ios::in);

	bool fileopened = filecfg.is_open();
	if (fileopened){
		std::string line;
		while(getline(filecfg, line)){
			line = Constant::Trim(Constant::replaceAll(Constant::replaceAll(line, "\r", ""), "\n", ""));
			if (line.length() > 1 && line.at(0) != '#' && line.find("=") != std::string::npos){
				std::vector<std::string> keyvalue = Constant::splitChar(line, '=');
				if (keyvalue.size() < 2)
					continue;

				const std::string key = Constant::Trim(keyvalue.at(0));
				const std::string value = Constant::Trim(keyvalue.at(1));

				int found = findKeyCfg(key);
				if (found > -1){
					cfg::t_cfg_props& prop = configMain[found]; // Usamos referencia para no escribir tanto

					switch (prop.type) {
						case cfg::CFG_TYPE_BOOL:
							prop.setPropValue(value == "yes" || value == "true" || value == "1");
							break;
						case cfg::CFG_TYPE_INT:
							prop.setPropValue(Constant::strToTipo<int>(value));
							break;
						case cfg::CFG_TYPE_FLOAT:
							prop.setPropValue(Constant::strToTipo<float>(value)); // Corregido a float
							break;
						case cfg::CFG_TYPE_STR:
							prop.setPropValue(value);
							break;
					}
				}
			}
		}
	}
	filecfg.close();

	checkSystemLang();

	if (fileopened){
		if (isDebug()) LOG_DEBUG("Loading emulators", "");
		std::string emulist;
		configMain[cfg::emulators].getPropValue(emulist);
		vector<std::string> emulators = Constant::splitChar(emulist, ' ');

		for (std::size_t i=0; i < emulators.size(); i++){
			loadEmuConfig(emulators.at(i));
		}
		if (isDebug()) cout << endl;         
	}

	//Adding always the configuration options
	std::unique_ptr<cfg::t_cfg_emu> salviaConfig(new cfg::t_cfg_emu);
	salviaConfig->config.generalConfig = true;
	salviaConfig->config.name = "Options";
	salviaConfig->config.title_bkg_assets = "assets\\cfg";
	emulators.push_back(std::move(salviaConfig));

	resolveShaderModes();
}

/* Traduce el nombre de preset guardado en la configuracion al indice vivo que
 * usan el menu y XBOX_SelectEffect. Se llama al final de loadMainConfig, con el
 * registro de shaders ya cargado (salvia.cpp lo hace antes de construir el
 * CfgLoader).
 *
 * Acepta tambien los valores numericos 0..12 que guardaban las versiones
 * anteriores: ShaderRegistry::migrateLegacyId los convierte al nombre
 * equivalente, asi que un .cfg antiguo sigue arrancando con el mismo filtro. */
void CfgLoader::resolveShaderModes(){
	ShaderRegistry* shaders = ShaderRegistry::instance();

	/* --- Shader global --- */
	std::string wanted = configMain[cfg::shaderMode].valueStr;
	int idx = shaders->indexOfStored(wanted);
	if (idx < 0){
		if (!wanted.empty())
			LOG_ERROR("El shader '%s' no existe en assets\\shaders; se usa '%s'\n",
			          wanted.c_str(), ShaderRegistry::defaultId());
		idx = shaders->indexOf(ShaderRegistry::defaultId());
		if (idx < 0) idx = 0;
	}
	configMain[cfg::shaderMode].valueInt = idx;
	configMain[cfg::shaderMode].valueStr = shaders->idAt(idx);

	/* --- Override por emulador. El 0 del menu es "Auto", de ahi el +1. --- */
	for (std::size_t i = 0; i < emulators.size(); i++){
		ConfigEmu& cfg = emulators[i]->config;
		std::string name = cfg.shaderName;
		if (name.empty() || name == "-1"){
			cfg.shaderMode = 0;
			cfg.shaderName = "";
			continue;
		}
		int e = shaders->indexOfStored(name);
		if (e < 0){
			/* Si el preset ya no esta, se degrada a Auto (usar el global) y no
			 * a otro shader cualquiera, que seria una sorpresa peor. */
			LOG_ERROR("%s: el shader '%s' no existe; se usa Auto\n",
			          cfg.name.c_str(), name.c_str());
			cfg.shaderMode = 0;
			cfg.shaderName = "";
		} else {
			cfg.shaderMode = e + 1;
			cfg.shaderName = shaders->idAt(e);
		}
	}
}

void CfgLoader::checkSystemLang(){
	Fileio fileio;
	std::string mainLang = configMain[cfg::mainLang].valueStr;
	std::string xmlRegion, xmlLang;
	
	if (configMain[cfg::mainLang].valueStr.empty()){
		//Try to guess the language
#ifdef _XBOX
		// Obtener el ID del idioma del sistema
		DWORD dwLanguage = XGetLanguage();

		std::string langXbox;
		switch (dwLanguage) {
			case XC_LANGUAGE_SPANISH:
				langXbox = "es";
				break;
			case XC_LANGUAGE_ENGLISH:
				langXbox = "en";
				break;
			case XC_LANGUAGE_FRENCH:
				langXbox = "fr";
				break;
			case XC_LANGUAGE_GERMAN:
				langXbox = "de";
				break;
			case XC_LANGUAGE_ITALIAN:
				langXbox = "it";
				break;
			default:
				// Idioma por defecto si no coincide
				langXbox = "en";
				break;
		}

		configMain[cfg::mainLang].setPropValue(langXbox);
#else
		configMain[cfg::mainLang].setPropValue(std::string("en"));
#endif
		configMain[cfg::scrapRegion] = cfg::t_cfg_props("scrapRegion", std::string("eu"));
		configMain[cfg::scrapLang] = cfg::t_cfg_props("scrapLang", configMain[cfg::mainLang].valueStr);
	}

	configMain[cfg::mainLang].getPropValue(mainLang);
	xmlRegion = fileio.cargarFichero(Constant::getAppDir() + "\\assets\\i18n\\regionsListe.xml");
	parsearRegiones(xmlRegion.c_str(), mainLang, region);
	xmlRegion.clear();
	
	xmlLang = fileio.cargarFichero(Constant::getAppDir() + "\\assets\\i18n\\languesListe.xml");
	parsearIdiomas(xmlLang.c_str(), mainLang, idioma);
	xmlLang.clear();
}

/**
*
*/
void CfgLoader::parsearIdiomas(const char* xmlData, const std::string& isoCode, 
                    std::vector<FieldIdDesc>& idioma) 
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlData);

    if (result.status != pugi::status_ok) return;

    // VS2010: Construccion manual del nombre del nodo
    std::string nombreNodo = "nom_" + isoCode;

    // Acceso al nodo raiz
    pugi::xml_node langues = doc.child("Data").child("langues");

    // Iteracion compatible con C++03 (Visual Studio 2010)
    for (pugi::xml_node langue = langues.child("langue"); langue; langue = langue.next_sibling("langue")) 
    {
        // Obtenemos los valores. child_value() devuelve "" si no lo encuentra.
        const char* nomcourt = langue.child_value("nomcourt");
        const char* desc = langue.child_value(nombreNodo.c_str());
		const int id = Constant::strToTipo<int>(langue.child_value("id"));

        idioma.push_back(FieldIdDesc(id, nomcourt, desc));
    }

	std::sort(idioma.begin(), idioma.end(), [](const FieldIdDesc& a, const FieldIdDesc& b) {
		return a.desc < b.desc; // Orden ascendente por el campo 'desc'
	});
}

/**
*
*/
void CfgLoader::parsearRegiones(const char* xmlData, const std::string& isoCode, 
                    std::vector<FieldIdDesc>& region) 
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlData);

    if (result.status != pugi::status_ok) return;

    // VS2010: Construccion manual del nombre del nodo
    std::string nombreNodo = "nom_" + isoCode;

    // Acceso al nodo raiz
    pugi::xml_node langues = doc.child("Data").child("regions");

    // Iteracion compatible con C++03 (Visual Studio 2010)
    for (pugi::xml_node langue = langues.child("region"); langue; langue = langue.next_sibling("region")) 
    {
        // Obtenemos los valores. child_value() devuelve "" si no lo encuentra.
        const char* nomcourt = langue.child_value("nomcourt");
        const char* desc = langue.child_value(nombreNodo.c_str());
		const int id = Constant::strToTipo<int>(langue.child_value("id"));

        region.push_back(FieldIdDesc(id, nomcourt, desc));
    }

	std::sort(region.begin(), region.end(), [](const FieldIdDesc& a, const FieldIdDesc& b) {
		return a.desc < b.desc; // Orden ascendente por el campo 'desc'
	});
}

/**
*
*/
int CfgLoader::findKeyCfg(const std::string& keyStr){
	for (int i=0; i < cfg::MAIN_CFG_MAX; i++){
		if (keyStr == configMain[i].name){
			return i;
		}
	}
	return -1;
}

/**
* 
*/
void CfgLoader::loadEmuConfig(std::string emuname){
	//ConfigEmu cfgEmu;
	dirutil dir;
	std::string strFilepath = Constant::getAppDir() + std::string(Constant::tempFileSep)
		+ "config" + std::string(Constant::tempFileSep) + emuname + ".cfg";
	const char *filepath = strFilepath.c_str();

	//cout << " " << emuname << endl;
	LOG_DEBUG("Emulator: %s\n", emuname.c_str());

	bool fileopened = false;
	//cout << "Checking if exists" <<endl;
	if (dir.fileExists(filepath) && !dir.isDir(filepath)){
		//if (dir.fileExists(filepath)){
		fstream fileCfg;
		//cout << "Opening file" <<endl;
		fileCfg.open(filepath, ios::in);

		//cout << "Checking if is open" <<endl;
		fileopened = fileCfg.is_open();
		if (fileopened){
			std::string line;
			std::unique_ptr<cfg::t_cfg_emu> cfgEmu(new cfg::t_cfg_emu);

			cfgEmu->config.internalName = emuname;
			cfgEmu->config.cfgFilePath = filepath;

			while(getline(fileCfg, line)){
				//cout << "reading line" <<endl;
				if (line.length() > 1 && line.at(0) != '#' && line.find("=") != std::string::npos){
					//cout << "splitting line and trimming" <<endl;
					std::vector<std::string> keyvalue = Constant::splitChar(line, '=');        
					std::string key = keyvalue.size() > 0 ? Constant::Trim(keyvalue.at(0)) : "";
					std::string value = keyvalue.size() > 1 ? Constant::Trim(keyvalue.at(1)) : "";

					if (keyvalue.size() < 2)
						continue;

					//cout << "assigning value for: " << key <<endl;
					if (key.compare("name") == 0){
						cfgEmu->config.name = value;
					} else if (key.compare("system") == 0){
						cfgEmu->config.system = value;
					} else if (key.compare("description") == 0){
						cfgEmu->config.description = value;
					} else if (key.compare("directory") == 0){
						cfgEmu->config.directory = value;
					} else if (key.compare("executable") == 0){
						getExecutables(value, cfgEmu.get());
					} else if (key.compare("global_options") == 0){
						cfgEmu->config.global_options = value;
					} else if (key.compare("map_file") == 0){
						cfgEmu->config.map_file = value;
					} else if (key.compare("options_before_rom") == 0){
						cfgEmu->config.options_before_rom = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("screen_shot_directory") == 0){
						cfgEmu->config.screen_shot_directory = value;
					} else if (key.compare("assets") == 0){
						cfgEmu->config.assets = value;
					} else if (key.compare("music_file") == 0){
						cfgEmu->config.music_file = value;
					} else if (key.compare("use_rom_file") == 0){
						cfgEmu->config.use_rom_file = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("rom_directory") == 0){
						cfgEmu->config.rom_directory = value;
					} else if (key.compare("rom_extension") == 0){
						cfgEmu->config.rom_extension = value;
					} else if (key.compare("use_extension") == 0){
						cfgEmu->config.use_extension = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("use_rom_directory") == 0){
						cfgEmu->config.use_rom_directory = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("no_uncompress") == 0){
						cfgEmu->config.no_uncompress = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("mame_roms_xml") == 0){
						cfgEmu->config.mame_roms_xml = value;
					} else if (key.compare("keyboard_type") == 0){
						cfgEmu->config.keyboard_type = value;
					} else if (key.compare("menu_show_directories") == 0){
						cfgEmu->config.menu_show_directories = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("menu_directory_recursive") == 0){
						cfgEmu->config.menu_directory_recursive = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("network_default_servers") == 0){
						cfgEmu->config.network_default_servers = value;
					} else if (key.compare("title_bkg_assets") == 0){
						cfgEmu->config.title_bkg_assets = value;
					} else if (key.compare("aspectRatio") == 0){
						//This option is to override the configMain, so the -1 value is for the auto option
						cfgEmu->config.aspectRatio = Constant::strToTipo<int>(value) + 1;
					} else if (key.compare("scaleMode") == 0){
						//This option is to override the configMain, so the -1 value is for the auto option
						cfgEmu->config.scaleMode = Constant::strToTipo<int>(value) + 1;
					} else if (key.compare("integerScale") == 0){
						//This option is to override the configMain, so the -1 value is for the auto option
						cfgEmu->config.integerScale = Constant::strToTipo<int>(value) + 1;
					} else if (key.compare("scaleIntMode") == 0){
						//This option is to override the configMain, so the -1 value is for the auto option
						cfgEmu->config.scaleIntMode = Constant::strToTipo<int>(value) + 1;
					} else if (key.compare("shaderMode") == 0){
						/* Nombre de preset (o un indice antiguo). Se guarda en crudo y
						 * se resuelve a indice en resolveShaderModes(), cuando el
						 * registro de shaders ya esta cargado. */
						cfgEmu->config.shaderName = value;
					} else if (key.compare("syncMode") == 0){
						//This option is to override the configMain, so the -1 value is for the auto option
						cfgEmu->config.syncMode = Constant::strToTipo<int>(value) + 1;
					}
				}
			}             
			emulators.push_back(std::move(cfgEmu));
		}
		//cout << "closing file..." <<endl;   
		fileCfg.close();
	}

	if (!fileopened){
		//textout_centre_ex(screen, font, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
		//textout_centre_ex(screen, font, "Press a key to continue", screen->w / 2, screen->h / 2 + (font->height + 3), textColor, -1);
		LOG_ERROR("There is no config file for %s. Exiting...\n", emuname.c_str());
		//readkey();
	}
}

void CfgLoader::getExecutables(std::string str, cfg::t_cfg_emu* emu){
	Constant::splitChar(str, ';', emu->config.cores);
	if (!emu->config.cores.empty()){
		//Simply obtain the first of the list
		emu->config.executable = emu->config.cores.front();
	}
}

/**
* Gets a string from the list of executables by moving the selected one
* to the beginning of the list. This way, when the list is loaded at startup
* the emulator, we know that the selected one is the first element
*/
std::string CfgLoader::getExecutablesStringOrdered(const ConfigEmu& cfg){
	std::string execs;

	// We make a copy to modify the vector freely
    std::vector<std::string> localCores = cfg.cores; 

	if (cfg.execIdx > 0 && (std::size_t)cfg.execIdx < localCores.size()){
		// std::rotate takes three iterators:
		// 1. The beginning of the range to modify (v.begin())
		// 2. The element that will become the FIRST (v.begin() + idx)
		// 3. The end of the range to modify (v.begin() + idx + 1)
		std::rotate(localCores.begin(), localCores.begin() + cfg.execIdx, localCores.begin() + cfg.execIdx + 1);
	}

	for (std::size_t i=0; i < localCores.size(); i++){
		execs += localCores[i] + (i < localCores.size() - 1 ? ";" : "");
	}
	return execs;
}

int CfgLoader::getWidth(){
	int val;
	configMain[cfg::resolution_width].getPropValue(val);
	return val < 0 ? 1280 : val;
}

int CfgLoader::getHeight(){
	int val;
	configMain[cfg::resolution_height].getPropValue(val);
	return val < 0 ? 1280 : val;
}

void CfgLoader::setWidth(int w){
	configMain[cfg::resolution_width].setPropValue(w);
}

void CfgLoader::setHeight(int h){
	configMain[cfg::resolution_height].setPropValue(h);
}

bool CfgLoader::isDebug(){
	bool val;
	configMain[cfg::debug].getPropValue(val);
	return val;
}

ConfigEmu* CfgLoader::getNextCfgEmu(){
    emuCfgPos++;
    emuCfgPos = emuCfgPos % emulators.size();
	return &emulators.at(emuCfgPos)->config;
}

ConfigEmu* CfgLoader::getPrevCfgEmu(){
    if (emuCfgPos <= 0 && emulators.size() > 0)
        emuCfgPos = emulators.size() - 1;
    else 
        emuCfgPos--;
	return &emulators.at(emuCfgPos)->config;
}

ConfigEmu* CfgLoader::getCfgEmu(){
    return &emulators.at(emuCfgPos)->config;
}

ConfigEmu* CfgLoader::findCfgEmu(std::string execName){
	LOG_DEBUG("Buscando el ejecutable %s\n", execName.c_str());
	for (unsigned int i=0; i < emulators.size(); i++){
		LOG_DEBUG("Ejecutable %s\n", emulators.at(i)->config.executable.c_str());
		if (emulators.at(i)->config.executable.find(execName) != std::string::npos){
			return &emulators.at(i)->config;
		}
	}
	return NULL;
}

std::map<std::string, std::unique_ptr<cfg::t_emu_props> >& CfgLoader::getLibretroParams() {
    // Retorna la referencia al mapa dentro del vector
    return startupLibretroParams;
}

std::string CfgLoader::saveMainParams(){
	std::vector<std::string> fileMainCfg;
	std::string line;

	//actualizamos algunos parametros que dependen de un indice externo
	configMain[cfg::scrapRegion].setPropValue(region[idxRegion].shortName);
	configMain[cfg::scrapLang].setPropValue(idioma[idxIdioma].shortName);
	/* El menu mueve el INDICE vivo (valueInt); lo que se persiste es el nombre
	 * del preset, porque la lista de assets\shaders es dinamica. */
	configMain[cfg::shaderMode].setPropValue(
		ShaderRegistry::instance()->idAt(configMain[cfg::shaderMode].valueInt));

	for (int i=0; i < cfg::MAIN_CFG_MAX; i++){
		if (configMain[i].name.empty()) continue;

		if (!configMain[i].desc.empty())
			fileMainCfg.push_back(configMain[i].desc);

		line = configMain[i].name + "=";

		switch (configMain[i].type){
			case cfg::CFG_TYPE_INT:
				line += Constant::TipoToStr<int>(configMain[i].valueInt);
				break;
			case cfg::CFG_TYPE_FLOAT:
				line += Constant::TipoToStr<float>(configMain[i].valueFloat);
				break;
			case cfg::CFG_TYPE_BOOL:
				line += configMain[i].valueBool ? "yes" : "no";
				break;
			case cfg::CFG_TYPE_STR:
				line += configMain[i].valueStr;
				break;
		}

		fileMainCfg.push_back(line);
	}

	std::string mainPath = Constant::getAppDir() + Constant::getFileSep() + CONFIGFILE;
	FileList::guardarVector(mainPath, fileMainCfg);
	return LanguageManager::instance()->get("msg.cfg.savelocation") + mainPath;
}

std::string CfgLoader::saveCoreParams(){
	std::vector<std::string> fileCoreCfg;
	std::string optionValues;

	for (auto it = startupLibretroParams.begin(); it != startupLibretroParams.end(); ++it) {
		optionValues = "";
		for (std::size_t i=0; i < it->second->values.size(); i++){
			optionValues += it->second->values[i] + (i<it->second->values.size() - 1 ? " | " : "");
		}
		fileCoreCfg.push_back("#" + optionValues);
		fileCoreCfg.push_back(it->first + "=" + Constant::TipoToStr(it->second->selected));
    }

	std::string corepath = getCoreCfgPath(true);
	FileList::guardarVector(corepath, fileCoreCfg);

	appliedFileParmsCore = dirutil::getFileName(corepath);
	return LanguageManager::instance()->get("msg.cfg.savelocation") + corepath;
}

bool CfgLoader::deleteCoreParams(){
	dirutil dir;
	const std::string corepath = getCoreCfgPath();
	if (!corepath.empty() && dir.fileExists(corepath.c_str()) && !dir.isDir(corepath.c_str())){
		dir.borrarArchivo(corepath);
		return true;
	}
	return false;
}

void CfgLoader::loadCoreParams(){
#ifdef SYSTEM_OPT
	//We recover the menu status to set the emuCfgPos and to store the cfg::lastOptSel
	struct ListStatus statusMenu;
	if (recoverGameMenuPos(statusMenu) == 0){
		configMain[cfg::lastOptSel].setPropValue(statusMenu.emuLoaded);
	}
#endif
	applyCoreParamsFile(getCoreCfgPath());
}

// Aplica un fichero de opciones de core (lineas "key=indice", '#'=comentario)
// ACTUALIZANDO `selected` in-place sobre las entradas existentes (conserva
// values/labels que el core rellena en SET_CORE_OPTIONS); si la key no existe
// aun, crea una entrada minima. Robusto tanto antes como despues de que el core
// declare las opciones. Devuelve true si el fichero existia y tenia contenido.
bool CfgLoader::applyCoreParamsFile(const std::string& path){
	std::vector<std::string> fileConfig;
	FileList::cargarVector(path, fileConfig);
	if (fileConfig.empty()) return false;

	appliedFileParmsCore = dirutil::getFileName(path);
	std::size_t pos = 0;
	for (unsigned int i=0; i<fileConfig.size(); i++){
		std::string linea = fileConfig.at(i);
		if (linea.empty() || linea[0] == '#') continue;
		if ((pos = linea.find("=")) != std::string::npos){
			std::string key = linea.substr(0, pos);
			int idx = Constant::strToTipo<int>(Constant::Trim(linea.substr(pos + 1)));
			std::map<std::string, std::unique_ptr<cfg::t_emu_props> >::iterator it = startupLibretroParams.find(key);
			if (it != startupLibretroParams.end()){
				it->second->selected = idx;
			} else {
				cfg::t_emu_props *ptr = new cfg::t_emu_props();
				ptr->selected = idx;
				startupLibretroParams[key] = std::unique_ptr<cfg::t_emu_props>(ptr);
			}
		}
	}
	return true;
}

// Ruta del fichero de opciones de core POR JUEGO: junto al juego, mismo nombre
// base + CORE_OPT_EXT (.opt). Distinta del .cfg del joystick por-juego.
std::string CfgLoader::getGameCoreCfgPath(const std::string& gamePath){
	dirutil dir;
	return dir.getFolder(gamePath) + Constant::getFileSep() + dir.getFileNameNoExt(gamePath) + CORE_OPT_EXT;
}

// Resetea TODAS las opciones (startupLibretroParams) a su valor por defecto
// declarado por el core (defaultSelected, poblado por applyEntry en
// SET_CORE_OPTIONS). Base para recargar limpio al cambiar de juego.
void CfgLoader::resetCoreParamsToDefaults(){
	appliedFileParmsCore = LanguageManager::instance()->get("menu.core.options.msg.default");
	for (std::map<std::string, std::unique_ptr<cfg::t_emu_props> >::iterator it = startupLibretroParams.begin();
	     it != startupLibretroParams.end(); ++it) {
		cfg::t_emu_props *p = it->second.get();
		int d = (p->defaultSelected >= 0 && p->defaultSelected < (int)p->values.size())
		        ? p->defaultSelected : 0;
		p->selected = d;
	}
}

// En la carga del juego, por CAPAS para no arrastrar opciones del juego anterior:
//   1) reset a los defaults del core (base limpia);
//   2) aplicar el config GENERAL del core (config/core_<core>.ini) si existe;
//   3) si hay fichero especifico del juego (<juego>.opt), sobreescribe.
// Asi, un juego SIN fichero propio vuelve a las opciones generales (o a los
// defaults si no hay config general), en vez de heredar las del juego anterior.
void CfgLoader::loadCoreParamsForGame(const std::string& gamePath){
	dirutil dir;
	resetCoreParamsToDefaults();
	applyCoreParamsFile(getCoreCfgPath());
	std::string gamecfg = getGameCoreCfgPath(gamePath);
	if (dir.fileExists(gamecfg.c_str())){
		LOG_DEBUG("Cargando opciones de core especificas del juego: %s", gamecfg.c_str());
		applyCoreParamsFile(gamecfg);
	}
}

// Igual que saveCoreParams pero al fichero por-juego (junto al juego).
std::string CfgLoader::saveGameCoreParams(const std::string& gamePath){
	if (gamePath.empty())
		return LanguageManager::instance()->get("msg.key.cfg.load");

	std::vector<std::string> fileCoreCfg;
	std::string optionValues;
	for (auto it = startupLibretroParams.begin(); it != startupLibretroParams.end(); ++it) {
		optionValues = "";
		for (std::size_t i=0; i < it->second->values.size(); i++){
			optionValues += it->second->values[i] + (i<it->second->values.size() - 1 ? " | " : "");
		}
		fileCoreCfg.push_back("#" + optionValues);
		fileCoreCfg.push_back(it->first + "=" + Constant::TipoToStr(it->second->selected));
	}

	std::string gamecfg = getGameCoreCfgPath(gamePath);
	FileList::guardarVector(gamecfg, fileCoreCfg);

	appliedFileParmsCore = dirutil::getFileName(gamecfg);
	return LanguageManager::instance()->get("msg.cfg.savelocation") + gamecfg;
}

bool CfgLoader::deleteGameParams(const std::string& gamePath){
	dirutil dir;
	std::string gamecfg = getGameCoreCfgPath(gamePath);
	if (!gamePath.empty() && dir.fileExists(gamecfg.c_str()) && !dir.isDir(gamePath.c_str())){
		dir.borrarArchivo(gamecfg);
		return true;
	}
	return false;
}

std::string CfgLoader::getCoreCfgPath(bool save){
	std::size_t last = configMain[cfg::path_prefix].valueStr.length() <= 0 ? 0 : configMain[cfg::path_prefix].valueStr.length() - 1;
	bool lastFileSep = true;
	if (last < configMain[cfg::path_prefix].valueStr.length()){
		configMain[cfg::path_prefix].valueStr[last] = Constant::getFileSep()[0];
	}

	std::string prefixOpt = configMain[cfg::libretro_core].valueStr;
	std::string pathOpt = configMain[cfg::path_prefix].valueStr + (lastFileSep ? "" : Constant::getFileSep()) + 
		"config" + Constant::getFileSep() + "core_" + prefixOpt + CORE_OPT_EXT;

// If SYSTEM_OPT is defined, a system configuration file has greater preference in comparison to the core configuration
// For example, the PUAE core can emulate 3 different systems: Amiga 500, Amiga 1200 and Amiga CD32. For this systems
// it might be desirable to have independent configuration (Amiga 1200 is to slow so we enable frameskip, but it's not
// needed for Amiga 500)
#ifdef SYSTEM_OPT

	//emuCfgPos will be set at the beginning to the last selected emulator
	int systemSelected = this->emuCfgPos;
	vector<string> v = Constant::splitChar(emulators.at(systemSelected)->config.system, '_');

	//if the actual selected emulator is not found in the preprocessor defined, 
	//load the last valid one stored in cfg::lastOptSel
	if (!v.empty() && v.back().find(string(SYSTEM_OPT)) == string::npos){
		systemSelected = configMain[cfg::lastOptSel].valueInt;
		v = Constant::splitChar(emulators.at(systemSelected)->config.system, '_');
	}

	if (!v.empty() && v.back().find(string(SYSTEM_OPT)) != string::npos){
		configMain[cfg::lastOptSel].setPropValue(systemSelected);
		prefixOpt = v.back();

		std::string tmpPathOpt = configMain[cfg::path_prefix].valueStr + (lastFileSep ? "" : Constant::getFileSep()) + 
		"config" + Constant::getFileSep() + "core_" + prefixOpt + CORE_OPT_EXT;

		if (dirutil::fileExists(tmpPathOpt.c_str()) || save){
			pathOpt = tmpPathOpt;
		}
	}
#endif

	return pathOpt;
}

/**
 * 
 */
int CfgLoader::recoverGameMenuPos(struct ListStatus &read_struct){
    FILE* infile;
    string filepath = Constant::getAppDir() + Constant::getFileSep() + MENUTMP;
    int ret = 0;

    // Open person.dat for reading
    infile = fopen(filepath.c_str(), "rb");
    if (infile == NULL) {
        cerr << "Error openning file: " << filepath << endl;
        return 1;
    }

    if (fread(&read_struct, sizeof(read_struct), 1, infile) > 0){
        LOG_DEBUG("emupos: %d; inipos: %d; endpos: %d; curpos: %d; maxlines: %d; layout: %d; animateBkg: %d", read_struct.emuLoaded,  
			read_struct.iniPos, read_struct.endPos, read_struct.curPos, read_struct.maxLines, read_struct.layout, read_struct.animateBkg);
        //Setting the emulator selected        
        emuCfgPos = read_struct.emuLoaded;
    } else {
        ret = 1;
    }

    fclose(infile);
    return ret;
}

void CfgLoader::findAllBgMusic(){
	const std::string autoOverrideTxt = LanguageManager::instance()->get("menu.core.overrides.auto");
	//Find all the mp3 files for the background music
	dirutil dir;
	std::string assetsDir = dirutil::getPathPrefix(ROUTE_ASSETS_BGMUSIC);
	vector<unique_ptr<FileProps>> files;
	dir.listFiles(assetsDir.c_str(), files, ".mp3", "", true, false);
	
	musicFiles.push_back(autoOverrideTxt);
	for (unsigned int i=0; i < files.size(); i++){
		musicFiles.push_back(files[i]->filename);
	}
}

// Metodo para guardar la configuracion en un archivo
std::string CfgLoader::saveCoreOverrideParams(int emuIdx){
	ConfigEmu& cfg = emulators.at(emuIdx).get()->config;
	
	//Get the executables ordered
	cfg.executable = CfgLoader::getExecutablesStringOrdered(cfg);
	const std::string rutaArchivo = cfg.cfgFilePath;
    std::ofstream archivo(rutaArchivo.c_str());
    if (!archivo.is_open()) return "";

	//Set the music based on its index
	if (cfg.music_file_index > 0 && cfg.music_file_index < (int)this->musicFiles.size()){
		cfg.music_file = dirutil::getPathPrefix(this->musicFiles[cfg.music_file_index], ROUTE_ASSETS_BGMUSIC);
	} else {
		cfg.music_file.clear();
	}

    /* El menu trabaja con el indice vivo (0 = "Auto"); lo que se persiste es el
     * nombre del preset. Se sincroniza justo antes de volcar. */
    cfg.shaderName = (cfg.shaderMode > 0)
        ? ShaderRegistry::instance()->idAt(cfg.shaderMode - 1) : std::string("");

    // 1. Escribimos los campos de tipo string de forma masiva
    struct MappingStr { const char* nombre; const char* descripcion; const std::string ConfigEmu::*puntero; };
    MappingStr strings[] = {
        {"name", "Emulator Name", &ConfigEmu::name},
        {"description", "Emulator description", &ConfigEmu::description},
        {"system", "Emulated system", &ConfigEmu::system},
        {"directory", "Emulator location, e.g.: c:\\mame", &ConfigEmu::directory},
		{"executable", "Name of the emulator executable, e.g., mame.exe" 
		"\n#More than one emulator can be specified separated by the character: \";\". The first one will be the default", &ConfigEmu::executable},
        {"global_options", "Global options passed to the emulator, e.g., -sound 1", &ConfigEmu::global_options},
        {"rom_directory", "ROM Directory", &ConfigEmu::rom_directory},
        {"rom_extension", "List of supported extensions for ROMs (without the \".\")", &ConfigEmu::rom_extension},
        {"assets", "This is the directory where images and information are stored.", &ConfigEmu::assets},
		{"music_file", "Menu music for this core, relative to the app directory."
					   "\n#Leave empty to use the general one from the main config (musicFile).", &ConfigEmu::music_file},
		{"title_bkg_assets", "Instead of loading the title and background from the assets directory, we load them from this path."
							 "\n#This is useful if, for example, we have many Mame images that are actually the same as those in fbneo.", &ConfigEmu::title_bkg_assets},
        {"screen_shot_directory", "This is the directory where the screenshots in .png format are located.", &ConfigEmu::screen_shot_directory},
        {"mame_roms_xml", "Xml file with Mame game names", &ConfigEmu::mame_roms_xml},
        {"map_file", "This is the list of pre-scanned ROMs (not supported yet).", &ConfigEmu::map_file},
        {"keyboard_type", "Keyboard type. Implemented for: msx and spectrum", &ConfigEmu::keyboard_type},
		{"network_default_servers", "List of servers to use. Quake specific", &ConfigEmu::network_default_servers},
        {"shaderMode", "Video shader override: name of a preset in assets\\shaders"
		"\n#(without the .hlslp extension). Leave EMPTY to use the global shader."
		, &ConfigEmu::shaderName}
    };
    
    for (std::size_t i = 0; i < sizeof(strings)/sizeof(strings[0]); ++i) {
		archivo << "#" << strings[i].descripcion << "\n";
        archivo << strings[i].nombre << " = " << cfg.*(strings[i].puntero) << "\n";
    }

    // 2. Escribimos los campos booleanos convirtiendo a "yes/no"
    struct MappingBool { const char* nombre; const char* descripcion; const bool ConfigEmu::*puntero; };
    MappingBool bools[] = {
		{"options_before_rom", "The options appear before the ROM at startup: \"yes\" o \"no\"."
		"\n# ej. yes: emulator.exe -option1 -option2 rom"
		"\n#     no: emulator.exe rom -option1 -option2", &ConfigEmu::options_before_rom},
        {"use_rom_file", "A ROM file is a list of ROMs to be used."
		"\n#If set to \"no\", the ROMs in the rom directory are scanned."
		"\n# If set to \"yes\", a ROM file (which is basically a list of ROMs) is used"
		"\n# instead of attempting to scan. The default is \"no\"."
		"\n# ROM files are useful for ROMs merged with MAME, where the actual"
		"\n# ROM names are buried inside a ZIP file.", &ConfigEmu::use_rom_file},
        {"use_extension", "Use the extension when starting the game: \"yes\" or \"no\""
		"\n# e.g., yes: \"emulator.exe rom.ext\""
		"\n# no: \"emulator.exe rom\"", &ConfigEmu::use_extension},
        {"use_rom_directory", "Use the ROM directory when launching the game: \"yes\" or \"no\""
		"\n# e.g., yes: \"emulator.exe c:\\full\\path\\rom\\"
		"\n# no: \"emulator.exe rom\"", &ConfigEmu::use_rom_directory},
        {"no_uncompress", "Avoids to uncompress the zip file", &ConfigEmu::no_uncompress},
        {"menu_show_directories", "Show directories in the game list", &ConfigEmu::menu_show_directories},
		{"menu_directory_recursive", "List directory contents recursively", &ConfigEmu::menu_directory_recursive},
        //{"generalConfig", &ConfigEmu::generalConfig}
    };

    for (std::size_t i = 0; i < sizeof(bools)/sizeof(bools[0]); ++i) {
		archivo << "#" << bools[i].descripcion << "\n";
        archivo << bools[i].nombre << " = " << (cfg.*(bools[i].puntero) ? "yes" : "no") << "\n";
    }

    // 3. Escribimos los enteros (aspectRatio, shaderMode, etc.)
    struct MappingInt { const char* nombre; const char* descripcion; const int ConfigEmu::*puntero; };
    MappingInt enteros[] = {
        {"aspectRatio", "Select the screen aspect ratio"
		"\n#AUTO          -1"
		"\n#RATIO_CORE     0"
		"\n#RATIO_4_3      1"
		"\n#RATIO_3_2      2" 
		"\n#RATIO_8_7      3"
		"\n#RATIO_10_9     4"
		"\n#RATIO_1_1      5"
		"\n#RATIO_5_4      6"
		"\n#RATIO_16_9     7"
		"\n#RATIO_16_10    8", &ConfigEmu::aspectRatio},
        {"scaleMode", "Scaler in SW mode. Not used", &ConfigEmu::scaleMode},
        {"integerScale", "Enable or disable the screen integer scale"
		"\n#AUTO                   -1"
		"\n#DISABLED                0"
		"\n#ENABLED                 1", &ConfigEmu::integerScale},
        {"scaleIntMode", "Integer screen scale mode used when integerScale is selected"
		"\n#AUTO                -1"
		"\n#SCALE INT REDUCE	 0"
		"\n#SCALE INT INCREASE   1"
		"\n#SCALE FIXED 1X		 2"
		"\n#SCALE FIXED 2X		 3"
		"\n#SCALE FIXED 3X	     4"
		"\n#SCALE FIXED 4X		 5"
		"\n#SCALE FIXED 5X		 6", &ConfigEmu::scaleIntMode},
		{"syncMode", "Synchronization mode"
		"\n#AUTO           -1"
		"\n#SYNC_TO_AUDIO	0"
		"\n#SYNC_TO_VIDEO   1"
		"\n#SYNC_NONE       2", &ConfigEmu::syncMode}
    };

	//The override list, has the option "auto", which is represented as -1. That's why we subtract 1
    for (std::size_t i = 0; i < sizeof(enteros)/sizeof(enteros[0]); ++i) {
		archivo << "#" << enteros[i].descripcion << "\n";
        archivo << enteros[i].nombre << " = " << (cfg.*(enteros[i].puntero) -1) << "\n";
    }

    archivo.close();

	//Restore the executable to the selected one
	if (cfg.execIdx >= 0 && (std::size_t)cfg.execIdx < cfg.cores.size())
		cfg.executable = cfg.cores[cfg.execIdx]; 

	return LanguageManager::instance()->get("msg.core.cfg.savelocation") + rutaArchivo;
}