#ifndef CHEATS_CHEATMANAGER_H
#define CHEATS_CHEATMANAGER_H

#include <string>
#include <vector>

// Un cheat leido de un .cht (formato RetroArch). Solo se usan los que tienen
// "code": el core los decodifica via retro_cheat_set (Game Genie / GameShark /
// Action Replay / PAR ...).
//
// Los cheats arrancan SIEMPRE desactivados y no se aplica nada en la carga (igual
// que RetroArch con "Apply Cheats After Load" desactivado). El usuario los activa a
// mano desde el menu, con el juego ya arrancado, y se aplican al instante. Asi se
// evita parchear la ROM antes de su checksum de arranque, que colgaba a juegos como
// Sonic 1 (pantalla roja).
struct Cheat {
	std::string desc;
	std::string code;
	bool        enabled;

	Cheat() : enabled(false) {}
};

// Gestor de cheats por juego (singleton, estilo Achievements::instance()).
// En cores que dejan retro_cheat_set vacio (pcsxr-360, beetle-vb, 3dox/opera,
// dosbox-pure, D3DQuakeX360, FBNeo/MAME) el menu carga pero activar no tiene efecto.
class CheatManager {
public:
	static CheatManager* instance() {
		static CheatManager _instance;
		return &_instance;
	}

	// Carga el .cht de la ruta dada (todos los cheats quedan desactivados).
	// Devuelve el numero de cheats con codigo. Si no existe, lista vacia y 0.
	int loadFromFile(const std::string& path);
	int reload() { return loadFromFile(currentPath); }

	// retro_cheat_reset() + retro_cheat_set() de los cheats habilitados.
	void applyToCore();

	// retro_cheat_reset() + vaciar la lista. Llamar al cerrar el juego, con el
	// core todavia cargado (antes de retro_unload_game).
	void clear();

	std::vector<Cheat>& list()        { return cheats; }
	bool                empty() const { return cheats.empty(); }

	// Nombre (basename) del .cht cargado actualmente; "" si no hay ninguno.
	std::string currentFilename() const {
		std::size_t p = currentPath.find_last_of("\\/");
		return (p == std::string::npos) ? currentPath : currentPath.substr(p + 1);
	}

	// Nombre REAL del .cht en el repo de GitHub que se descargo (lo fija descargarCheats tras
	// una descarga con exito). Se conserva entre recargas; se limpia en clear() (cierre de juego).
	void setSourceName(const std::string& n) { sourceName = n; }

	// Nombre a mostrar en el menu: el real de GitHub si se conoce (descarga), si no el basename
	// local (nombre de ROM, con el que se guarda romPaths.cht).
	std::string displayFilename() const {
		return sourceName.empty() ? currentFilename() : sourceName;
	}

private:
	CheatManager() {}
	CheatManager(const CheatManager&);
	CheatManager& operator=(const CheatManager&);

	std::vector<Cheat> cheats;
	std::string        currentPath;
	std::string        sourceName;   // nombre real del .cht en GitHub (solo tras descarga)
};

#endif // CHEATS_CHEATMANAGER_H
