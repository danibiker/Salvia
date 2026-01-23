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

	int getWidth();
	int getHeight();
	void setWidth(int);
	void setHeight(int);
	bool isDebug();
private:
	static const std::string CONFIGFILE; 

	void initMainConfig();
	void loadMainConfig();
	void loadEmuConfig(std::string);
	int findKeyCfg(const std::string&);
};


