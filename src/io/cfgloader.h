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

	static cfg::t_cfg_props configMain [cfg::MAIN_CFG_MAX];
	static std::string appliedFileParmsCore;

	std::vector<std::unique_ptr<cfg::t_cfg_emu>> emulators;
	std::map<std::string, std::unique_ptr<cfg::t_emu_props> > startupLibretroParams;
	std::map<std::string, std::unique_ptr<cfg::t_emu_props> > gameSpecificLibretroParams;
	// libretro V2 core-option categories (category_key -> display desc), kept in the
	// order the core declares them. Runtime only, not persisted; used to build the
	// per-category submenus in the core options menu.
	std::vector<std::pair<std::string, std::string> > libretroCategories;
	std::vector<std::string> musicFiles;

	std::string saveCoreParams();
	void loadCoreParams();
	// [XBOX360] Opciones del core POR JUEGO: fichero junto al juego con su mismo
	// nombre base + CORE_OPT_EXT. En la carga, si existe se aplican en lugar de
	// las generales del core. Ver launchGame (salvia.cpp).
	std::string getGameCoreCfgPath(const std::string& gamePath);
	std::string saveGameCoreParams(const std::string& gamePath);
	void loadCoreParamsForGame(const std::string& gamePath);
	// Resetea startupLibretroParams a los defaults declarados por el core.
	void resetCoreParamsToDefaults();
	// Aplica un fichero de opciones (key=indice) actualizando `selected` in-place
	// sobre las entradas existentes (crea minima si falta). Devuelve true si existia.
	bool applyCoreParamsFile(const std::string& path);
	std::string saveMainParams();
	std::string saveCoreOverrideParams(int emuIdx);
	void findAllBgMusic();
	
	bool deleteCoreParams();
	bool deleteGameParams(const std::string& gamePath);
	//unsigned int findConfigIndex(std::string);
	
	int getWidth();
	int getHeight();
	void setWidth(int);
	void setHeight(int);
	bool isDebug();

	ConfigEmu *getNextCfgEmu();
    ConfigEmu *getPrevCfgEmu();
	ConfigEmu *getCfgEmu();
	ConfigEmu *findCfgEmu(std::string execName);
	std::string getCoreCfgPath(bool save=false);
	int recoverGameMenuPos(struct ListStatus &);

	std::map<std::string, std::unique_ptr<cfg::t_emu_props> >& getLibretroParams();
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
	// Traduce el nombre de preset guardado (assets\shaders) al indice vivo
	// que usan el menu y XBOX_SelectEffect, con migracion de los valores
	// numericos 0..12 de versiones anteriores.
	void resolveShaderModes();
	int findKeyCfg(const std::string&);
	void checkSystemLang();
	
	void parsearIdiomas(const char*, const std::string&, std::vector<FieldIdDesc>&);
	void parsearRegiones(const char*, const std::string&, std::vector<FieldIdDesc>&);
	void getExecutables(std::string, cfg::t_cfg_emu*);
	std::string getExecutablesStringOrdered(const ConfigEmu& cfg);
};


