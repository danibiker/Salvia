#pragma once

#include <string>
#include <map>

namespace cfg {
	typedef enum {CFG_TYPE_INT = 0, CFG_TYPE_FLOAT, CFG_TYPE_BOOL, CFG_TYPE_STR} CFG_PROPS_TYPES;

	typedef enum {emulators = 0, debug, resolution_width, resolution_height, path_prefix, alsaReset, background_music, mp3_file, aspectRatio, 
			scaleMode, syncMode, soundMode, libretrosystem, libretro_lang, libretro_save, libretro_state, libretro_core, libretro_core_version, 
			libretro_core_extensions,
			showFps, forceFS, animBG,
			mainLang, scrapRegion, scrapLang, scrapOrigin, apikeytgdb, raUser, raPass, enableAchievements, hardcoreRA,
			coreGenesis, coreSnes, corePce, corePceCd,
			MAIN_CFG_MAX} MAIN_CFG_PROPS_KEYS;

	typedef enum{generalConfig = 0, name,
		EMU_CFG_MAX
	} EMU_CFG_PROPS_KEYS;

	struct t_emu_props{
		std::vector<std::string> values;
		std::string description;
		int selected;

		t_emu_props(){
			selected = 0;
		}
	};

	struct t_controller_port {
		int current_device_id;			// ID seleccionado actualmente (ej. RETRO_DEVICE_JOYPAD)
		std::string current_desc;       // Descripción amigable (ej. "SuperScope")
		// Lista de opciones que el core nos dio para este puerto
		std::vector<std::pair<unsigned, std::string>> available_types; 
		t_controller_port(){
			current_device_id = -1;
		}	
	};

	struct t_cfg_emu{
		ConfigEmu config;
		//std::map<std::string, std::unique_ptr<cfg::t_emu_props>> libretroParams;
		// Un array para los puertos soportados (normalmente 2 a 5)
		t_controller_port g_ports[MAX_PLAYERS];
	};

	struct t_cfg_props{
		float valueFloat;    // 4 bytes
		int valueInt;        // 4 bytes
		cfg::CFG_PROPS_TYPES type; // 4 bytes (generalmente)
		bool valueBool;      // 1 byte (+3 padding)
		std::string name;    // Objeto complejo
		std::string desc;    // Objeto complejo
		std::string valueStr;// Objeto complejo
		
		t_cfg_props() : type(CFG_TYPE_INT), valueInt(0), valueFloat(0.f), valueBool(false), name(""), desc(""), valueStr("") {}


		t_cfg_props(std::string pstr, int val) : name(pstr), type(CFG_TYPE_INT), valueInt(val), valueFloat(0.f), valueBool(false), valueStr("") {};
		t_cfg_props(std::string pstr, float val) : name(pstr), type(CFG_TYPE_FLOAT), valueFloat(val), valueInt(0), valueBool(false), valueStr("") {};
		t_cfg_props(std::string pstr, bool val) : name(pstr), type(CFG_TYPE_BOOL), valueBool(val), valueInt(0), valueFloat(0.f), valueStr("") {};
		t_cfg_props(std::string pstr, std::string val) : name(pstr), type(CFG_TYPE_STR), valueStr(val), valueInt(0), valueFloat(0.f), valueBool(false) {};
		t_cfg_props(std::string pstr, const char * val) : name(pstr), type(CFG_TYPE_STR), valueStr(val), valueInt(0), valueFloat(0.f), valueBool(false) {};

		int&   getIntRef()   { return valueInt; }
		float& getFloatRef() { return valueFloat; }
		bool&  getBoolRef()  { return valueBool; }
		std::string getStringRef()  { return valueStr; }

		void getPropValue(int& output) {
			output = this->valueInt;
		}

		void getPropValue(float& output) {
			output = this->valueFloat;
		}

		void getPropValue(bool& output) {
			output = this->valueBool;
		}

		void getPropValue(std::string& output) {
			output = this->valueStr;
		}

		void setPropValue(int input) {
			this->valueInt = input;
		}

		void setPropValue(float input) {
			this->valueFloat = input;
		}

		void setPropValue(bool input) {
			this->valueBool = input;
		}

		void setPropValue(std::string input) {
			this->valueStr = input;
		}
	};
};
