#include "cfgloader.h"
#include <utils/langmanager.h>
#include <const/constant.h>
#include <const/cfgconst.h>
#include <http/pugixml.hpp>
#include <io/filelist.h>
#include <io/dirutil.h>
#include <io/fileio.h>

#include <libretro/libretro.h>

#include <sstream>
#include <fstream>
#include <iostream>

extern "C"{
	void retro_get_system_info(struct retro_system_info *info);
}

const std::string CfgLoader::coreDefault = "core.default.";
cfg::t_cfg_props CfgLoader::configMain [cfg::MAIN_CFG_MAX];

CfgLoader::CfgLoader(){
	emuCfgPos = 0;
	idxRegion = 0;
	idxIdioma = 0;
	initMainConfig();
	loadMainConfig();
	loadCoreParams();
}

CfgLoader::~CfgLoader(){
}

void CfgLoader::initMainConfig(){
	dirutil dir;
	//Cargamos valores por defecto
	configMain[cfg::emulators] = cfg::t_cfg_props("emulators", "");
	configMain[cfg::debug] = cfg::t_cfg_props("debug", false);
	configMain[cfg::resolution_width] = cfg::t_cfg_props("resolution_width", 1280);
	configMain[cfg::resolution_height] = cfg::t_cfg_props("resolution_height", 720);
	configMain[cfg::path_prefix] = cfg::t_cfg_props("path_prefix", dir.getDirActual() + Constant::getFileSep());
	configMain[cfg::alsaReset] = cfg::t_cfg_props("alsaReset", false);
	configMain[cfg::background_music] = cfg::t_cfg_props("background_music", false);
	configMain[cfg::mp3_file] = cfg::t_cfg_props("mp3_file", "");
	configMain[cfg::aspectRatio] = cfg::t_cfg_props("aspectRatio", (int)RATIO_CORE);
	configMain[cfg::scaleMode] = cfg::t_cfg_props("scaleMode", (int)FULLSCREEN);
	configMain[cfg::syncMode] = cfg::t_cfg_props("syncMode", (int)OPT_SYNC_VIDEO);
	configMain[cfg::soundMode] = cfg::t_cfg_props("soundMode", true);
	configMain[cfg::libretrosystem] = cfg::t_cfg_props("libretrosystem", dir.getDirActual() + Constant::getFileSep() + "system");
	configMain[cfg::libretro_save] = cfg::t_cfg_props("libretro_save", dir.getDirActual() + Constant::getFileSep() + "data" + Constant::getFileSep() + "saves");
	configMain[cfg::libretro_state] = cfg::t_cfg_props("libretro_state", dir.getDirActual() + Constant::getFileSep() + "data" + Constant::getFileSep() + "states");
	configMain[cfg::libretro_lang] = cfg::t_cfg_props("libretro_lang", (int)RETRO_LANGUAGE_SPANISH);
	configMain[cfg::showFps] = cfg::t_cfg_props("showFps", false);
	configMain[cfg::forceFS] = cfg::t_cfg_props("forceFS", true);
	configMain[cfg::animBG] = cfg::t_cfg_props("animBG", (int)BG_WAVES);
	configMain[cfg::apikeytgdb] = cfg::t_cfg_props("apikey.tgdb", "");
	configMain[cfg::mainLang] = cfg::t_cfg_props("mainLang", "");
	configMain[cfg::scrapRegion] = cfg::t_cfg_props("scrapRegion", "");
	configMain[cfg::scrapLang] = cfg::t_cfg_props("scrapLang", "");
	configMain[cfg::scrapOrigin] = cfg::t_cfg_props("scrapOrigin", (int)SC_SCREENCSRAPER);
	configMain[cfg::enableAchievements] = cfg::t_cfg_props("enableAchievements", true);
	configMain[cfg::hardcoreRA] = cfg::t_cfg_props("hardcoreRA", true);
	configMain[cfg::raUser] = cfg::t_cfg_props("raUser", "");
	configMain[cfg::raPass] = cfg::t_cfg_props("raPass", "");
	configMain[cfg::coreGenesis] = cfg::t_cfg_props(coreDefault + "genesis", (int)0);
	configMain[cfg::coreSnes] = cfg::t_cfg_props(coreDefault + "snes", (int)0);
	configMain[cfg::corePce] = cfg::t_cfg_props(coreDefault + "pce", (int)0);
	configMain[cfg::corePceCd] = cfg::t_cfg_props(coreDefault + "pcecd", (int)0);

	struct retro_system_info info;
	memset(&info, 0, sizeof(info));
	retro_get_system_info(&info);
	configMain[cfg::libretro_core].setPropValue(std::string(info.library_name));
	configMain[cfg::libretro_core_version].setPropValue(std::string(info.library_version));
	configMain[cfg::libretro_core_extensions].setPropValue(std::string(info.valid_extensions));
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
	// Opción para C++11 (donde no existe make_unique)
	std::unique_ptr<cfg::t_cfg_emu> salviaConfig(new cfg::t_cfg_emu);
	salviaConfig->config.generalConfig = true;
	salviaConfig->config.name = "Options";
	emulators.push_back(std::move(salviaConfig));
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

    // VS2010: Construcción manual del nombre del nodo
    std::string nombreNodo = "nom_" + isoCode;

    // Acceso al nodo raíz
    pugi::xml_node langues = doc.child("Data").child("langues");

    // Iteración compatible con C++03 (Visual Studio 2010)
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

    // VS2010: Construcción manual del nombre del nodo
    std::string nombreNodo = "nom_" + isoCode;

    // Acceso al nodo raíz
    pugi::xml_node langues = doc.child("Data").child("regions");

    // Iteración compatible con C++03 (Visual Studio 2010)
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

unsigned int CfgLoader::findConfigIndex(std::string propName){
	unsigned int selectedIndex = 0;
	for (int i=cfg::coreGenesis; i < cfg::MAIN_CFG_MAX; i++){
		if (configMain[i].name == propName){
			selectedIndex = i;
			break;
		}
	}
	return selectedIndex;
}

void CfgLoader::getExecutables(std::string str, cfg::t_cfg_emu* emu){
	Constant::splitChar(str, ';', emu->config.cores);
	std::string emuInternalName = Constant::Trim(emu->config.internalName);
	unsigned int selectedIndex = findConfigIndex(coreDefault + emuInternalName);
	//Si tiene configurado un emulador por defecto, lo obtenemos
	unsigned int selectedValue = 0;
	if (selectedIndex < cfg::MAIN_CFG_MAX && selectedIndex > 0){
		selectedValue = configMain[selectedIndex].valueInt;		
	} 
	if (selectedValue < emu->config.cores.size()){
		emu->config.executable = emu->config.cores[selectedValue];
	}
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

	for (int i=0; i < cfg::MAIN_CFG_MAX; i++){
		if (configMain[i].name.empty()) continue;

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

	std::string corepath = getCoreCfgPath();
	FileList::guardarVector(corepath, fileCoreCfg);
	return LanguageManager::instance()->get("msg.cfg.savelocation") + corepath;
}

void CfgLoader::loadCoreParams(){
	std::string corepath = getCoreCfgPath();
	std::vector<std::string> fileConfig;
	FileList::cargarVector(corepath, fileConfig);

	if (fileConfig.empty()) return;

	if (fileConfig.size() > 0){
		std::string linea = "";
		std::size_t pos = 0;

        for (unsigned int i=0; i<fileConfig.size(); i++){
            linea = fileConfig.at(i);
			if (linea.empty() || linea[0] == '#') continue;

			if ((pos = linea.find("=")) != std::string::npos){
				cfg::t_emu_props *ptr = new cfg::t_emu_props();
				std::string value = Constant::Trim(linea.substr(pos + 1));
				ptr->selected = Constant::strToTipo<int>(value);
				startupLibretroParams[linea.substr(0, pos)] = std::unique_ptr<cfg::t_emu_props>(ptr);
			}
		}
	}
}

std::string CfgLoader::getCoreCfgPath(){
	std::size_t last = configMain[cfg::path_prefix].valueStr.length() <= 0 ? 0 : configMain[cfg::path_prefix].valueStr.length() - 1;
	bool lastFileSep = true;
	if (last < configMain[cfg::path_prefix].valueStr.length()){
		configMain[cfg::path_prefix].valueStr[last] = Constant::getFileSep()[0];
	}

	return configMain[cfg::path_prefix].valueStr + (lastFileSep ? "" : Constant::getFileSep()) + 
		"config" + Constant::getFileSep() + "core_" + configMain[cfg::libretro_core].valueStr + ".cfg";
}
