#pragma once

#include <http/pugixml.hpp>
#include <map>
#include <beans/structures.h>
#include <const/constant.h>

inline void parse_mame_xml(std::string filepath, std::map<std::string, GameData>& mameDatabase){
	pugi::xml_document doc;
	if (!doc.load_file(filepath.c_str())) return;

	pugi::xml_node mame = doc.child("mame");
	for (pugi::xml_node game = mame.child("game"); game; game = game.next_sibling("game")) 
	{
		std::string zipName = game.attribute("name").value();
		GameData data;

		// Atributos del nodo <game>
		data.cloneof = game.attribute("cloneof").value();
		data.romof   = game.attribute("romof").value();

		// Nodos hijos
		data.description  = game.child_value("description");
		data.year         = game.child_value("year");
		data.manufacturer = game.child_value("manufacturer");

		// Atributos del nodo <driver>
		data.driverStatus = game.child("driver").attribute("status").value();

		// Guardar en el map
		mameDatabase[zipName] = data;
	}
}