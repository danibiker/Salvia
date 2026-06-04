#pragma once

#include <http/pugixml.hpp>
#include <map>
#include <beans/structures.h>
#include <const/constant.h>

/**
* Parsea un xml custom obtenido de https://raw.githubusercontent.com/RetroPie/EmulationStation/master/resources/mamenames.xml
* Segun parece suele tener todos los nombres del catalogo de mame, fbneo, etc.
* Lo usamos preferentemente, puesto que es muy liviano, aunque no tiene informacion
*/
inline void parse_mame_names(std::string filepath, std::map<std::string, GameData>& mameDatabase) {
	pugi::xml_document doc;
	if (!doc.load_file(filepath.c_str())) return;

	pugi::xml_node root = doc.first_child();
	for (pugi::xml_node game = root; game; game = game.next_sibling()) {
		std::string zipName = game.child_value("mamename");
		if (zipName.empty()) continue;

		GameData data;
		data.setDescription(game.child_value("realname"));

		mameDatabase[zipName] = data;
	}
}

inline uint16_t convertYear(const char *year){
	if (!year || !*year) return 0;
    char buf[5] = {};
    std::size_t len = 0;
	bool foundQuestion = false;

    for (const char* p = year; *p && len < 4; ++p) {
		if (*p == '?'){
			buf[len++] = '0';
			foundQuestion = true;
		} else {
			buf[len++] = *p;
		}
    }
	uint16_t anyo = (uint16_t)atoi(buf);

	if (anyo == 0){
		//Se mostrara como anyo desconocido en gamemenu.cpp
		anyo = 9999;
	} else if (foundQuestion){
		//Para juegos con anyos inciertos, nos vamos a quedar con los dos ultimos digitos del anyo.
		//Por ello vamos a restar 1900
		int anyoCentenar = (int)anyo - 1900;
		if (anyoCentenar <= 0){
			//Hay juegos clasificados en el anyo 1900... wtf
			anyo = 9999;
		} else if (anyoCentenar >= 100){
			//Para juegos posteriores al anyo 2000
			anyo = 20;
		} else {
			//Juegos anteriores al 2000
			anyo = anyoCentenar;
		}
		//LOG_DEBUG("Anyo convertido: %d, original %s, reemplazado %s \n", anyo, year, buf);
	}
	
    return anyo;
}
/**
* Parsea un xml oficial de mame. Para la version 2003+, se utiliza la etiqueta <game> por defecto, pero se puede sustituir
* por la etiqueta <machine> que se usa en las versiones posteriores
*/
inline void parse_mame_xml(std::string filepath, std::map<std::string, GameData>& mameDatabase, const char* nodeName = "game"){
	pugi::xml_document doc;
	if (!doc.load_file(filepath.c_str())) return;

	pugi::xml_node mame = doc.child("mame");
	for (pugi::xml_node game = mame.child(nodeName); game; game = game.next_sibling(nodeName)) 
	{
		std::string zipName = game.attribute("name").value();
		GameData data;

		data.setCloneof(game.attribute("cloneof").value());
		data.setRomof(game.attribute("romof").value());
		data.setSourcefile(game.attribute("sourcefile").value());

		data.setDescription(game.child_value("description"));
		data.year = convertYear(game.child_value("year"));
		data.setManufacturer(game.child_value("manufacturer"));

		//data.driverStatus = game.child("driver").attribute("status").value();

		mameDatabase[zipName] = data;
	}
}

/**
* Guardamos un xml que contenga solo la informacion necesaria en el mismo formato que las
* versiones oficiales de MAME, solo que unicamente con los campos que nos interesan
*/
inline void write_mame_xml(const std::string& filepath, const std::map<std::string, GameData>& mameDatabase) {
	pugi::xml_document doc;

	pugi::xml_node mame = doc.append_child("mame");
	mame.append_attribute("build") = " Salvia";

	for (std::map<std::string, GameData>::const_iterator it = mameDatabase.begin(); it != mameDatabase.end(); ++it) {
		const std::string& zipName = it->first;
		const GameData& data = it->second;

		pugi::xml_node game = mame.append_child("game");
		game.append_attribute("name").set_value(zipName.c_str());

		
		game.append_attribute("cloneof").set_value(data.cloneof);
		game.append_attribute("romof").set_value(data.romof);
		game.append_attribute("sourcefile").set_value(data.sourcefile);

		game.append_child("description").append_child(pugi::node_pcdata).set_value(data.description);
		game.append_child("year").text().set(data.year);
		game.append_child("manufacturer").append_child(pugi::node_pcdata).set_value(data.manufacturer);

		pugi::xml_node driver = game.append_child("driver");
		//driver.append_attribute("status").set_value(data.driverStatus.c_str());
	}
	doc.save_file(filepath.c_str(), "  ", pugi::format_indent | pugi::format_write_bom, pugi::encoding_utf8);
}

