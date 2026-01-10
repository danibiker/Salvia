#include "cfgloader.h"
#include "const/constant.h"
#include "io/dirutil.h"
#include "const/cfgconst.h"

#include <sstream>
#include <fstream>
#include <iostream>


CfgLoader::CfgLoader(){
	initMainConfig();
	loadMainConfig();
}

CfgLoader::~CfgLoader(){
}

void CfgLoader::initMainConfig(){
	//Cargamos valores por defecto
	configMain[cfg::emulators] = cfg::t_cfg_props("emulators", "");
	configMain[cfg::debug] = cfg::t_cfg_props("debug", false);
	configMain[cfg::resolution_width] = cfg::t_cfg_props("resolution_width", 1280);
	configMain[cfg::resolution_height] = cfg::t_cfg_props("resolution_height", 720);
	configMain[cfg::path_prefix] = cfg::t_cfg_props("path_prefix", ".\\");
	configMain[cfg::alsaReset] = cfg::t_cfg_props("alsaReset", false);
	configMain[cfg::background_music] = cfg::t_cfg_props("background_music", false);
	configMain[cfg::mp3_file] = cfg::t_cfg_props("mp3_file", "");
	configMain[cfg::aspectRatio] = cfg::t_cfg_props("aspectRatio", (int)RATIO_CORE);
	configMain[cfg::scaleMode] = cfg::t_cfg_props("scaleMode", (int)FULLSCREEN);
	configMain[cfg::syncMode] = cfg::t_cfg_props("syncMode", (int)OPT_SYNC_VIDEO);
	configMain[cfg::soundMode] = cfg::t_cfg_props("soundMode", true);
	configMain[cfg::libretrosystem] = cfg::t_cfg_props("libretrosystem", ".\\system");
	configMain[cfg::region] = cfg::t_cfg_props("region", "auto"); //pal, ntsc, auto
	configMain[cfg::nospritelimit] = cfg::t_cfg_props("nospritelimit", "enabled"); //enabled, disabled
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
	ConfigEmu salviaConfig;
	salviaConfig.generalConfig = true;
	salviaConfig.name = "Options";
	configEmus.push_back(std::unique_ptr<ConfigEmu>(new ConfigEmu(salviaConfig)));
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
	ConfigEmu cfgEmu;
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
			ConfigEmu cfgEmu;

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
						cfgEmu.name = value;
					} else if (key.compare("system") == 0){
						cfgEmu.system = value;
					} else if (key.compare("description") == 0){
						cfgEmu.description = value;
					} else if (key.compare("directory") == 0){
						cfgEmu.directory = value;
					} else if (key.compare("executable") == 0){
						cfgEmu.executable = value;
					} else if (key.compare("global_options") == 0){
						cfgEmu.global_options = value;
					} else if (key.compare("map_file") == 0){
						cfgEmu.map_file = value;
					} else if (key.compare("options_before_rom") == 0){
						cfgEmu.options_before_rom = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("screen_shot_directory") == 0){
						cfgEmu.screen_shot_directory = value;
					} else if (key.compare("assets") == 0){
						cfgEmu.assets = value;
					} else if (key.compare("use_rom_file") == 0){
						cfgEmu.use_rom_file = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("rom_directory") == 0){
						cfgEmu.rom_directory = value;
					} else if (key.compare("rom_extension") == 0){
						cfgEmu.rom_extension = value;
					} else if (key.compare("use_extension") == 0){
						cfgEmu.use_extension = value.compare("yes") == 0 ? true : false;
					} else if (key.compare("use_rom_directory") == 0){
						cfgEmu.use_rom_directory = value.compare("yes") == 0 ? true : false;
					}
				}
			}             
			//cout << "adding emu " << configEmus.size() <<endl;   
			//configEmus.emplace_back(cfgEmu);            
			configEmus.push_back(std::unique_ptr<ConfigEmu>(new ConfigEmu(cfgEmu)));
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