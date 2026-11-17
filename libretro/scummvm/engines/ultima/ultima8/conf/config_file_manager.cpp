/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ultima/ultima8/conf/config_file_manager.h"

#include "common/debug.h"
#include "common/file.h"

namespace Ultima {
namespace Ultima8 {

ConfigFileManager *ConfigFileManager::_configFileManager = nullptr;

ConfigFileManager::ConfigFileManager() {
	debug(1, "Creating ConfigFileManager...");

	_configFileManager = this;
}

ConfigFileManager::~ConfigFileManager() {
	debug(1, "Destroying ConfigFileManager...");

	clear();
	_configFileManager = nullptr;
}

bool ConfigFileManager::readConfigFile(const Common::Path &fname, const Common::String &category) {
	Common::File f;
	if (!f.open(fname))
		return false;

	ConfigFile *configFile = new ConfigFile();
	configFile->_category = category;

	// We need various characters as the inis are used for translations.
	configFile->_iniFile.allowNonEnglishCharacters();

	if (!configFile->_iniFile.loadFromStream(f)) {
		delete configFile;
		return false;
	}

	_configFiles.push_back(configFile);
	return true;
}

void ConfigFileManager::clear() {
	for (auto rfIt66 = _configFiles.begin(), rfEnd66 = _configFiles.end(); rfIt66 != rfEnd66; ++rfIt66) {
		auto *i = *rfIt66;
		delete i;
	}
	_configFiles.clear();
}

void ConfigFileManager::clearRoot(const Common::String &category) {
	auto i = _configFiles.begin();

	while (i != _configFiles.end()) {
		if (category.equalsIgnoreCase((*i)->_category)) {
			delete(*i);
			i = _configFiles.erase(i);
		} else {
			++i;
		}
	}
}

bool ConfigFileManager::get(const Common::String &category, const Common::String &section, const Common::String &key, Common::String &ret) const {
	for (int i = _configFiles.size() - 1; i >= 0; --i) {
		const ConfigFile *file = _configFiles[i];
		if (category.equalsIgnoreCase(file->_category)) {
			if (file->_iniFile.getKey(key, section, ret)) {
				return true;
			}
		}
	}

	return false;
}


bool ConfigFileManager::get(const Common::String &category, const Common::String &section, const Common::String &key, int &ret) const {
	Common::String stringval;
	if (!get(category, section, key, stringval))
		return false;

	ret = strtol(stringval.c_str(), 0, 0);
	return true;
}

bool ConfigFileManager::get(const Common::String &category, const Common::String &section, const Common::String &key, bool &ret) const {
	Common::String stringval;
	if (!get(category, section, key, stringval))
		return false;

	ret = (stringval == "yes" || stringval == "true");
	return true;
}

Common::Array<Common::String> ConfigFileManager::listSections(const Common::String &category) const {
	Common::Array<Common::String> sections;
	for (auto rfIt119 = _configFiles.begin(), rfEnd119 = _configFiles.end(); rfIt119 != rfEnd119; ++rfIt119) {
		const auto *i = *rfIt119;
		if (category.equalsIgnoreCase(i->_category)) {
			Common::INIFile::SectionList sectionList = i->_iniFile.getSections();
			for (auto rfIt122 = sectionList.begin(), rfEnd122 = sectionList.end(); rfIt122 != rfEnd122; ++rfIt122) {
				const auto &j = *rfIt122;
				sections.push_back(j.name);
			}
		}
	}

	return sections;
}

KeyMap ConfigFileManager::listKeyValues(const Common::String &category, const Common::String &section) const {
	KeyMap values;
	for (auto rfIt133 = _configFiles.begin(), rfEnd133 = _configFiles.end(); rfIt133 != rfEnd133; ++rfIt133) {
		const auto *c = *rfIt133;
		if (category.equalsIgnoreCase(c->_category) && c->_iniFile.hasSection(section)) {
			Common::INIFile::SectionKeyList keys = c->_iniFile.getKeys(section);
			for (auto rfIt136 = keys.begin(), rfEnd136 = keys.end(); rfIt136 != rfEnd136; ++rfIt136) {
				const auto &j = *rfIt136;
				values[j.key] = j.value;
			}
		}
	}

	return values;
}

} // End of namespace Ultima8
} // End of namespace Ultima
