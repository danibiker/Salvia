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
	//ConfigMain configMain;
	cfg::t_cfg_props configMain [cfg::MAIN_CFG_MAX];

	std::vector<std::unique_ptr<ConfigEmu>> configEmus;

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


