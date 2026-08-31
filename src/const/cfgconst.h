#pragma once

#include <string>
#include <map>
#include <vector>

#include <beans/structures.h>

namespace cfg {
	typedef enum {CFG_TYPE_INT = 0, CFG_TYPE_FLOAT, CFG_TYPE_BOOL, CFG_TYPE_STR} CFG_PROPS_TYPES;

	typedef enum {emulators = 0, debug, resolution_width, resolution_height, fullscreen, path_prefix, aspectRatio, packedImages,
			scaleMode, scaleIntMode, syncMode, shaderMode, fastForwardMult, libretrosystem, libretro_save, libretro_state, libretro_core, libretro_core_version, 
			libretro_core_extensions, roms_path,
			showFps, integerScale, animBG, musicVolume, musicEnabled, musicFile,
			mainLang, scrapRegion, scrapLang, scrapOrigin, scrapUser, scrapPass, apikeytgdb, raUser, raPass, enableAchievements, hardcoreRA,
			showEmptyEmulators, overscan_x, overscan_y, resolutionIndex,
			lastOptSel,
			MAIN_CFG_MAX} MAIN_CFG_PROPS_KEYS;

	typedef enum{generalConfig = 0, name,
		EMU_CFG_MAX
	} EMU_CFG_PROPS_KEYS;

	struct t_emu_props {
		std::vector<std::string> values;
		std::vector<std::string> labels;
		std::string description;
		std::string cachedValue;
		std::string category;   // libretro V2 category_key (runtime only, not persisted)
		int selected;
		int defaultSelected;    // indice por defecto declarado por el core (para "restaurar valores por defecto")
		bool isForThisGame;

		t_emu_props() : selected(0), defaultSelected(0), isForThisGame(false) {}

		// Constructor de movimiento
		t_emu_props(t_emu_props&& other) {
			*this = std::move(other);
		}

		// Operador de asignaci�n de movimiento
		t_emu_props& operator=(t_emu_props&& other) {
			if (this != &other) {
				values      = std::move(other.values);
				labels      = std::move(other.labels);
				description = std::move(other.description);
				cachedValue = std::move(other.cachedValue);
				category    = std::move(other.category);
				selected    = other.selected;
				defaultSelected = other.defaultSelected;
				other.selected = 0;
			}
			return *this;
		}

		// VS2010 requiere que mantengas los de copia si los vas a usar
		t_emu_props(const t_emu_props& other)
			: values(other.values), labels(other.labels), description(other.description),
			  cachedValue(other.cachedValue), category(other.category), selected(other.selected),
			  defaultSelected(other.defaultSelected) {}

		t_emu_props& operator=(const t_emu_props& other) {
			if (this != &other) {
				values = other.values;
				labels = other.labels;
				description = other.description;
				cachedValue = other.cachedValue;
				category = other.category;
				selected = other.selected;
				defaultSelected = other.defaultSelected;
			}
			return *this;
		}
	};

	struct t_cfg_emu{
		ConfigEmu config;
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
