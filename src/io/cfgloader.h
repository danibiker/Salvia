#pragma once

#include "beans/structures.h"
#include "const/cfgconst.h"

#include <string>
#include <vector>
#include <map>

struct FieldIdDesc{
	int id;
	std::string shortName;
	std::string desc;

	FieldIdDesc(int pid, std::string pshortName, std::string pdesc){
		id = pid;
		desc = pdesc;
		shortName = pshortName;
	}
};

class CfgLoader{
public:
	CfgLoader();
	~CfgLoader();

	cfg::t_cfg_props configMain [cfg::MAIN_CFG_MAX];
	std::vector<std::unique_ptr<cfg::t_cfg_emu>> emulators;
	std::map<std::string, std::unique_ptr<cfg::t_emu_props>> startupLibretroParams;
	cfg::t_controller_port g_ports[MAX_PLAYERS];
	std::string saveCoreParams();
	void loadCoreParams();

	std::string saveMainParams();
	
	int getWidth();
	int getHeight();
	void setWidth(int);
	void setHeight(int);
	bool isDebug();

	ConfigEmu *getNextCfgEmu();
    ConfigEmu *getPrevCfgEmu();
	ConfigEmu *getCfgEmu();
	std::map<std::string, std::unique_ptr<cfg::t_emu_props>>& getLibretroParams();
	int emuCfgPos;

	std::vector<FieldIdDesc> region;
	std::vector<FieldIdDesc> idioma;
	int idxRegion;
	int idxIdioma;

private:
	static const std::string CONFIGFILE; 
	void initMainConfig();
	void loadMainConfig();
	void loadEmuConfig(std::string);
	int findKeyCfg(const std::string&);
	void checkSystemLang();
	std::string getCoreCfgPath();
	void parsearIdiomas(const char*, const std::string&, std::vector<FieldIdDesc>&);
	void parsearRegiones(const char*, const std::string&, std::vector<FieldIdDesc>&);

};


