#pragma once

#include "beans/structures.h"
#include "const/cfgconst.h"

#include <string>
#include <vector>
#include <map>

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

private:
	static const std::string CONFIGFILE; 
	void initMainConfig();
	void loadMainConfig();
	void loadEmuConfig(std::string);
	int findKeyCfg(const std::string&);
	std::string getCoreCfgPath();
};


